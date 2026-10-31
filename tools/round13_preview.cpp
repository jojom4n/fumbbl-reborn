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
    // The actor IS the attacker (real game: block reports arrive while the
    // attacker acts) — the turn-11 marker pair needs it. The defender is
    // parked ADJACENT to the attacker (a real block: the two touch), so the
    // between-the-two marker sits on their shared edge.
    {
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerId"), homePid,
               QJsonValue(homePid));
        const FFBPlayer *att0 = w.model()->findPlayerById(homePid);
        const FFBPlayer *def0 = w.model()->findPlayerById(awayPid);
        if (att0 && att0->hasCoords && def0 && def0->hasCoords) {
            // put the defender right below the attacker (adjacent row)
            change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"),
                   awayPid, QJsonArray() << att0->fieldX << (att0->fieldY - 1));
        }
        // The fixture parks HOME #2 on the same cell as the defender — park
        // it elsewhere so the block pair reads cleanly in the crop.
        for (const FFBPlayer &p : w.model()->rosterHome())
            if (p.hasCoords && p.playerId != homePid
                && p.fieldX == att0->fieldX && p.fieldY == att0->fieldY - 1) {
                change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"),
                       p.playerId, QJsonArray() << (p.fieldX - 2) << p.fieldY);
                break;
            }
        w.onModelSync(ch, QJsonArray());
    }
    w.onModelSync(QJsonArray(), reports);
    // Let the tokens finish their glide to the new cells (the preview moved
    // the defender + home #2), then capture.
    for (int i = 0; i < 8; ++i) {
        QThread::msleep(100);
        QApplication::processEvents();
    }

    // --- Round 13 turn 4: the RED BLOCK GLOW. The blockRoll above armed the
    //     glow on the DEFENDER (awayPid); capture a crop around him BEFORE the
    //     trail simulation switches the actor (which resolves the block).
    //     Turn 11: the crop also shows the HELMET marker between the pair.
    {
        const FFBPlayer *victim = w.model()->findPlayerById(awayPid);
        if (victim && victim->hasCoords) {
            const qreal cell = field->fieldRect().width() / 26.0;
            const QPoint c = field->cellCenterPx(victim->fieldX, victim->fieldY);
            const int pad = int(cell * 3.5);
            QRect r(c.x() - pad, c.y() - pad, pad * 2, pad * 2);
            saveCrop(*field, r.intersected(field->rect()), outDir + "/round13_blockglow.png");
        }
    }

    // --- Turn 11: the block CHOICE -> the between-the-two marker switches
    //     helmet -> the chosen die's SYMBOL (transparent). Face 2 = skull+arrow.
    {
        QJsonArray rep;
        QJsonObject r;
        r.insert("reportId", "blockChoice");
        r.insert("defenderId", awayPid);
        QJsonArray fr;
        fr << 6 << 2;
        r.insert("blockRoll", fr);
        r.insert("diceIndex", 1);
        rep.append(r);
        w.onModelSync(QJsonArray(), rep);
        QApplication::processEvents();
    }

    // --- Turn 13: the green CHECK — switch the actor away and back so the
    //     previous actor gets marked as "already acted this turn", then
    //     re-arm the block glow + choice (the actor switch cleared them).
    {
        QString awaySecond;
        for (const FFBPlayer &p : w.model()->rosterAway())
            if (p.hasCoords && p.playerId != awayPid) {
                awaySecond = p.playerId;
                break;
            }
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerId"), QString(),
               QJsonValue(awaySecond));
        w.onModelSync(ch, QJsonArray());
        QJsonArray ch2;
        change(ch2, QStringLiteral("actingPlayerSetPlayerId"), QString(),
               QJsonValue(homePid));
        QJsonArray rep2;
        QJsonObject r2;
        r2.insert("reportId", "blockRoll");
        r2.insert("choosingTeamId", w.model()->teamHomeId());
        QJsonArray faces2;
        faces2 << 6 << 2;
        r2.insert("blockRoll", faces2);
        r2.insert("defenderId", awayPid);
        rep2.append(r2);
        w.onModelSync(ch2, rep2);
        QJsonArray rep3;
        QJsonObject r3;
        r3.insert("reportId", "blockChoice");
        r3.insert("defenderId", awayPid);
        QJsonArray fr3;
        fr3 << 6 << 2;
        r3.insert("blockRoll", fr3);
        r3.insert("diceIndex", 1);
        rep3.append(r3);
        w.onModelSync(QJsonArray(), rep3);
        for (int i = 0; i < 8; ++i) {
            QThread::msleep(100);
            QApplication::processEvents();
        }
    }

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

    // --- Turn 12/14 (user): per-turn token badges, numbered trail cells,
    //     the white pass-receiver glow, and the ball flight (growing ball).
    {
        QJsonArray ch;
        // Blitzed this turn -> gold lightning top-left on the actor.
        change(ch, QStringLiteral("turnDataSetBlitzUsed"), QString(), QJsonValue(true));
        // A confused player (state bit 512) -> gold "?" bottom-left.
        for (const FFBPlayer &p : away)
            if (p.hasCoords && p.playerId != awayPid) {
                change(ch, QStringLiteral("fieldModelSetPlayerState"), p.playerId,
                       QJsonValue(512));
                break;
            }
        // Numbered trail cells (server: fieldModelAddTrackNumber) — the
        // numbers land just above the trail the actor just drew.
        const auto trackAt = [&](int x, int y, int n) {
            QJsonObject o;
            o.insert(QStringLiteral("coordinate"), QJsonArray() << x << y);
            o.insert(QStringLiteral("number"), n);
            change(ch, QStringLiteral("fieldModelAddTrackNumber"), QString(), o);
        };
        trackAt(trailX - 1, trailY, 1);
        trackAt(trailX - 2, trailY, 2);
        trackAt(trailX - 2, trailY - 1, 3);
        trackAt(trailX - 2, trailY - 2, 4);
        // Pass receiver -> WHITE glow (fieldModelSetTargetSelectionState).
        for (const FFBPlayer &p : away)
            if (p.hasCoords && p.playerId != awayPid) {
                QJsonObject o;
                o.insert(QStringLiteral("playerId"), p.playerId);
                o.insert(QStringLiteral("targetSelectionStatus"),
                         QStringLiteral("SELECTED"));
                o.insert(QStringLiteral("targetSelectionStatusIsCommitted"), false);
                o.insert(QStringLiteral("usedSkills"), QJsonArray());
                change(ch, QStringLiteral("fieldModelSetTargetSelectionState"),
                       QString(), o);
                break;
            }
        w.onModelSync(ch, QJsonArray());
        QApplication::processEvents();
    }

    // Ball flight: the throw steps through the grid; the ball GROWS with
    // every step (turn 14). Grab it mid-flight (settle timer = 900ms).
    {
        // Throw LEFT (towards the home TD) — that side of the fixture is
        // empty, so the ball lands on free squares (a ball on a player's
        // square is intentionally not drawn).
        const FFBPlayer *baller = w.model()->findPlayerById(homePid);
        int bx = qBound(4, (baller ? baller->fieldX : 10) - 1, 22);
        int by = qBound(2, baller ? baller->fieldY : 7, 12);
        QJsonArray ch;
        change(ch, QStringLiteral("fieldModelSetBallCoordinate"), QString(),
               QJsonArray() << bx << by);
        w.onModelSync(ch, QJsonArray());
        QThread::msleep(150);
        QApplication::processEvents();
        for (int step = 1; step <= 3; ++step) {
            QJsonArray ch2;
            change(ch2, QStringLiteral("fieldModelSetBallCoordinate"), QString(),
                   QJsonArray() << (bx - step) << by);
            w.onModelSync(ch2, QJsonArray());
            QThread::msleep(150);
            QApplication::processEvents();
        }
        const QPoint c = field->cellCenterPx(bx - 3, by);
        const qreal cell = field->fieldRect().width() / 26.0;
        const int pad = int(cell * 4);
        saveCrop(*field, QRect(c.x() - pad, c.y() - pad, pad * 2, pad * 2)
                        .intersected(field->rect()),
                 outDir + "/round13_ballflight.png");
    }

    // Let the logos land.
    for (int i = 0; i < 10; ++i) {
        QThread::msleep(300);
        QApplication::processEvents();
    }

    // --- Whole HUD -----------------------------------------------------------------
    fprintf(stderr, "DBG field geom in window: %d,%d %dx%d\n",
            field->mapTo(&w, QPoint(0, 0)).x(), field->mapTo(&w, QPoint(0, 0)).y(),
            field->width(), field->height());
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
