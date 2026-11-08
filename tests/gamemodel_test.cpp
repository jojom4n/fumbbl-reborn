// =============================================================================
// gamemodel_test — GameModel port verification
//
// Fixture: serverGameState_fixture.json (a REAL serverGameState capture,
// vector 6 of lzstring_vectors.json — "Orcs United" vs "Dwarves FC").
// =============================================================================
#include <QtTest>

#include <QFile>
#include <QJsonDocument>

#include "core/gamemodel.h"

namespace {
QJsonObject loadFixtureGame()
{
    QFile f(QStringLiteral("serverGameState_fixture.json"));
    if (!f.open(QIODevice::ReadOnly))
        qFatal("cannot open serverGameState_fixture.json");
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        qFatal("bad fixture JSON");
    return doc.object().value(QStringLiteral("game")).toObject();
}

const FieldToken *tokenById(const QVector<FieldToken> &tokens, const QString &id)
{
    for (const FieldToken &t : tokens)
        if (t.playerId == id)
            return &t;
    return nullptr;
}
} // namespace

class GameModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initializeFromRealGameState();
    void initializeFromLiveGameState();
    void modelChangesEssentials();
    void boxAndRemovePlayer();
    void moveTrailAccumulatesAndClears();
    void moveNumbersContinuousAcrossBlitzBlockAndFrenzy();
    void gfiExtraMoveWithoutActionReEmit();
    void turnMarksAndBlockMarkerLifecycle();
    void staticHelpers();
    void positionNames();
    void resetClearsState();
};

namespace {
QJsonObject loadLiveFixtureGame()
{
    QFile f(QStringLiteral("liveGameState_fixture.json"));
    if (!f.open(QIODevice::ReadOnly))
        qFatal("cannot open liveGameState_fixture.json");
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        qFatal("bad live fixture JSON");
    return doc.object().value(QStringLiteral("game")).toObject();
}
} // namespace

void GameModelTest::initializeFromRealGameState()
{
    GameModel model;
    QVERIFY(!model.isInitialized());

    QSignalSpy updated(&model, &GameModel::stateUpdated);
    QVERIFY(model.initialize(loadFixtureGame()));
    QVERIFY(model.isInitialized());
    QCOMPARE(updated.count(), 1);

    // Teams
    QCOMPARE(model.teamHomeName(), QStringLiteral("Orcs United"));
    QCOMPARE(model.teamAwayName(), QStringLiteral("Dwarves FC"));
    QCOMPARE(model.rosterHome().size(), 11);
    QCOMPARE(model.rosterAway().size(), 11);

    // Field: 22 players with coordinates, none in box
    const QVector<FieldToken> tokens = model.fieldTokens();
    QCOMPARE(tokens.size(), 22);
    QCOMPARE(model.boxTokens(true).size(), 0);
    QCOMPARE(model.boxTokens(false).size(), 0);

    // Ball on an empty square: (8,5), nobody standing on it
    int bx = -1, by = -1;
    QVERIFY(model.ballAt(bx, by));
    QCOMPARE(bx, 8);
    QCOMPARE(by, 5);
    for (const FieldToken &t : tokens)
        QVERIFY(!t.hasBall);

    // Game state
    QCOMPARE(model.turn(), 7);
    QCOMPARE(model.half(), 1);
    QCOMPARE(model.turnMode(), QStringLiteral("regular"));
    QVERIFY(model.homePlaying());
    QCOMPARE(model.scoreHome(), 0);
    QCOMPARE(model.scoreAway(), 0);
    QCOMPARE(model.activePlayerId(), QStringLiteral("18109690"));

    // The acting player is on the field and flagged
    const FieldToken *acting = tokenById(tokens, QStringLiteral("18109690"));
    QVERIFY(acting);
    QVERIFY(acting->isActing);
    QCOMPARE(acting->x, 3);
    QCOMPARE(acting->y, 2);

    // A roster entry carries stats + skills
    const QVector<FFBPlayer> home = model.rosterHome();
    const FFBPlayer *p0 = nullptr;
    for (const FFBPlayer &p : home)
        if (p.playerId == QLatin1String("18109690"))
            p0 = &p;
    QVERIFY(p0);
    QVERIFY(p0->number > 0);
    QVERIFY(!p0->position.isEmpty());
    QVERIFY(!p0->skills.isEmpty());
    QCOMPARE(p0->state, 1); // STANDING from playerDataArray
}

