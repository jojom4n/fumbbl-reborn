// =============================================================================
// GameModel — implementation (see gamemodel.h for references)
// =============================================================================
#include "core/gamemodel.h"

#include <QJsonDocument>

namespace {

// The server sends playerIds as strings ("18109690") but be tolerant of
// numbers: normalize everything to a string key.
QString playerIdKey(const QJsonValue &v)
{
    if (v.isDouble())
        return QString::number(static_cast<qint64>(v.toDouble()));
    return v.toString();
}

// Coordinate value: a JSON array [x, y] (integers). Returns false if the
// value is not a 2-element numeric array (e.g. null = ball out of play).
bool readCoords(const QJsonValue &value, int &x, int &y)
{
    if (!value.isArray())
        return false;
    const QJsonArray a = value.toArray();
    if (a.size() < 2 || !a.at(0).isDouble() || !a.at(1).isDouble())
        return false;
    x = static_cast<int>(a.at(0).toDouble());
    y = static_cast<int>(a.at(1).toDouble());
    return true;
}

// Enums on the wire are usually {"name": ...} but some (PlayerAction in
// actingPlayerSetPlayerAction, verified live) are PLAIN strings.
QString enumNameValue(const QJsonValue &v)
{
    if (v.isObject())
        return v.toObject().value(QStringLiteral("name")).toString();
    if (v.isString())
        return v.toString();
    return QString();
}

// The moving PlayerActions (port of ffb-common PlayerAction.isMoving(),
// plus standUpBlitz — it stands up AND blitz-moves). Wire names are the
// enum's lowercase fName ("move", "blitzMove", ...).
bool isMovingActionName(const QString &n)
{
    static const QSet<QString> kMoving = {
        QStringLiteral("move"),                  QStringLiteral("blitzMove"),
        QStringLiteral("handOverMove"),          QStringLiteral("passMove"),
        QStringLiteral("foulMove"),              QStringLiteral("throwTeamMateMove"),
        QStringLiteral("kickTeamMateMove"),      QStringLiteral("gazeMove"),
        QStringLiteral("putridRegurgitationMove"), QStringLiteral("kickEmBlitz"),
        QStringLiteral("secureTheBall"),         QStringLiteral("puntMove"),
        QStringLiteral("standUpBlitz"),
    };
    return kMoving.contains(n);
}

int readInt(const QJsonValue &v)
{
    if (v.isDouble())
        return static_cast<int>(v.toDouble());
    return v.toString().toInt();
}

// Absolute team logo URL: the game state carries a relative logoUrl
// ("i/<id>") and a CDN base (baseIconPath, e.g. "https://cdn.fumbbl.com/").
QString teamLogoUrl(const QJsonObject &team)
{
    const QString rel = team.value(QStringLiteral("logoUrl")).toString();
    if (rel.isEmpty())
        return QString();
    if (rel.startsWith(QLatin1String("http")))
        return rel;
    QString base = team.value(QStringLiteral("baseIconPath")).toString();
    if (base.isEmpty())
        base = QStringLiteral("https://cdn.fumbbl.com/");
    if (!base.endsWith(QLatin1Char('/')))
        base += QLatin1Char('/');
    return base + rel;
}

} // namespace

GameModel::GameModel(QObject *parent) : QObject(parent)
{
}

// -----------------------------------------------------------------------------
// Initialization (port of fumbblGameModel.ts initialize())
// -----------------------------------------------------------------------------
void GameModel::clearState()
{
    m_initialized = false;
    m_teamHomeName.clear();
    m_teamAwayName.clear();
    m_teamHomeId.clear();
    m_teamAwayId.clear();
    m_teamHomeRace.clear();
    m_teamAwayRace.clear();
    m_teamHomeCoach.clear();
    m_teamAwayCoach.clear();
    m_teamHomeLogoUrl.clear();
    m_teamAwayLogoUrl.clear();
    m_reRollsHome = 0;
    m_reRollsAway = 0;
    m_scoreHome = 0;
    m_scoreAway = 0;
    m_turn = 1;
    m_half = 0;
    m_turnMode = QStringLiteral("setup");
    m_homePlaying = true;
    m_playerIndex.clear();
    m_players.clear();
    m_rosterHomeIds.clear();
    m_rosterAwayIds.clear();
    m_positionNameHome.clear();
    m_positionNameAway.clear();
    m_positionShortHome.clear();
    m_positionShortAway.clear();
    m_positionKeywordsHome.clear();
    m_positionKeywordsAway.clear();
    m_activePlayerId.clear();
    m_ballOnField = false;
    m_ballX = -1;
    m_ballY = -1;
    m_weather.clear();
    m_traceActor.clear();
    clearMoveTrace();
    m_trackNumbers.clear();
    m_actedThisTurn.clear();
    m_actorActivated = false;
    m_passTargetId.clear();
    m_blitzingPlayerId.clear();
}

void GameModel::reset()
{
    clearState();
    emit stateUpdated();
}

void GameModel::clearMoveTrace()
{
    m_moveSegments.clear();
    m_blockStepMarks.clear();
    m_nextStepNumber = 1;
    m_driveUnnumbered = false;
    m_blockResolved = false;
    m_gfiPendingFor.clear();
    m_movePending = false;
}

// Flat cell view of the segments (kept for the tests).
QVector<QVector<QPoint>> GameModel::moveTrace() const
{
    QVector<QVector<QPoint>> v;
    for (const MoveSegment &s : m_moveSegments)
        v.append(s.cells);
    return v;
}

