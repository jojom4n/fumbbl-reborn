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
#include <QFileInfo>
#include <QJsonDocument>
#include <QBuffer>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QSet>
#include <QResource>
#include <QSignalSpy>
#include <QSpinBox>
#include <QToolButton>

#include "core/assets.h"
#include "core/gamemodel.h"
#include "core/matchlist.h"
#include "core/reportparser.h"
#include "ui/actionlog.h"
#include "ui/chatwidget.h"
#include "ui/fieldview.h"
#include "ui/headerview.h"
#include "ui/mainwindow.h"
#include "ui/panel.h"
#include "ui/playerinfowidget.h"
#include "ui/spectatedialog.h"

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
    void fieldClickShowsCard();
    void fieldClickEmptyHidesCard();
    void glowFollowsRemoteSelection();
    void cardFollowsModelSync();
    void gameTimeDrivesTimer();
    void talkAndJoinPopulateChatAndSpectators();
    void assetsLoad();
    void panelLayoutPersistAndReset();
    void savedLayoutRestoredOnFirstShow();
    void matchListParsing();
    void faceBareNameLookup();
    void diceLogShowsPlayerName();
    void newGameStateReplacesOldMatch();
    void manualGameIdDoesNotOverrideSelection();
    // ---- Round 9 (live-report alignment + UI fixes) ----
    void playerActionReportShowsSubject();
    void blockReportsShowDiceAndNames();
    void unresolvedNumericIdNeverShown();
    void capturedLiveReportsParse();
    void chatCollapsesToPillWithUnreadBadge();
    void actionLogChevronPointsToNextState();
    void selectionDoesNotOpenActionWheel();
    void skillChipsWrapWithoutClipping();
    // ---- Round 10 (header spacing, logos, chat default, upward log, glow) ----
    void chatStartsCollapsedByDefault();
    void actionLogExpandsUpward();
    void hoverAndClickDoNotMoveGlow();
    void playerInfoCardIsChromeless();
    void matchListLogoFallbackUsedWhenGameStateNull();
    void proneAndStunnedMarksRendered();
    // ---- Round 11 (side-zone lines, dice, downed faces, card growth) ----
    void wideZoneLinesDrawn();
    void downedFaceNotSquashed();
    void diceAssetsPreScaled();
    void playerCardGrowsWithSkillCount();
    // ---- Round 12 (remote glow, animated rain) ----
    void rainSimulationDropsAndSplashes();
    void fieldRainAnimationActive();
};

namespace {
// Deterministic click point: compute the center of a cell with the SAME math
// as FieldView (the field now fills the whole widget, 26x15 cells, phase 10).
QPoint cellCenter(const FieldView *field, int x, int y)
{
    // Round 13 scene: [dugout 5 | gap 1 | pitch 26 | gap 1 | dugout 5] =
    // 38x15 cells, SQUARE (the scene is fitted into the widget and centered
    // — letterbox). The pitch starts at column 6 (dugout 5 + gap 1).
    const qreal W = field->width(), H = field->height();
    const qreal cell = qMin(W / 38.0, H / 15.0);
    const qreal ox = (W - 38.0 * cell) / 2.0;
    const qreal oy = (H - 15.0 * cell) / 2.0;
    return QPoint(int(ox + (6 + x + 0.5) * cell), int(oy + (y + 0.5) * cell));
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
    ActionLogWidget *actionLog = w.findChild<ActionLogWidget *>();
    // Round 8: rosters are gone (field box strips cover box/KO/reserve).
    QVERIFY(!w.findChild<QWidget *>(QStringLiteral("roster_home")));
    QVERIFY(!w.findChild<QWidget *>(QStringLiteral("roster_away")));
    QVERIFY(model && field && actionLog);

    w.onGameState(fixtureGame());

    // Model initialized
    QVERIFY(model->isInitialized());
    QCOMPARE(model->teamHomeName(), QStringLiteral("Orcs United"));
    QCOMPARE(model->teamAwayName(), QStringLiteral("Dwarves FC"));

    // Header (Mockup F) is custom-painted: verify its state via accessors.
    HeaderView *hv = w.findChild<HeaderView *>();
    QVERIFY(hv);
    QCOMPARE(hv->teamHomeName(), QStringLiteral("Orcs United"));
    QCOMPARE(hv->teamAwayName(), QStringLiteral("Dwarves FC"));
    QCOMPARE(hv->scoreHome(), 0);
    QCOMPARE(hv->scoreAway(), 0);

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
    HeaderView *hv = w.findChild<HeaderView *>();
    QVERIFY(hv);
    QCOMPARE(hv->scoreHome(), 3);
    QCOMPARE(hv->scoreAway(), 0);
    QCOMPARE(field->ballPosition(), QPoint(12, 3));
    // One player moved into the box -> 21 on field
    QCOMPARE(field->playerCount(), 21);
}

// Round 12: clicking a player token only shows its info card (the same as
// hovering it). The field rim glow is the REMOTE coach's selection (the
// model's actingPlayer) and a local click never moves it.
void UiWiringTest::fieldClickShowsCard()
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

    // The fixture's REMOTE acting player is 18109690 at (3,2): the glow is
    // on it before any local input.
    QCOMPARE(field->selectedPlayer(), QStringLiteral("18109690"));

    // Find ANOTHER on-field player and click it.
    const FFBPlayer *other = nullptr;
    for (const FieldToken &t : model->fieldTokens()) {
        if (t.playerId == QStringLiteral("18109690"))
            continue;
        if (const FFBPlayer *c = model->findPlayerById(t.playerId)) {
            other = c;
            break;
        }
    }
    QVERIFY(other && other->hasCoords);
    const QPoint pt = cellCenter(field, other->fieldX, other->fieldY);

    QTest::mouseClick(field, Qt::LeftButton, {}, pt);

    // The CARD shows the clicked player...
    QCOMPARE(info->shownPlayerId(), other->playerId);
    // ...but the GLOW stays on the remote acting player.
    QCOMPARE(field->selectedPlayer(), QStringLiteral("18109690"));
    // The panel shows the player's name (uppercase, Mockup F card) and a
    // stat (ST from the model).
    QVERIFY(labelContains(info, other->name.toUpper()));
    QVERIFY(labelContains(info, QStringLiteral("ST")));
}

// Round 12: clicking an empty square hides the card; the glow (remote
// selection) is untouched.
void UiWiringTest::fieldClickEmptyHidesCard()
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

    // Glow: the fixture's remote acting player.
    QCOMPARE(field->selectedPlayer(), QStringLiteral("18109690"));

    // Show the card for 18109690 with a real click (it stands at (3,2)).
    QTest::mouseClick(field, Qt::LeftButton, {}, cellCenter(field, 3, 2));
    QCOMPARE(info->shownPlayerId(), QStringLiteral("18109690"));

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

    QTest::mouseClick(field, Qt::LeftButton, {}, cellCenter(field, ex, ey));

    QCOMPARE(info->shownPlayerId(), QString()); // card hidden
    QCOMPARE(field->selectedPlayer(), QStringLiteral("18109690")); // glow unchanged
}