// A REAL serverGameState capture taken live from fumbbl.com (game 1953034,
// Big Monsters Big Spooky vs OwaOgre). Exercises the live payload shape:
// players carry positionId (not positionName), passing, playerStatus,
// lastingInjuries, skillValuesMap, usedSkills; coordinates only in
// fieldModel.playerDataArray.
void GameModelTest::initializeFromLiveGameState()
{
    GameModel model;
    QVERIFY(model.initialize(loadLiveFixtureGame()));
    QVERIFY(model.isInitialized());

    QCOMPARE(model.teamHomeName(), QStringLiteral("Big Monsters Big Spooky (BBT18"));
    QCOMPARE(model.teamAwayName(), QStringLiteral("OwaOgre"));
    QCOMPARE(model.teamHomeRace(), QStringLiteral("Necromantic Horror"));
    QCOMPARE(model.teamAwayRace(), QStringLiteral("Old World Alliance"));
    QCOMPARE(model.rosterHome().size(), 13);
    QCOMPARE(model.rosterAway().size(), 13);

    // Turn / half / side
    QCOMPARE(model.half(), 1);
    QVERIFY(!model.homePlaying());
    QCOMPARE(model.turn(), 5); // home turnNr wins

    // Field: 17 on-field tokens, ball at (12,12)
    QCOMPARE(model.fieldTokens().size(), 17);
    int bx = -1, by = -1;
    QVERIFY(model.ballAt(bx, by));
    QCOMPARE(bx, 12);
    QCOMPARE(by, 12);

    // Adrian Custennin: full stats incl. passing, position resolved via
    // positionId 66250 -> "Werewolf" (shorthand "W", keywords incl. Blitzer)
    const FFBPlayer *adrian = model.findPlayerById(QStringLiteral("18234934"));
    QVERIFY(adrian);
    QCOMPARE(adrian->ma, 8);
    QCOMPARE(adrian->st, 3);
    QCOMPARE(adrian->ag, 3);
    QCOMPARE(adrian->pa, 3);
    QCOMPARE(adrian->av, 9);
    QCOMPARE(adrian->playerStatus, QStringLiteral("active"));
    QCOMPARE(adrian->position, QStringLiteral("Werewolf"));
    QCOMPARE(adrian->positionShorthand, QStringLiteral("W"));
    QVERIFY(adrian->positionKeywords.contains(QStringLiteral("Blitzer")));
    QVERIFY(adrian->positionKeywords.contains(QStringLiteral("Undead")));
    QVERIFY(adrian->skills.contains(QStringLiteral("Frenzy")));
    QCOMPARE(adrian->state, 257); // STANDING + ACTIVE flag
    QCOMPARE(GameModel::stateText(adrian->state), QStringLiteral("Standing"));

    // He is on the field at (10,14) and findable by coordinate
    QVERIFY(adrian->hasCoords);
    QCOMPARE(adrian->fieldX, 10);
    QCOMPARE(adrian->fieldY, 14);
    QCOMPARE(model.playerAt(10, 14)->playerId, adrian->playerId);
    QVERIFY(model.playerAt(0, 0) == nullptr);

    // Dean Wuthmon: skillValuesMap (Hatred "treeman")
    const FFBPlayer *dean = model.findPlayerById(QStringLiteral("18234940"));
    QVERIFY(dean);
    QCOMPARE(dean->skillValues.value(QStringLiteral("Hatred")),
             QStringLiteral("treeman"));

    // Julia Fasten: lasting injury parsed
    const FFBPlayer *julia = model.findPlayerById(QStringLiteral("18304199"));
    QVERIFY(julia);
    QCOMPARE(julia->lastingInjuries.size(), 1);
    QCOMPARE(julia->lastingInjuries.first(), QStringLiteral("Seriously Hurt (MNG)"));
    QCOMPARE(julia->position, QStringLiteral("Wraith")); // positionId 66249

    // Unknown id
    QVERIFY(model.findPlayerById(QStringLiteral("nope")) == nullptr);
}

void GameModelTest::modelChangesEssentials()
{
    GameModel model;
    QVERIFY(model.initialize(loadFixtureGame()));

    QSignalSpy updated(&model, &GameModel::stateUpdated);

    QJsonArray changes;
    auto change = [](const QString &id, const QString &key, const QJsonValue &value) {
        QJsonObject c;
        c.insert(QStringLiteral("modelChangeId"), id);
        c.insert(QStringLiteral("modelChangeKey"), key);
        c.insert(QStringLiteral("modelChangeValue"), value);
        return c;
    };
    QJsonArray coords;
    coords << 12 << 3;
    QJsonObject turnMode;
    turnMode.insert(QStringLiteral("name"), QStringLiteral("halftime"));

    changes.append(change(QStringLiteral("fieldModelSetBallCoordinate"),
                          QString(), QJsonValue(coords)));
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"),
                          QStringLiteral("18109690"), QJsonValue(coords)));
    changes.append(change(QStringLiteral("fieldModelSetPlayerState"),
                          QStringLiteral("18109690"), QJsonValue(5))); // KO
    changes.append(change(QStringLiteral("gameSetHalf"), QString(), QJsonValue(2)));
    changes.append(change(QStringLiteral("gameSetHomePlaying"), QString(), QJsonValue(false)));
    changes.append(change(QStringLiteral("gameSetTurnMode"), QString(), QJsonValue(turnMode)));
    changes.append(change(QStringLiteral("gameSetPassCoordinate"), QString(), QJsonValue(coords)));
    changes.append(change(QStringLiteral("turnDataSetTurnNr"), QStringLiteral("home"), QJsonValue(8)));
    changes.append(change(QStringLiteral("teamResultSetScore"), QStringLiteral("home"), QJsonValue(2)));
    changes.append(change(QStringLiteral("teamResultSetScore"), QStringLiteral("away"), QJsonValue(1)));
    changes.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(),
                          QJsonValue(QStringLiteral("18109691"))));
    // Unknown modelChangeId: must be a silent no-op (the server ignores
    // unknown ids on the way in; we must too)
    changes.append(change(QStringLiteral("fieldModelAddPrayer"), QString(), QJsonValue(1)));

    model.applyModelChanges(changes);
    QCOMPARE(updated.count(), 1);

    // Ball moved to the pass coordinate (last writer wins: pass after set)
    int bx = -1, by = -1;
    QVERIFY(model.ballAt(bx, by));
    QCOMPARE(bx, 12);
    QCOMPARE(by, 3);

    // Player 18109690 moved to (12,3) and is now on the ball square -> hasBall
    const QVector<FieldToken> tokens = model.fieldTokens();
    const FieldToken *moved = tokenById(tokens, QStringLiteral("18109690"));
    QVERIFY(moved);
    QCOMPARE(moved->x, 12);
    QCOMPARE(moved->y, 3);
    QCOMPARE(moved->state, 5);
    QVERIFY(moved->hasBall);
    QCOMPARE(GameModel::stateLabel(moved->state), QStringLiteral("KO"));

    // Game state updates
    QCOMPARE(model.half(), 2);
    QVERIFY(!model.homePlaying());
    QCOMPARE(model.turnMode(), QStringLiteral("halftime"));
    QCOMPARE(model.turn(), 8);
    QCOMPARE(model.scoreHome(), 2);
    QCOMPARE(model.scoreAway(), 1);
    QCOMPARE(model.activePlayerId(), QStringLiteral("18109691"));
    // The new acting player is flagged, the old one no longer
    const FieldToken *newActing = tokenById(tokens, QStringLiteral("18109691"));
    QVERIFY(newActing && newActing->isActing);
}

