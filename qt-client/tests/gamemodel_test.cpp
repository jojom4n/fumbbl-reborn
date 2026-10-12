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
    void modelChangesEssentials();
    void boxAndRemovePlayer();
    void staticHelpers();
};

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
    // Unknown modelChangeId: must be a silent no-op (TS reference behaviour)
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

    // Removed player is gone from the roster too
    QCOMPARE(model.rosterHome().size(), 10);
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
}

QTEST_MAIN(GameModelTest)
#include "gamemodel_test.moc"