// Round 12: the rim glow is the REMOTE human's selection. It follows the
// actingPlayerSetPlayerId model changes (the moment the coach picks a
// player to act) and clears when the action ends (null value).
void UiWiringTest::glowFollowsRemoteSelection()
{
    MainWindow w;
    w.show();
    GameModel *model = w.model();
    FieldView *field = w.findChild<FieldView *>();
    QVERIFY(model && field);

    w.onGameState(fixtureGame());
    field->setFixedSize(780, 450);
    QApplication::processEvents();

    // Initial: the fixture's acting player.
    QCOMPARE(field->selectedPlayer(), QStringLiteral("18109690"));

    // The remote coach picks another player: the glow moves there.
    const FFBPlayer *other = nullptr;
    for (const FieldToken &t : model->fieldTokens()) {
        if (t.playerId == QStringLiteral("18109690"))
            continue;
        if (model->findPlayerById(t.playerId)) {
            other = model->findPlayerById(t.playerId);
            break;
        }
    }
    QVERIFY(other);
    QJsonArray changes;
    QJsonObject c;
    c.insert(QStringLiteral("modelChangeId"), QStringLiteral("actingPlayerSetPlayerId"));
    c.insert(QStringLiteral("modelChangeValue"), other->playerId);
    changes.append(c);
    w.onModelSync(changes, QJsonArray());
    QCOMPARE(field->selectedPlayer(), other->playerId);

    // The action ends: actingPlayer cleared (null) -> no glow.
    QJsonArray changes2;
    QJsonObject c2;
    c2.insert(QStringLiteral("modelChangeId"), QStringLiteral("actingPlayerSetPlayerId"));
    c2.insert(QStringLiteral("modelChangeValue"), QJsonValue()); // null
    changes2.append(c2);
    w.onModelSync(changes2, QJsonArray());
    QCOMPARE(field->selectedPlayer(), QString());
}

// The info card survives model updates: state changes refresh it; removing
// the player hides it (round 12: the card is opened by hover/click but its
// data stays live).
void UiWiringTest::cardFollowsModelSync()
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
    QTest::mouseClick(field, Qt::LeftButton, {}, cellCenter(field, 3, 2));
    QCOMPARE(info->shownPlayerId(), id);

    // The player is KO'd into the home box: card kept, shows the box state.
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

    QCOMPARE(info->shownPlayerId(), id);
    QVERIFY(labelContains(info, QStringLiteral("KO")));

    // ROUND 13 (point 17 fix): fieldModelRemovePlayer clears the player's
    // FIELD DATA but does NOT remove the player from the game — the server
    // emits it for every player before each re-placement (post-TD / half
    // setup). The old code deleted the player from the model here, which
    // (a) made players vanish after every drive and (b) wrongly hid the card.
    // Correct behavior: the player survives, field coordinates are cleared,
    // and the card STAYS (the player is still in the match).
    QJsonArray changes2;
    QJsonObject c3;
    c3.insert(QStringLiteral("modelChangeId"), QStringLiteral("fieldModelRemovePlayer"));
    c3.insert(QStringLiteral("modelChangeKey"), id);
    c3.insert(QStringLiteral("modelChangeValue"), QJsonValue());
    changes2.append(c3);
    w.onModelSync(changes2, QJsonArray());

    QCOMPARE(info->shownPlayerId(), id); // still in the game -> card kept
    const FFBPlayer *after = model->findPlayerById(id);
    QVERIFY(after && !after->hasCoords); // field data cleared
}

void UiWiringTest::gameTimeDrivesTimer()
{
    MainWindow w;
    w.show();

    // Mockup F: the turn timer COUNTS UP (FUMBBL semantics), no countdown.
    // turnTime = 15000 ms elapsed -> 0:15
    HeaderView *hv = w.findChild<HeaderView *>();
    QVERIFY(hv);
    w.onGameTime(0, 15000);
    QCOMPARE(hv->elapsedSeconds(), 15);

    // no clamping: 99000 ms -> 1:39
    w.onGameTime(0, 99000);
    QCOMPARE(hv->elapsedSeconds(), 99);
}

void UiWiringTest::talkAndJoinPopulateChatAndSpectators()
{
    MainWindow w;
    w.show();
    ChatWidget *chat = w.findChild<ChatWidget *>();
    ActionLogWidget *dice = w.findChild<ActionLogWidget *>();
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

    // Round 8: the rosters are gone — use the player info panel (it still
    // participates in the layout system until hover drives it).
    PlayerInfoWidget *info = w.findChild<PlayerInfoWidget *>();
    QVERIFY(info);
    FloatingPanel *panel = panelFor(&w, info);
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
        { "player_info", 40, 350, 380, 220 },
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
    // Round 8: rosters gone — assert on the player_info seed.
    PlayerInfoWidget *info = w.findChild<PlayerInfoWidget *>();
    QVERIFY(info);
    FloatingPanel *panel = panelFor(&w, info);
    QVERIFY(panel);

    QTRY_VERIFY_WITH_TIMEOUT(panel->isVisible(), 3000);
    QCOMPARE(panel->x(), 40);
    QCOMPARE(panel->y(), 350);
    QCOMPARE(panel->width(), 380);
    QCOMPARE(panel->height(), 220);
}

void UiWiringTest::matchListParsing()
{
    // Phase 11: /api/match/current response (real capture) must parse into
    // filterable live matches (id, status, division, both teams).
    QFile f(QStringLiteral("matchlist_fixture.json"));
    QVERIFY2(f.open(QIODevice::ReadOnly), "matchlist_fixture.json not found");
    const QVector<FFBLiveMatch> matches = parseMatchList(f.readAll());
    QVERIFY(matches.size() >= 10);

    // First match of the fixture: Orc (Wotan83) vs Chaos Chosen (diomed),
    // second half, competitive.
    const FFBLiveMatch &m0 = matches.first();
    QCOMPARE(m0.id, 1954643);
    QCOMPARE(m0.half, 2);
    QCOMPARE(m0.division, QStringLiteral("Competitive"));
    QCOMPARE(m0.home.coach, QStringLiteral("Wotan83"));
    QCOMPARE(m0.home.race, QStringLiteral("Orc"));
    QCOMPARE(m0.away.coach, QStringLiteral("diomed"));
    QCOMPARE(m0.away.race, QStringLiteral("Chaos Chosen"));
    QCOMPARE(m0.home.score, 0);
    QCOMPARE(m0.away.score, 1);
    QVERIFY(m0.statusText().startsWith(QStringLiteral("2H")));

    // Every match has a game id and both teams named.
    for (const FFBLiveMatch &m : matches) {
        QVERIFY(m.id > 0);
        QVERIFY(!m.home.name.isEmpty() && !m.home.coach.isEmpty());
        QVERIFY(!m.away.name.isEmpty() && !m.away.coach.isEmpty());
    }
    // Not-started matches report half == 0 and a "—" status.
    bool sawNotStarted = false;
    for (const FFBLiveMatch &m : matches) {
        if (m.half == 0) {
            sawNotStarted = true;
            QCOMPARE(m.statusText(), QStringLiteral("—"));
        }
    }
    QVERIFY(sawNotStarted);
}

