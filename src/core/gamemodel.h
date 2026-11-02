// =============================================================================
// GameModel — FUMBBL game state for the spectator
//
// C++ port of the TS client's fumbblGameModel.ts, which is itself copied from
// the official ffbclient (model/Game.java + ModelChangeProcessor.java).
// Reference (authoritative):
//   /home/saruman/Projects/ffb/ffb-common/src/main/java/com/fumbbl/ffb/
//     - FieldCoordinate.java  (field + box coordinate constants)
//     - PlayerState.java      (state ids and bit flags)
//     - model/change/ModelChangeId.java
//
// Spectator scope: the ESSENTIAL model changes are handled (ball, player
// coordinates/states, removal, half, side, turn mode, pass coordinate,
// turn number, score, acting player); every other modelChangeId is a no-op,
// exactly like the TS model.
//
// Coordinate space (server): field x 0..25 (0 = home TD column), y 0..14.
// Box (off-field) coordinates, from FieldCoordinate.java:
//   home: -1 RSV, -2 KO, -3 BH, -4 SI, -5 RIP, -6 BAN, -7 MNG
//   away: 30 RSV, 31 KO, 32 BH, 33 SI, 34 RIP, 35 BAN, 36 MNG
// =============================================================================
#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>

// A player from a team roster (teamX.playerArray), with live field data
// merged in from fieldModel.playerDataArray and model changes.
struct FFBPlayer {
    QString playerId;   // server id, string (e.g. "18109690")
    QString name;
    int number = 0;     // playerNr
    QString race;
    QString position;   // position name ("Blocker", "Werewolf", ...) —
                        // from the player itself or resolved via positionId
                        // against the team roster's positionArray
    QString positionShorthand; // "L", "W", ... (roster positionArray)
    QStringList positionKeywords; // "Blitzer", "Undead", ...
    int ma = 0;         // movement
    int st = 0;         // strength
    int ag = 0;         // agility
    int pa = 0;         // passing
    int av = 0;         // armour
    QStringList skills; // skillArray (full names on live payloads)
    // Skill name -> value (skillValuesMap, e.g. Loner -> "4", Juggernaut -> "dwarf")
    QHash<QString, QString> skillValues;
    QStringList usedSkills; // skills already used this turn (usedSkills)
    QStringList cards;      // active inducement card names (fieldModel)
    QString playerStatus;   // "active" / ... (playerStatus)
    QString playerType;     // "Regular" / "Star" / "Sub" ... (playerType)
    QStringList lastingInjuries; // e.g. "Seriously Hurt (MNG)"
    QString recoveringInjury;    // text of the injury the player is recovering from
    bool sideHome = true;

    // Live field data (from fieldModel.playerDataArray / model changes).
    int fieldX = -1;    // -1 = no coordinate yet
    int fieldY = -1;
    bool hasCoords = false;
    int state = 0;      // ffb PlayerState id (use GameModel::baseState() for base)
};

// A token for field rendering: a player that currently has a field coordinate
// (not a box coordinate), enriched for the UI.
struct FieldToken {
    QString playerId;
    QString name;
    int number = 0;
    bool sideHome = true;
    int x = -1;
    int y = -1;
    int state = 0;
    bool hasBall = false;  // standing on the same square as the ball
    bool isActing = false; // the player with the actingPlayer turn
    QString positionName;  // "Zombie Lineman", "Ogre", ... — face lookup key
};

class GameModel : public QObject
{
    Q_OBJECT

public:
    explicit GameModel(QObject *parent = nullptr);

    // Initialize from a full game state (the "game" object of serverGameState).
    bool initialize(const QJsonObject &game);
    // Forget everything (switching to a different game): back to the
    // pre-game state, and the UI must clear itself (see MainWindow).
    void reset();

    // Apply a batch of model changes:
    // serverModelSync.modelChangeList.modelChangeArray
    // (each: { modelChangeId, modelChangeKey, modelChangeValue }).
    void applyModelChanges(const QJsonArray &changes);

    bool isInitialized() const { return m_initialized; }