void GameModelTest::boxAndRemovePlayer()
{
    GameModel model;
    QVERIFY(model.initialize(loadFixtureGame()));

    // 18109690 (home) to the home KO box (x=-2) -> leaves the field, enters box
    QJsonArray changes;
    QJsonObject c1;
    c1.insert(QStringLiteral("modelChangeId"), QStringLiteral("fieldModelSetPlayerCoordinate"));
    c1.insert(QStringLiteral("modelChangeKey"), QStringLiteral("18109690"));
    QJsonArray boxCoords;
    boxCoords << -2 << 0;
    c1.insert(QStringLiteral("modelChangeValue"), QJsonValue(boxCoords));
    changes.append(c1);

    QJsonObject c2;
    c2.insert(QStringLiteral("modelChangeId"), QStringLiteral("fieldModelSetPlayerState"));
    c2.insert(QStringLiteral("modelChangeKey"), QStringLiteral("18109690"));
    c2.insert(QStringLiteral("modelChangeValue"), QJsonValue(5));
    changes.append(c2);

    QJsonObject c3;
    c3.insert(QStringLiteral("modelChangeId"), QStringLiteral("fieldModelRemovePlayer"));
    c3.insert(QStringLiteral("modelChangeKey"), QStringLiteral("18109691"));
    c3.insert(QStringLiteral("modelChangeValue"), QJsonValue());
    changes.append(c3);

    model.applyModelChanges(changes);

    // 20 players on the field (22 - KO box - removed)
    const QVector<FieldToken> tokens = model.fieldTokens();
    QCOMPARE(tokens.size(), 20);
    QVERIFY(!tokenById(tokens, QStringLiteral("18109690")));
    QVERIFY(!tokenById(tokens, QStringLiteral("18109691")));

    // KO player in the home box
    const QVector<FieldToken> box = model.boxTokens(true);
    QCOMPARE(box.size(), 1);
    QCOMPARE(box.first().playerId, QStringLiteral("18109690"));
    QCOMPARE(box.first().state, 5);

    // fieldModelRemovePlayer clears the player's FIELD DATA but does NOT
    // delete the player from the roster. The server
    // emits it for EVERY player before each re-placement (post-TD / half
    // setup) — removing them from the roster is what made players vanish
    // after every drive (they were re-placed with setPlayerCoordinate, but
    // the model had already dropped them). The player must survive with its
    // field coordinates cleared.
    const FFBPlayer *removed = model.findPlayerById(QStringLiteral("18109691"));
    QVERIFY(removed != nullptr);          // still in the roster
    QCOMPARE(model.rosterHome().size(), 11);
    QVERIFY(!removed->hasCoords);         // field data cleared
}

