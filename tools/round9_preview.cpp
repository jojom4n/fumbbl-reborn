// =============================================================================
// round9_preview — offscreen visual verification of the round-9 changes
//
// Feeds a real MainWindow (offscreen) with a live game-state fixture + a
// set of round-9 reports, then saves PNGs of the pieces the user reviews:
//
//   round9_window.png      whole HUD (field + action log with dice +
//                          selection glow + chat + header)
//   round9_header.png      header strip (score/center-row gap, round 9 #8)
//   round9_actionlog.png   action log expanded (names + dice, chevron ▾)
//   round9_chat_collapsed.png   mockup pill + unread badge (round 9 #4)
//   round9_chat_expanded.png    card with header + collapse chevron
//   round9_card_skills.png  player card with 2-row skill chips (round 9 #10)
//
// Run: QT_QPA_PLATFORM=offscreen ./build/round9_preview [out_dir]
// =============================================================================
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QThread>

#include "core/gamemodel.h"
#include "core/reportparser.h"
#include "ui/actionlog.h"
#include "ui/chatwidget.h"
#include "ui/fieldview.h"
#include "ui/headerview.h"
#include "ui/mainwindow.h"
#include "ui/playerinfowidget.h"

static void saveCrop(QWidget &w, const QRect &r, const QString &path)
{
    w.grab(r).save(path);
    qInfo().noquote() << "saved" << path;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QString outDir = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                    : QStringLiteral("/tmp/round9");
    QDir().mkpath(outDir);

    // --- Fixture game state (live capture: has teamId, real players) -------
    QFile f(QStringLiteral("tests/liveGameState_fixture.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        qFatal("tests/liveGameState_fixture.json missing");
    }
    const QJsonObject game =
        QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("game")).toObject();

    MainWindow w;
    w.resize(1400, 860);
    w.show();
    w.onGameState(game);
    QApplication::processEvents();

    FieldView *field = w.findChild<FieldView *>();
    ActionLogWidget *log = w.findChild<ActionLogWidget *>();
    ChatWidget *chat = w.findChild<ChatWidget *>();
    HeaderView *header = w.findChild<HeaderView *>();
    if (!field || !log || !chat || !header)
        qFatal("missing a widget");

    // --- Players: pick on-field ids for reports -----------------------------
    QString homePid, homeName, awayPid, awayName;
    const QVector<FFBPlayer> home = w.model()->rosterHome();
    const QVector<FFBPlayer> away = w.model()->rosterAway();
    // Prefer a player visible in the upper/middle field (the bottom rows
    // are covered by the player-info panel in the screenshot).
    for (const FFBPlayer &p : home)
        if (p.hasCoords && GameModel::isOnField(p.fieldX, p.fieldY) && p.fieldY <= 8) {
            homePid = p.playerId;
            homeName = p.name;
            break;
        }
    if (homePid.isEmpty())
        for (const FFBPlayer &p : home)
            if (p.hasCoords && GameModel::isOnField(p.fieldX, p.fieldY)) {
                homePid = p.playerId;
                homeName = p.name;
                break;
            }
    for (const FFBPlayer &p : away)
        if (p.hasCoords && GameModel::isOnField(p.fieldX, p.fieldY)) {
            awayPid = p.playerId;
            awayName = p.name;
            break;
        }
    if (homePid.isEmpty() || awayPid.isEmpty())
        qFatal("fixture has no on-field players");
    const QString homeTeamId = w.model()->teamHomeId();
    const QString awayTeamId = w.model()->teamAwayId();

    // --- Round-9 reports: subjects + dice -----------------------------------
    QJsonArray reports;
    {
        QJsonObject r;
        r.insert("reportId", "playerAction");
        r.insert("actingPlayerId", homePid);
        r.insert("playerAction", QJsonObject{{"name", "move"}});
        reports.append(r);
    }
    {
        QJsonObject r;
        r.insert("reportId", "dodgeRoll");
        r.insert("playerId", awayPid);
        r.insert("roll", 3);
        r.insert("minimumRoll", 4);
        r.insert("successful", false);
        r.insert("reRolled", false);
        reports.append(r);
    }
    {
        QJsonObject r;
        r.insert("reportId", "block");
        r.insert("defenderId", awayPid);
        reports.append(r);
    }
    {
        QJsonObject r;
        r.insert("reportId", "blockRoll");
        r.insert("choosingTeamId", homeTeamId);
        QJsonArray faces;
        faces << 6 << 2 << 3 << 4 << 5 << 6;
        r.insert("blockRoll", faces);
        r.insert("defenderId", awayPid);
        reports.append(r);
    }
    {
        QJsonObject r;
        r.insert("reportId", "blockChoice");
        r.insert("choosingTeamId", homeTeamId);
        QJsonArray faces;
        faces << 6 << 2 << 3 << 4 << 5 << 6;
        r.insert("blockRoll", faces);
        r.insert("diceIndex", 0);
        r.insert("blockResult", QJsonObject{{"name", "BOTH DOWN"}});
        r.insert("defenderId", awayPid);
        reports.append(r);
    }
    {
        QJsonObject r;
        r.insert("reportId", "injury");
        r.insert("defenderId", awayPid);
        QJsonArray ar;
        ar << 2;
        r.insert("armorRoll", ar);
        r.insert("armorBroken", true);
        QJsonArray ir;
        ir << 5;
        r.insert("injuryRoll", ir);
        r.insert("injuryType", QJsonObject{{"name", "bruisedRibs"}});
        reports.append(r);
    }
    {
        QJsonObject r;
        r.insert("reportId", "throwIn");
        r.insert("direction", QJsonObject{{"name", "left"}});
        r.insert("directionRoll", 3);
        QJsonArray dist;
        dist << 2 << 4;
        r.insert("distanceRoll", dist);
        reports.append(r);
    }
    {
        QJsonObject r;
        r.insert("reportId", "foul");
        r.insert("defenderId", awayPid);
        reports.append(r);
    }
    {
        QJsonObject r;
        r.insert("reportId", "standUpRoll");
        r.insert("playerId", homePid);
        r.insert("roll", 5);
        r.insert("minimumRoll", 5);
        r.insert("successful", true);
        reports.append(r);
    }
    w.onModelSync(QJsonArray(), reports);

    // --- Selection: blue glowing radial circle (pulse) -----------------------
    field->setSelectedPlayer(homePid);
    QThread::msleep(120); // let the pulse timer run a few frames
    QApplication::processEvents();

    // --- Chat: messages + collapse to the mockup pill ------------------------
    chat->addMessage("coach_home", "nice block!", QColor(0xc8, 0x96, 0x3c));
    chat->addMessage("coach_away", "ouch.", QColor(0x4a, 0x6a, 0xb8));
    QApplication::processEvents();
    saveCrop(*chat, chat->rect(), outDir + "/round9_chat_expanded.png");
    chat->setCollapsed(true);
    chat->addMessage("coach_home", "unread one");
    QApplication::processEvents();
    saveCrop(*chat, chat->rect(), outDir + "/round9_chat_collapsed.png");
    chat->setCollapsed(false);
    QApplication::processEvents();

    // --- Action log expanded (chevron ▾, dice rows) --------------------------
    // Simulate the chevron click: find the chevron label and send a press.
    for (QLabel *l : log->findChildren<QLabel *>())
        if (l->text() == QString::fromUtf8("\u25b4")) {
            const QPoint at = l->mapTo(log, QPoint(l->width() / 2, l->height() / 2));
            QMouseEvent press(QMouseEvent::MouseButtonPress, at, Qt::LeftButton,
                              Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(log, &press);
            break;
        }
    QApplication::processEvents();
    saveCrop(*log, log->rect(), outDir + "/round9_actionlog.png");

    // --- Header (score/center-row gap) ----------------------------------------
    saveCrop(*header, header->rect(), outDir + "/round9_header.png");

    // --- Whole HUD -------------------------------------------------------------
    QThread::msleep(60);
    QApplication::processEvents();
    w.grab().save(outDir + "/round9_window.png");
    qInfo().noquote() << "saved" << outDir << "/round9_window.png";

    // --- Player card with 2-row skill chips ------------------------------------
    {
        PlayerInfoWidget card;
        PlayerInfoData d;
        d.playerId = homePid;
        d.name = homeName;
        d.number = 7;
        d.position = QStringLiteral("Orc");
        d.race = QStringLiteral("Orc");
        d.teamName = w.model()->teamHomeName();
        d.sideHome = true;
        d.ma = 4;
        d.st = 5;
        d.ag = 4;
        d.pa = 3;
        d.av = 2;
        d.stateText = QStringLiteral("Standing");
        const QStringList skillNames = {
            "Block", "Juggernaut", "Sidestep", "Nerves of Steel",
            "Loner", "Agile", "Douse", "Extra Arms"};
        for (const QString &s : skillNames) {
            PlayerInfoData::Skill sk;
            sk.name = s;
            d.skills.append(sk);
        }
        card.showPlayer(d);
        card.resize(300, 620);
        card.show();
        QApplication::processEvents();
        card.grab().save(outDir + "/round9_card_skills.png");
        qInfo().noquote() << "saved" << outDir << "/round9_card_skills.png";
    }

    qInfo() << "round9 preview complete ->" << outDir;
    return 0;
}
