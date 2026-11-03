// =============================================================================
// move_numbers_preview — offscreen visual verification of the
// BETWEEN-CELLS move numbers:
//
//   movenum_move3.png      a 3-step L move: numbers 1,2,3 BETWEEN the cells
//                          (3 between the second-to-last and the LAST cell,
//                          none on a cell, none on the trail line)
//   movenum_move1.png      a 1-SQUARE move: its "1" appears between the two
//                          cells (before: nothing at all)
//   movenum_blitz.png      a BLITZ (2 squares): numbers 1,2 between the cells
//
// The numbers are derived from the move trace itself (path index + 1), so
// they share the trail's lifecycle exactly.
//
// Run: QT_QPA_PLATFORM=offscreen ./build/move_numbers_preview [out_dir]
// =============================================================================
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QThread>

#include <QLabel>
#include <QMouseEvent>
#include <QScrollBar>
#include <QScrollArea>

#include "core/gamemodel.h"
#include "ui/actionlog.h"
#include "ui/fieldview.h"
#include "ui/mainwindow.h"

static void saveCrop(QWidget &w, const QRect &r, const QString &path)
{
    w.grab(r).save(path);
    qInfo().noquote() << "saved" << path;
}

static void change(QJsonArray &out, const QString &id, const QString &key,
                   const QJsonValue &value)
{
    QJsonObject c;
    c.insert(QStringLiteral("modelChangeId"), id);
    c.insert(QStringLiteral("modelChangeKey"), key);
    c.insert(QStringLiteral("modelChangeValue"), value);
    out.append(c);
}