// The CUMULATIVE move trail of the remote acting player.
// Semantics:
//   * the server steps the actor ONE SQUARE AT A TIME, so the trail grows
//     progressively behind him (one cell per fieldModelSetPlayerCoordinate);
//   * a NEW move by the SAME player APPENDS another path (its own arrow) —
//     the spectator sees the whole cumulative route (no timeout, no replace);
//   * when ANOTHER player is activated the whole trace is wiped and the new
//     actor starts clean.
void GameModelTest::moveTrailAccumulatesAndClears()
{
    GameModel model;
    QVERIFY(model.initialize(loadFixtureGame()));
    // Fixture: 18109690 at (3,2); 18109691 at (4,3).

    QJsonArray changes;
    auto change = [](const QString &id, const QString &key, const QJsonValue &value) {
        QJsonObject c;
        c.insert(QStringLiteral("modelChangeId"), id);
        c.insert(QStringLiteral("modelChangeKey"), key);
        c.insert(QStringLiteral("modelChangeValue"), value);
        return c;
    };
    auto coords = [](int x, int y) { QJsonArray a; a << x << y; return QJsonValue(a); };
    const QString actor = QStringLiteral("18109690");
    const QString other = QStringLiteral("18109691");

    // Activate the actor + first move: (3,2) -> (3,3) -> (4,3).
    changes.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(), QJsonValue(actor)));
    changes.append(change(QStringLiteral("actingPlayerSetPlayerAction"), QString(), QJsonValue(QStringLiteral("move"))));
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor, coords(3, 3)));
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor, coords(4, 3)));
    model.applyModelChanges(changes);

    QCOMPARE(model.moveTraceActor(), actor);
    QCOMPARE(model.moveTrace().size(), 1);
    QCOMPARE(model.moveTrace().first().size(), 3);
    QCOMPARE(model.moveTrace().first().at(0), QPoint(3, 2)); // start square
    QCOMPARE(model.moveTrace().first().at(1), QPoint(3, 3));
    QCOMPARE(model.moveTrace().first().at(2), QPoint(4, 3));

    // A SECOND move by the SAME player: (4,3) -> (5,3). It APPENDS a new
    // path (the trace is cumulative, not replaced).
    changes = QJsonArray();
    changes.append(change(QStringLiteral("actingPlayerSetPlayerAction"), QString(), QJsonValue(QStringLiteral("move"))));
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor, coords(5, 3)));
    model.applyModelChanges(changes);

    QCOMPARE(model.moveTrace().size(), 2);
    QCOMPARE(model.moveTrace().last().size(), 2);
    QCOMPARE(model.moveTrace().last().at(0), QPoint(4, 3));
    QCOMPARE(model.moveTrace().last().at(1), QPoint(5, 3));

    // A non-actor moving (a defended/pushed player) does NOT touch the trace.
    changes = QJsonArray();
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), other, coords(5, 4)));
    model.applyModelChanges(changes);
    QCOMPARE(model.moveTrace().size(), 2);

    // Activating ANOTHER player wipes the previous actor's whole trace.
    changes = QJsonArray();
    changes.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(), QJsonValue(other)));
    model.applyModelChanges(changes);
    QCOMPARE(model.moveTraceActor(), other);
    QCOMPARE(model.moveTrace().size(), 0);

    // A NEW turn (setup/kickoff) also clears the trace.
    changes = QJsonArray();
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), other, coords(6, 4)));
    QJsonObject ko; ko.insert(QStringLiteral("name"), QStringLiteral("kickoff"));
    changes.append(change(QStringLiteral("gameSetTurnMode"), QString(), QJsonValue(ko)));
    model.applyModelChanges(changes);
    QCOMPARE(model.moveTrace().size(), 0);
}