// Deterministic face comparison (QPixmap/QImage have no usable operator==
// on Qt 6.12): PNG-encode and compare bytes.
QByteArray faceBytes(const QString &name, const QString &race = QString())
{
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    assets::playerFace(name, race).save(&buf, "PNG");
    return out;
}

// FUMBBL payloads often carry a BARE piece/role name ("Blitzer", "Zombie",
// "Wight", "Treeman") instead of the official piece name. Before the fix
// these all collapsed to the generic face — the "Elf vs Orc, both #2s look
// the same" bug. They must now resolve via the authoritative BB2025 race
// roster (dadidimerda.it) for the team's race.
void UiWiringTest::faceBareNameLookup()
{
    // Bare role name -> that race's piece for that role (roster order: the
    // standard piece comes first).
    QCOMPARE(faceBytes(QStringLiteral("Blitzer"), QStringLiteral("Skaven")),
             faceBytes(QStringLiteral("Skaven Blitzer")));
    QCOMPARE(faceBytes(QStringLiteral("Blitzer"), QStringLiteral("Orc")),
             faceBytes(QStringLiteral("Orc Blitzer")));
    QCOMPARE(faceBytes(QStringLiteral("Blitzer"), QStringLiteral("High Elf")),
             faceBytes(QStringLiteral("White Lion"))); // first Blitzer of the race
    QCOMPARE(faceBytes(QStringLiteral("Blocker"), QStringLiteral("Chaos Chosen")),
             faceBytes(QStringLiteral("Chaos Chosen")));
    QCOMPARE(faceBytes(QStringLiteral("Thrower"), QStringLiteral("Norse")),
             faceBytes(QStringLiteral("Valkyrie")));
    QCOMPARE(faceBytes(QStringLiteral("Lineman"), QStringLiteral("Orc")),
             faceBytes(QStringLiteral("Orc Lineman")));
    // Bare creature name -> the piece carrying it in that race.
    QCOMPARE(faceBytes(QStringLiteral("Zombie"), QStringLiteral("Necromantic Horror")),
             faceBytes(QStringLiteral("Zombie Lineman")));
    QCOMPARE(faceBytes(QStringLiteral("Treeman"), QStringLiteral("Gnome")),
             faceBytes(QStringLiteral("Altern Forest Treeman")));
    QCOMPARE(faceBytes(QStringLiteral("Rat Ogre"), QStringLiteral("Skaven")),
             faceBytes(QStringLiteral("Rat Ogre")));
    // Prefix fallback (no exact keyword for "Wight").
    QCOMPARE(faceBytes(QStringLiteral("Wight"), QStringLiteral("Shambling Undead")),
             faceBytes(QStringLiteral("Wight Blitzer")));
    // Shared big guy: exact name, no race needed.
    QCOMPARE(faceBytes(QStringLiteral("Troll"), QStringLiteral("Orc")),
             faceBytes(QStringLiteral("Troll")));
    // Official names (the user's own examples) keep resolving directly.
    QCOMPARE(faceBytes(QStringLiteral("Dragon Prince"), QStringLiteral("High Elf")),
             faceBytes(QStringLiteral("Dragon Prince")));
    QCOMPARE(faceBytes(QStringLiteral("Witch Elf"), QStringLiteral("Dark Elf")),
             faceBytes(QStringLiteral("Witch Elf")));
    QCOMPARE(faceBytes(QStringLiteral("White Lion"), QStringLiteral("High Elf")),
             faceBytes(QStringLiteral("White Lion")));
    // Custom-named star piece + known race -> the race's representative face
    // (NOT the generic one).
    QCOMPARE(faceBytes(QStringLiteral("Ghorgash Ugluk"), QStringLiteral("Orc")),
             faceBytes(QStringLiteral("Orc Lineman")));
    // Custom star + unknown race -> generic.
    QCOMPARE(faceBytes(QStringLiteral("Ghorgash Ugluk"), QStringLiteral("Not A Race")),
             faceBytes(QStringLiteral("Totally Unknown Piece")));
    // No name at all -> race representative.
    QCOMPARE(faceBytes(QString(), QStringLiteral("Skaven")),
             faceBytes(QStringLiteral("Skaven Clanrat")));
}

// The action log must show the player's NAME, not the raw numeric id from
// the report payload (user reported "18234934 dodged…" instead of the name).
void UiWiringTest::diceLogShowsPlayerName()
{
    MainWindow w;
    w.show();
    ActionLogWidget *dice = w.findChild<ActionLogWidget *>();
    QVERIFY(dice);
    w.onGameState(fixtureGame());

    // NOTE: rosterHome() returns a COPY — keep the id/name by value.
    const QVector<FFBPlayer> home = w.model()->rosterHome();
    const FFBPlayer *p = nullptr;
    for (const FFBPlayer &cand : home)
        if (!cand.playerId.isEmpty()) {
            p = &cand;
            break;
        }
    QVERIFY(p);
    const QString pid = p->playerId;
    const QString name = p->name;

    QJsonObject dodge;
    dodge.insert(QStringLiteral("reportId"), QStringLiteral("dodgeRoll"));
    dodge.insert(QStringLiteral("playerId"), pid);
    dodge.insert(QStringLiteral("roll"), QJsonValue(3));
    dodge.insert(QStringLiteral("minimumRoll"), QJsonValue(4));
    dodge.insert(QStringLiteral("successful"), QJsonValue(true));
    QJsonArray reports;
    reports.append(dodge);
    w.onModelSync(QJsonArray(), reports);

    const QString text = dice->toPlainText();
    QVERIFY2(text.contains(name), qPrintable(text));
    QVERIFY2(!text.contains(pid), qPrintable(text)); // no raw id
}

// "Spectate Game" on a different match must fully replace the previous one —
// the user reported staying on the old game after picking a new one.
void UiWiringTest::newGameStateReplacesOldMatch()
{
    MainWindow w;
    w.show();

    // Match A.
    w.onGameState(fixtureGame()); // Orcs United vs Dwarves FC
    QCOMPARE(w.model()->teamHomeName(), QStringLiteral("Orcs United"));

    // The window clears the match state when a new game is requested
    // (clearMatchUi() in the Spectate Game flow).
    w.model()->reset();
    QVERIFY(!w.model()->isInitialized());
    QVERIFY(w.model()->rosterHome().isEmpty());

    // Match B arrives: the UI shows B, not leftovers of A.
    QFile f(QStringLiteral("liveGameState_fixture.json"));
    QVERIFY2(f.open(QIODevice::ReadOnly), "liveGameState_fixture.json missing");
    const QJsonObject gameB =
        QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("game")).toObject();
    w.onGameState(gameB);
    QVERIFY(w.model()->teamHomeName().startsWith(QStringLiteral("Big Monsters")));
    HeaderView *hvB = w.findChild<HeaderView *>();
    QVERIFY(hvB && hvB->teamHomeName().startsWith(QStringLiteral("Big Monsters")));
}