static void pump(int ms)
{
    for (int t = 0; t < ms; t += 10) {
        QThread::msleep(10);
        QApplication::processEvents();
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QString outDir = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                    : QStringLiteral("/tmp/movenum");
    QDir().mkpath(outDir);

    QFile f(QStringLiteral("tests/liveGameState_fixture.json"));
    if (!f.open(QIODevice::ReadOnly))
        qFatal("tests/liveGameState_fixture.json missing");
    const QJsonObject game =
        QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("game")).toObject();

    MainWindow w;
    w.showNormal();
    w.resize(1900, 1100); // bigger cells -> readable verification crops
    w.show();
    w.setFallbackTeamLogos(QStringLiteral("/i/785766"), QStringLiteral("/i/769338"));
    w.onGameState(game);

    FieldView *field = w.findChild<FieldView *>();
    if (!field)
        qFatal("missing the field widget");

    // The actor: first home player on the field. Relocate to a clear,
    // in-bounds origin (plain placement — no move action, no trace) so the
    // three sequences have room to the LEFT and UP.
    QString actor;
    for (const FFBPlayer &p : w.model()->rosterHome())
        if (p.hasCoords && GameModel::isOnField(p.fieldX, p.fieldY)) {
            actor = p.playerId;
            break;
        }
    if (actor.isEmpty())
        qFatal("no home player on the field in the fixture");
    const int sx = 12, sy = 9; // clear origin, mid-field
    {
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerId"), actor,
               QJsonValue(actor));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << sx << sy);
        w.onModelSync(ch, QJsonArray());
        pump(400);
    }

    const auto dumpTrace = [&]() {
        const auto segs = w.model()->moveSegments();
        fprintf(stderr, "TRACE segments=%d\n", segs.size());
        for (int si = 0; si < segs.size(); ++si) {
            fprintf(stderr, "  seg %d: gfi=%d cells:", si,
                    segs[si].goForIt ? 1 : 0);
            for (const QPoint &c : segs[si].cells)
                fprintf(stderr, " (%d,%d)", c.x(), c.y());
            fprintf(stderr, " numbers:");
            for (int n : segs[si].stepNumbers)
                fprintf(stderr, " %d", n);
            fprintf(stderr, "\n");
        }
        for (const GameModel::BlockStepMark &mk : w.model()->blockStepMarks())
            fprintf(stderr, "  BLOCK n=%d (%d,%d)->(%d,%d)\n", mk.number,
                    mk.from.x(), mk.from.y(), mk.to.x(), mk.to.y());
        const FFBPlayer *a = w.model()->findPlayerById(actor);
        fprintf(stderr, "  actor at (%d,%d)\n",
                a ? a->fieldX : -1, a ? a->fieldY : -1);
    };

    const auto cropCells = [&](int x0, int y0, int x1, int y1, const QString &name) {
        const QRectF fr = field->fieldRect();
        const qreal cw = fr.width() / 26.0, ch = fr.height() / 15.0;
        const QPoint c0 = field->cellCenterPx(x0, y0);
        const QPoint c1 = field->cellCenterPx(x1, y1);
        saveCrop(*field,
                 QRect(c0.x() - cw, c0.y() - ch,
                       (c1.x() - c0.x()) + cw * 2, (c1.y() - c0.y()) + ch * 2)
                     .intersected(field->rect()),
                 outDir + "/" + name);
    };

    // Emulate a fresh action window (live: a different player acts, or a
    // new turn clears the trace — the numbering restarts at 1).
    const auto freshTurn = [&](int ax, int ay) {
        w.model()->clearMoveTrace();
        QJsonArray ch;
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << ax << ay);
        w.onModelSync(ch, QJsonArray());
        pump(150);
    };

    // Place an away player at a cell (plain re-placement, no trace).
    const auto placeDef = [&](const QString &id, int x, int y) {
        QJsonArray ch;
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), id,
               QJsonArray() << x << y);
        w.onModelSync(ch, QJsonArray());
        pump(100);
    };

    QStringList awayOnField;
    for (const FFBPlayer &p : w.model()->rosterAway())
        if (p.hasCoords && GameModel::isOnField(p.fieldX, p.fieldY))
            awayOnField << p.playerId;
    if (awayOnField.size() < 3)
        qFatal("need >=3 away players on the field in the fixture");

    // --- Sequence A: a 3-step L move (left, left, up) -------------------------
    // Numbers 1..3 BETWEEN the cells; "3" between the second-to-last and the
    // last cell (the user's key requirement: the last cell is numbered too).
    {
        freshTurn(sx, sy);
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("move")));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx - 1) << sy);
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx - 2) << sy);
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx - 2) << (sy - 1));
        w.onModelSync(ch, QJsonArray());
        pump(900); // let the glide settle
        dumpTrace();
        cropCells(sx - 2, sy - 1, sx, sy, QStringLiteral("movenum_move3.png"));
    }

    // --- Sequence B: a 1-SQUARE move (left) ------------------------------------
    // The "1" must appear between the two cells (before: nothing was painted,
    // the server's only TrackNumber for that move is the 0 on the starting
    // cell, which was not painted).
    {
        freshTurn(sx - 2, sy - 1);
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("move")));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx - 3) << (sy - 1));
        w.onModelSync(ch, QJsonArray());
        pump(900);
        dumpTrace();
        cropCells(sx - 4, sy - 2, sx - 1, sy, QStringLiteral("movenum_move1.png"));
    }

    // --- Sequence C: a BLITZ move (2 squares, up then right) --------------------
    // Numbers 1,2 between the cells (a blitz moves 2 squares).
    {
        freshTurn(sx - 3, sy - 2);
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("blitzMove")));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx - 3) << (sy - 3));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx - 2) << (sy - 3));
        w.onModelSync(ch, QJsonArray());
        pump(900);
        dumpTrace();
        cropCells(sx - 4, sy - 4, sx - 1, sy - 1,
                 QStringLiteral("movenum_blitz.png"));
    }

    // --- Sequence D: the reported BUG scenario (blitz + block + push +
    // backward follow) — the EXACT wire flow of ffb-server:
    //   actingPlayerSetPlayerAction=blitzMove
    //   -> sideways step (the blitz's 1 square)
    //   -> `block` report (defender armed)
    //   -> push (defender's coordinates)
    //   -> gameSetDefenderId=null + actingPlayerSetPlayerAction=blitzMove
    //      (StepEndBlocking re-dispatches the action for the follow move)
    //   -> the follow steps (here: back, 4 squares)
    // Expected numbering: sideways=1, block=2 (between attacker/defender),
    // backward steps=3,4,5,6 — ONE continuous count.
    {
        const QString def1 = awayOnField[0];
        freshTurn(sx, sy);
        placeDef(def1, sx + 2, sy); // adjacent to the future contact cell
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("blitzMove")));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx + 1) << sy); // sideways step (contact)
        QJsonArray rep;
        {
            QJsonObject r;
            r.insert(QStringLiteral("reportId"), QStringLiteral("block"));
            r.insert(QStringLiteral("defenderId"), def1);
            rep.append(r);
        }
        w.onModelSync(ch, rep);
        pump(200);
        QJsonArray ch2;
        change(ch2, QStringLiteral("fieldModelSetPlayerCoordinate"), def1,
               QJsonArray() << (sx + 3) << sy); // push, away from the actor
        change(ch2, QStringLiteral("gameSetDefenderId"), QString(), QJsonValue());
        change(ch2, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("blitzMove"))); // re-dispatch
        change(ch2, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << sx << sy);       // back 1
        change(ch2, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx - 1) << sy); // back 2
        change(ch2, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx - 2) << sy); // back 3
        change(ch2, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx - 3) << sy); // back 4
        w.onModelSync(ch2, QJsonArray());
        pump(900);
        dumpTrace();
        cropCells(sx - 3, sy - 1, sx + 3, sy + 1,
                 QStringLiteral("blitz_bug.png"));
    }

    // --- Sequence E: FRENZY (blitz + block + second block + follow) ------------
    // Expected: sideways=1, block1=2, block2 (frenzy)=3, follow=4.
    // (Different defenders than sequence D: the UI's report dedup drops a
    // `block` report identical to one within 2.5 s — a tool artifact, in
    // the live game blocks are turns apart.)
    {
        const QString def1 = awayOnField[1];
        const QString def2 = awayOnField[2];
        freshTurn(sx - 1, sy);
        placeDef(def1, sx + 1, sy);  // adjacent to the future contact cell
        placeDef(def2, sx, sy - 1);  // down, adjacent to the contact cell
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("blitzMove")));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << sx << sy); // sideways step (contact)
        QJsonArray rep;
        {
            QJsonObject r;
            r.insert(QStringLiteral("reportId"), QStringLiteral("block"));
            r.insert(QStringLiteral("defenderId"), def1);
            rep.append(r);
        }
        w.onModelSync(ch, rep);
        pump(200);
        QJsonArray ch2;
        change(ch2, QStringLiteral("gameSetDefenderId"), QString(), QJsonValue());
        // FRENZY: a fresh `block` report for the down player next door.
        QJsonArray rep2;
        {
            QJsonObject r;
            r.insert(QStringLiteral("reportId"), QStringLiteral("block"));
            r.insert(QStringLiteral("defenderId"), def2);
            rep2.append(r);
        }
        w.onModelSync(ch2, rep2);
        pump(200);
        QJsonArray ch3;
        change(ch3, QStringLiteral("gameSetDefenderId"), QString(), QJsonValue());
        change(ch3, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("blitzMove"))); // re-dispatch
        change(ch3, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx - 1) << sy); // follow: one back
        w.onModelSync(ch3, QJsonArray());
        pump(900);
        dumpTrace();
        cropCells(sx - 2, sy - 2, sx + 2, sy + 1,
                 QStringLiteral("frenzy.png"));
    }

    // --- Sequence F: GO FOR IT (blitz + failed block + GFI extra move) ---------
    // Expected: sideways=1, block=2, GFI move=3 drawn RED.
    {
        pump(3000); // out of the report-dedup window (2.5 s)
        const QString def1 = awayOnField[0];
        freshTurn(sx - 1, sy);
        placeDef(def1, sx + 1, sy); // adjacent to the future contact cell
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("blitzMove")));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << sx << sy); // sideways step (contact)
        QJsonArray rep;
        {
            QJsonObject r;
            r.insert(QStringLiteral("reportId"), QStringLiteral("block"));
            r.insert(QStringLiteral("defenderId"), def1);
            rep.append(r);
        }
        w.onModelSync(ch, rep);
        pump(200);
        QJsonArray ch2;
        change(ch2, QStringLiteral("gameSetDefenderId"), QString(), QJsonValue());
        // The GFI roll succeeds -> the next new route is the extra move.
        QJsonArray rep2;
        {
            QJsonObject r;
            r.insert(QStringLiteral("reportId"), QStringLiteral("goForItRoll"));
            r.insert(QStringLiteral("playerId"), actor);
            r.insert(QStringLiteral("successful"), true);
            rep2.append(r);
        }
        w.onModelSync(ch2, rep2);
        pump(200);
        QJsonArray ch3;
        change(ch3, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("move")));
        change(ch3, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << sx << (sy - 1)); // the GFI extra step
        w.onModelSync(ch3, QJsonArray());
        pump(900);
        dumpTrace();
        cropCells(sx - 2, sy - 3, sx + 2, sy + 1,
                 QStringLiteral("gfi.png"));
    }

    // --- Sequence G: PLAIN BLOCK of an adjacent player (no move in this
    // drive) + push + follow. Wire flow:
    //   actingPlayerSetPlayerAction=block
    //   -> `block` report
    //   -> push (defender's coordinates)
    //   -> the follow step (BARE — StepFollowup, no action re-dispatch)
    //   -> gameSetDefenderId=null
    // Expected: ONE segment (the trail is drawn), NO step numbers, NO
    // block mark (for a blocked adjacent player the trail alone is enough
    // — no cell counting).
    {
        pump(3000); // out of the report-dedup window (2.5 s)
        const QString def1 = awayOnField[1];
        freshTurn(sx, sy);
        placeDef(def1, sx + 1, sy); // ADJACENT (the block target)
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("block")));
        QJsonArray rep;
        {
            QJsonObject r;
            r.insert(QStringLiteral("reportId"), QStringLiteral("block"));
            r.insert(QStringLiteral("defenderId"), def1);
            rep.append(r);
        }
        w.onModelSync(ch, rep);
        pump(200);
        QJsonArray ch2;
        change(ch2, QStringLiteral("fieldModelSetPlayerCoordinate"), def1,
               QJsonArray() << (sx + 2) << sy); // push away
        change(ch2, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx + 1) << sy); // follow (bare, block open)
        change(ch2, QStringLiteral("gameSetDefenderId"), QString(), QJsonValue());
        w.onModelSync(ch2, QJsonArray());
        pump(900);
        dumpTrace();
        cropCells(sx, sy - 1, sx + 2, sy + 1,
                 QStringLiteral("plainblock.png"));
    }

    // --- Section H: ACTION LOG WRAP — a long line ("Catch (Inaccurate Pass
    // or Scattered)" + dice + result) must WRAP, not clip. The log's height
    // is fixed (compact), so each shot carries just the lines it needs:
    // compact width (a short line + the long one) and a narrow width (the
    // long one alone, wrapped harder).
    {
        const auto shootLog = [&](const QString &name, int width,
                                  bool withShort) {
            const int height = ActionLogWidget::compactHeight();
            auto *holder = new QWidget(&w);
            holder->resize(width + 4, height + 4);
            holder->move(qMax(0, 1900 - width - 120), 40);
            auto *log = new ActionLogWidget(holder);
            log->resize(width, height);
            holder->show();
            log->show();

            if (withShort) {
                DiceLogLine short1;
                short1.text = QStringLiteral("Ball scattered");
                short1.blockFaces = QStringList() << QStringLiteral("3");
                short1.actorIsPlayer = false;
                log->appendLine(short1, QStringLiteral("Ball"));
            }

            // The reported line: long action text + block face + d6 roll
            // with target and result.
            DiceLogLine long1;
            long1.text =
                QStringLiteral("Catch (Inaccurate Pass or Scattered)");
            long1.actorId = QStringLiteral("18109690");
            long1.blockFaces = QStringList() << QStringLiteral("6");
            long1.d6Roll = 3;
            long1.d6Target = 3;
            long1.d6Ok = false;
            long1.d6ShowResult = true;
            log->appendLine(long1, QStringLiteral("Meelos \"The Hero\" Sizzig (3)"));

            pump(800);
            // Stick-to-bottom check: after a wrapped row settles, the
            // view must show the newest line (never a phantom blank
            // stretch at the bottom of an over-grown stack).
            if (auto *sc = log->findChild<QScrollArea *>()) {
                const auto rows = sc->widget()->findChildren<QWidget *>(QString(),
                                                                        Qt::FindDirectChildrenOnly);
                const int content = rows.isEmpty() ? 0 : rows.last()->geometry().bottom() + 2;
                const int vh = sc->viewport()->height();
                qInfo().noquote() << name << "stackH=" << sc->widget()->height()
                                  << "content=" << content << "viewport=" << vh;
                if (auto *sb = sc->verticalScrollBar())
                    qInfo().noquote() << name << "sb value=" << sb->value()
                                      << "max=" << sb->maximum();
            }
            log->grab().save(outDir + "/" + name);
            qInfo().noquote() << "saved" << outDir + "/" + name;

            // Expand via a synthetic chevron click (the user's exact path)
            // and shoot the fully-expanded log (the whole wrapped row).
            for (auto *lbl : log->findChildren<QLabel *>()) {
                if (lbl->text() == QString::fromUtf8("\u25b4")) {
                    const QPoint at = lbl->mapTo(log, lbl->rect().center());
                    QMouseEvent ev(QEvent::MouseButtonPress, at, QPointF(at),
                                   Qt::LeftButton, Qt::LeftButton,
                                   Qt::NoModifier);
                    QApplication::sendEvent(log, &ev);
                    break;
                }
            }
            pump(800);
            log->grab().save(outDir + "/" + name + QStringLiteral("_expanded.png"));
            qInfo().noquote() << "saved" << outDir + "/" + name + QStringLiteral("_expanded.png");
            holder->deleteLater();
        };
        // 560 = the REAL live width (mainwindow: setFixedWidth(560)).
        shootLog(QStringLiteral("logwrap_real.png"), 560, true);
        shootLog(QStringLiteral("logwrap_compact.png"), 360, true);
        shootLog(QStringLiteral("logwrap_narrow.png"), 240, false);
    }

    qInfo() << "movenum preview complete ->" << outDir;
    return 0;
}