// `attacker` empty = the current actor at arming time (the block reports
// arrive while the attacker is the actor).
void GameModel::setBlockTargetId(const QString &playerId,
                                 const QString &attacker)
{
    m_blockTargetId = playerId;
    m_blockGlowOnly = false; // a block is not glow-only
    if (!m_blockTargetId.isEmpty())
        m_blockAttackerId = attacker.isEmpty() ? m_activePlayerId : attacker;
}

// Reserve the drive's next number for a block and record
// where its number is painted (between the attacker's cell and the
// defender's cell). A re-send of the `block` report for the SAME still-
// open block (a server step retry outliving the UI's dedup window) is
// dropped; a FRENZY second block (the previous block RESOLVED —
// gameSetDefenderId null landed) always gets its own number, even against
// the same defender.
void GameModel::noteBlockStep(const QString &defenderId, const QString &attackerId)
{
    const QString att = attackerId.isEmpty() ? m_activePlayerId : attackerId;
    if (defenderId.isEmpty() || att.isEmpty() || att != m_traceActor)
        return;
    const FFBPlayer *a = findPlayerById(att);
    const FFBPlayer *d = findPlayerById(defenderId);
    if (!a || !d || !isOnField(a->fieldX, a->fieldY)
        || !isOnField(d->fieldX, d->fieldY))
        return;
    const QPoint from(a->fieldX, a->fieldY);
    const QPoint to(d->fieldX, d->fieldY);
    // A PLAIN block
    // action — the drive STARTED with this block, no movement before it —
    // is UNNUMBERED for its whole lifetime: no block mark, no follow-step
    // numbers, just the trail. A BLITZ (a movement before the block) is
    // numbered: the block takes the next number.
    if (m_driveUnnumbered || m_moveSegments.isEmpty()) {
        m_driveUnnumbered = true;
        return;
    }
    if (!m_blockResolved && m_blockTargetId == defenderId
        && !m_blockStepMarks.isEmpty()
        && m_blockStepMarks.last().from == from
        && m_blockStepMarks.last().to == to)
        return; // the re-send of the same open block
    m_blockResolved = false;
    m_blockStepMarks.append({from, to, m_nextStepNumber++});
}

// A successful go-for-it roll — the player's next NEW route is the extra
// move, drawn red.
void GameModel::setGoForItPending(const QString &playerId)
{
    if (!playerId.isEmpty() && playerId == m_traceActor)
        m_gfiPendingFor = playerId;
}

bool GameModel::initialize(const QJsonObject &game)
{
    // Reset live state.
    clearState();

    const QJsonObject home = game.value(QStringLiteral("teamHome")).toObject();
    const QJsonObject away = game.value(QStringLiteral("teamAway")).toObject();

    if (home.isEmpty() && away.isEmpty())
        return false; // not a game state

    m_teamHomeName = home.value(QStringLiteral("teamName")).toString();
    m_teamAwayName = away.value(QStringLiteral("teamName")).toString();
    // The team id lives in `teamId`, not `id` (verified on the live state):
    // with the wrong key every team reference in the reports (prayers,
    // bribes, throw-ins, block rolls...) stays an unresolved numeric id.
    m_teamHomeId = !home.value(QStringLiteral("teamId")).toString().isEmpty()
                       ? home.value(QStringLiteral("teamId")).toString()
                       : home.value(QStringLiteral("id")).toString();
    m_teamAwayId = !away.value(QStringLiteral("teamId")).toString().isEmpty()
                       ? away.value(QStringLiteral("teamId")).toString()
                       : away.value(QStringLiteral("id")).toString();
    m_teamHomeRace = home.value(QStringLiteral("race")).toString();
    m_teamAwayRace = away.value(QStringLiteral("race")).toString();
    m_teamHomeCoach = home.value(QStringLiteral("coach")).toString();
    m_teamAwayCoach = away.value(QStringLiteral("coach")).toString();
    // Logo: relative path ("i/<id>") against the team's icon CDN base.
    m_teamHomeLogoUrl = teamLogoUrl(home);
    m_teamAwayLogoUrl = teamLogoUrl(away);
    m_reRollsHome = readInt(home.value(QStringLiteral("reRolls")));
    m_reRollsAway = readInt(away.value(QStringLiteral("reRolls")));
    // The colored header dots follow the CURRENT-TURN remaining rerolls
    // (turnData.reRolls; capture-verified: team total vs per-turn values).
    m_turnReRollsHome = readInt(
        game.value(QStringLiteral("turnDataHome")).toObject()
            .value(QStringLiteral("reRolls")));
    m_turnReRollsAway = readInt(
        game.value(QStringLiteral("turnDataAway")).toObject()
            .value(QStringLiteral("reRolls")));

    // Position lookup first: live players carry only the positionId.
    parseRosterPositions(home, true);
    parseRosterPositions(away, false);

    parsePlayerArray(home.value(QStringLiteral("playerArray")).toArray(), true);
    parsePlayerArray(away.value(QStringLiteral("playerArray")).toArray(), false);

    // Field model sets coordinates + states for players on the field.
    applyFieldModel(game.value(QStringLiteral("fieldModel")).toObject());

    if (game.value(QStringLiteral("actingPlayer")).isObject())
        m_activePlayerId = playerIdKey(
            game.value(QStringLiteral("actingPlayer")).toObject().value(QStringLiteral("playerId")));

    if (game.contains(QStringLiteral("half")))
        m_half = readInt(game.value(QStringLiteral("half")));
    if (game.contains(QStringLiteral("homePlaying")))
        m_homePlaying = game.value(QStringLiteral("homePlaying")).toBool(true);

    const QJsonObject turnModeObj = game.value(QStringLiteral("turnMode")).toObject();
    if (turnModeObj.contains(QStringLiteral("name")))
        m_turnMode = turnModeObj.value(QStringLiteral("name")).toString();
    else if (game.value(QStringLiteral("turnMode")).isString())
        m_turnMode = game.value(QStringLiteral("turnMode")).toString();

    // Turn number: home's turnNr if set, else away's (TS reference).
    const int turnNrHome = game.value(QStringLiteral("turnDataHome")).toObject().value(QStringLiteral("turnNr")).toInt(0);
    const int turnNrAway = game.value(QStringLiteral("turnDataAway")).toObject().value(QStringLiteral("turnNr")).toInt(0);
    m_turn = turnNrHome != 0 ? turnNrHome : (turnNrAway != 0 ? turnNrAway : 1);

    // Score: top-level score object first, then per-team scores override.
    const QJsonObject score = game.value(QStringLiteral("score")).toObject();
    if (score.contains(QStringLiteral("team1")))
        m_scoreHome = readInt(score.value(QStringLiteral("team1")));
    if (score.contains(QStringLiteral("team2")))
        m_scoreAway = readInt(score.value(QStringLiteral("team2")));
    if (home.contains(QStringLiteral("score")))
        m_scoreHome = readInt(home.value(QStringLiteral("score")));
    if (away.contains(QStringLiteral("score")))
        m_scoreAway = readInt(away.value(QStringLiteral("score")));

    m_initialized = true;
    emit stateUpdated();
    return true;
}

