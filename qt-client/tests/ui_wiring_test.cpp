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
#include <QSet>

#include "core/assets.h"
#include "core/gamemodel.h"
#include "ui/chatwidget.h"
#include "ui/dicelogwidget.h"
#include "ui/fieldview.h"
#include "ui/headerbar.h"
#include "ui/mainwindow.h"
#include "ui/panel.h"
#include "ui/playerinfowidget.h"
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
    void fieldClickSelectsPlayer();
    void fieldClickEmptyDeselects();
    void selectionFollowsModelSync();
    void gameTimeDrivesTimer();
    void talkAndJoinPopulateChatAndSpectators();
    void assetsLoad();
    void panelLayoutPersistAndReset();
    void savedLayoutRestoredOnFirstShow();
};

namespace {
// Deterministic click point: compute the center of a cell with the SAME math
// as FieldView (the field now fills the whole widget, 26x15 cells, phase 10).
QPoint cellCenter(const FieldView *field, int x, int y)
{
    const qreal cellW = field->width() / 26.0;
    const qreal cellH = field->height() / 15.0;
    return QPoint(int((x + 0.5) * cellW), int((y + 0.5) * cellH));
}

// The FloatingPanel that wraps `content` (the HUD overlay of phase 10).
FloatingPanel *panelFor(QWidget *root, QWidget *content)
{
    const auto panels = root->findChildren<FloatingPanel *>();
    for (FloatingPanel *p : panels)
        if (p->contentWidget() == content)
            return p;
    return nullptr;
}
} // namespace

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

// Clicking a player token selects it: the Player Info panel fills with the
// model data and the field highlights the token.
void UiWiringTest::fieldClickSelectsPlayer()
{
    MainWindow w;
    w.show();
    GameModel *model = w.model();
    FieldView *field = w.findChild<FieldView *>();
    PlayerInfoWidget *info = w.findChild<PlayerInfoWidget *>();
    QVERIFY(model && field && info);

    w.onGameState(fixtureGame());

    // Fix the field size for deterministic click math.
    field->setFixedSize(780, 450);
    QApplication::processEvents();

    // Acting player 18109690 stands at (3,2).
    const FFBPlayer *p = model->findPlayerById(QStringLiteral("18109690"));
    QVERIFY(p && p->hasCoords);
    const QPoint pt = cellCenter(field, 3, 2);
    QCOMPARE(field->tokenAt(pt)->playerId, QStringLiteral("18109690"));

    QTest::mouseClick(field, Qt::LeftButton, {}, pt);

    QCOMPARE(w.selectedPlayerId(), QStringLiteral("18109690"));
    QCOMPARE(info->shownPlayerId(), QStringLiteral("18109690"));
    QCOMPARE(field->selectedPlayer(), QStringLiteral("18109690"));
    // The panel shows the player's name and a stat (ST from the model).
    QVERIFY(labelContains(info, p->name));
    QVERIFY(labelContains(info, QStringLiteral("ST")));
}

// Clicking an empty square deselects (placeholder back, no highlight).
void UiWiringTest::fieldClickEmptyDeselects()
{
    MainWindow w;
    w.show();
    GameModel *model = w.model();
    FieldView *field = w.findChild<FieldView *>();
    PlayerInfoWidget *info = w.findChild<PlayerInfoWidget *>();
    QVERIFY(model && field && info);

    w.onGameState(fixtureGame());
    field->setFixedSize(780, 450);
    QApplication::processEvents();

    // Find a field cell with no player on it.
    QSet<QPoint> occupied;
    for (const FieldToken &t : model->fieldTokens())
        occupied.insert(QPoint(t.x, t.y));
    int ex = -1, ey = -1;
    for (int x = 0; x < 26 && ex < 0; ++x)
        for (int y = 0; y < 15; ++y)
            if (!occupied.contains(QPoint(x, y))) {
                ex = x;
                ey = y;
                break;
            }
    QVERIFY(ex >= 0);

    // Select first, then click the empty cell.
    const FieldToken first = model->fieldTokens().first();
    const FFBPlayer *any = model->findPlayerById(first.playerId);
    QVERIFY(any);
    w.selectPlayer(first.playerId);
    QCOMPARE(w.selectedPlayerId(), first.playerId);

    QTest::mouseClick(field, Qt::LeftButton, {}, cellCenter(field, ex, ey));

    QCOMPARE(w.selectedPlayerId(), QString());
    QCOMPARE(info->shownPlayerId(), QString());
    QCOMPARE(field->selectedPlayer(), QString());
}

