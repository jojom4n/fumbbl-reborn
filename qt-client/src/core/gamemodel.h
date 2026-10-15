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

    // Apply a batch of model changes:
    // serverModelSync.modelChangeList.modelChangeArray
    // (each: { modelChangeId, modelChangeKey, modelChangeValue }).
    void applyModelChanges(const QJsonArray &changes);

    bool isInitialized() const { return m_initialized; }

    // --- Teams / score / turn ----------------------------------------------
    QString teamHomeName() const { return m_teamHomeName; }
    QString teamAwayName() const { return m_teamAwayName; }
    // Team race (live players usually carry no per-player race).
    QString teamHomeRace() const { return m_teamHomeRace; }
    QString teamAwayRace() const { return m_teamAwayRace; }
    int scoreHome() const { return m_scoreHome; }
    int scoreAway() const { return m_scoreAway; }
    int turn() const { return m_turn; }
    int half() const { return m_half; }
    QString turnMode() const { return m_turnMode; } // setup|first_turn|regular|halftime|overtime|ended
    bool homePlaying() const { return m_homePlaying; }

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
    QString m_teamHomeRace;
    QString m_teamAwayRace;

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
    bool m_homePlaying = true;

    // Players: single source of truth + per-side roster ordering.
    QHash<QString, int> m_playerIndex;     // playerId -> index into m_players
    QVector<FFBPlayer> m_players;
    QStringList m_rosterHomeIds;
    QStringList m_rosterAwayIds;

    // Acting player (turn holder).
    QString m_activePlayerId;

    // Ball
    bool m_ballOnField = false;
    int m_ballX = -1;
    int m_ballY = -1;
};