// The between-cells numbers are CONTINUOUS across the whole action (the
// reported bug was a BLITZ whose follow move restarted the count at 1).
// Spec: the sideways square is move 1, the block itself is a movement (2),
// the follow steps continue (3,4,...), a frenzy's second block is the next
// number, and a go-for-it extra move continues the count and is flagged
// red.
void GameModelTest::moveNumbersContinuousAcrossBlitzBlockAndFrenzy()
{
    GameModel model;
    QVERIFY(model.initialize(loadFixtureGame()));
    // Fixture: 18109690 at (3,2); 18109691 at (4,3).

    QJsonArray changes;
    auto change = [](const QString &id, const QString &key, const QJsonValue &value) {
        QJsonObject c;
        c.insert(QStringLiteral("modelChangeId"), id);
        c.insert(QStringLiteral("modelChangeKey"), key);
        c.insert(QStringLiteral("modelChangeValue"), value);
        return c;
    };
    auto coords = [](int x, int y) { QJsonArray a; a << x << y; return QJsonValue(a); };
    const QString actor = QStringLiteral("18109690");
    const QString defender = QStringLiteral("18109691");

    // BLITZ: sideways step (3,2) -> (3,3) = move 1.
    changes.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(),
                          QJsonValue(actor)));
    changes.append(change(QStringLiteral("actingPlayerSetPlayerAction"), QString(),
                          QJsonValue(QStringLiteral("blitzMove"))));
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
                          coords(3, 3)));
    model.applyModelChanges(changes);
    QCOMPARE(model.moveSegments().size(), 1);
    QCOMPARE(model.moveSegments().first().stepNumbers.size(), 1);
    QCOMPARE(model.moveSegments().first().stepNumbers.first(), 1);

    // BLOCK (the `block` report lands while the attacker is the actor):
    // the block COSTS A MOVEMENT — number 2, painted between the
    // attacker's cell (3,3) and the defender's cell (4,3).
    model.setBlockTargetId(defender);
    model.noteBlockStep(defender);
    QCOMPARE(model.blockStepMarks().size(), 1);
    QCOMPARE(model.blockStepMarks().first().number, 2);
    QCOMPARE(model.blockStepMarks().first().from, QPoint(3, 3));
    QCOMPARE(model.blockStepMarks().first().to, QPoint(4, 3));

    // A re-sent `block` report for the same still-open block adds NO second
    // mark.
    model.noteBlockStep(defender);
    QCOMPARE(model.blockStepMarks().size(), 1);

    // Push (defender moves — never traced), the block resolves (null
    // defender id), and the server RE-DISPATCHES the action for the follow
    // move (StepEndBlocking). The follow steps CONTINUE the count: 3,4.
    changes = QJsonArray();
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), defender,
                          coords(5, 3)));
    changes.append(change(QStringLiteral("gameSetDefenderId"), QString(),
                          QJsonValue()));
    changes.append(change(QStringLiteral("actingPlayerSetPlayerAction"), QString(),
                          QJsonValue(QStringLiteral("blitzMove"))));
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
                          coords(3, 2)));
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
                          coords(2, 2)));
    model.applyModelChanges(changes);
    QCOMPARE(model.moveSegments().size(), 2);
    const QVector<int> follow = model.moveSegments().last().stepNumbers;
    QCOMPARE(follow.size(), 2);
    QCOMPARE(follow.first(), 3);
    QCOMPARE(follow.last(), 4);
    QVERIFY(!model.moveSegments().last().goForIt);

    // FRENZY: a second block (fresh `block` report after the resolution)
    // costs another movement — number 5.
    changes = QJsonArray();
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), defender,
                          coords(1, 2)));
    model.applyModelChanges(changes);
    model.noteBlockStep(defender);
    QCOMPARE(model.blockStepMarks().size(), 2);
    QCOMPARE(model.blockStepMarks().last().number, 5);
    QCOMPARE(model.blockStepMarks().last().from, QPoint(2, 2));
    QCOMPARE(model.blockStepMarks().last().to, QPoint(1, 2));

    // GO FOR IT: a successful roll — the next new route is the extra move,
    // continuing the count (6) and flagged red.
    changes = QJsonArray();
    changes.append(change(QStringLiteral("gameSetDefenderId"), QString(),
                          QJsonValue()));
    changes.append(change(QStringLiteral("actingPlayerSetPlayerAction"), QString(),
                          QJsonValue(QStringLiteral("move"))));
    model.applyModelChanges(changes);
    model.setGoForItPending(actor);
    changes = QJsonArray();
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
                          coords(2, 1)));
    model.applyModelChanges(changes);
    QCOMPARE(model.moveSegments().size(), 3);
    QCOMPARE(model.moveSegments().last().stepNumbers.size(), 1);
    QCOMPARE(model.moveSegments().last().stepNumbers.first(), 6);
    QVERIFY(model.moveSegments().last().goForIt);

    // A new actor wipes segments, marks and the counter alike.
    changes = QJsonArray();
    changes.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(),
                          QJsonValue(defender)));
    model.applyModelChanges(changes);
    QCOMPARE(model.moveSegments().size(), 0);
    QCOMPARE(model.blockStepMarks().size(), 0);

    // --- A PLAIN block action (the drive STARTS with the block, no
    //     movement before it) is UNNUMBERED for its whole lifetime: no
    //     block mark, no follow-step numbers. Just the trail (for a
    //     blocked adjacent player the trail alone is enough — no cell
    //     counting).
    // Geometry: the defender (new actor) sits at (1,2); put the other
    // player (at (2,1)) adjacent at (2,2).
    changes = QJsonArray();
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
                          coords(2, 2)));
    changes.append(change(QStringLiteral("actingPlayerSetPlayerAction"), QString(),
                          QJsonValue(QStringLiteral("block"))));
    model.applyModelChanges(changes);
    model.setBlockTargetId(actor);
    model.noteBlockStep(actor); // the FIRST event of the drive is the block
    QCOMPARE(model.blockStepMarks().size(), 0); // unnumbered: no mark

    // Push + a BARE follow step (StepFollowup — no action re-dispatch)
    // + resolution: the follow is TRACED (the trail is drawn) but carries
    // NO numbers.
    changes = QJsonArray();
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
                          coords(3, 2))); // push
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), defender,
                          coords(1, 1))); // the follow (block still open)
    changes.append(change(QStringLiteral("gameSetDefenderId"), QString(),
                          QJsonValue()));
    model.applyModelChanges(changes);
    QCOMPARE(model.moveSegments().size(), 1);
    QCOMPARE(model.moveSegments().first().cells, QVector<QPoint>({ QPoint(1, 2), QPoint(1, 1) }));
    QVERIFY(model.moveSegments().first().stepNumbers.isEmpty());
    QVERIFY(!model.moveSegments().first().goForIt);

    // Same, but the block FAILS (no push): the go-for-it extra move is a
    // red TRAIL too, still unnumbered.
    changes = QJsonArray();
    changes.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(),
                          QJsonValue(actor))); // fresh drive
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), defender,
                          coords(3, 1))); // adjacent to the actor (3,2)
    changes.append(change(QStringLiteral("actingPlayerSetPlayerAction"), QString(),
                          QJsonValue(QStringLiteral("block"))));
    model.applyModelChanges(changes);
    model.setBlockTargetId(defender);
    model.noteBlockStep(defender);
    QCOMPARE(model.blockStepMarks().size(), 0);
    changes = QJsonArray();
    changes.append(change(QStringLiteral("gameSetDefenderId"), QString(),
                          QJsonValue())); // the block failed
    changes.append(change(QStringLiteral("actingPlayerSetPlayerAction"), QString(),
                          QJsonValue(QStringLiteral("move"))));
    model.applyModelChanges(changes);
    model.setGoForItPending(actor);
    changes = QJsonArray();
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
                          coords(4, 2))); // the GFI extra step
    model.applyModelChanges(changes);
    QCOMPARE(model.moveSegments().size(), 1);
    QVERIFY(model.moveSegments().first().stepNumbers.isEmpty());
    QVERIFY(model.moveSegments().first().goForIt); // red trail
}