// Fills the positionId -> name/shorthand/keywords maps for one team, from
// teamX.roster.positionArray (each: { positionId, positionName, shorthand,
// keywords[] }).
void GameModel::parseRosterPositions(const QJsonObject &team, bool homeSide)
{
    QHash<QString, QString> &names = homeSide ? m_positionNameHome : m_positionNameAway;
    QHash<QString, QString> &shorts = homeSide ? m_positionShortHome : m_positionShortAway;
    QHash<QString, QString> &kwds = homeSide ? m_positionKeywordsHome : m_positionKeywordsAway;

    const QJsonArray positions =
        team.value(QStringLiteral("roster")).toObject().value(QStringLiteral("positionArray")).toArray();
    for (const QJsonValue &pv : positions) {
        const QJsonObject pos = pv.toObject();
        const QString id = playerIdKey(pos.value(QStringLiteral("positionId")));
        if (id.isEmpty())
            continue;
        names.insert(id, pos.value(QStringLiteral("positionName")).toString());
        shorts.insert(id, pos.value(QStringLiteral("shorthand")).toString());
        QStringList keywords;
        for (const QJsonValue &k : pos.value(QStringLiteral("keywords")).toArray())
            keywords << k.toString();
        kwds.insert(id, keywords.join(QStringLiteral(", ")));
    }
}

void GameModel::parsePlayerArray(const QJsonArray &array, bool homeSide)
{
    for (const QJsonValue &pv : array) {
        const QJsonObject p = pv.toObject();
        if (p.isEmpty())
            continue;

        FFBPlayer player;
        player.playerId = playerIdKey(p.value(QStringLiteral("playerId")));
        player.name = p.value(QStringLiteral("playerName")).toString();
        player.number = readInt(p.value(QStringLiteral("playerNr")));
        player.race = p.value(QStringLiteral("race")).toString();
        player.position = p.value(QStringLiteral("positionName")).toString();
        player.playerStatus = p.value(QStringLiteral("playerStatus")).toString();
        player.playerType = p.value(QStringLiteral("playerType")).toString();
        player.ag = readInt(p.value(QStringLiteral("agility")));
        player.st = readInt(p.value(QStringLiteral("strength")));
        player.ma = readInt(p.value(QStringLiteral("movement")));
        player.pa = readInt(p.value(QStringLiteral("passing")));
        player.av = readInt(p.value(QStringLiteral("armour")));
        for (const QJsonValue &s : p.value(QStringLiteral("skillArray")).toArray())
            player.skills << s.toString();
        const QJsonObject skillValues = p.value(QStringLiteral("skillValuesMap")).toObject();
        for (const QString &key : skillValues.keys())
            if (!skillValues.value(key).toString().isEmpty())
                player.skillValues.insert(key, skillValues.value(key).toString());
        for (const QJsonValue &s : p.value(QStringLiteral("usedSkills")).toArray())
            player.usedSkills << s.toString();
        for (const QJsonValue &s : p.value(QStringLiteral("lastingInjuries")).toArray())
            player.lastingInjuries << s.toString();
        const QJsonObject recovering = p.value(QStringLiteral("recoveringInjury")).toObject();
        if (!recovering.isEmpty())
            player.recoveringInjury =
                recovering.value(QStringLiteral("injuryName")).toString();
        player.sideHome = homeSide;

        // Resolve the position name via positionId when the player itself
        // doesn't carry it (live payloads carry only the id).
        const QString positionId = playerIdKey(p.value(QStringLiteral("positionId")));
        if (!player.position.isEmpty()) {
            // keep the direct name, but enrich shorthand/keywords if possible
            if (!positionId.isEmpty()) {
                const auto &shorts = homeSide ? m_positionShortHome : m_positionShortAway;
                const auto &kwds = homeSide ? m_positionKeywordsHome : m_positionKeywordsAway;
                player.positionShorthand = shorts.value(positionId);
                player.positionKeywords = kwds.value(positionId).split(QStringLiteral(", "),
                                                                       Qt::SkipEmptyParts);
            }
        } else if (!positionId.isEmpty()) {
            const auto &names = homeSide ? m_positionNameHome : m_positionNameAway;
            const auto &shorts = homeSide ? m_positionShortHome : m_positionShortAway;
            const auto &kwds = homeSide ? m_positionKeywordsHome : m_positionKeywordsAway;
            player.position = names.value(positionId);
            player.positionShorthand = shorts.value(positionId);
            player.positionKeywords = kwds.value(positionId).split(QStringLiteral(", "),
                                                                   Qt::SkipEmptyParts);
        }

        // Some payloads already carry the live coordinate/state in the
        // roster entry itself; treat them like fieldModel data.
        int x = -1, y = -1;
        if (readCoords(p.value(QStringLiteral("playerCoordinate")), x, y)) {
            player.fieldX = x;
            player.fieldY = y;
            player.hasCoords = true;
        }
        if (p.contains(QStringLiteral("playerState")))
            player.state = readInt(p.value(QStringLiteral("playerState")));

        if (!player.playerId.isEmpty() && !m_playerIndex.contains(player.playerId)) {
            m_playerIndex.insert(player.playerId, m_players.size());
            m_players.append(player);
            (homeSide ? m_rosterHomeIds : m_rosterAwayIds) << player.playerId;
        }
    }
}