// The selection survives model updates: state changes refresh the panel;
// removing the player deselects.
void UiWiringTest::selectionFollowsModelSync()
{
    MainWindow w;
    w.show();
    GameModel *model = w.model();
    FieldView *field = w.findChild<FieldView *>();
    PlayerInfoWidget *info = w.findChild<PlayerInfoWidget *>();
    QVERIFY(model && field && info);

    w.onGameState(fixtureGame());
    field->setFixedSize(780, 450);
    QApplication::processEvents();

    const QString id = QStringLiteral("18109690");
    w.selectPlayer(id);
    QCOMPARE(w.selectedPlayerId(), id);

    // The selected player is KO'd into the home box: selection kept, panel
    // shows the box state.
    QJsonArray changes;
    QJsonArray ko;
    ko << -2 << 0;
    QJsonObject c1;
    c1.insert(QStringLiteral("modelChangeId"), QStringLiteral("fieldModelSetPlayerCoordinate"));
    c1.insert(QStringLiteral("modelChangeKey"), id);
    c1.insert(QStringLiteral("modelChangeValue"), QJsonValue(ko));
    changes.append(c1);
    QJsonObject c2;
    c2.insert(QStringLiteral("modelChangeId"), QStringLiteral("fieldModelSetPlayerState"));
    c2.insert(QStringLiteral("modelChangeKey"), id);
    c2.insert(QStringLiteral("modelChangeValue"), QJsonValue(5)); // KO
    changes.append(c2);
    w.onModelSync(changes, QJsonArray());

    QCOMPARE(w.selectedPlayerId(), id);
    QCOMPARE(info->shownPlayerId(), id);
    QVERIFY(labelContains(info, QStringLiteral("KO")));

    // The player is removed from the game: the selection is dropped.
    QJsonArray changes2;
    QJsonObject c3;
    c3.insert(QStringLiteral("modelChangeId"), QStringLiteral("fieldModelRemovePlayer"));
    c3.insert(QStringLiteral("modelChangeKey"), id);
    c3.insert(QStringLiteral("modelChangeValue"), QJsonValue());
    changes2.append(c3);
    w.onModelSync(changes2, QJsonArray());

    QCOMPARE(w.selectedPlayerId(), QString());
    QCOMPARE(info->shownPlayerId(), QString());
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

void UiWiringTest::assetsLoad()
{
    QVERIFY(assets::isAvailable());
    // Faces: exact roster names (case-insensitive), slug fallback, and the
    // generic fallback for unknown pieces.
    const QStringList rosterNames = { QStringLiteral("Zombie Lineman"),
                                      QStringLiteral("Werewolf"),
                                      QStringLiteral("Ogre"),
                                      QStringLiteral("Troll Slayer"),
                                      QStringLiteral("Dark Elf Assassin"),
                                      QStringLiteral("Kroxigor"),
                                      QStringLiteral("Minotaur"),
                                      QStringLiteral("'Ooligan"),
                                      QStringLiteral("Altern Forest Treeman") };
    for (const QString &n : rosterNames)
        QVERIFY2(!assets::playerFace(n).isNull(), qPrintable(n));
    QVERIFY(!assets::playerFace(QStringLiteral("OGRE")).isNull()); // case
    QVERIFY(!assets::playerFace(QStringLiteral("Totally Unknown Piece"))
                 .isNull()); // generic face
    // Custom-named star pieces fall back to the race representative face.
    const QPixmap star =
        assets::playerFace(QStringLiteral("Count Luthor von Drakenborg"),
                           QStringLiteral("Vampire"));
    const QPixmap raceRep = assets::playerFace(QStringLiteral("Vampire Runner"));
    QVERIFY(!star.isNull());
    QCOMPARE(star.size(), raceRep.size());
    QCOMPARE(assets::playerFace(QStringLiteral("Crumbleberry"),
                                 QStringLiteral("Old World Alliance"))
                 .size(),
             assets::playerFace(QStringLiteral("Human Lineman")).size());
    QCOMPARE(assets::faceSlug(QStringLiteral("Zombie Lineman")),
             QStringLiteral("zombie_lineman"));
    QCOMPARE(assets::faceSlug(QStringLiteral("Wraith")), QStringLiteral("wraith"));
    // skill icons (known + fallback)
    QVERIFY(!assets::skillPixmap(QStringLiteral("Dodge")).isNull());
    QVERIFY(!assets::skillPixmap(QStringLiteral("Knockdown")).isNull());
    QVERIFY(!assets::skillPixmap(QStringLiteral("TotallyBogusSkill")).isNull());
    QCOMPARE(assets::skillIconSlug(QStringLiteral("DODGE")), QStringLiteral("dodge"));
    QCOMPARE(assets::skillIconSlug(QStringLiteral("NeverHeardOfIt")),
             QStringLiteral("generic"));
    // card icons
    QVERIFY(!assets::cardPixmap(QStringLiteral("Rabbit's Foot")).isNull());
    QCOMPARE(assets::cardIconSlug(QStringLiteral("rabbit's foot")),
             QStringLiteral("rabbit_s_foot"));
}

void UiWiringTest::panelLayoutPersistAndReset()
{
    // Phase 10 HUD: the panels float over the field. Verify that:
    //  1) the default layout is applied on the first sizing,
    //  2) "Reset Layout" restores the default geometry (position AND size —
    //     the old QDS-based reset lost the sizes),
    //  3) savedLayout()/restoreLayout() round-trip the user's sizes.
    QSettings settings;
    settings.remove(QStringLiteral("mainwindow/panelLayout")); // no stale state

    MainWindow w;
    w.show();

    RosterWidget *home = w.findChild<RosterWidget *>(QStringLiteral("roster_home"));
    QVERIFY(home);
    FloatingPanel *panel = panelFor(&w, home);
    QVERIFY(panel);

    // The default layout lands on the first stage resize.
    QTRY_VERIFY_WITH_TIMEOUT(panel->isVisible(), 3000);
    const QRect defaultGeom = panel->geometry();
    QVERIFY(defaultGeom.width() >= FloatingPanel::kMinWidth);
    QVERIFY(defaultGeom.height() >= FloatingPanel::kMinHeight);

    // The user moves + resizes the panel...
    QRect custom = defaultGeom;
    custom.moveLeft(defaultGeom.x() + 120);
    custom.setWidth(defaultGeom.width() + 90);
    panel->setGeometry(custom);
    QCOMPARE(panel->geometry(), custom);

    // ... "Reset Layout" restores the default position AND size.
    w.resetLayout();
    QCOMPARE(panel->geometry(), defaultGeom);

    // ... and the serialized state round-trips the user's size.
    panel->setGeometry(custom);
    const QByteArray state = w.savedLayout();
    QVERIFY(!state.isEmpty());
    QVERIFY(QString::fromUtf8(state).contains(QStringLiteral("\"w\"")));
    panel->setGeometry(defaultGeom);
    w.restoreLayout(state);
    QCOMPARE(panel->geometry(), custom);
}

void UiWiringTest::savedLayoutRestoredOnFirstShow()
{
    // Regression (phase 10): a layout saved by a PREVIOUS run must be
    // restored on the first stage resize — while the top-level is still
    // mid-show. The old action-sync used panel->isVisible() (false at that
    // moment) and its toggled signal re-hid every panel.
    QSettings settings;
    QJsonObject panels;
    const struct { const char *name; int x, y, w, h; } seed[] = {
        { "home_roster", 40, 40, 300, 400 },
        { "away_roster", 450, 40, 260, 300 },
        { "player_info", 40, 350, 380, 220 },
        { "dice_log", 430, 350, 280, 220 },
        { "chat", 450, 60, 260, 220 },
        { "protocol", 700, 40, 200, 200 },
    };
    for (const auto &s2 : seed) {
        QJsonObject p;
        p.insert(QStringLiteral("x"), s2.x);
        p.insert(QStringLiteral("y"), s2.y);
        p.insert(QStringLiteral("w"), s2.w);
        p.insert(QStringLiteral("h"), s2.h);
        p.insert(QStringLiteral("v"), true);
        panels.insert(s2.name, p);
    }
    QJsonObject root;
    root.insert(QStringLiteral("panels"), panels);
    settings.setValue(QStringLiteral("mainwindow/panelLayout"),
                      QJsonDocument(root).toJson(QJsonDocument::Compact));

    MainWindow w; // ctor shows maximized; the saved layout lands on first sizing
    RosterWidget *home = w.findChild<RosterWidget *>(QStringLiteral("roster_home"));
    QVERIFY(home);
    FloatingPanel *panel = panelFor(&w, home);
    QVERIFY(panel);

    QTRY_VERIFY_WITH_TIMEOUT(panel->isVisible(), 3000);
    QCOMPARE(panel->x(), 40);
    QCOMPARE(panel->y(), 40);
    QCOMPARE(panel->width(), 300);
    QCOMPARE(panel->height(), 400);
}

QTEST_MAIN(UiWiringTest)
#include "ui_wiring_test.moc"