    // --- Teams / score / turn ----------------------------------------------
    QString teamHomeName() const { return m_teamHomeName; }
    QString teamAwayName() const { return m_teamAwayName; }
    // Team ids (FBF): reports reference them as choosingTeamId & co — the UI
    // resolves them to team names instead of printing raw numbers.
    QString teamHomeId() const { return m_teamHomeId; }
    QString teamAwayId() const { return m_teamAwayId; }
    // Team race (live players usually carry no per-player race).
    QString teamHomeRace() const { return m_teamHomeRace; }
    QString teamAwayRace() const { return m_teamAwayRace; }
    // Team coach names (header sublines; the team on turn gets END TURN).
    QString teamHomeCoach() const { return m_teamHomeCoach; }
    QString teamAwayCoach() const { return m_teamAwayCoach; }
    // Team logo URLs (absolute; empty when the state carries no logo).
    QString teamHomeLogoUrl() const { return m_teamHomeLogoUrl; }
    QString teamAwayLogoUrl() const { return m_teamAwayLogoUrl; }
    // Turn 12 (user): the header shows as many dots as the TEAM has
    // rerolls (team.reRolls — could be 0 or 6), and a dot turns white as
    // it is used THIS TURN (turnData.reRolls = remaining this turn).
    int reRollsHome() const { return m_reRollsHome; }
    int reRollsAway() const { return m_reRollsAway; }
    int turnReRollsHome() const { return m_turnReRollsHome; }
    int turnReRollsAway() const { return m_turnReRollsAway; }
    int scoreHome() const { return m_scoreHome; }
    int scoreAway() const { return m_scoreAway; }
    int turn() const { return m_turn; }
    int half() const { return m_half; }
    QString turnMode() const { return m_turnMode; } // setup|first_turn|regular|halftime|overtime|ended

    // Turn 15 (user, point 11): the KICKOFF flight. The wire sends only the
    // landing cell (no path, no kicker) — the model synthesizes the flight
    // origin (previous on-field ball position in the kicker's half, or the
    // kicker's 30-yd line) and the kicker (nearest kicking-team field
    // player to the origin). `kickoffSeq` bumps once per kickoff so the UI
    // triggers the animation exactly once.
    int kickoffSeq() const { return m_kickoffSeq; }
    QPoint kickoffOrigin() const { return m_kickoffOrigin; }
    QPoint kickoffLanding() const { return m_kickoffLanding; }
    QString kickoffKickerId() const { return m_kickoffKickerId; }
    bool homePlaying() const { return m_homePlaying; }
    // Weather display name ("Nice Weather", "Light Rain", "Pouring Rain",
    // ...). Empty before the first state.
    QString weather() const { return m_weather; }

    // Full rosters (ALL playerArray players, in server order).
    QVector<FFBPlayer> rosterHome() const;
    QVector<FFBPlayer> rosterAway() const;

    // First player of a side with the given shirt number (nullptr if none).
    const FFBPlayer *findPlayerByNumber(int number, bool homeSide) const;
    // Player by server id (nullptr if unknown) — used for click selection.
    const FFBPlayer *findPlayerById(const QString &playerId) const;
    // On-field player standing at the given field cell (nullptr if none).
    const FFBPlayer *playerAt(int x, int y) const;

    // --- Field ---------------------------------------------------------------
    // Players with a field coordinate (box coordinates excluded).
    QVector<FieldToken> fieldTokens() const;
    // Players in a box coordinate (reserve/KO/BH/SI/RIP/BAN/MNG), for rosters.
    QVector<FieldToken> boxTokens(bool homeSide) const;
    // Ball position (false = ball has no field coordinate, e.g. in a hand).
    bool ballAt(int &x, int &y) const;
    QString activePlayerId() const { return m_activePlayerId; }