void GameModel::applyFieldModel(const QJsonObject &fieldModel)
{
    for (const QJsonValue &dv : fieldModel.value(QStringLiteral("playerDataArray")).toArray()) {
        const QJsonObject pd = dv.toObject();
        if (pd.isEmpty())
            continue;
        FFBPlayer *player = findPlayer(playerIdKey(pd.value(QStringLiteral("playerId"))));
        if (!player)
            continue;
        int x = -1, y = -1;
        if (readCoords(pd.value(QStringLiteral("playerCoordinate")), x, y)) {
            player->fieldX = x;
            player->fieldY = y;
            player->hasCoords = true;
        }
        if (pd.contains(QStringLiteral("playerState")))
            player->state = readInt(pd.value(QStringLiteral("playerState")));
        // Active inducement cards (JSON string array of card names).
        for (const QJsonValue &cv : pd.value(QStringLiteral("cards")).toArray())
            player->cards << cv.toString();
    }

    int x = -1, y = -1;
    if (readCoords(fieldModel.value(QStringLiteral("ballCoordinate")), x, y)) {
        m_ballOnField = true;
        m_ballX = x;
        m_ballY = y;
    } else {
        m_ballOnField = false;
        m_ballX = -1;
        m_ballY = -1;
    }

    // Weather display name ("Nice Weather", "Light Rain", ...).
    m_weather = fieldModel.value(QStringLiteral("weather")).toString(m_weather);
}

// -----------------------------------------------------------------------------
// Model changes (port of fumbblGameModel.ts applyModelChanges())
// -----------------------------------------------------------------------------
void GameModel::applyModelChanges(const QJsonArray &changes)
{
    if (!m_initialized)
        return; // ignore syncs before the first full state

    for (const QJsonValue &cv : changes) {
        const QJsonObject c = cv.toObject();
        if (c.isEmpty())
            continue;
        const QString id = c.value(QStringLiteral("modelChangeId")).toString();
        const QString key = c.value(QStringLiteral("modelChangeKey")).toString();
        handleModelChange(id, key, c.value(QStringLiteral("modelChangeValue")));
    }
    emit stateUpdated();
}

