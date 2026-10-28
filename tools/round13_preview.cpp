// =============================================================================
// round13_preview — offscreen visual verification of the round-13 changes
//
//   round13_window.png     whole HUD: [dugout | gap | pitch | gap | dugout],
//                          grass-only texture + CODE-drawn grid/TD/TOUCHDOWN,
//                          full-height dugouts, move trail, spaced 44px dice
//   round13_field.png      the pitch: grass texture + vector grid, 1-column
//                          crimson TD zones, vertical TOUCHDOWN, center line
//   round13_dugouts.png    the two full-height dugout panels + the visible
//                          gap (apron) between each panel and the TD column
//   round13_actionlog.png  action log: block + d6 dice at 44px with spacing
//   round13_header.png     header: active-team breathing glow, equal font
//                          for both team names, more line spacing
//   round13_trail.png      the actor's cumulative move trail (grid-following
//                          polyline + arrow per move)
//
// Run: QT_QPA_PLATFORM=offscreen ./build/round13_preview [out_dir]
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
#include "ui/fieldview.h"
#include "ui/headerview.h"
#include "ui/mainwindow.h"

static void saveCrop(QWidget &w, const QRect &r, const QString &path)
{
    w.grab(r).save(path);
    qInfo().noquote() << "saved" << path;
}

// Append one model change.
static void change(QJsonArray &out, const QString &id, const QString &key,
                   const QJsonValue &value)
{
    QJsonObject c;
    c.insert(QStringLiteral("modelChangeId"), id);
    c.insert(QStringLiteral("modelChangeKey"), key);
    c.insert(QStringLiteral("modelChangeValue"), value);
    out.append(c);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QString outDir = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                    : QStringLiteral("/tmp/round13");
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

    // --- Reports: block dice + d6 for the log (44px, spaced) --------------------
    const QVector<FFBPlayer> home = w.model()->rosterHome();
    const QVector<FFBPlayer> away = w.model()->rosterAway();
    QString homePid, awayPid;
    for (const FFBPlayer &p : home)
        if (p.hasCoords && GameModel::isOnField(p.fieldX, p.fieldY)) {
            homePid = p.playerId;
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
        faces << 6 << 2 << 3 << 4;
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

    // --- Simulate a MOVE so the trail draws (round 13) --------------------------
    // The actor makes two moves from its REAL position, stepping ADJACENT
    // squares (like the server does): the trace must ACCUMULATE (one arrow
    // per move), following the grid square by square.
    int trailX = 5, trailY = 5; // where the actor starts (for the trail crop)
    {
        const FFBPlayer *actor = w.model()->findPlayerById(homePid);
        int sx = actor ? actor->fieldX : 5;
        int sy = actor ? actor->fieldY : 5;
        trailX = sx; trailY = sy;
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerId"), homePid, QJsonValue(homePid));
        change(ch, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("move")));
        // Move 1: two steps towards the left (towards home's TD).
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), homePid,
               QJsonArray() << (sx - 1) << sy);
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), homePid,
               QJsonArray() << (sx - 2) << sy);
        // Move 2 (a new path, its own arrow): two steps UP (toward y=0).
        // (The fixture actor stands on the bottom row, so -y stays in bounds.)
        change(ch, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("move")));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), homePid,
               QJsonArray() << (sx - 2) << (sy - 1));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), homePid,
               QJsonArray() << (sx - 2) << (sy - 2));
        w.onModelSync(ch, QJsonArray());
    }

    // Let the logos land.
    for (int i = 0; i < 10; ++i) {
        QThread::msleep(300);
        QApplication::processEvents();
    }

    // --- Whole HUD -----------------------------------------------------------------
    w.grab().save(outDir + "/round13_window.png");
    qInfo().noquote() << "saved" << outDir << "/round13_window.png";

    // Round 13 letterbox geometry (38x15 scene, square cells, centered).
    const int W = field->width(), H = field->height();
    const int cell = qMin(W / 38, H / 15);
    const int ox = (W - 38 * cell) / 2, oy = (H - 15 * cell) / 2;
    const int pitchX = ox + cell * 6; // 5 dugout + 1 gap

    // --- Field (pitch): grass + vector grid/TD/TOUCHDOWN -------------------------
    saveCrop(*field, QRect(pitchX, oy, cell * 26, cell * 15), outDir + "/round13_field.png");

    // --- Dugouts + gap: left [panel+gap], right [gap+panel] ----------------------
    {
        // Left: dugout columns 0..4 + gap column 5 (panel + the apron gap).
        const QImage left = field->grab(QRect(ox, oy, cell * 6, cell * 15)).toImage();
        // Right: gap column 32 + dugout columns 33..37.
        const QImage right = field->grab(QRect(ox + cell * 32, oy, cell * 6, cell * 15)).toImage();
        QImage sheet(cell * 12 + 20, cell * 15, QImage::Format_RGB32);
        sheet.fill(QColor(58, 56, 54));
        QPainter sp(&sheet);
        sp.drawImage(0, 0, left);
        sp.drawImage(cell * 6 + 20, 0, right);
        sp.end();
        sheet.save(outDir + "/round13_dugouts.png");
        qInfo().noquote() << "saved" << outDir << "/round13_dugouts.png";
    }

    // --- Header strip --------------------------------------------------------------
    header->grab().save(outDir + "/round13_header.png");
    qInfo().noquote() << "saved" << outDir << "/round13_header.png";

    // --- Action log: 44px spaced dice --------------------------------------------
    for (QLabel *l : log->findChildren<QLabel *>())
        if (l->text() == QString::fromUtf8("\u25b4")) {
            const QPoint at = l->mapTo(log, QPoint(l->width() / 2, l->height() / 2));
            QMouseEvent press(QMouseEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton,
                              Qt::NoModifier);
            QApplication::sendEvent(log, &press);
            break;
        }
    QApplication::processEvents();
    saveCrop(*log, log->rect(), outDir + "/round13_actionlog.png");

    // --- Move trail crop: the area the actor moved through ------------------------
    // The actor moved from (trailX,trailY) left 2 and down 2: cover that box.
    {
        const int cx = pitchX + cell * (trailX - 3), cy = oy + cell * (trailY - 3);
        saveCrop(*field, QRect(cx, cy, cell * 7, cell * 6), outDir + "/round13_trail.png");
    }

    qInfo() << "round13 preview complete ->" << outDir;
    return 0;
}