    // --- Block target (round 13 turn 4, user: the player being BLOCKED gets
    // a RED selection glow, like the actor's blue one) ---------------------
    // Armed by the UI on the blockRoll/block report (the defenderId — the
    // player being blocked); cleared when the block RESOLVES: the target's
    // state changes (fieldModelSetPlayerState), a new turn/half starts, or
    // another player becomes the actor.
    QString blockTargetId() const { return m_blockTargetId; }
    // Round 13 turn 8 (user, FINAL rule): the red glow on the DEFENDER
    // lives as long as the block action — it dies when the ATTACKER loses
    // his (blue) actor glow. `attacker` empty = the current actor at arming
    // time (the block reports arrive while the attacker is the actor).
    void setBlockTargetId(const QString &playerId, const QString &attacker = QString());
    // Round 15 (user: "mostriamo anche il movimento che costa il blocco
    // durante il blitz"): a block COSTS A MOVEMENT. Called by the UI on a
    // (deduped) `block` report — the start of a block sequence, including
    // a FRENZY's second block — it reserves the next number of the drive
    // and records where the number is painted (between the attacker's
    // cell and the defender's cell).
    void noteBlockStep(const QString &defenderId,
                       const QString &attackerId = QString());
    // Turn 11: the attacker of an open block (the marker pair) + the CHOSEN
    // block die face (0 = not chosen yet, 1..6 = blockChoice blockRoll[diceIndex]).
    QString blockAttackerId() const { return m_blockAttackerId; }
    int blockDieFace() const { return m_blockDieFace; }
    void setBlockDieFace(int face) { m_blockDieFace = face; }
    // Turn 16 (user: "il target del blitz fallo gloware di rosso"): a FOUL
    // target gets the same red glow as a block defender — but WITHOUT the
    // marker pair (there is no die). The glow's lifecycle is the block
    // glow's: it dies when the fouler (the actor) stops being the actor,
    // on a new turn or half, or when the target is replaced.
    void setFoulTargetId(const QString &playerId)
    {
        m_blockTargetId = playerId;
        m_blockAttackerId = m_activePlayerId; // lifecycle anchor = the fouler
        m_blockDieFace = 0;
        m_blockGlowOnly = true;
    }
    bool blockGlowOnly() const { return m_blockGlowOnly; }
    // Turn 12 (user): the marker (helmet / chosen die) between attacker
    // and defender dies as soon as the ATTACKER acts on the follow
    // decision — it must not stay attached to the player. The RED GLOW on
    // the defender keeps its own (turn 8) lifecycle and is NOT touched
    // here.
    bool blockMarkerActive() const { return !m_blockAttackerId.isEmpty(); }
    void clearBlockMarker()
    {
        m_blockAttackerId.clear();
        m_blockDieFace = 0;
        m_blockGlowOnly = false;
    }

    // Turn 13 (user, "come su FUMBBL"): the numbered cells of the current
    // movement trail. GROUND TRUTH (game_1957772 capture): the server sends
    // fieldModelAddTrackNumber / fieldModelRemoveTrackNumber with
    // {coordinate: [x,y], number: N} — the official client paints N on the
    // cell (FieldLayerUnderPlayers.drawTrackNumber). We mirror the server
    // data 1:1.
    QHash<QPoint, int> trackNumbers() const { return m_trackNumbers; }

    // Turn 13 (user, "come su FUMBBL"): the green check on a field player
    // who ALREADY ACTED this turn. No server flag exists (verified: no
    // playedThisTurn in the state nor in the changes) — the client tracks
    // it: a player is marked the moment they STOP being the actor.
    QSet<QString> actedThisTurn() const { return m_actedThisTurn; }

    // Turn 14 (user): the player designated as PASS RECEIVER glows (white,
    // user: "il glow fallo bianco"). GROUND TRUTH (capture):
    // fieldModelSetTargetSelectionState {playerId, targetSelectionStatus:
    // "SELECTED", ...}; value null = the selection is gone.
    QString passTargetId() const { return m_passTargetId; }

    // Turn 13 (user): the BLITZ symbol (top-left of the ring) on the player
    // who blitzed this turn. GROUND TRUTH (ffb-common TurnData.setBlitzUsed):
    // the blitzer is exactly the ACTING PLAYER at the moment
    // turnDataSetBlitzUsed goes true; it dies when the flag goes false or
    // the turn changes.
    QString blitzingPlayerId() const { return m_blitzingPlayerId; }