// Regression: the manual "Game ID" field is pre-filled with lastGameId (the
// last spectated match) and used to override the row the user picked — so a
// double-click on a live match silently joined the stale (finished) game and
// the server never confirmed it (only ping/pong in the protocol log).
// Selecting a row must clear the manual id so the clicked match wins.
void UiWiringTest::manualGameIdDoesNotOverrideSelection()
{
    SpectateDialog dialog;
    dialog.show();

    // Simulate a stale manual id, as loadCredentials() would set from
    // the saved lastGameId.
    dialog.manualGameIdForTest()->setValue(111111);
    QCOMPARE(dialog.manualGameIdForTest()->value(), 111111);

    // Inject a live match list (no network).
    FFBLiveMatch m;
    m.id = 222222;
    m.half = 1;
    m.turn = 10;
    m.division = QStringLiteral("League");
    m.scheduler = QStringLiteral("Blackbox");
    m.home = FFBMatchTeam{QStringLiteral("Home Team"), QStringLiteral("coachA"),
                          QStringLiteral("Orcs"), QString(), 0};
    m.away = FFBMatchTeam{QStringLiteral("Away Team"), QStringLiteral("coachB"),
                          QStringLiteral("Gnomes"), QString(), 0};
    QVector<FFBLiveMatch> matches;
    matches.push_back(m);
    dialog.loadMatchesForTest(matches);
    QVERIFY2(dialog.tableRowCount() >= 1, "match list not populated");

    // The user double-clicks the listed match.
    QVERIFY2(dialog.selectGame(222222), "could not select the match row");

    // The stale manual id must be cleared and the selected match must win.
    QCOMPARE(dialog.manualGameIdForTest()->value(), 0);
    QCOMPARE(dialog.selectedGameId(), 222222);
}

// =============================================================================
// Round 9 — live report alignment + UI fixes
// =============================================================================

// Round 9 point 3: the "move"/"stand up" lines had no subject —
// ReportPlayerAction carries the subject in `actingPlayerId` (NOT `playerId`,
// which the old parser read and found empty).
void UiWiringTest::playerActionReportShowsSubject()
{
    MainWindow w;
    w.show();
    ActionLogWidget *log = w.findChild<ActionLogWidget *>();
    QVERIFY(log);
    w.onGameState(fixtureGame());
    const QVector<FFBPlayer> home = w.model()->rosterHome();
    QVERIFY(!home.isEmpty());
    const QString pid = home.first().playerId;
    const QString name = home.first().name;
    QVERIFY(!pid.isEmpty() && !name.isEmpty());

    auto actionReport = [&pid](const QString &action) {
        QJsonObject r;
        r.insert(QStringLiteral("reportId"), QStringLiteral("playerAction"));
        r.insert(QStringLiteral("actingPlayerId"), pid);
        QJsonObject act;
        act.insert(QStringLiteral("name"), action);
        r.insert(QStringLiteral("playerAction"), act);
        return r;
    };
    QJsonArray reports;
    reports.append(actionReport(QStringLiteral("move")));
    reports.append(actionReport(QStringLiteral("standUp")));
    w.onModelSync(QJsonArray(), reports);

    const QString text = log->toPlainText();
    QVERIFY2(text.contains(name), qPrintable(text));
    QVERIFY2(!text.contains(pid), qPrintable(text)); // no raw id
    QVERIFY(text.contains(QStringLiteral("Move")));
    QVERIFY(text.contains(QStringLiteral("Stand Up")));
}

// Round 9 point 6: the six block dice live in the `blockRoll` report
// (choosingTeamId + blockRoll[6] + defenderId); the `block` report carries
// ONLY defenderId (the player being blocked) and no dice. The old parser
// looked for the dice inside `block` and never found them.
void UiWiringTest::blockReportsShowDiceAndNames()
{
    MainWindow w;
    w.show();
    ActionLogWidget *log = w.findChild<ActionLogWidget *>();
    QVERIFY(log);
    w.onGameState(fixtureGame());
    const QVector<FFBPlayer> away = w.model()->rosterAway();
    QVERIFY(!away.isEmpty());
    const QString defenderId = away.first().playerId;
    const QString defenderName = away.first().name;
    const QString homeTeamId = w.model()->teamHomeId();
    const QString homeTeamName = w.model()->teamHomeName();

    QJsonObject blk;
    blk.insert(QStringLiteral("reportId"), QStringLiteral("block"));
    blk.insert(QStringLiteral("defenderId"), defenderId);
    QJsonObject blkRoll;
    blkRoll.insert(QStringLiteral("reportId"), QStringLiteral("blockRoll"));
    blkRoll.insert(QStringLiteral("choosingTeamId"), homeTeamId);
    QJsonArray faces;
    faces << 6 << 2 << 3 << 4 << 5 << 6;
    blkRoll.insert(QStringLiteral("blockRoll"), faces);
    blkRoll.insert(QStringLiteral("defenderId"), defenderId);
    QJsonArray reports;
    reports.append(blk);
    reports.append(blkRoll);
    w.onModelSync(QJsonArray(), reports);

    // Names (never raw ids)...
    const QString text = log->toPlainText();
    QVERIFY2(text.contains(defenderName), qPrintable(text));
    QVERIFY2(!text.contains(defenderId), qPrintable(text));
    QVERIFY2(text.contains(homeTeamName), qPrintable(text));
    // ...and the block dice are rendered as die-face images (HTML <img>).
    const auto labels = log->findChildren<QLabel *>();
    bool sawDice = false;
    for (QLabel *l : labels)
        if (l->text().contains(QStringLiteral(":/dice/block6_s.png"))
            && l->text().contains(QStringLiteral(":/dice/block2_s.png")))
            sawDice = true;
    QVERIFY2(sawDice, "block dice not rendered as die face images");
}

// Round 9 point 5 (user rule "MAI id numerici a schermo"): a report whose
// actor id the model cannot resolve must not leak the raw numeric id into
// the log — the line keeps its text, the id is dropped.
void UiWiringTest::unresolvedNumericIdNeverShown()
{
    MainWindow w;
    w.show();
    ActionLogWidget *log = w.findChild<ActionLogWidget *>();
    QVERIFY(log);
    w.onGameState(fixtureGame());

    QJsonObject weird;
    weird.insert(QStringLiteral("reportId"), QStringLiteral("someBrandNewReport"));
    weird.insert(QStringLiteral("playerId"), QStringLiteral("987654321"));
    QJsonArray reports;
    reports.append(weird);
    w.onModelSync(QJsonArray(), reports);

    const QString text = log->toPlainText();
    QVERIFY2(!text.contains(QStringLiteral("987654321")), qPrintable(text));
    // The report is still visible (nothing is silently lost) as a named line.
    QVERIFY(text.contains(QStringLiteral("Some Brand New Report")));
}

// Round 9 point 4: chat = mockup F — the chevron collapses the chat DELTA to
// the mockup pill; messages arriving while collapsed count as unread (badge);
// opening the chat clears the badge and keeps the stored messages.
void UiWiringTest::chatCollapsesToPillWithUnreadBadge()
{
    MainWindow w;
    w.show();
    ChatWidget *chat = w.findChild<ChatWidget *>();
    QVERIFY(chat);

    chat->setCollapsed(true);
    QCOMPARE(chat->isCollapsed(), true);
    QCOMPARE(chat->size(), ChatWidget::pillSize());

    chat->addMessage(QStringLiteral("coach"), QStringLiteral("hello"));
    QCOMPARE(chat->unreadCount(), 1);
    chat->addMessage(QStringLiteral("coach"), QStringLiteral("again"));
    QCOMPARE(chat->unreadCount(), 2);

    chat->setCollapsed(false);
    QCOMPARE(chat->isCollapsed(), false);
    QCOMPARE(chat->unreadCount(), 0);
    QCOMPARE(chat->size(), QSize(560, 250));
    // Messages received while collapsed were still stored.
    const QString text = chat->toPlainText();
    QVERIFY(text.contains(QStringLiteral("hello")));
    QVERIFY(text.contains(QStringLiteral("again")));
}

