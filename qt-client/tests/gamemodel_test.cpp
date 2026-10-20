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

    // Human-readable base states (PlayerState.getButtonText port)
    QCOMPARE(GameModel::stateText(1), QStringLiteral("Standing"));
    QCOMPARE(GameModel::stateText(3), QStringLiteral("Prone"));
    QCOMPARE(GameModel::stateText(4 | 0x100), QStringLiteral("has been stunned"));
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

QTEST_MAIN(GameModelTest)
#include "gamemodel_test.moc"