    // --- Move trail (round 13, user request) ---------------------------------
    // The CUMULATIVE movement trace of the remote acting player: each entry
    // is one MOVE ACTION, a list of field cells from the start square to the
    // last stepped square (one arrow per path in the UI).
    //
    // The server steps the player ONE SQUARE AT A TIME (ffb-server StepMove:
    // the MOVE_STACK is consumed square by square, each square a
    // fieldModelSetPlayerCoordinate), so the trail draws progressively
    // behind the player, exactly like the official client's step-by-step
    // movement.
    //
    // Persistence semantics (user, round 13):
    //   * NO timeout — the trace stays until the actor changes or a new
    //     turn starts;
    //   * a NEW move by the SAME player does NOT replace the trace: it
    //     APPENDS another path (its own arrow) — the spectator sees the
    //     whole cumulative route;
    //   * when the coach activates ANOTHER player, the whole trace of the
    //     previous player disappears and the new one starts clean.
    //
    // Protocol ground truth (live capture game_1957772, 28/ott):
    //   actingPlayerSetPlayerId    = who is acting (null between actions)
    //   actingPlayerSetPlayerAction = "move"/"standUp"/... (plain string)
    //   fieldModelSetPlayerCoordinate [x,y] per stepped square
    // Round 15 (user): continuous numbering across the WHOLE action — the
    // blitz's sideways step is 1, the block (a movement in itself) is 2,
    // the follow steps are 3,4,5..., a frenzy's second block is the next
    // number, and a go-for-it extra move continues the count (drawn red).
    struct MoveSegment {
        QVector<QPoint> cells; // the route's cells, center to center
        QVector<int> stepNumbers; // the number between cells[i] and cells[i+1]
        bool goForIt = false; // a Go-For-It extra move: the trail is red
    };
    // A block that cost a movement (blitz block, frenzy block, plain
    // block): its number is painted between the attacker's cell and the
    // defender's cell, like the other between-cells numbers.
    struct BlockStepMark {
        QPoint from; // the attacker's cell when the block started
        QPoint to; // the defender's cell
        int number;
    };
    QVector<MoveSegment> moveSegments() const { return m_moveSegments; }
    QVector<BlockStepMark> blockStepMarks() const { return m_blockStepMarks; }
    // The cell view of the segments (round 13 API, kept for the tests).
    QVector<QVector<QPoint>> moveTrace() const;
    QString moveTraceActor() const { return m_traceActor; }
    // Round 15 (user: "in caso di go for it, mostriamo in rosso i
    // movimenti extra"): a successful go-for-it roll — the player's next
    // NEW route is the GFI extra move and is drawn red. Called by the UI
    // on the (deduped) `goForItRoll` report with successful=true.
    void setGoForItPending(const QString &playerId);
    void clearMoveTrace();

    // --- Coordinate helpers (port of ffb-common FieldCoordinate) -------------
    // Field square: x 0..25, y 0..14.
    static bool isOnField(int x, int y);
    // Box column: x in {-1..-7} (home) or {30..36} (away).
    static bool isBoxX(int x);

    // --- Player state helpers (port of ffb-common PlayerState) ---------------
    // State ids carry bit flags (0x100 ACTIVE, 0x200 CONFUSED, ...); the base
    // state (UNKNOWN..IN_THE_AIR, 0..19) is id & 0xFF.
    static int baseState(int state) { return state & 0xFF; }
    // Short label for the UI: "KO", "BH", "SI", "RIP", "RES", "BAN", "STN",
    // "PRN", "MISS", "BLD" or empty for on-field active states.
    static QString stateLabel(int state);
    // Human-readable base state (port of PlayerState.getButtonText):
    // "Standing", "Prone", "has been stunned", "Knocked Out", ...
    static QString stateText(int state);
    // Active flag bits worth showing (PlayerState.java):
    // 0x100 ACTIVE, 0x200 CONFUSED, 0x400 ROOTED, 0x800 HYPNOTIZED,
    // 0x20000 EYE GOUGED, 0x40000 CHOMPED. Returns e.g. ["Confused", "Rooted"].
    static QStringList stateFlags(int state);

signals:
    // Emitted after initialize() and after each applyModelChanges() batch.
    void stateUpdated();

private:
    void parsePlayerArray(const QJsonArray &array, bool homeSide);
    // Fills the positionId -> name/shorthand/keywords maps for one team.
    void clearState();
    void parseRosterPositions(const QJsonObject &team, bool homeSide);
    void applyFieldModel(const QJsonObject &fieldModel);
    void handleModelChange(const QString &id, const QString &key, const QJsonValue &value);
    FFBPlayer *findPlayer(const QString &playerId);
    // Updates fieldX/fieldY/hasCoords for a player (value = [x, y]).
    void setPlayerCoords(FFBPlayer *player, const QJsonValue &value);

