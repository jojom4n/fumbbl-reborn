// Hover crash probe v2: hover EXACTLY on field tokens and dugout faces
// (precise positions from the model / boxEntryAt scan), interleaved with
// model syncs that mutate the field AND the box (player sent to the box ->
// box vectors reallocate). If the "ASSERT: p in qarraydatapointer.h"
// use-after-free exists, this must hit it.
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMouseEvent>
#include <QPoint>
#include <QRect>

#include "ui/mainwindow.h"
#include "ui/fieldview.h"
#include "ui/actionlog.h"
#include "core/gamemodel.h"

static QJsonArray blockDiceReport(const QString &pid, const QString &tid,
                                  const QStringList &faces)
{
    QJsonObject r;
    r.insert("reportId", "blockRoll");
    r.insert("choosingTeamId", tid);
    QJsonArray f;
    for (const QString &v : faces)
        f.append(v);
    r.insert("blockRoll", f);
    r.insert("defenderId", pid);
    QJsonArray out;
    out.append(r);
    return out;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QFile f("tests/liveGameState_fixture.json");
    if (!f.open(QIODevice::ReadOnly)) {
        fprintf(stderr, "fixture missing\n");
        return 2;
    }
    const QJsonObject game =
        QJsonDocument::fromJson(f.readAll()).object().value("game").toObject();

    MainWindow w;
    w.showNormal();
    w.resize(1400, 860);
    w.show();
    w.onGameState(game);
    QApplication::processEvents();

    FieldView *field = w.findChild<FieldView *>();
    if (!field) {
        fprintf(stderr, "field missing\n");
        return 2;
    }

    auto sendMove = [&](const QPoint &widgetPos) {
        QMouseEvent ev(QEvent::MouseMove, QPointF(widgetPos),
                       field->mapToGlobal(widgetPos),
                       Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(field, &ev);
    };
    auto sendLeave = [&]() {
        QMouseEvent ev(QEvent::Leave, QPointF(-5, -5), QPoint(-5, -5),
                       Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(field, &ev);
    };

    // Precise hover targets: every on-field token center.
    auto tokenPositions = [&]() -> QVector<QPoint> {
        QVector<QPoint> out;
        for (const FieldToken &t : w.model()->fieldTokens()) {
            out.append(field->cellCenterPx(t.x, t.y));
        }
        return out;
    };
    // Precise hover targets: scan the dugout rects on a fine grid and keep
    // the points that hit a bench face (boxEntryAt).
    auto facePositions = [&](const QRect &r) -> QVector<QPoint> {
        QVector<QPoint> out;
        for (int y = r.y() + 4; y < r.bottom() - 2; y += 5)
            for (int x = r.x() + 4; x < r.right() - 2; x += 5) {
                bool side = true;
                if (field->boxEntryAt(QPoint(x, y), side))
                    out.append(QPoint(x, y));
            }
        return out;
    };

    const QRect dgh = field->dugoutRect(true).toAlignedRect();
    const QRect dga = field->dugoutRect(false).toAlignedRect();

    // A field player to send INTO the box (mutates the box vectors).
    QString boxCand;
    for (const FFBPlayer &p : w.model()->rosterHome())
        if (p.hasCoords && GameModel::isOnField(p.fieldX, p.fieldY)) {
            boxCand = p.playerId;
            break;
        }

    for (int i = 0; i < 300; ++i) {
        // 1) Hover a field token (exact center).
        const auto toks = tokenPositions();
        if (!toks.isEmpty()) {
            sendMove(toks[i % toks.size()]);
            QApplication::processEvents();
        }
        // 2) Hover a dugout face (exact hit point), both sides.
        if (i % 2 == 0) {
            const auto faces = facePositions(dgh);
            if (!faces.isEmpty()) {
                sendMove(faces[i % faces.size()]);
                QApplication::processEvents();
            }
        } else {
            const auto faces = facePositions(dga);
            if (!faces.isEmpty()) {
                sendMove(faces[i % faces.size()]);
                QApplication::processEvents();
            }
        }
        // 3) A report batch (dice HTML) while hovering.
        if (i % 2 == 0) {
            w.onModelSync(QJsonArray(),
                          blockDiceReport(w.model()->rosterHome().first().playerId,
                                          w.model()->teamHomeId(),
                                          { "3", "2", "6", "4" }));
            QApplication::processEvents();
        }
        // 4) Every 4th: move the candidate player INTO the box (the box
        //    vectors reallocate), then back onto the field.
        if (i % 4 == 0) {
            QJsonArray ch;
            QJsonObject c;
            c.insert("modelChangeId", "fieldModelSetPlayerCoordinate");
            c.insert("modelChangeKey", boxCand);
            c.insert("modelChangeValue", QJsonArray() << (-1) << (i / 4 % 6));
            ch.append(c);
            w.onModelSync(ch, QJsonArray());
            QApplication::processEvents();
        } else if (i % 4 == 2) {
            QJsonArray ch;
            QJsonObject c;
            c.insert("modelChangeId", "fieldModelSetPlayerCoordinate");
            c.insert("modelChangeKey", boxCand);
            c.insert("modelChangeValue", QJsonArray() << 3 << 7);
            ch.append(c);
            w.onModelSync(ch, QJsonArray());
            QApplication::processEvents();
        }
        // 5) Every 7th: leave the widget (hover cleared) then re-enter.
        if (i % 7 == 0) {
            sendLeave();
            QApplication::processEvents();
        }
        if (i % 50 == 0)
            fprintf(stderr, "iter %d\n", i);
    }
    fprintf(stderr, "probe v2 survived 300 iterations\n");
    return 0;
}