// The GO-FOR-IT extra move, as it actually comes on the wire (the bug the
// user reported: the red trail NEVER appeared). GROUND TRUTH (ffb-common
// ActingPlayer.setPlayerAction: `if (pPlayerAction == fPlayerAction)
// return;` — NO sync when the action does not change): after a successful
// GFI roll the server re-selects the SAME player (Id=X is always emitted)
// but does NOT re-emit actingPlayerSetPlayerAction=move (it is already
// "move"). m_movePending therefore stays false, and the GFI step must
// still open a NEW RED route (not be appended to the previous white one),
// continuing the count.
void GameModelTest::gfiExtraMoveWithoutActionReEmit()
{
    GameModel model;
    QVERIFY(model.initialize(loadFixtureGame()));
    // Fixture: 18109690 at (3,2); 18109691 at (4,3).

    QJsonArray changes;
    auto change = [](const QString &id, const QString &key, const QJsonValue &value) {
        QJsonObject c;
        c.insert(QStringLiteral("modelChangeId"), id);
        c.insert(QStringLiteral("modelChangeKey"), key);
        c.insert(QStringLiteral("modelChangeValue"), value);
        return c;
    };
    auto coords = [](int x, int y) { QJsonArray a; a << x << y; return QJsonValue(a); };
    const QString actor = QStringLiteral("18109690");

    // MOVE: (3,2) -> (4,2) -> (5,2) — the white route, numbers 1,2.
    changes.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(),
                          QJsonValue(actor)));
    changes.append(change(QStringLiteral("actingPlayerSetPlayerAction"), QString(),
                          QJsonValue(QStringLiteral("move"))));
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
                          coords(4, 2)));
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
                          coords(5, 2)));
    model.applyModelChanges(changes);
    QCOMPARE(model.moveSegments().size(), 1);
    QCOMPARE(model.moveSegments().first().stepNumbers, QVector<int>({1, 2}));
    QVERIFY(!model.moveSegments().first().goForIt);

    // The action ENDS (Id=None between actions — the capture-verified
    // pattern). The trace (and the pending state) must survive it.
    changes = QJsonArray();
    changes.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(),
                          QJsonValue()));
    model.applyModelChanges(changes);

    // The GFI roll succeeds.
    model.setGoForItPending(actor);

    // The GFI move: re-selection of the SAME player (always emitted) and
    // NO actingPlayerSetPlayerAction. The steps (5,2) -> (6,2) -> (6,3)
    // open a NEW RED route, count continues (3,4).
    changes = QJsonArray();
    changes.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(),
                          QJsonValue(actor)));
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
                          coords(6, 2)));
    changes.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
                          coords(6, 3)));
    model.applyModelChanges(changes);
    QCOMPARE(model.moveSegments().size(), 2);
    const GameModel::MoveSegment &gfi = model.moveSegments().last();
    QCOMPARE(gfi.cells,
             QVector<QPoint>({QPoint(5, 2), QPoint(6, 2), QPoint(6, 3)}));
    QCOMPARE(gfi.stepNumbers, QVector<int>({3, 4}));
    QVERIFY(gfi.goForIt);
    // The original route stays white.
    QVERIFY(!model.moveSegments().first().goForIt);
}

