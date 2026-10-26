// =============================================================================
// round10_preview — offscreen visual verification of the round-10 changes
//
// Feeds a real MainWindow (offscreen) with the live game-state fixture,
// real match-list logo fallback (network fetch), model state changes
// (prone/stunned), and saves PNGs of the pieces the user reviews:
//
//   round10_window.png       whole HUD (rim glow + state marks + logos +
//                            chat pill + action log)
//   round10_header.png       header strip (taller TURN/HALF pills, logos)
//   round10_chat_default.png chat as it STARTS (collapsed pill, round 10 #3)
//   round10_actionlog_up.png bottom-left stage area: log EXPANDED upward
//   round10_card.png         chromeless hover card (no title, name margin)
//   round10_marks.png        field crop: prone "/" + stunned "X" tokens
//
// Run: QT_QPA_PLATFORM=offscreen ./build/round10_preview [out_dir]
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
                                    : QStringLiteral("/tmp/round10");
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
    // Round 10 #2: match-list logo fallback (the fixture state has no
    // logoUrl) — real network fetch, wait for it below.
    w.setFallbackTeamLogos(QStringLiteral("/i/785766"), QStringLiteral("/i/769338"));
    w.onGameState(game);

    FieldView *field = w.findChild<FieldView *>();
    ActionLogWidget *log = w.findChild<ActionLogWidget *>();
    ChatWidget *chat = w.findChild<ChatWidget *>();
    HeaderView *header = w.findChild<HeaderView *>();
    PlayerInfoWidget *info = w.findChild<PlayerInfoWidget *>();
    if (!field || !log || !chat || !header)
        qFatal("missing a widget");

    // --- Round 10 #3: the chat STARTS collapsed — grab it right away ---------
    saveCrop(*chat, chat->rect(), outDir + "/round10_chat_default.png");

    // --- Players -------------------------------------------------------------
    const QVector<FFBPlayer> home = w.model()->rosterHome();
    const QVector<FFBPlayer> away = w.model()->rosterAway();
    QString homePid, homeName;
    for (const FFBPlayer &p : home)
        if (p.hasCoords && GameModel::isOnField(p.fieldX, p.fieldY) && p.fieldY <= 8) {
            homePid = p.playerId;
            homeName = p.name;
            break;
        }
    // Two away players to knock down (prone + stunned).
    QString pronePid, stunnedPid;
    for (const FFBPlayer &p : away) {
        if (!p.hasCoords || !GameModel::isOnField(p.fieldX, p.fieldY))
            continue;
        if (pronePid.isEmpty())
            pronePid = p.playerId;
        else if (stunnedPid.isEmpty())
            stunnedPid = p.playerId;
    }

    // Round 10 #7: prone + stunned state marks (model sync, like the game).
    {
        QJsonArray changes;
        auto setState = [&](const QString &id, int state) {
            QJsonObject c;
            c.insert(QStringLiteral("modelChangeId"),
                     QStringLiteral("fieldModelSetPlayerState"));
            c.insert(QStringLiteral("modelChangeKey"), id);
            c.insert(QStringLiteral("modelChangeValue"), QJsonValue(state));
            changes.append(c);
        };
        if (!pronePid.isEmpty())
            setState(pronePid, 3);
        if (!stunnedPid.isEmpty())
            setState(stunnedPid, 4);
        w.onModelSync(changes, QJsonArray());
    }
    QApplication::processEvents();

    // --- Selection: rim glow (round 10 #5) ------------------------------------
    w.selectPlayer(homePid);
    QThread::msleep(120); // pulse frames
    QApplication::processEvents();

    // --- Let the logos land (real fetch) --------------------------------------
    for (int i = 0; i < 24; ++i) {
        QThread::msleep(500);
        QApplication::processEvents();
    }

    // --- Chat: a couple of messages, back to the pill --------------------------
    chat->setCollapsed(false);
    chat->addMessage(QStringLiteral("coach_home"), QStringLiteral("nice block!"),
                     QColor(0xc8, 0x96, 0x3c));
    chat->addMessage(QStringLiteral("coach_away"), QStringLiteral("ouch."),
                     QColor(0x4a, 0x6a, 0xb8));
    chat->setCollapsed(true);
    QApplication::processEvents();

    // --- Action log: expand UPWARD (round 10 #4) --------------------------------
    for (QLabel *l : log->findChildren<QLabel *>())
        if (l->text() == QString::fromUtf8("\u25b4")) {
            const QPoint at = l->mapTo(log, QPoint(l->width() / 2, l->height() / 2));
            QMouseEvent press(QMouseEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton,
                              Qt::NoModifier);
            QApplication::sendEvent(log, &press);
            break;
        }
    // Deselect first: the hover card may overlap the bottom-left log. (The
    // preview never drove a hover, so also hide the panel directly — a real
    // mouse-over-empty-cell would do it via the hover path.)
    for (FloatingPanel *fp : w.findChildren<FloatingPanel *>())
        if (fp->contentWidget() == info)
            fp->hide();
    {
        const auto occupied = [&](int x, int y) {
            for (const FieldToken &t : w.model()->fieldTokens())
                if (t.x == x && t.y == y)
                    return true;
            return false;
        };
        int ex = -1, ey = -1;
        for (int y = 0; y < 15 && ex < 0; ++y)
            for (int x = 0; x < 26; ++x)
                if (!occupied(x, y)) {
                    ex = x;
                    ey = y;
                    break;
                }
        if (ex >= 0) {
            const qreal cw = field->width() / 26.0;
            const qreal ch = field->height() / 15.0;
            const QPoint empty(int((ex + 0.5) * cw), int((ey + 0.5) * ch));
            QMouseEvent press(QMouseEvent::MouseButtonPress, empty, Qt::LeftButton, Qt::LeftButton,
                              Qt::NoModifier);
            QApplication::sendEvent(field, &press);
            QMouseEvent release(QMouseEvent::MouseButtonRelease, empty, Qt::NoButton, Qt::NoButton,
                                Qt::NoModifier);
            QApplication::sendEvent(field, &release);
            // A move over the empty cell: the hover path hides the card
            // (exactly what a real mouse does right after the click).
            QMouseEvent move(QMouseEvent::MouseMove, empty, Qt::NoButton, Qt::NoButton,
                             Qt::NoModifier);
            QApplication::sendEvent(field, &move);
            QApplication::processEvents();
        }
    }
    // Grab the bottom-left STAGE area (not just the log) so the upward
    // expansion is visible against the field behind it.
    QWidget *stage = log->parentWidget();
    const QRect logArea(log->geometry().x() - 30,
                        log->geometry().top() - 60,
                        log->geometry().width() + 60,
                        stage->height() - log->geometry().top() + 40);
    saveCrop(*stage, logArea, outDir + "/round10_actionlog_up.png");

    // Re-select the player: the rim glow back for the window shot.
    if (!homePid.isEmpty())
        w.selectPlayer(homePid);
    QApplication::processEvents();

    // --- Header: taller pills + logos (round 10 #1, #2) -------------------------
    saveCrop(*header, header->rect(), outDir + "/round10_header.png");

    // --- Whole HUD ---------------------------------------------------------------
    w.grab().save(outDir + "/round10_window.png");
    qInfo().noquote() << "saved" << outDir << "/round10_window.png";

    // --- Field crop: the downed tokens (round 10 #7) -----------------------------
    {
        const auto cellRect = [field](int x, int y) -> QRect {
            const qreal cw = field->width() / 26.0;
            const qreal ch = field->height() / 15.0;
            return QRect(int(x * cw), int(y * ch), int(cw * 4), int(ch * 4));
        };
        if (!pronePid.isEmpty()) {
            const FFBPlayer *p = w.model()->findPlayerById(pronePid);
            if (p) {
                QRect r = cellRect(p->fieldX, p->fieldY);
                r = r.adjusted(-12, -12, 12, 12);
                saveCrop(*field, r, outDir + "/round10_marks.png");
            }
        }
    }

    // --- Chromeless hover card (round 10: no title, name margin) -----------------
    {
        const FFBPlayer *p = w.model()->findPlayerById(homePid);
        if (p) {
            // The player is already selected: grab the whole panel.
            FloatingPanel *panel = nullptr;
            for (FloatingPanel *fp : w.findChildren<FloatingPanel *>())
                if (fp->contentWidget() == info)
                    panel = fp;
            if (panel) {
                panel->raise();
                QApplication::processEvents();
                panel->grab().save(outDir + "/round10_card.png");
                qInfo().noquote() << "saved" << outDir << "/round10_card.png";
            }
        }
    }

    qInfo() << "round10 preview complete ->" << outDir;
    return 0;
}