void GameModel::handleModelChange(const QString &id, const QString &key, const QJsonValue &value)
{
    // ESSENTIAL handlers for a spectator (TS reference: fumbblGameModel.ts).
    // Everything else is a no-op, exactly like the TS model.
    if (id == QLatin1String("fieldModelSetBallCoordinate")) {
        int x = -1, y = -1;
        if (readCoords(value, x, y)) {
            // KICKOFF flight. GROUND TRUTH (game_1957772 capture): the wire
            // sends ONLY the landing cell
            // (+ later scatter bounces) — no flight path, no kicker id.
            // The trajectory is synthesized: from the ball's previous
            // on-field position (a post-TD kick starts in the scorer's
            // end zone) or from the kicker's half, to the landing cell.
            if (m_pendingKickoff && isOnField(x, y)) {
                m_pendingKickoff = false;
                // The ball lands in the REceiving team's half — the kicking
                // team is the other one (it kicks from its own half).
                const bool homeReceives = (x <= 12);
                const bool awayKicks = homeReceives;
                const bool kickSideHome = !awayKicks;
                QPoint origin;
                const bool prevOnField = m_ballOnField && isOnField(m_ballX, m_ballY);
                if (prevOnField
                    && ((kickSideHome ? m_ballX <= 12 : m_ballX > 12)))
                    origin = QPoint(m_ballX, m_ballY); // e.g. the TD end zone
                else
                    origin = kickSideHome ? QPoint(7, 7) : QPoint(18, 7); // 30-yd line
                m_kickoffOrigin = origin;
                // The KICKER: the kicking team's field player nearest the
                // origin (the coach places the ball anywhere in their own
                // half — the nearest player is the best visual stand-in).
                const auto roster = kickSideHome ? rosterHome() : rosterAway();
                QString best;
                qreal bestD = 1e9;
                for (const FFBPlayer &pl : roster) {
                    if (!pl.hasCoords || !isOnField(pl.fieldX, pl.fieldY))
                        continue;
                    if (pl.sideHome != kickSideHome)
                        continue;
                    const qreal d = qSqrt(qPow(pl.fieldX - origin.x(), 2)
                                          + qPow(pl.fieldY - origin.y(), 2));
                    if (d < bestD) {
                        bestD = d;
                        best = pl.playerId;
                    }
                }
                m_kickoffKickerId = best;
                ++m_kickoffSeq;
            }
            m_ballOnField = true;
            m_ballX = x;
            m_ballY = y;
        } else {
            m_ballOnField = false;
            m_ballX = -1;
            m_ballY = -1;
        }
        return;
    }

    if (id == QLatin1String("fieldModelSetPlayerCoordinate")) {
        FFBPlayer *player = findPlayer(key);
        if (player) {
            // The move trail. Only the ACTOR's steps are traced
            // (pushed/defended players are not). The server steps the actor
            // one square at a time, so each change appends the new cell;
            // a step whose previous cell does not match the path's tail
            // (e.g. the player was pushed meanwhile) starts a new path.
            int nx = -1, ny = -1;
            if (readCoords(value, nx, ny) && !isOnField(nx, ny) && key == m_traceActor) {
                // The actor left the field (into a box: knocked out, sent
                // to the bench...) — its route is over (the numbers and
                // the block marks die with it).
                clearMoveTrace();
            }
            if (readCoords(value, nx, ny) && isOnField(nx, ny) && key == m_traceActor) {
                // The arrow must NOT appear before the player actually
                // MOVES. The server re-sends the actor's coordinate at
                // selection / turn setup (a same-cell no-op re-write, or a
                // placement right after fieldModelRemovePlayer, with NO
                // pending move action). Those are NOT move steps: tracing
                // them creates a degenerate 1-point path -> a premature
                // arrow next to the just-selected player. Only REAL move
                // steps are traced: a step that continues the current route,
                // or the first step after a moving action.
                const bool sameCell = player->hasCoords
                    && isOnField(player->fieldX, player->fieldY)
                    && player->fieldX == nx && player->fieldY == ny;
                if (!sameCell) {
                    // A route starts. Its FIRST step number is
                    // the drive's next number — the count is CONTINUOUS
                    // across routes of the same action (the blitz's
                    // sideways step, the block — noteBlockStep — and the
                    // follow all share one counter), and a go-for-it
                    // extra move is marked red.
                    const auto newRoute = [this, key, &nx, &ny](
                                               const FFBPlayer *player) {
                        MoveSegment seg;
                        if (player->hasCoords
                            && isOnField(player->fieldX, player->fieldY))
                            seg.cells << QPoint(player->fieldX, player->fieldY);
                        seg.cells << QPoint(nx, ny);
                        // A drive that STARTED with a block (a plain block
                        // action) is unnumbered: trail only, no badges.
                        if (!m_driveUnnumbered)
                            seg.stepNumbers << m_nextStepNumber++;
                        seg.goForIt = (m_gfiPendingFor == key);
                        m_gfiPendingFor.clear();
                        m_moveSegments.append(seg);
                    };
                    if (m_movePending) {
                        // First step of a NEW move (a moving action was
                        // just set — or re-set: the server re-dispatches
                        // the action after a block for the follow/GFI
                        // move, StepEndBlocking): start a fresh route from
                        // the old cell, continuing the numbering.
                        newRoute(player);
                        m_movePending = false;
                        // A NEW action by the attacker also ends the block
                        // — the marker is not "left attached" to the token.
                        if (key == m_blockAttackerId && m_blockDieFace > 0)
                            clearBlockMarker();
                    } else {
                        const bool cont = !m_moveSegments.isEmpty()
                            && m_moveSegments.last().cells.last()
                                   == QPoint(player->fieldX, player->fieldY);
                        if (cont) {
                            // Next step of the ongoing route.
                            m_moveSegments.last().cells.append(QPoint(nx, ny));
                            if (!m_driveUnnumbered)
                                m_moveSegments.last().stepNumbers
                                    .append(m_nextStepNumber++);
                        } else if (!m_blockTargetId.isEmpty() && key == m_blockAttackerId) {
                            // The FOLLOW: after the block choice the
                            // attacker follows the defender. The follow is
                            // NOT a moving action on the wire (capture: the
                            // coordinate step lands straight after the
                            // followupChoice dialog, no
                            // actingPlayerSetPlayerAction), so without this
                            // it would be untraced. Any actor step while a
                            // block is open IS the follow.
                            newRoute(player);
                            // The helmet/die marker dies the moment the
                            // attacker takes the follow step. (The red glow
                            // on the defender keeps its own lifecycle.)
                            if (m_blockDieFace > 0)
                                clearBlockMarker();
                        }
                        // else: a coordinate change with no move action and
                        // no active route (turn-setup placement, a push) —
                        // it starts no route.
                    }
                }
            }
            setPlayerCoords(player, value);
        }
        return;
    }

    // The block is fully resolved when the server clears the defender id
    // (capture: gameSetDefenderId null lands right after the follow
    // decision, even when the attacker did NOT follow — the marker must be
    // gone in both cases). The red glow keeps living until the attacker's
    // actor glow dies.
    if (id == QLatin1String("gameSetDefenderId")) {
        if (value.isNull()) {
            const bool glowOnly = m_blockGlowOnly;
            clearBlockMarker();
            m_blockResolved = true; // a FRENZY second block now gets its
                                    // own number
            // For a FOUL the null defender id is the foul's resolution
            // (ffb-server StepEndFouling does exactly this) — the glow-only
            // mark dies with it. A block's red glow keeps its own lifecycle
            // (dies with the attacker's actor glow).
            if (glowOnly)
                m_blockTargetId.clear();
        }
        return;
    }

    // The colored reroll dots follow the CURRENT-TURN remaining rerolls
    // (capture: turnDataSetReRolls {key, value}).
    if (id == QLatin1String("turnDataSetReRolls")) {
        const int n = value.toInt();
        if (key == QLatin1String("home"))
            m_turnReRollsHome = n;
        else if (key == QLatin1String("away"))
            m_turnReRollsAway = n;
        return;
    }

    if (id == QLatin1String("fieldModelSetPlayerState")) {
        FFBPlayer *player = findPlayer(key);
        if (player)
            player->state = readInt(value);
        // The defender's state changes DURING the block (down, box, KO, ...)
        // — the red glow must SURVIVE them. It dies only when the
        // attacker's actor glow disappears (actingPlayerSetPlayerId) or a
        // new turn/half starts.
        return;
    }

    if (id == QLatin1String("fieldModelRemovePlayer")) {
        // ffb-common FieldModel.remove(player) clears ONLY the player's
        // LIVE FIELD DATA (coordinate + state map) — the player REMAINS in
        // the team roster. The server does this for EVERY player at the
        // start of each turn (post-TD / half setup) right before re-placing
        // them with fieldModelSetPlayerCoordinate (verified in the live
        // capture game_1957772: 22x removePlayer + 22x setPlayerCoordinate
        // in one batch). The player must NOT be deleted from the model:
        // the follow-up setPlayerCoordinate would find no player and the
        // player would stay gone forever (and every report referencing it
        // would lose its subject: "Move" without a name).
        FFBPlayer *player = findPlayer(key);
        if (player) {
            player->fieldX = -1;
            player->fieldY = -1;
            player->hasCoords = false;
        }
        return;
    }

    if (id == QLatin1String("gameSetHalf")) {
        m_half = readInt(value);
        clearMoveTrace(); // a new half: the trace dies with the old turn
        m_blockTargetId.clear(); // a new half kills the glow
        m_actedThisTurn.clear();
        m_actorActivated = false;
        m_blitzingPlayerId.clear();
        m_trackNumbers.clear();
        m_passTargetId.clear();
        return;
    }

    if (id == QLatin1String("gameSetHomePlaying")) {
        m_homePlaying = value.toBool(m_homePlaying);
        return;
    }

    if (id == QLatin1String("fieldModelSetWeather")) {
        // Value: a Weather object ({name, shortName}) or a plain string.
        const QJsonObject obj = value.toObject();
        m_weather = obj.contains(QStringLiteral("name"))
                        ? obj.value(QStringLiteral("name")).toString()
                        : value.toString(m_weather);
        return;
    }

    if (id == QLatin1String("gameSetTurnMode")) {
        const QJsonObject obj = value.toObject();
        m_turnMode = obj.contains(QStringLiteral("name"))
                         ? obj.value(QStringLiteral("name")).toString()
                         : value.toString();
        // A KICKOFF turn starts — the next on-field ball placement is the
        // kickoff landing: the flight (trajectory + bounces + kicker glow)
        // is synthesized from it.
        m_pendingKickoff = (m_turnMode == QStringLiteral("kickoff"));
        // Turn START (post-TD or post-half setup: the players are about to
        // be re-placed) — the trace of the old drive is gone. (Live
        // capture: the mode sequence at a new turn is ...regular ->
        // setup -> kickoff -> regular/first_turn.)
        static const QSet<QString> kTurnStartModes = {
            QStringLiteral("setup"),      QStringLiteral("kickoff"),
            QStringLiteral("quick_snap"), QStringLiteral("first_turn"),
            QStringLiteral("halftime"),   QStringLiteral("overtime"),
            QStringLiteral("ended"),
        };
        if (kTurnStartModes.contains(m_turnMode)) {
            clearMoveTrace();
            m_blockTargetId.clear(); // new turn = no block
        }
        return;
    }

    if (id == QLatin1String("gameSetPassCoordinate")) {
        // The ball travels to the pass target coordinate.
        int x = -1, y = -1;
        if (readCoords(value, x, y)) {
            m_ballOnField = true;
            m_ballX = x;
            m_ballY = y;
        }
        return;
    }

    if (id == QLatin1String("turnDataSetTurnNr")) {
        // TS reference: only the "home" key updates the displayed turn.
        if (key == QLatin1String("home"))
            m_turn = readInt(value);
        // A new turn resets the per-turn marks (green checks, blitzer,
        // trail numbers — the server removes them individually, but a new
        // turn starts a fresh numbering anyway). The block marker dies
        // with the turn too.
        m_actedThisTurn.clear();
        m_actorActivated = false;
        m_blitzingPlayerId.clear();
        m_trackNumbers.clear();
        clearBlockMarker();
        return;
    }

    // The numbered trail cells ("come su FUMBBL").
    // GROUND TRUTH (capture): value = {coordinate: [x, y], number: N}.
    if (id == QLatin1String("fieldModelAddTrackNumber")) {
        const QJsonObject o = value.toObject();
        int x = -1, y = -1;
        if (readCoords(o.value(QStringLiteral("coordinate")), x, y) && isOnField(x, y))
            m_trackNumbers.insert(QPoint(x, y),
                                  o.value(QStringLiteral("number")).toInt());
        return;
    }
    if (id == QLatin1String("fieldModelRemoveTrackNumber")) {
        const QJsonObject o = value.toObject();
        int x = -1, y = -1;
        if (readCoords(o.value(QStringLiteral("coordinate")), x, y))
            m_trackNumbers.remove(QPoint(x, y));
        return;
    }

    // The PASS RECEIVER (white glow). GROUND TRUTH (capture):
    // {playerId, targetSelectionStatus: "SELECTED", ...}; a null value
    // ends the selection.
    if (id == QLatin1String("fieldModelSetTargetSelectionState")) {
        if (value.isNull())
            m_passTargetId.clear();
        else {
            const QJsonObject o = value.toObject();
            if (o.value(QStringLiteral("targetSelectionStatus")).toString()
                    == QLatin1String("SELECTED"))
                m_passTargetId = o.value(QStringLiteral("playerId")).toString();
            else
                m_passTargetId.clear();
        }
        return;
    }

    // The BLITZ symbol. GROUND TRUTH (ffb-common TurnData.setBlitzUsed):
    // the blitzer is the acting player at the moment the flag goes true.
    if (id == QLatin1String("turnDataSetBlitzUsed")) {
        if (value.toBool())
            m_blitzingPlayerId = m_activePlayerId;
        else
            m_blitzingPlayerId.clear();
        return;
    }

    if (id == QLatin1String("teamResultSetScore")) {
        if (key == QLatin1String("home"))
            m_scoreHome = readInt(value);
        else if (key == QLatin1String("away"))
            m_scoreAway = readInt(value);
        return;
    }

    if (id == QLatin1String("actingPlayerSetPlayerId")) {
        const QString newActor = playerIdKey(value);
        const QString oldActor = m_activePlayerId;
        m_activePlayerId = newActor;
        // Trace semantics: the value is null BETWEEN actions of
        // the same player (the trace must survive — a new move of the same
        // player appends, it does not replace), so only a DIFFERENT non-
        // null actor wipes the previous player's whole trace.
        if (!m_activePlayerId.isEmpty() && m_activePlayerId != m_traceActor) {
            m_traceActor = m_activePlayerId;
            clearMoveTrace(); // numbers + block marks die too
        }
        // The red block glow on the DEFENDER lives exactly as long as the
        // block action — it dies when the ATTACKER loses his (blue) actor
        // glow: the actor becomes another player OR nobody (null). Clearing
        // on any different non-null actor was wrong: with re-rolls the
        // attacker re-armed in the same batch and the glow flickered / was
        // often missing.
        if (!m_blockTargetId.isEmpty() && m_activePlayerId != m_blockAttackerId)
            m_blockTargetId.clear();
        // The previous actor is NOT marked "acted" here: marking on actor
        // change was wrong for the select -> DESELECT -> reselect flow —
        // the human selects a player to think, cancels (actor -> null, no
        // action taken), selects them again and only THEN activates them,
        // so the green check would appear before the real action.
        //
        // Server ground truth (ffb-server StepInitMoving +
        // UtilActingPlayer.changeActingPlayer, capture-verified pattern
        // "Id=None, Id=X, Action=move, Id=None"):
        //   - SELECTION  = actingPlayerSetPlayerId=X, playerAction still null
        //   - ACTIVATION = actingPlayerSetPlayerAction=<action> (the human
        //                  definitively chose an action)
        //   - DESELECT / action end = actingPlayerSetPlayerId=null
        // Only ACTIVATION is definitive: the mark is made in the
        // actingPlayerSetPlayerAction handler below; here the activation
        // window simply resets for any new actor (X -> null and X -> Y).
        if (newActor != m_activePlayerId)
            m_actorActivated = false;
        return;
    }

    if (id == QLatin1String("actingPlayerSetPlayerAction")) {
        // Value: a plain PlayerAction name on the wire ("move", "standUp",
        // "blitzMove", ... — verified in the live capture; null = a bare
        // selection, no action yet).
        const QString an = enumNameValue(value);
        if (isMovingActionName(an))
            m_movePending = true;
        // The GREEN CHECK ("acted this turn") appears exactly when the
        // acting player is DEFINITIVELY activated — the human chose an
        // action. A select-then-deselect (no action on the wire) never
        // marks; a reselected player marks only when they finally act.
        if (!an.isEmpty() && !m_activePlayerId.isEmpty()) {
            m_actedThisTurn.insert(m_activePlayerId);
            m_actorActivated = true;
        }
        return;
    }

    // ~130 remaining modelChangeIds: no-op (TS reference).
}

