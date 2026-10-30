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
        // ReportThrowIn wire fields: direction (enum) + directionRoll (int)
        // + distanceRoll (int[]). (The old fixture used a non-existent
        // "throwInRoll" key, so the line rendered with no dice.)
        QJsonObject r;
        r.insert("reportId", "throwIn");
        r.insert("defenderId", awayPid);
        r.insert("direction", QJsonObject{{"name", "right"}});
        r.insert("directionRoll", 3);
        QJsonArray tr;
        tr << 2 << 4 << 6;
        r.insert("distanceRoll", tr);
        reports.append(r);
    }
    w.onModelSync(QJsonArray(), reports);
    QApplication::processEvents();

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

    // --- New stadium geometry (round 13): the scene is the STADIUM (cover);
    //     the field is fitted into the manifest pit; the dugouts are the top
    //     band. Crop using the widget-space geometry (zoom 1 / pan 0).
    {
        const QRect fr = field->fieldRect().toAlignedRect();
        saveCrop(*field, fr, outDir + "/round13_field.png");
        const QRect hr = field->dugoutRect(true).toAlignedRect();
        const QRect ar = field->dugoutRect(false).toAlignedRect();
        const int topX = qMax(0, qMin(hr.left(), ar.left()));
        const int topY = qMax(0, qMin(hr.top(), ar.top()));
        const int topW = qMin(field->width() - topX,
                              qMax(hr.right(), ar.right()) - topX + 8);
        const int topH = qMin(field->height() - topY,
                              qMax(hr.bottom(), ar.bottom()) - topY + 10);
        saveCrop(*field, QRect(topX, topY, topW, topH), outDir + "/round13_dugouts.png");
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
        const qreal cell = field->fieldRect().width() / 26.0;
        const QPoint a = field->cellCenterPx(trailX, trailY);
        const QPoint b = field->cellCenterPx(trailX - 2, trailY - 2);
        QRect r(qMin(a.x(), b.x()), qMin(a.y(), b.y()),
                qAbs(a.x() - b.x()), qAbs(a.y() - b.y()));
        const int pad = int(cell * 3);
        r.adjust(-pad, -pad, pad, pad);
        saveCrop(*field, r.intersected(field->rect()), outDir + "/round13_trail.png");
    }

    // --- Rain: switch to pouring rain, let drops + dynamic puddles build up -------
    // (round 13: top-down rain over the WHOLE field, puddles appear over time).
    {
        QJsonArray ch;
        change(ch, QStringLiteral("fieldModelSetWeather"), QString(),
               QJsonValue(QStringLiteral("Pouring Rain")));
        w.onModelSync(ch, QJsonArray());
        for (int i = 0; i < 20; ++i) { // ~4s of rain
            QThread::msleep(200);
            QApplication::processEvents();
        }
        saveCrop(*field, field->fieldRect().toAlignedRect(), outDir + "/round13_rain.png");
        qInfo().noquote() << "saved" << outDir << "/round13_rain.png";
    }

    qInfo() << "round13 preview complete ->" << outDir;
    return 0;
}