// Round 9: run the REAL parser against a LIVE capture (reports_capturer
// .jsonl of a real match — game 1956995). Ground truth for field alignment:
// every report yields >=1 line, no raw JSON leaks into the text, the block
// dice come out structured (blockFaces), and the known report ids hit their
// dedicated branches (a generic dump would signal a field-name drift).
void UiWiringTest::capturedLiveReportsParse()
{
    QFile f(QStringLiteral("live_reports_capture.jsonl"));
    QVERIFY2(f.open(QIODevice::ReadOnly), "live_reports_capture.jsonl missing");

    int reports = 0;
    int lines = 0;
    int blockDiceLines = 0;
    int d6Lines = 0;
    bool sawPlayerActionSubject = false;
    const QByteArray all = f.readAll();
    for (const QByteArray raw : all.split('\n')) {
        if (raw.trimmed().isEmpty())
            continue;
        const QJsonObject report = QJsonDocument::fromJson(raw).object();
        if (report.isEmpty())
            continue;
        const QString id = report.value(QStringLiteral("reportId")).toString();
        const QVector<DiceLogLine> parsed = parseReport(report);
        QVERIFY2(!parsed.isEmpty(), qPrintable(QStringLiteral("report %1 yielded no lines").arg(id)));
        ++reports;
        for (const DiceLogLine &l : parsed) {
            ++lines;
            // No raw JSON / payload may leak into the visible text.
            QVERIFY(!l.text.contains(QLatin1String("reportId")));
            QVERIFY(!l.text.contains(QLatin1Char('{')));
            if (id == QLatin1String("blockRoll"))
                ++blockDiceLines;
            if (l.d6Roll > 0 || !l.d6Rolls.isEmpty())
                ++d6Lines;
            if (id == QLatin1String("playerAction") && !l.actorId.isEmpty())
                sawPlayerActionSubject = true;
        }
        // The block dice must be structured faces (1..6), never in the text.
        if (id == QLatin1String("blockRoll")) {
            const DiceLogLine &l = parsed.last();
            QVERIFY(!l.blockFaces.isEmpty());
            for (const QString &face : l.blockFaces)
                QVERIFY(face.toInt() >= 1 && face.toInt() <= 6);
        }
    }
    QVERIFY(reports >= 20); // the capture has dozens of reports
    QVERIFY(lines >= reports); // nothing silently lost
    QVERIFY2(blockDiceLines > 0, "no block-roll line carried dice");
    QVERIFY2(d6Lines > 0, "no d6 line at all");
    QVERIFY2(sawPlayerActionSubject, "playerAction lost its subject (actingPlayerId?)");
}