    bool m_initialized = false;

    // Teams / score / turn
    QString m_teamHomeName;
    QString m_teamAwayName;
    QString m_teamHomeId;
    QString m_teamAwayId;
    QString m_teamHomeRace;
    QString m_teamAwayRace;
    QString m_teamHomeCoach;
    QString m_teamAwayCoach;
    QString m_teamHomeLogoUrl;
    QString m_teamAwayLogoUrl;
    int m_reRollsHome = 0;   // team total (number of dots)
    int m_reRollsAway = 0;
    int m_turnReRollsHome = 0; // remaining THIS turn (dots still colored)
    int m_turnReRollsAway = 0;
    QHash<QPoint, int> m_trackNumbers; // turn 13: server trail cell numbers
    QSet<QString> m_actedThisTurn;    // turn 13: players already acted
    bool m_actorActivated = false;    // turn 16: the current actor chose an
                                      // action (green-check gate)
    QString m_passTargetId;           // turn 14: pass receiver (white glow)
    QString m_blitzingPlayerId;       // turn 13: this turn's blitzer

    // Position lookup: positionId -> positionName/shorthand/keywords,
    // filled from teamX.roster.positionArray (live players carry only the
    // positionId, the name lives in the team's roster definition).
    QHash<QString, QString> m_positionNameHome;
    QHash<QString, QString> m_positionNameAway;
    QHash<QString, QString> m_positionShortHome;
    QHash<QString, QString> m_positionShortAway;
    QHash<QString, QString> m_positionKeywordsHome;
    QHash<QString, QString> m_positionKeywordsAway;
    int m_scoreHome = 0;
    int m_scoreAway = 0;
    int m_turn = 1;
    int m_half = 0;
    QString m_turnMode = QStringLiteral("setup");
    bool m_pendingKickoff = false; // turn 15: kickoff turn started, flight pending
    int m_kickoffSeq = 0;          // turn 15: bumps once per kickoff flight
    QPoint m_kickoffOrigin;
    QPoint m_kickoffLanding;
    QString m_kickoffKickerId;
    bool m_homePlaying = true;
    QString m_weather;

    // Players: single source of truth + per-side roster ordering.
    QHash<QString, int> m_playerIndex;     // playerId -> index into m_players
    QVector<FFBPlayer> m_players;
    QStringList m_rosterHomeIds;
    QStringList m_rosterAwayIds;

    // Acting player (turn holder).
    QString m_activePlayerId;
    QString m_blockTargetId; // round 13 turn 4: the player being blocked
    bool m_blockGlowOnly = false; // turn 16: foul target (red glow, no marker)
    QString m_blockAttackerId; // turn 8: whose actor glow holds the block open
    int m_blockDieFace = 0; // turn 11: chosen block die (0 = pending)

    // Move trail of the acting player (round 13; round 15: continuous
    // numbering across the whole action — see MoveSegment/BlockStepMark).
    QString m_traceActor;                  // player whose trace is tracked
    QVector<MoveSegment> m_moveSegments;   // one entry per move route
    QVector<BlockStepMark> m_blockStepMarks; // blocks that cost a movement
    int m_nextStepNumber = 1;              // next number of the drive
    bool m_driveUnnumbered = false;        // the drive STARTED with a block
                                           // (plain block of an adjacent
                                           // player): trail only, no numbers
                                           // (user: "basta il solo trail")
    bool m_blockResolved = false;          // gameSetDefenderId null landed
                                           // since the last block opened
    QString m_gfiPendingFor;               // player with a successful GFI
                                           // roll: its next NEW segment is red
    bool m_movePending = false;            // a move action was dispatched:
                                           // the next coordinate change of
                                           // the actor starts a NEW path

    // Ball
    bool m_ballOnField = false;
    int m_ballX = -1;
    int m_ballY = -1;
};
