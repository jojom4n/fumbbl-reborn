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
    m_tracePaths.clear();
    m_movePending = false;
}

void GameModel::reset()
{
    clearState();
    emit stateUpdated();
}

void GameModel::clearMoveTrace()
{
    m_tracePaths.clear();
    m_movePending = false;
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
    // Round 9 (verified on the live game state): the team id lives in
    // `teamId`, not `id` — with the wrong key every team reference in the
    // reports (prayers, bribes, throw-ins, block rolls...) stayed an
    // unresolved numeric id.
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
            // Round 13: the move trail. Only the ACTOR's steps are traced
            // (pushed/defended players are not). The server steps the actor
            // one square at a time, so each change appends the new cell;
            // a step whose previous cell does not match the path's tail
            // (e.g. the player was pushed meanwhile) starts a new path.
            int nx = -1, ny = -1;
            if (readCoords(value, nx, ny) && !isOnField(nx, ny) && key == m_traceActor) {
                // The actor left the field (into a box: knocked out, sent
                // to the bench...) — its route is over.
                m_tracePaths.clear();
                m_movePending = false;
            }
            if (readCoords(value, nx, ny) && isOnField(nx, ny) && key == m_traceActor) {
                // Round 13 (user): the arrow must NOT appear before the
                // player actually MOVES. The server re-sends the actor's
                // coordinate at selection / turn setup (a same-cell no-op
                // re-write, or a placement right after fieldModelRemovePlayer,
                // with NO pending move action). Those are NOT move steps and
                // used to create a degenerate 1-point (or A,A) path -> a
                // premature arrow next to the just-selected player. Now only
                // REAL move steps are traced: a step that continues the
                // current route, or the first step after a moving action.
                const bool sameCell = player->hasCoords
                    && isOnField(player->fieldX, player->fieldY)
                    && player->fieldX == nx && player->fieldY == ny;
                if (!sameCell) {
                    if (m_movePending) {
                        // First step of a NEW move (a moving action was just
                        // set): start a fresh route from the old cell.
                        QVector<QPoint> path;
                        if (player->hasCoords && isOnField(player->fieldX, player->fieldY))
                            path << QPoint(player->fieldX, player->fieldY);
                        path << QPoint(nx, ny);
                        m_tracePaths.append(path);
                        m_movePending = false;
                    } else {
                        const bool cont = !m_tracePaths.isEmpty()
                            && m_tracePaths.last().last()
                                   == QPoint(player->fieldX, player->fieldY);
                        if (cont)
                            // Next step of the ongoing route.
                            m_tracePaths.last().append(QPoint(nx, ny));
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

    if (id == QLatin1String("fieldModelSetPlayerState")) {
        FFBPlayer *player = findPlayer(key);
        if (player)
            player->state = readInt(value);
        // Round 13 turn 4: the block RESOLVED for its target (knocked down,
        // prone, banned, ...): the red block glow dies with the new state.
        if (key == m_blockTargetId)
            m_blockTargetId.clear();
        return;
    }

    if (id == QLatin1String("fieldModelRemovePlayer")) {
        // ROUND 13 BUG FIX (user report: players vanished after every TD /
        // half end). ffb-common FieldModel.remove(player) clears ONLY the
        // player's LIVE FIELD DATA (coordinate + state map) — the player
        // REMAINS in the team roster. The server does this for EVERY player
        // at the start of each turn (post-TD / half setup) right before
        // re-placing them with fieldModelSetPlayerCoordinate (verified in
        // the live capture game_1957772: 22x removePlayer + 22x
        // setPlayerCoordinate in one batch). The old code deleted the
        // player from the model — the follow-up setPlayerCoordinate then
        // found no player and the player stayed gone forever (and every
        // report referencing it lost its subject: "Move" without a name).
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
        m_blockTargetId.clear(); // round 13 turn 4: a new half kills the glow
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
            m_blockTargetId.clear(); // round 13 turn 4: new turn = no block
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
        // Round 13 trace semantics: the value is null BETWEEN actions of
        // the same player (the trace must survive — a new move of the same
        // player appends, it does not replace), so only a DIFFERENT non-
        // null actor wipes the previous player's whole trace.
        if (!m_activePlayerId.isEmpty() && m_activePlayerId != m_traceActor) {
            m_traceActor = m_activePlayerId;
            m_tracePaths.clear();
            m_movePending = false;
        }
        // Round 13 turn 4: a DIFFERENT non-null actor means the previous
        // action (and its block, if any) is over — the red glow dies.
        if (!m_activePlayerId.isEmpty() && m_activePlayerId != oldActor)
            m_blockTargetId.clear();
        return;
    }

    if (id == QLatin1String("actingPlayerSetPlayerAction")) {
        // Value: a plain PlayerAction name on the wire ("move", "standUp",
        // "blitzMove", ... — verified in the live capture). A MOVING action
        // arms the trace: the actor's next coordinate change starts a NEW
        // path (its own arrow).
        const QString an = enumNameValue(value);
        if (isMovingActionName(an))
            m_movePending = true;
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
const FFBPlayer *GameModel::findPlayerByNumber(int number, bool homeSide) const
{
    const QStringList &ids = homeSide ? m_rosterHomeIds : m_rosterAwayIds;
    for (const QString &id : ids) {
        const auto it = m_playerIndex.constFind(id);
        if (it != m_playerIndex.constEnd()) {
            const FFBPlayer &p = m_players.at(it.value());
            if (p.number == number)
                return &p;
        }
    }
    return nullptr;
}

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
    case 4: return QStringLiteral("Stunned");        // STUNNED (round 11: user — no "has been")
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