void GameModelTest::staticHelpers()
{
    // Field bounds (26x15, x 0..25 left=home TD, y 0..14)
    QVERIFY(GameModel::isOnField(0, 0));
    QVERIFY(GameModel::isOnField(25, 14));
    QVERIFY(!GameModel::isOnField(26, 7));
    QVERIFY(!GameModel::isOnField(12, 15));
    QVERIFY(!GameModel::isOnField(-1, 7));

    // Box columns: home -1..-7, away 30..36
    for (int x = -7; x <= -1; ++x)
        QVERIFY(GameModel::isBoxX(x));
    for (int x = 30; x <= 36; ++x)
        QVERIFY(GameModel::isBoxX(x));
    QVERIFY(!GameModel::isBoxX(0));
    QVERIFY(!GameModel::isBoxX(25));
    QVERIFY(!GameModel::isBoxX(29));
    QVERIFY(!GameModel::isBoxX(-8));

    // State labels + bit flags (ACTIVE bit 0x100 must not leak into the base)
    QCOMPARE(GameModel::stateLabel(5), QStringLiteral("KO"));
    QCOMPARE(GameModel::stateLabel(5 | 0x100), QStringLiteral("KO"));
    QCOMPARE(GameModel::stateLabel(6), QStringLiteral("BH"));
    QCOMPARE(GameModel::stateLabel(7), QStringLiteral("SI"));
    QCOMPARE(GameModel::stateLabel(8), QStringLiteral("RIP"));
    QCOMPARE(GameModel::stateLabel(9), QStringLiteral("RES"));
    QCOMPARE(GameModel::stateLabel(13), QStringLiteral("BAN"));
    QCOMPARE(GameModel::stateLabel(1), QString()); // STANDING -> no label
    QCOMPARE(GameModel::baseState(1 | 0x100 | 0x200), 1);

    // Human-readable base states (PlayerState.getButtonText port)
    QCOMPARE(GameModel::stateText(1), QStringLiteral("Standing"));
    QCOMPARE(GameModel::stateText(3), QStringLiteral("Prone"));
    QCOMPARE(GameModel::stateText(4 | 0x100), QStringLiteral("Stunned"));
    QCOMPARE(GameModel::stateText(5), QStringLiteral("Knocked Out"));
    QCOMPARE(GameModel::stateText(257), QStringLiteral("Standing")); // flags masked

    // Flag bits
    QVERIFY(GameModel::stateFlags(0x100).isEmpty()); // ACTIVE alone: no flag
    QCOMPARE(GameModel::stateFlags(0x200).size(), 1);
    QCOMPARE(GameModel::stateFlags(0x200).first(), QStringLiteral("Confused"));
    QCOMPARE(GameModel::stateFlags(0x400).first(), QStringLiteral("Rooted"));
    QCOMPARE(GameModel::stateFlags(0x800).first(), QStringLiteral("Hypnotized"));
    QCOMPARE(GameModel::stateFlags(0x20000).first(), QStringLiteral("Eye gouged"));
    QCOMPARE(GameModel::stateFlags(0x40000).first(), QStringLiteral("Chomped"));
    QCOMPARE(GameModel::stateFlags(1 | 0x200 | 0x400).size(), 2);
}

void GameModelTest::positionNames()
{
    // Live players carry only a positionId; the model must resolve the
    // roster position name (it is the face-lookup key).
    GameModel m;
    QVERIFY(m.initialize(loadLiveFixtureGame()));
    bool sawName = false;
    for (const FFBPlayer &p : m.rosterHome())
        if (!p.position.isEmpty())
            sawName = true;
    QVERIFY(sawName);
    // Necromantic Horror fixture: the position names are the real piece
    // names ("Zombie Lineman", "Werewolf", ...) — exactly what faces.txt
    // is keyed by.
    QStringList have;
    for (const FFBPlayer &p : m.rosterHome())
        have << p.position;
    QVERIFY(have.contains(QStringLiteral("Zombie Lineman")));
    QVERIFY(have.contains(QStringLiteral("Werewolf")));
    // Every on-field token carries its position name.
    const auto tokens = m.fieldTokens();
    QVERIFY(!tokens.isEmpty());
    for (const FieldToken &t : tokens)
        QVERIFY(!t.positionName.isEmpty());
}

void GameModelTest::resetClearsState()
{
    // Switching to a different game resets the model to its pre-game state
    // (the UI clears itself from the same signal).
    GameModel m;
    QVERIFY(m.initialize(loadLiveFixtureGame()));
    QVERIFY(m.isInitialized());
    QVERIFY(!m.rosterHome().isEmpty());

    m.reset();
    QVERIFY(!m.isInitialized());
    QVERIFY(m.rosterHome().isEmpty());
    QVERIFY(m.rosterAway().isEmpty());
    QVERIFY(m.teamHomeName().isEmpty());
    QVERIFY(m.teamAwayName().isEmpty());
    QCOMPARE(m.fieldTokens().size(), 0);

    // Re-initialization still works after a reset.
    QVERIFY(m.initialize(loadLiveFixtureGame()));
    QVERIFY(m.isInitialized());
    QVERIFY(!m.rosterHome().isEmpty());
}