void GameModel::setPlayerCoords(FFBPlayer *player, const QJsonValue &value)
{
    int x = -1, y = -1;
    if (readCoords(value, x, y)) {
        player->fieldX = x;
        player->fieldY = y;
        player->hasCoords = true;
    }
}

FFBPlayer *GameModel::findPlayer(const QString &playerId)
{
    const auto it = m_playerIndex.find(playerId);
    if (it == m_playerIndex.end())
        return nullptr;
    return &m_players[it.value()];
}

// -----------------------------------------------------------------------------
// Rosters / field tokens
// -----------------------------------------------------------------------------
const FFBPlayer *GameModel::findPlayerById(const QString &playerId) const
{
    const auto it = m_playerIndex.constFind(playerId);
    if (it == m_playerIndex.constEnd())
        return nullptr;
    return &m_players.at(it.value());
}

const FFBPlayer *GameModel::playerAt(int x, int y) const
{
    for (const FFBPlayer &p : m_players) {
        if (p.hasCoords && p.fieldX == x && p.fieldY == y &&
            isOnField(p.fieldX, p.fieldY) && !isBoxX(p.fieldX))
            return &p;
    }
    return nullptr;
}

QVector<FFBPlayer> GameModel::rosterHome() const
{
    QVector<FFBPlayer> result;
    result.reserve(m_rosterHomeIds.size());
    for (const QString &id : m_rosterHomeIds) {
        const auto it = m_playerIndex.constFind(id);
        if (it != m_playerIndex.constEnd())
            result.append(m_players.at(it.value()));
    }
    return result;
}

