// =============================================================================
// round14_preview — offscreen visual verification of the BETWEEN-CELLS move
// numbers (user: "li mettiamo tra casella e casella ... ben leggibili"):
//
//   round14_move3.png      a 3-step L move: numbers 1,2,3 BETWEEN the cells
//                          (3 between the second-to-last and the LAST cell,
//                          none on a cell, none on the trail line)
//   round14_move1.png      a 1-SQUARE move: its "1" appears between the two
//                          cells (before: nothing at all)
//   round14_blitz.png      a BLITZ (2 squares): numbers 1,2 between the cells
//
// The numbers are derived from the move trace itself (path index + 1), so
// they share the trail's lifecycle exactly.
//
// Run: QT_QPA_PLATFORM=offscreen ./build/round14_preview [out_dir]
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

#include "core/gamemodel.h"
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
                                    : QStringLiteral("/tmp/round14");
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
        const auto paths = w.model()->moveTrace();
        fprintf(stderr, "TRACE paths=%d\n", paths.size());
        for (int pi = 0; pi < paths.size(); ++pi) {
            fprintf(stderr, "  path %d: %d cells\n", pi, paths[pi].size());
            for (const QPoint &c : paths[pi])
                fprintf(stderr, "    (%d,%d)\n", c.x(), c.y());
        }
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

    // --- Sequence A: a 3-step L move (left, left, up) -------------------------
    // Numbers 1..3 BETWEEN the cells; "3" between the second-to-last and the
    // last cell (the user's key requirement: the last cell is numbered too).
    {
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
        cropCells(sx - 2, sy - 1, sx, sy, QStringLiteral("round14_move3.png"));
    }

    // --- Sequence B: a 1-SQUARE move (left) ------------------------------------
    // The "1" must appear between the two cells (before: nothing was painted,
    // the server's only TrackNumber for it being the start cell's 0).
    {
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("move")));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx - 3) << (sy - 1));
        w.onModelSync(ch, QJsonArray());
        pump(900);
        dumpTrace();
        cropCells(sx - 4, sy - 2, sx - 1, sy, QStringLiteral("round14_move1.png"));
    }

    // --- Sequence C: a BLITZ (2 squares, up then right) ------------------------
    // Numbers 1,2 between the cells (the user: "anche in caso di blitz").
    {
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("blitzMove")));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx - 3) << (sy - 2));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx - 2) << (sy - 2));
        w.onModelSync(ch, QJsonArray());
        pump(900);
        dumpTrace();
        cropCells(sx - 4, sy - 3, sx - 1, sy, QStringLiteral("round14_blitz.png"));
    }

    // --- Sequence D: the reported BUG scenario (blitz + block + push +
    // backward follow) ----------------------------------------------
    // #4 blitzes: 1 square sideways to the right (contact), blocks #5,
    // #5 is pushed right, #4 does NOT follow forward but moves back 4
    // squares. Expected by the user: ONE continuous numbering — sideways =
    // 1, block = 2, backward steps = 3,4,5,6.
    {
        QString def;
        for (const FFBPlayer &p : w.model()->rosterAway())
            if (p.hasCoords && GameModel::isOnField(p.fieldX, p.fieldY)) {
                def = p.playerId;
                break;
            }
        if (def.isEmpty())
            qFatal("no away player on the field in the fixture");
        // Emulate a fresh turn (the real game clears the trace on the
        // turn-mode change) and relocate the actor + defender in place.
        w.model()->clearMoveTrace();
        {
            QJsonArray ch;
            change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
                   QJsonArray() << sx << sy);
            change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), def,
                   QJsonArray() << (sx + 2) << sy);
            w.onModelSync(ch, QJsonArray());
            pump(200);
        }
        QJsonArray ch;
        change(ch, QStringLiteral("actingPlayerSetPlayerAction"), QString(),
               QJsonValue(QStringLiteral("blitzMove")));
        change(ch, QStringLiteral("fieldModelSetPlayerCoordinate"), actor,
               QJsonArray() << (sx + 1) << sy); // sideways step (contact)
        w.onModelSync(ch, QJsonArray());
        pump(200);
        w.model()->setBlockTargetId(def, actor); // block arms (UI-side)
        QJsonArray ch2;
        change(ch2, QStringLiteral("fieldModelSetPlayerCoordinate"), def,
               QJsonArray() << (sx + 3) << sy); // push, away from the actor
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
                 QStringLiteral("round14_blitzbug.png"));
    }

    qInfo() << "round14 preview complete ->" << outDir;
    return 0;
}