// The per-turn marks and the block-marker lifecycle — the model side of
// the field badges:
//   * track numbers parse the server's {coordinate, number} payload,
//   * the blitzer is the actor when turnDataSetBlitzUsed flips true,
//   * the moment a player stops acting they are marked "acted this turn",
//   * the block marker (helmet/die) dies when the attacker starts a NEW
//     move, when the defender id goes null, or on a new turn.
void GameModelTest::turnMarksAndBlockMarkerLifecycle()
{
    GameModel model;
    QVERIFY(model.initialize(loadFixtureGame()));

    auto change = [](const QString &id, const QString &key, const QJsonValue &value) {
        QJsonObject c;
        c.insert(QStringLiteral("modelChangeId"), id);
        c.insert(QStringLiteral("modelChangeKey"), key);
        c.insert(QStringLiteral("modelChangeValue"), value);
        return c;
    };

    // --- track numbers: value = {coordinate: [x, y], number: N} ---------
    QJsonObject tn;
    tn.insert(QStringLiteral("coordinate"), QJsonArray() << 4 << 5);
    tn.insert(QStringLiteral("number"), 0);
    QJsonArray ch;
    ch.append(change(QStringLiteral("fieldModelAddTrackNumber"), QString(), QJsonValue(tn)));
    QJsonObject tn2;
    tn2.insert(QStringLiteral("coordinate"), QJsonArray() << 3 << 5);
    tn2.insert(QStringLiteral("number"), 1);
    ch.append(change(QStringLiteral("fieldModelAddTrackNumber"), QString(), QJsonValue(tn2)));
    model.applyModelChanges(ch);
    QCOMPARE(model.trackNumbers().value(QPoint(4, 5)), 0);
    QCOMPARE(model.trackNumbers().value(QPoint(3, 5)), 1);
    QJsonObject rm;
    rm.insert(QStringLiteral("coordinate"), QJsonArray() << 4 << 5);
    ch = QJsonArray();
    ch.append(change(QStringLiteral("fieldModelRemoveTrackNumber"), QString(), QJsonValue(rm)));
    model.applyModelChanges(ch);
    QVERIFY(!model.trackNumbers().contains(QPoint(4, 5)));
    QVERIFY(model.trackNumbers().contains(QPoint(3, 5)));

    // --- blitz: the actor carries the lightning while blitzUsed = true ---
    ch = QJsonArray();
    ch.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(),
                     QJsonValue(QStringLiteral("18109690"))));
    ch.append(change(QStringLiteral("turnDataSetBlitzUsed"), QString(), QJsonValue(true)));
    model.applyModelChanges(ch);
    QCOMPARE(model.blitzingPlayerId(), QStringLiteral("18109690"));
    ch = QJsonArray();
    ch.append(change(QStringLiteral("turnDataSetBlitzUsed"), QString(), QJsonValue(false)));
    model.applyModelChanges(ch);
    QVERIFY(model.blitzingPlayerId().isEmpty());

    // --- acted this turn (the green check appeared on the wrong event):
    //     it must appear on
    // DEFINITIVE activation (actingPlayerSetPlayerAction), NOT when the
    // actor leaves. Select -> deselect must NOT mark.
    ch = QJsonArray();
    ch.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(),
                     QJsonValue(QStringLiteral("18109690"))));
    model.applyModelChanges(ch);
    QVERIFY(!model.actedThisTurn().contains(QStringLiteral("18109690")));
    // Deselect: the human wants to think — the actor goes null, no action
    // was taken. Old (buggy) behavior marked here; now: still unmarked.
    ch = QJsonArray();
    ch.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(),
                     QJsonValue())); // null: deselection
    model.applyModelChanges(ch);
    QVERIFY(!model.actedThisTurn().contains(QStringLiteral("18109690")));
    // Reselect + ACTIVATE: now — and only now — the green check appears.
    ch = QJsonArray();
    ch.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(),
                     QJsonValue(QStringLiteral("18109690"))));
    ch.append(change(QStringLiteral("actingPlayerSetPlayerAction"), QString(),
                     QJsonValue(QStringLiteral("move"))));
    model.applyModelChanges(ch);
    QVERIFY(model.actedThisTurn().contains(QStringLiteral("18109690")));
    // The next selected actor is NOT acted just from being selected.
    ch = QJsonArray();
    ch.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(),
                     QJsonValue(QStringLiteral("18109691"))));
    model.applyModelChanges(ch);
    QVERIFY(!model.actedThisTurn().contains(QStringLiteral("18109691")));

    // --- block marker lifecycle -----------------------------------------
    // The attacker IS the actor (real flow: block reports arrive while the
    // attacker acts).
    ch = QJsonArray();
    ch.append(change(QStringLiteral("actingPlayerSetPlayerId"), QString(),
                     QJsonValue(QStringLiteral("18109690"))));
    model.applyModelChanges(ch);
    model.setBlockTargetId(QStringLiteral("18109691")); // attacker = actor
    model.setBlockDieFace(2); // the chosen die: the marker shows the symbol
    QVERIFY(model.blockMarkerActive());
    QCOMPARE(model.blockDieFace(), 2);

    // A NEW move by the attacker ends the block: the marker is not left
    // attached to the token.
    ch = QJsonArray();
    QJsonArray coords;
    coords << 14 << 5;
    ch.append(change(QStringLiteral("actingPlayerSetPlayerAction"), QString(),
                     QJsonValue(QStringLiteral("move"))));
    ch.append(change(QStringLiteral("fieldModelSetPlayerCoordinate"),
                     QStringLiteral("18109690"), QJsonValue(coords)));
    model.applyModelChanges(ch);
    QVERIFY(!model.blockMarkerActive());
    QCOMPARE(model.blockDieFace(), 0);
    // ...but the defender's red glow survives (turn-8 rule).
    QCOMPARE(model.blockTargetId(), QStringLiteral("18109691"));

    // A new turn wipes the per-turn marks (and any re-armed marker).
    model.setBlockTargetId(QStringLiteral("18109691"));
    model.setBlockDieFace(3);
    QVERIFY(model.blockMarkerActive());
    ch = QJsonArray();
    ch.append(change(QStringLiteral("turnDataSetTurnNr"), QStringLiteral("home"),
                     QJsonValue(6)));
    model.applyModelChanges(ch);
    QVERIFY(!model.blockMarkerActive());
    QVERIFY(model.actedThisTurn().isEmpty());
    QVERIFY(model.blitzingPlayerId().isEmpty());
}

QTEST_MAIN(GameModelTest)
#include "gamemodel_test.moc"