// Round 9 point 2: the action log chevron points where the click takes the
// log — compact shows ▴ ("click to expand"), expanded shows ▾ ("click to
// collapse"). The round-8 mapping was inverted.
void UiWiringTest::actionLogChevronPointsToNextState()
{
    MainWindow w;
    w.show();
    ActionLogWidget *log = w.findChild<ActionLogWidget *>();
    QVERIFY(log);
    const auto chevron = [log]() -> QLabel * {
        for (QLabel *l : log->findChildren<QLabel *>())
            if (l->text() == QString::fromUtf8("\u25b4")
                || l->text() == QString::fromUtf8("\u25be"))
                return l;
        return nullptr;
    };
    const auto clickChevron = [log, chevron]() {
        QLabel *c = chevron();
        QVERIFY(c);
        const QPoint at = c->mapTo(log, QPoint(c->width() / 2, c->height() / 2));
        QMouseEvent press(QMouseEvent::MouseButtonPress, at, Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(log, &press);
    };

    // Compact: ▴.
    QLabel *c = chevron();
    QVERIFY(c);
    QCOMPARE(c->text(), QString::fromUtf8("\u25b4"));
    QCOMPARE(log->height(), ActionLogWidget::compactHeight());

    // Click -> expanded, chevron becomes ▾.
    clickChevron();
    QCOMPARE(log->height(), ActionLogWidget::expandedHeight());
    c = chevron();
    QCOMPARE(c->text(), QString::fromUtf8("\u25be"));

    // Click -> compact again, chevron back to ▴.
    clickChevron();
    QCOMPARE(log->height(), ActionLogWidget::compactHeight());
    c = chevron();
    QCOMPARE(c->text(), QString::fromUtf8("\u25b4"));
}

// Round 9 point 7: selection != action wheel. The click selection must NOT
// open the wheel; only an explicit target does (play mode: right-click on a
// player sets it + emits; spectator mode: right-click stays a view reset).
void UiWiringTest::selectionDoesNotOpenActionWheel()
{
    MainWindow w;
    w.show();
    FieldView *field = w.findChild<FieldView *>();
    QVERIFY(field);
    w.onGameState(fixtureGame());

    // A player with on-field coordinates.
    const QVector<FFBPlayer> home = w.model()->rosterHome();
    const FFBPlayer *p = nullptr;
    for (const FFBPlayer &cand : home)
        if (cand.hasCoords && GameModel::isOnField(cand.fieldX, cand.fieldY)) {
            p = &cand;
            break;
        }
    QVERIFY(p);
    const QString pid = p->playerId;

    // Selection: glow only, no wheel.
    field->setSelectedPlayer(pid);
    QCOMPARE(field->selectedPlayer(), pid);
    QCOMPARE(field->actionWheelTarget(), QString());

    // Spectator (default): a right CLICK on the player does NOT open the
    // wheel and — round 13 — no longer resets the view either (reset moved
    // to the MIDDLE click; the right button is the drag-pan again).
    field->setPlayMode(false);
    const QPoint at = cellCenter(field, p->fieldX, p->fieldY);
    {
        QMouseEvent press(QMouseEvent::MouseButtonPress, at, Qt::RightButton,
                          Qt::RightButton, Qt::NoModifier);
        QApplication::sendEvent(field, &press);
        QMouseEvent release(QMouseEvent::MouseButtonRelease, at, Qt::RightButton,
                            Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(field, &release);
    }
    QCOMPARE(field->actionWheelTarget(), QString());

    // Play mode: right-click on the player = explicit wheel target + signal.
    QSignalSpy spy(field, &FieldView::actionWheelRequested);
    field->setPlayMode(true);
    {
        QMouseEvent press(QMouseEvent::MouseButtonPress, at, Qt::RightButton,
                          Qt::RightButton, Qt::NoModifier);
        QApplication::sendEvent(field, &press);
        QMouseEvent release(QMouseEvent::MouseButtonRelease, at, Qt::RightButton,
                            Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(field, &release);
    }
    QCOMPARE(spy.count(), 1);
    if (spy.count() == 1)
        QCOMPARE(spy.first().at(0).toString(), pid);
    QCOMPARE(field->actionWheelTarget(), pid);
}

// Round 9 point 10: with a narrow card the skill chips wrap to several rows
// and the widget must be as tall as the wrapped content (the old Fixed
// vertical policy made the box layout use the one-row sizeHint and clipped
// every extra row in half).
void UiWiringTest::skillChipsWrapWithoutClipping()
{
    PlayerInfoWidget card;
    PlayerInfoData d;
    d.playerId = QStringLiteral("1");
    d.name = QStringLiteral("Test");
    d.number = 1;
    for (int i = 0; i < 8; ++i) {
        PlayerInfoData::Skill s;
        s.name = QStringLiteral("Skill Number %1").arg(i);
        d.skills.append(s);
    }
    card.showPlayer(d);
    card.resize(180, 600); // narrow card: the chips must wrap
    card.show();
    QApplication::processEvents();

    SkillChipsWidget *chips = card.findChild<SkillChipsWidget *>();
    QVERIFY(chips);
    const int wrapped = chips->heightForWidth(chips->width());
    QVERIFY2(wrapped >= 2 * 22, // 8 long chips at 180px = several rows
             qPrintable(QStringLiteral("expected a multi-row wrap (hfw=%1 w=%2)")
                            .arg(wrapped)
                            .arg(chips->width())));
    QCOMPARE(chips->height(), wrapped); // no clipping
}

// ---------------------------------------------------------------------------
// Round 10 tests
// ---------------------------------------------------------------------------

// Round 10 point 3: the chat starts COLLAPSED (the mockup F pill in the
// bottom-right corner) — it opens on click.
void UiWiringTest::chatStartsCollapsedByDefault()
{
    MainWindow w;
    w.show();
    ChatWidget *chat = w.findChild<ChatWidget *>();
    QVERIFY(chat);
    QVERIFY2(chat->isCollapsed(), "the chat must start collapsed (mockup F pill)");
    QCOMPARE(chat->size(), ChatWidget::pillSize());
}

// Round 10 point 4: the action log EXPANDS UPWARD — pinned to the bottom of
// the stage. Expanding grows the height and lifts the top edge; the bottom
// edge stays put (round 9 grew it downwards, off the visible bottom).
void UiWiringTest::actionLogExpandsUpward()
{
    MainWindow w;
    // The constructor shows maximized; the offscreen platform's maximized
    // size is flaky, so pin a deterministic normal size for geometry checks.
    w.showNormal();
    w.resize(1200, 750);
    QApplication::processEvents();
    ActionLogWidget *log = w.findChild<ActionLogWidget *>();
    QVERIFY(log);

    const int compactH = ActionLogWidget::compactHeight();
    const int expandedH = ActionLogWidget::expandedHeight();
    QVERIFY(expandedH > compactH);
    QCOMPARE(log->height(), compactH);
    // Round 10 regression: expanding must grow the log UPWARD (top edge
    // lifts) while the bottom edge stays pinned — the round-9 bug let it
    // grow downwards, off the visible bottom. We assert that relationship
    // directly (±1px: the offscreen window can settle by a pixel).
    const int yCompact = log->geometry().y();
    const int bottomBefore = log->geometry().bottom();

    // Click the chevron -> expand.
    QLabel *chevron = nullptr;
    for (QLabel *l : log->findChildren<QLabel *>())
        if (l->text() == QString::fromUtf8("\u25b4"))
            chevron = l;
    QVERIFY(chevron);
    {
        const QPoint at = chevron->mapTo(log, QPoint(chevron->width() / 2, chevron->height() / 2));
        QMouseEvent press(QMouseEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QApplication::sendEvent(log, &press);
    }
    QCOMPARE(log->height(), expandedH);
    QVERIFY2(log->geometry().y() < yCompact, "the log must expand UPWARD (top edge lifts)");
    QVERIFY2(qAbs(log->geometry().bottom() - bottomBefore) <= 1,
             qPrintable(QStringLiteral("bottom edge must stay pinned (got %1, was %2)")
                            .arg(log->geometry().bottom())
                            .arg(bottomBefore)));

    // Collapse again -> back to the compact slot.
    for (QLabel *l : log->findChildren<QLabel *>())
        if (l->text() == QString::fromUtf8("\u25be"))
            chevron = l;
    QVERIFY(chevron);
    {
        const QPoint at = chevron->mapTo(log, QPoint(chevron->width() / 2, chevron->height() / 2));
        QMouseEvent press(QMouseEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QApplication::sendEvent(log, &press);
    }
    QCOMPARE(log->height(), compactH);
    QVERIFY2(qAbs(log->geometry().y() - yCompact) <= 1, "back to the compact slot");
}

// Round 12: NO local mouse input moves the rim glow — it is the REMOTE
// human's selection (model-driven). Hover and click only drive the info
// card; only an actingPlayerSetPlayerId model change moves the glow.
void UiWiringTest::hoverAndClickDoNotMoveGlow()
{
    MainWindow w;
    w.show();
    FieldView *field = w.findChild<FieldView *>();
    QVERIFY(field);
    w.onGameState(fixtureGame());
    field->setFixedSize(780, 450);
    PlayerInfoWidget *info = w.findChild<PlayerInfoWidget *>();
    QVERIFY(info);

    // The remote acting player (fixture): the glow starts on it.
    const QString acting = QStringLiteral("18109690");
    QCOMPARE(field->selectedPlayer(), acting);

    // Two other on-field players: A and B.
    const FFBPlayer *a = nullptr;
    const FFBPlayer *b = nullptr;
    for (const FieldToken &t : w.model()->fieldTokens()) {
        if (t.playerId == acting)
            continue;
        const FFBPlayer *cand = w.model()->findPlayerById(t.playerId);
        if (!cand || !cand->hasCoords || !GameModel::isOnField(cand->fieldX, cand->fieldY))
            continue;
        if (!a)
            a = cand;
        else if (cand->playerId != a->playerId)
            b = cand;
    }
    QVERIFY(a && b);

    // Hover B (real field mouse path): the card follows the hover, the GLOW
    // stays on the remote acting player.
    {
        const QPoint at = cellCenter(field, b->fieldX, b->fieldY);
        QMouseEvent move(QMouseEvent::MouseMove, at, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(field, &move);
        QApplication::processEvents();
    }
    QCOMPARE(info->shownPlayerId(), b->playerId); // card follows hover
    QCOMPARE(field->selectedPlayer(), acting); // glow unchanged

    // CLICK B (real mouse path): the card stays on B, the GLOW is STILL
    // untouched — the spectator does not select players.
    QTest::mouseClick(field, Qt::LeftButton, {}, cellCenter(field, b->fieldX, b->fieldY));
    QCOMPARE(info->shownPlayerId(), b->playerId);
    QCOMPARE(field->selectedPlayer(), acting);

    // The remote coach now selects B: ONLY a model change moves the glow.
    QJsonArray changes;
    QJsonObject c;
    c.insert(QStringLiteral("modelChangeId"), QStringLiteral("actingPlayerSetPlayerId"));
    c.insert(QStringLiteral("modelChangeValue"), b->playerId);
    changes.append(c);
    w.onModelSync(changes, QJsonArray());
    QCOMPARE(field->selectedPlayer(), b->playerId);
}

// Round 10 (player_info.png): the hover card is CHROMELESS — no "Player
// Info" title text, no close button; the content fills the whole panel.
// The protocol panel keeps its full chrome.
void UiWiringTest::playerInfoCardIsChromeless()
{
    MainWindow w;
    w.show();
    PlayerInfoWidget *info = w.findChild<PlayerInfoWidget *>();
    QVERIFY(info);
    FloatingPanel *panel = panelFor(&w, info);
    QVERIFY2(panel, "the player info card is a FloatingPanel");
    QVERIFY2(panel->isChromeless(), "the hover card must be chromeless (no title text)");
    QToolButton *close = panel->findChild<QToolButton *>();
    QVERIFY2(close && close->isHidden(), "chromeless card: the x button is hidden");

    QPlainTextEdit *proto = w.findChild<QPlainTextEdit *>();
    QVERIFY(proto);
    FloatingPanel *protoPanel = panelFor(&w, proto);
    QVERIFY(protoPanel);
    QVERIFY(!protoPanel->isChromeless());
    QToolButton *protoClose = protoPanel->findChild<QToolButton *>();
    QVERIFY(protoClose && !protoClose->isHidden());
}

// Round 10 point 2: the game state often carries logoUrl = null, but the
// match list (fetched by the Spectate dialog) always has the team logos —
// MainWindow must fall back to them, resolved against the CDN base.
void UiWiringTest::matchListLogoFallbackUsedWhenGameStateNull()
{
    MainWindow w;
    w.show();
    QPlainTextEdit *proto = w.findChild<QPlainTextEdit *>();
    QVERIFY(proto);

    // The fixture game state has NO logoUrl — the match-list logos (as the
    // Spectate dialog would supply them) become the source.
    w.setFallbackTeamLogos(QStringLiteral("/i/785766"), QStringLiteral("/i/769338"));
    w.onGameState(fixtureGame());
    const QString text = proto->toPlainText();
    QVERIFY2(text.contains(QStringLiteral("logo request (home): https://cdn.fumbbl.com/i/785766")),
             qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("logo request (away): https://cdn.fumbbl.com/i/769338")),
             qPrintable(text));
    QVERIFY(!text.contains(QStringLiteral("no logo source")));
}

// Round 10 point 7: player state marks on the field — prone = "/", stunned
// = "X", drawn over the face (white strokes). Pixel-level check: the marked
// tokens carry significantly more near-white pixels in the token center
// than a standing token.
void UiWiringTest::proneAndStunnedMarksRendered()
{
    const auto renderState = [](int state) -> QImage {
        FieldView field;
        field.resize(1440, 600); // round 12: 36x15 scene of 40px cells
        FieldView::PlayerToken t;
        t.x = 13;
        t.y = 7;
        t.state = state;
        t.number = 7;
        t.playerId = QStringLiteral("mark");
        field.setPlayers({ t }, {});
        field.show();
        QApplication::processEvents();
        return field.grab().toImage();
    };
    const auto nearWhite = [](const QImage &img, const QRect &r) {
        int n = 0;
        for (int y = r.top(); y < r.bottom(); ++y)
            for (int x = r.left(); x < r.right(); ++x) {
                const QRgb c = img.pixel(x, y);
                if (qRed(c) > 230 && qGreen(c) > 230 && qBlue(c) > 230)
                    ++n;
            }
        return n;
    };

    // Cell (13,7) center = ((5+13.5)*40, (7.5)*40) = (740, 300) (round 12:
// +5 dugout columns).
    const QRect patch(740 - 12, 300 - 12, 24, 24);
    const QImage clean = renderState(1);  // standing
    const QImage prone = renderState(3);  // prone  -> "/"
    const QImage stunned = renderState(4); // stunned -> "X"

    const int wClean = nearWhite(clean, patch);
    const int wProne = nearWhite(prone, patch);
    const int wStunned = nearWhite(stunned, patch);
    QVERIFY2(wProne > wClean + 15,
             qPrintable(QStringLiteral("prone '/' mark not visible (clean=%1 prone=%2)")
                            .arg(wClean)
                            .arg(wProne)));
    QVERIFY2(wStunned > wClean + 15,
             qPrintable(QStringLiteral("stunned 'X' mark not visible (clean=%1 stunned=%2)")
                            .arg(wClean)
                            .arg(wStunned)));
    QVERIFY2(wStunned >= wProne, "an 'X' should carry at least a '/' worth of ink");
}

// ---------------------------------------------------------------------------
// Round 11 tests
// ---------------------------------------------------------------------------

// Round 11: the field is missing the WIDE ZONE boundary lines. Ground truth
// (ffb-common FieldCoordinateBounds): upper wide zone = y 0..3, lower =
// y 11..14, centre field = y 4..10 — so emphasized lines run at y=4 and
// y=11, full pitch length. Pixel check: those rows carry a significantly
// brighter line than a plain grid row (the texture stripe noise cancels
// because we compare the line against its own row).
void UiWiringTest::wideZoneLinesDrawn()
{
    FieldView field;
    field.resize(1520, 600); // round 13: 38x15 scene of 40px cells
    field.show();
    QApplication::processEvents();
    const QImage img = field.grab().toImage();
    const auto lum = [&](int x, int y) {
        const QRgb c = img.pixel(x, y);
        return qRed(c) + qGreen(c) + qBlue(c);
    };
    // Brightness the line adds over the 8px-below pixel (same grass stripe).
    const auto lineBoost = [&](int row) {
        int sum = 0, n = 0;
        // The pitch spans x 240..1280 (round 13: 5 dugout + 1 gap on each
        // side) — sample the interior only.
        for (int x = 280; x < 1240; x += 10) {
            sum += qMax(0, lum(x, row * 40) - lum(x, row * 40 + 8));
            ++n;
        }
        return sum / n;
    };
    const int grid = lineBoost(2);     // plain grid row
    const int wzTop = lineBoost(4);    // wide-zone boundary (upper)
    const int wzBottom = lineBoost(11); // wide-zone boundary (lower)
    QVERIFY2(wzTop > grid + 25,
             qPrintable(QStringLiteral("y=4 wide-zone line not emphasized (grid=%1 wz=%2)")
                            .arg(grid)
                            .arg(wzTop)));
    QVERIFY2(wzBottom > grid + 25,
             qPrintable(QStringLiteral("y=11 wide-zone line not emphasized (grid=%1 wz=%2)")
                            .arg(grid)
                            .arg(wzBottom)));
}

// Round 11: the round-8 down-player rendering stretched the face into a flat
// ellipse ("faccia schiacciata" — user report, faces-work player_face.png).
// Down players now keep the FULL round token; pixel check at 12 o'clock in
// the gold ring between the face clip (r~16.2) and the circle edge (r~18.4):
// a full circle puts team color there, a squashed ellipse puts grass.
void UiWiringTest::downedFaceNotSquashed()
{
    const auto render = [](int state) -> QImage {
        FieldView field;
        field.resize(1440, 600); // round 12: 36x15 scene of 40px cells
        FieldView::PlayerToken t;
        t.x = 13;
        t.y = 7;
        t.state = state;
        t.playerId = QStringLiteral("t");
        field.setPlayers({ t }, {});
        field.show();
        QApplication::processEvents();
        return field.grab().toImage();
    };
    const auto goldish = [](const QImage &im, int x, int y) {
        const QRgb c = im.pixel(x, y);
        return qRed(c) > 120 && qRed(c) >= qGreen(c) && qGreen(c) > qBlue(c);
    };

    const QImage standing = render(1);
    const QImage prone = render(3);
    // Token center (740, 300) (round 12: +5 dugout columns); the 12-o'clock
    // ring pixel is (740, 283).
    QVERIFY2(goldish(standing, 740, 283), "geometry sanity: standing token ring is gold");
    QVERIFY2(goldish(prone, 740, 283),
             "downed token must keep the FULL round circle (face not squashed)");
}

// Round 11: the dice in the log use pre-scaled 28px assets rendered 1:1 (the
// 256px originals were fast-scaled by the rich text layout — grainy d6, square
// pips, unreadable skulls). Verify the assets exist in the resource at 28px.
void UiWiringTest::diceAssetsPreScaled()
{
    for (int n = 1; n <= 6; ++n) {
        const QString d6 = QStringLiteral(":/dice/d6_%1_s.png").arg(n);
        const QString bl = QStringLiteral(":/dice/block%1_s.png").arg(n);
        QVERIFY2(QFileInfo(d6).exists(), qPrintable(d6));
        QVERIFY2(QFileInfo(bl).exists(), qPrintable(bl));
        // Round 13: 32px (the 44px were too big / the log too confusing).
        QCOMPARE(QImage(d6).size(), QSize(32, 32));
        QCOMPARE(QImage(bl).size(), QSize(32, 32));
    }
}

// Round 11: cards with many skills must not clip — the panel is sized to the
// content (measured at the final width so the chip wrap height counts).
void UiWiringTest::playerCardGrowsWithSkillCount()
{
    auto makeData = [](int nSkills) {
        PlayerInfoData d;
        d.playerId = QStringLiteral("1");
        d.name = QStringLiteral("Test");
        d.number = 1;
        d.position = QStringLiteral("Ogre");
        d.teamName = QStringLiteral("Team");
        d.ma = 4;
        d.st = 5;
        for (int i = 0; i < nSkills; ++i) {
            PlayerInfoData::Skill s;
            s.name = QStringLiteral("Skill Number %1").arg(i);
            d.skills.append(s);
        }
        return d;
    };
    PlayerInfoWidget card;
    card.show(); // the layout only activates on a shown widget
    card.showPlayer(makeData(2));
    card.resize(408, 100);
    QApplication::processEvents();
    const int h2 = card.sizeHint().height();
    card.showPlayer(makeData(12));
    card.resize(408, 100);
    QApplication::processEvents();
    const int h12 = card.sizeHint().height();
    QVERIFY2(h12 > h2 + 30,
             qPrintable(QStringLiteral("card must grow with the skills (2=%1 12=%2)")
                            .arg(h2)
                            .arg(h12)));

    // MainWindow level: the panel must be at least as tall as the card's
    // content (the round-11 fix sizes it that way; the old fixed height
    // clipped the bottom). The offscreen maximized window is flaky (it can
    // settle smaller after the first pinning, and clampPanelsToStage would
    // then squeeze the card) — pin a deterministic size first.
    MainWindow w;
    w.showNormal();
    w.resize(1200, 750);
    w.resize(1200, 749);
    QApplication::processEvents();
    w.resize(1200, 750);
    QApplication::processEvents();
    w.onGameState(fixtureGame());
    PlayerInfoWidget *info = w.findChild<PlayerInfoWidget *>();
    const QVector<FFBPlayer> home = w.model()->rosterHome();
    const FFBPlayer *p = nullptr;
    for (const FFBPlayer &cand : home)
        if (cand.hasCoords && GameModel::isOnField(cand.fieldX, cand.fieldY)) {
            p = &cand;
            break;
        }
    QVERIFY(p);
    w.showPlayerCardById(p->playerId);
    QApplication::processEvents();
    FloatingPanel *panel = panelFor(&w, info);
    QVERIFY2(panel, "the card is a FloatingPanel");
    QVERIFY2(panel->height() - 12 >= info->sizeHint().height() - 2,
             qPrintable(QStringLiteral("panel (%1px) must fit the card content (%2px)")
                            .arg(panel->height())
                            .arg(info->sizeHint().height())));
}

// Round 12: the rain simulation — deterministic drops fall, landings become
// splash ripples, and drops landing in the puddle make PUDDLE splashes
// (which live longer than the grass ones).
void UiWiringTest::rainSimulationDropsAndSplashes()
{
    RainSim sim;
    sim.seed(42);
    // A big puddle (most of the field) so puddle landings are frequent and
    // the test is fast and deterministic (seeded sim).
    const QVector<QRectF> puddles = { QRectF(100, 100, 600, 300) };
    sim.configure(800, 500, puddles);
    QVERIFY(sim.active());
    QVERIFY(sim.drops().size() > 20);

    // ~3 seconds at 30fps: every pre-populated drop lands at least once.
    for (int i = 0; i < 91; ++i)
        sim.step(33.0f);
    QVERIFY2(sim.landingsOnGrass() > 0, "drops landed on the grass");
    QVERIFY2(sim.landingsOnPuddle() > 0, "drops landed in the puddle");
    QVERIFY(!sim.splashes().isEmpty());

    // All splashes are inside the field; puddle splashes are inside the
    // puddle.
    for (const RainSim::Splash &s : sim.splashes()) {
        QVERIFY2(s.x >= 0 && s.x <= 800 && s.y >= 0 && s.y <= 500,
                 "splash inside the field");
        if (s.onPuddle)
            QVERIFY(puddles[0].contains(QPointF(s.x, s.y)));
    }

    // Puddle splashes outlive grass splashes (650ms vs 380ms): step until a
    // splash outlives the grass lifetime — only a puddle splash can be that
    // old (seeded sim, so this is deterministic and fast).
    bool sawOldPuddleSplash = false;
    for (int i = 0; i < 400 && !sawOldPuddleSplash; ++i) {
        sim.step(33.0f);
        for (const RainSim::Splash &s : sim.splashes())
            if (s.ageMs > 380.0f) {
                QVERIFY2(s.onPuddle, "a grass splash could not be this old");
                sawOldPuddleSplash = true;
                break;
            }
    }
    QVERIFY2(sawOldPuddleSplash, "a puddle ripple outlived the grass lifetime");
}

// Round 12: the field starts/stops the rain animation with the weather.
void UiWiringTest::fieldRainAnimationActive()
{
    FieldView field;
    field.setFixedSize(800, 500);
    field.show();
    QApplication::processEvents();

    QVERIFY(!field.isRaining());
    field.setWeather(QStringLiteral("Pouring Rain"));
    QVERIFY(field.isRaining());
    field.setWeather(QStringLiteral("Nice Weather"));
    QVERIFY(!field.isRaining());
    field.setWeather(QStringLiteral("Light Rain"));
    QVERIFY(field.isRaining());
}

QTEST_MAIN(UiWiringTest)
#include "ui_wiring_test.moc"