QVector<FFBPlayer> GameModel::rosterAway() const
{
    QVector<FFBPlayer> result;
    result.reserve(m_rosterAwayIds.size());
    for (const QString &id : m_rosterAwayIds) {
        const auto it = m_playerIndex.constFind(id);
        if (it != m_playerIndex.constEnd())
            result.append(m_players.at(it.value()));
    }
    return result;
}

QVector<FieldToken> GameModel::fieldTokens() const
{
    QVector<FieldToken> tokens;
    tokens.reserve(m_players.size());
    for (const FFBPlayer &p : m_players) {
        if (!p.hasCoords || isBoxX(p.fieldX) || !isOnField(p.fieldX, p.fieldY))
            continue;
        FieldToken t;
        t.playerId = p.playerId;
        t.name = p.name;
        t.number = p.number;
        t.sideHome = p.sideHome;
        t.x = p.fieldX;
        t.y = p.fieldY;
        t.state = p.state;
        t.hasBall = m_ballOnField && p.fieldX == m_ballX && p.fieldY == m_ballY;
        t.isActing = !m_activePlayerId.isEmpty() && p.playerId == m_activePlayerId;
        t.positionName = p.position;
        tokens.append(t);
    }
    return tokens;
}

QVector<FieldToken> GameModel::boxTokens(bool homeSide) const
{
    QVector<FieldToken> tokens;
    for (const FFBPlayer &p : m_players) {
        if (p.sideHome != homeSide || !p.hasCoords || !isBoxX(p.fieldX))
            continue;
        FieldToken t;
        t.playerId = p.playerId;
        t.name = p.name;
        t.number = p.number;
        t.sideHome = p.sideHome;
        t.x = p.fieldX;
        t.y = p.fieldY;
        t.state = p.state;
        tokens.append(t);
    }
    return tokens;
}

