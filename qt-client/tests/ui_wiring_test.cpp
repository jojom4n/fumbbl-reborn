// =============================================================================
// ui_wiring_test — core -> UI wiring (spectator)
//
// Drives a real MainWindow (offscreen) by calling its public controller entry
// points (onGameState / onModelSync / onGameTime / onTalk / onJoined) and
// feeding the game model, then asserts the UI widgets update. This exercises
// the production data path:
//   model state -> stateUpdated -> applyModelToUi -> widgets
//   onModelSync -> model.applyModelChanges + dice log
//   onGameTime/onTalk/onJoined -> header/chat/spectators
//
// (Emitting SpectatorSession signals with arguments through the meta-object is
// not a stable public API on Qt 6.12, so the session layer is tested separately
// by the live spectator_smoke tool.)
// =============================================================================
#include <QtTest>

#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QLabel>

#include "core/gamemodel.h"
#include "ui/chatwidget.h"
#include "ui/dicelogwidget.h"
#include "ui/fieldview.h"
#include "ui/headerbar.h"
#include "ui/mainwindow.h"
#include "ui/rosterwidget.h"

namespace {

QJsonObject fixtureGame()
{
    QFile f(QStringLiteral("serverGameState_fixture.json"));
    if (!f.open(QIODevice::ReadOnly))
        qFatal("cannot open fixture");
    return QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("game")).toObject();
}

QJsonObject makeChange(const QString &id, const QString &key, const QJsonValue &value)
{
    QJsonObject c;
    c.insert(QStringLiteral("modelChangeId"), id);
    c.insert(QStringLiteral("modelChangeKey"), key);
    c.insert(QStringLiteral("modelChangeValue"), value);
    return c;
}

bool labelContains(QWidget *root, const QString &needle)
{
    const auto labels = root->findChildren<QLabel *>();
    for (QLabel *l : labels)
        if (l->text().contains(needle))
            return true;
    return false;
}

} // namespace

class UiWiringTest : public QObject
{
    Q_OBJECT

private slots:
    void gameStatePopulatesUi();
    void modelSyncUpdatesUi();
    void gameTimeDrivesTimer();
    void talkAndJoinPopulateChatAndSpectators();
};

void UiWiringTest::gameStatePopulatesUi()
{
    MainWindow w;
    w.show();
    GameModel *model = w.model();
    FieldView *field = w.findChild<FieldView *>();
    RosterWidget *home = w.findChild<RosterWidget *>(QStringLiteral("roster_home"));
    RosterWidget *away = w.findChild<RosterWidget *>(QStringLiteral("roster_away"));
    QVERIFY(model && field && home && away);

    w.onGameState(fixtureGame());

    // Model initialized
    QVERIFY(model->isInitialized());
    QCOMPARE(model->teamHomeName(), QStringLiteral("Orcs United"));
    QCOMPARE(model->teamAwayName(), QStringLiteral("Dwarves FC"));

    // Header shows teams + default score
    QVERIFY(labelContains(&w, QStringLiteral("Orcs United  vs  Dwarves FC")));
    QVERIFY(labelContains(&w, QStringLiteral("0 : 0")));

    // Rosters populated (11 each)
    QCOMPARE(home->count(), 11);
    QCOMPARE(away->count(), 11);

    // Field: 22 tokens, ball at (8,5)
    QCOMPARE(field->playerCount(), 22);
    QCOMPARE(field->ballPosition(), QPoint(8, 5));
}

void UiWiringTest::modelSyncUpdatesUi()
{
    MainWindow w;
    w.show();
    GameModel *model = w.model();
    FieldView *field = w.findChild<FieldView *>();

    w.onGameState(fixtureGame());
    QCOMPARE(field->playerCount(), 22);

    // A sync: score changes, ball moves, a player is KO'd into the home box.
    QJsonArray changes;
    changes.append(makeChange(QStringLiteral("teamResultSetScore"), QStringLiteral("home"), QJsonValue(3)));
    QJsonArray ball;
    ball << 12 << 3;
    changes.append(makeChange(QStringLiteral("fieldModelSetBallCoordinate"), QString(), QJsonValue(ball)));
    QJsonArray ko;
    ko << -2 << 0;
    changes.append(makeChange(QStringLiteral("fieldModelSetPlayerCoordinate"),
                              QStringLiteral("18109690"), QJsonValue(ko)));
    QJsonArray reports; // none
    w.onModelSync(changes, reports);

    QCOMPARE(model->scoreHome(), 3);
    QVERIFY(labelContains(&w, QStringLiteral("3 : 0")));
    QCOMPARE(field->ballPosition(), QPoint(12, 3));
    // One player moved into the box -> 21 on field
    QCOMPARE(field->playerCount(), 21);
}

void UiWiringTest::gameTimeDrivesTimer()
{
    MainWindow w;
    w.show();

    // turnTime = 15000 ms elapsed -> 30 - 15 = 15 s remaining
    w.onGameTime(0, 15000);
    QVERIFY(labelContains(&w, QStringLiteral("15s")));

    // clamp at 0
    w.onGameTime(0, 99000);
    QVERIFY(labelContains(&w, QStringLiteral("0s")));
}

void UiWiringTest::talkAndJoinPopulateChatAndSpectators()
{
    MainWindow w;
    w.show();
    ChatWidget *chat = w.findChild<ChatWidget *>();
    DiceLogWidget *dice = w.findChild<DiceLogWidget *>();
    QVERIFY(chat && dice);

    // serverTalk: coach + talks[]
    QJsonObject talk;
    talk.insert(QStringLiteral("coach"), QStringLiteral("Saruman"));
    QJsonArray talks;
    talks.append(QStringLiteral("gg"));
    talk.insert(QStringLiteral("talks"), QJsonValue(talks));
    w.onTalk(talk);
    QVERIFY(chat->toPlainText().contains(QStringLiteral("Saruman: gg")));

    // serverJoin: spectators count
    QJsonObject join;
    join.insert(QStringLiteral("spectators"), QJsonValue(5));
    join.insert(QStringLiteral("coach"), QStringLiteral("Saruman"));
    w.onJoined(join);
    QVERIFY(labelContains(&w, QStringLiteral("5")));

    // A modelSync with a report lands in the dice log
    QJsonObject dodge;
    dodge.insert(QStringLiteral("reportId"), QStringLiteral("dodgeRoll"));
    dodge.insert(QStringLiteral("playerId"), QStringLiteral("18109690"));
    dodge.insert(QStringLiteral("roll"), QJsonValue(3));
    dodge.insert(QStringLiteral("minimumRoll"), QJsonValue(4));
    dodge.insert(QStringLiteral("successful"), QJsonValue(true));
    QJsonArray reports;
    reports.append(dodge);
    QJsonArray changes;
    w.onModelSync(changes, reports);
    QVERIFY(dice->toPlainText().contains(QStringLiteral("Dodge")));
}

QTEST_MAIN(UiWiringTest)
#include "ui_wiring_test.moc"
