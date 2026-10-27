// =============================================================================
// round12_preview — offscreen visual verification of the round-12 changes
//
//   round12_window.png     whole HUD: [dugout | pitch | dugout] scene, header
//                          with big bold names + GLOWING GREEN active team,
//                          spaced dice, card
//   round12_dugouts.png    the two dugout panels (soil asset, RESERVE/KO/
//                          INJURED bands, bench faces) side by side
//   round12_header.png     header strip: green emboss + glow on the active
//                          team, bigger names
//   round12_actionlog.png  action log: d6 + block dice with spacing, block
//                          dice in the d6 style (white on red)
//   round12_rain.png       field with ANIMATED rain: streaks + splash
//                          ripples, puddles on the grass
//   round12_card.png       the hover card (unchanged content, new scene)
//
// Run: QT_QPA_PLATFORM=offscreen ./build/round12_preview [out_dir]
// =============================================================================
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QThread>

#include "core/gamemodel.h"
#include "core/reportparser.h"
#include "ui/actionlog.h"
#include "ui/chatwidget.h"
#include "ui/fieldview.h"
#include "ui/headerview.h"
#include "ui/mainwindow.h"
#include "ui/panel.h"
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
                                    : QStringLiteral("/tmp/round12");
    QDir().mkpath(outDir);

    QFile f(QStringLiteral("tests/liveGameState_fixture.json"));
    if (!f.open(QIODevice::ReadOnly))
        qFatal("tests/liveGameState_fixture.json missing");
    const QJsonObject game =
        QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("game")).toObject();

    MainWindow w;
    w.showNormal();
    w.resize(1400, 860);
    w.show();
    w.setFallbackTeamLogos(QStringLiteral("/i/785766"), QStringLiteral("/i/769338"));
    w.onGameState(game);

    FieldView *field = w.findChild<FieldView *>();
    ActionLogWidget *log = w.findChild<ActionLogWidget *>();
    HeaderView *header = w.findChild<HeaderView *>();
    if (!field || !log || !header)
        qFatal("missing a widget");

    // --- Reports: block dice + d6 for the log ---------------------------------
    const QVector<FFBPlayer> home = w.model()->rosterHome();
    const QVector<FFBPlayer> away = w.model()->rosterAway();
    QString homePid, homeName, awayPid;
    for (const FFBPlayer &p : home)
        if (p.hasCoords && GameModel::isOnField(p.fieldX, p.fieldY) && p.fieldY <= 8) {
            homePid = p.playerId;
            homeName = p.name;
            break;
        }
    const QString homeTeamId = w.model()->teamHomeId();
    for (const FFBPlayer &p : away)
        if (p.hasCoords && GameModel::isOnField(p.fieldX, p.fieldY)) {
            awayPid = p.playerId;
            break;
        }
    QJsonArray reports;
    {
        QJsonObject r;
        r.insert("reportId", "dodgeRoll");
        r.insert("playerId", awayPid);
        r.insert("roll", 3);
        r.insert("minimumRoll", 4);
        r.insert("successful", false);
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
        r.insert("reportId", "injury");
        r.insert("defenderId", awayPid);
        QJsonArray ar;
        ar << 2;
        r.insert("armorRoll", ar);
        QJsonArray ir;
        ir << 5;
        r.insert("injuryRoll", ir);
        r.insert("injuryType", QJsonObject{{"name", "bruisedRibs"}});
        reports.append(r);
    }
    // A multi-die d6 roll (throw-in style: 3 dice) to show d6 spacing too.
    {
        QJsonObject r;
        r.insert("reportId", "throwIn");
        r.insert("defenderId", awayPid);
        QJsonArray tr;
        tr << 2 << 4 << 6;
        r.insert("throwInRoll", tr);
        reports.append(r);
    }
    w.onModelSync(QJsonArray(), reports);

    // --- ANIMATED RAIN (round 12) ---------------------------------------------
    {
        QJsonArray changes;
        QJsonObject c;
        c.insert(QStringLiteral("modelChangeId"), QStringLiteral("fieldModelSetWeather"));
        c.insert(QStringLiteral("modelChangeValue"),
                 QJsonObject{{"name", "Pouring Rain"}, {"shortName", "Rain"}});
        changes.append(c);
        w.onModelSync(changes, QJsonArray());
    }

    // --- Card -------------------------------------------------------------------
    w.showPlayerCardById(homePid);
    QThread::msleep(150);
    QApplication::processEvents();

    // Let the logos land + the rain accumulate splashes.
    for (int i = 0; i < 24; ++i) {
        QThread::msleep(500);
        QApplication::processEvents();
    }

    // --- Whole HUD -----------------------------------------------------------------
    w.grab().save(outDir + "/round12_window.png");
    qInfo().noquote() << "saved" << outDir << "/round12_window.png";

    // Round 12 letterbox geometry (square cells, centered scene).
    const int W = field->width(), H = field->height();
    const int cell = qMin(W / 36, H / 15);
    const int ox = (W - 36 * cell) / 2, oy = (H - 15 * cell) / 2;

    // --- Dugouts: left panel + right panel side by side -----------------------------
    {
        const QImage left = field->grab(QRect(ox, oy, cell * 5, cell * 15)).toImage();
        const QImage right = field->grab(QRect(ox + cell * 31, oy, cell * 5, cell * 15)).toImage();
        QImage sheet(cell * 10 + 20, cell * 15, QImage::Format_RGB32);
        sheet.fill(Qt::white);
        QPainter sp(&sheet);
        sp.drawImage(0, 0, left);
        sp.drawImage(cell * 5 + 20, 0, right);
        sp.end();
        sheet.save(outDir + "/round12_dugouts.png");
        qInfo().noquote() << "saved" << outDir << "/round12_dugouts.png";
    }

    // --- Header strip ----------------------------------------------------------------
    header->grab().save(outDir + "/round12_header.png");
    qInfo().noquote() << "saved" << outDir << "/round12_header.png";

    // --- Action log: spaced dice --------------------------------------------------------
    for (QLabel *l : log->findChildren<QLabel *>())
        if (l->text() == QString::fromUtf8("\u25b4")) {
            const QPoint at = l->mapTo(log, QPoint(l->width() / 2, l->height() / 2));
            QMouseEvent press(QMouseEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton,
                              Qt::NoModifier);
            QApplication::sendEvent(log, &press);
            break;
        }
    QApplication::processEvents();
    saveCrop(*log, log->rect(), outDir + "/round12_actionlog.png");

    // --- Rain crop: left half of the pitch (puddles + streaks + ripples) -----------------
    {
        // pitch starts at column 5; take columns 5..20 (puddles live there).
        saveCrop(*field, QRect(ox + cell * 5, oy, cell * 15, cell * 15),
                 outDir + "/round12_rain.png");
    }

    // --- Card ------------------------------------------------------------------------------
    {
        PlayerInfoWidget *info = w.findChild<PlayerInfoWidget *>();
        FloatingPanel *panel = nullptr;
        for (FloatingPanel *p : w.findChildren<FloatingPanel *>())
            if (p->contentWidget() == info)
                panel = p;
        if (panel) {
            panel->grab().save(outDir + "/round12_card.png");
            qInfo().noquote() << "saved" << outDir << "/round12_card.png";
        }
    }

    qInfo() << "round12 preview complete ->" << outDir;
    return 0;
}