bool GameModel::ballAt(int &x, int &y) const
{
    if (!m_ballOnField)
        return false;
    x = m_ballX;
    y = m_ballY;
    return true;
}

// -----------------------------------------------------------------------------
// Coordinate / state helpers (ports of ffb-common)
// -----------------------------------------------------------------------------
bool GameModel::isOnField(int x, int y)
{
    return x >= 0 && x <= 25 && y >= 0 && y <= 14;
}

bool GameModel::isBoxX(int x)
{
    // Home box: -1 RSV, -2 KO, -3 BH, -4 SI, -5 RIP, -6 BAN, -7 MNG
    // Away box: 30 RSV, 31 KO, 32 BH, 33 SI, 34 RIP, 35 BAN, 36 MNG
    return (x >= -7 && x <= -1) || (x >= 30 && x <= 36);
}

QString GameModel::stateLabel(int state)
{
    switch (baseState(state)) {
    case 5: return QStringLiteral("KO");    // KNOCKED_OUT
    case 6: return QStringLiteral("BH");    // BADLY_HURT
    case 7: return QStringLiteral("SI");    // SERIOUS_INJURY
    case 8: return QStringLiteral("RIP");   // RIP (killed)
    case 9: return QStringLiteral("RES");   // RESERVE
    case 10: return QStringLiteral("MISS"); // MISSING
    case 13: return QStringLiteral("BAN");  // BANNED
    case 3: return QStringLiteral("PRN");   // PRONE
    case 4: return QStringLiteral("STN");   // STUNNED
    default: return QString(); // UNKNOWN/STANDING/MOVING/... on-field active states
    }
}

// Port of PlayerState.getButtonText (ffb-common PlayerState.java).
QString GameModel::stateText(int state)
{
    switch (baseState(state)) {
    case 1: return QStringLiteral("Standing");      // STANDING
    case 2: return QStringLiteral("Moving");        // MOVING
    case 3: return QStringLiteral("Prone");         // PRONE
    case 4: return QStringLiteral("Stunned");        // STUNNED
    case 5: return QStringLiteral("Knocked Out");   // KNOCKED_OUT
    case 6: return QStringLiteral("Badly Hurt");    // BADLY_HURT
    case 7: return QStringLiteral("Serious Injury"); // SERIOUS_INJURY
    case 8: return QStringLiteral("Killed");        // RIP
    case 9: return QStringLiteral("Reserve");       // RESERVE
    case 10: return QStringLiteral("Missing");      // MISSING
    case 11: return QStringLiteral("Falling Down"); // FALLING
    case 12: return QStringLiteral("Blocked");      // BLOCKED
    case 13: return QStringLiteral("Banned");       // BANNED
    case 14: return QStringLiteral("Exhausted");    // EXHAUSTED
    case 15: return QStringLiteral("Being Dragged"); // BEING_DRAGGED
    case 16: return QStringLiteral("Picked Up");    // PICKED_UP
    case 17: return QStringLiteral("Hit on the ground"); // HIT_ON_GROUND
    case 18: return QStringLiteral("Can't be set up");   // SETUP_PREVENTED
    case 19: return QStringLiteral("In the air");    // IN_THE_AIR
    default: return QString(); // UNKNOWN
    }
}

QStringList GameModel::stateFlags(int state)
{
    QStringList flags;
    if (state & 0x00200) flags << QStringLiteral("Confused");
    if (state & 0x00400) flags << QStringLiteral("Rooted");
    if (state & 0x00800) flags << QStringLiteral("Hypnotized");
    if (state & 0x20000) flags << QStringLiteral("Eye gouged");
    if (state & 0x40000) flags << QStringLiteral("Chomped");
    return flags;
}
