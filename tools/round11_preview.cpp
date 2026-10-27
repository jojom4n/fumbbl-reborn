// =============================================================================
// round11_preview — offscreen visual verification of the round-11 changes
//
//   round11_window.png        whole HUD (side-zone lines, smooth logo
//                             watermark, visible rim glow, 28px dice,
//                             round downed tokens, auto-sized card)
//   round11_field_lines.png   field crop: wide-zone boundary lines (y=4/11)
//   round11_actionlog.png     action log: the new 28px dice
//   round11_card_long.png     card with MANY skills (auto height, no clip)
//   round11_downed.png        field crop: prone/stunned tokens (face NOT
//                             squashed — round 8 bug)
//
// Run: QT_QPA_PLATFORM=offscreen ./build/round11_preview [out_dir]
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
                                    : QStringLiteral("/tmp/round11");
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
    PlayerInfoWidget *info = w.findChild<PlayerInfoWidget *>();
    if (!field || !log || !header)
        qFatal("missing a widget");

    // --- Downed players (prone + stunned) ------------------------------------
    const QVector<FFBPlayer> away = w.model()->rosterAway();
    QString pronePid, stunnedPid;
    for (const FFBPlayer &p : away) {
        if (!p.hasCoords || !GameModel::isOnField(p.fieldX, p.fieldY))
            continue;
        if (pronePid.isEmpty())
            pronePid = p.playerId;
        else if (stunnedPid.isEmpty())
            stunnedPid = p.playerId;
    }
    {
        QJsonArray changes;
        auto setState = [&](const QString &id, int state) {
            QJsonObject c;
            c.insert(QStringLiteral("modelChangeId"), QStringLiteral("fieldModelSetPlayerState"));
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

    // --- Reports: block dice + d6 for the log ---------------------------------
    const QVector<FFBPlayer> home = w.model()->rosterHome();
    QString homePid, homeName;
    for (const FFBPlayer &p : home)
        if (p.hasCoords && GameModel::isOnField(p.fieldX, p.fieldY) && p.fieldY <= 8) {
            homePid = p.playerId;
            homeName = p.name;
            break;
        }
    const QString homeTeamId = w.model()->teamHomeId();
    const QString awayTeamId = w.model()->teamAwayId();
    QString awayPid;
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
    w.onModelSync(QJsonArray(), reports);

    // --- Selection: the rim glow (must be clearly visible now) ---------------
    w.showPlayerCardById(homePid);
    QThread::msleep(120);
    QApplication::processEvents();

    // --- Let the logos land ----------------------------------------------------
    for (int i = 0; i < 20; ++i) {
        QThread::msleep(500);
        QApplication::processEvents();
    }

    // --- Whole HUD ---------------------------------------------------------------
    w.grab().save(outDir + "/round11_window.png");
    qInfo().noquote() << "saved" << outDir << "/round11_window.png";

    // --- Field crop: side-zone lines (left half, full height) --------------------
    {
        const int cw = field->width() / 26;
        const int ch = field->height() / 15;
        saveCrop(*field, QRect(0, 0, cw * 14, field->height()),
                 outDir + "/round11_field_lines.png");
    }

    // --- Action log: 28px dice -----------------------------------------------------
    for (QLabel *l : log->findChildren<QLabel *>())
        if (l->text() == QString::fromUtf8("\u25b4")) {
            const QPoint at = l->mapTo(log, QPoint(l->width() / 2, l->height() / 2));
            QMouseEvent press(QMouseEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton,
                              Qt::NoModifier);
            QApplication::sendEvent(log, &press);
            break;
        }
    QApplication::processEvents();
    saveCrop(*log, log->rect(), outDir + "/round11_actionlog.png");

    // --- Downed tokens crop ------------------------------------------------------------
    {
        const FFBPlayer *p = w.model()->findPlayerById(pronePid);
        if (p) {
            const int cw = field->width() / 26;
            const int ch = field->height() / 15;
            QRect r(p->fieldX * cw - cw, p->fieldY * ch - ch, cw * 5, ch * 5);
            r = r.intersected(field->rect());
            saveCrop(*field, r, outDir + "/round11_downed.png");
        }
    }

    // --- Card with MANY skills (auto height, nothing clipped) -------------------------
    {
        PlayerInfoWidget card;
        PlayerInfoData d;
        d.playerId = homePid;
        d.name = homeName;
        d.number = 7;
        d.position = QStringLiteral("Ogre");
        d.race = QStringLiteral("Orc");
        d.teamName = w.model()->teamHomeName();
        d.sideHome = true;
        d.ma = 4;
        d.st = 5;
        d.ag = 4;
        d.pa = 3;
        d.av = 2;
        d.stateText = QStringLiteral("Stunned");
        d.stateFlags << QStringLiteral("Confused");
        const QStringList skillNames = {
            "Block", "Juggernaut", "Sidestep", "Nerves of Steel",
            "Loner", "Agile", "Douse", "Extra Arms", "Mighty Blow",
            "Thick Skull", "Bone Head", "Steady"};
        for (const QString &s : skillNames) {
            PlayerInfoData::Skill sk;
            sk.name = s;
            d.skills.append(sk);
        }
        card.showPlayer(d);
        card.resize(420, 100);
        card.show();
        QApplication::processEvents();
        const int need = card.sizeHint().height() + 16;
        card.resize(420, need);
        QApplication::processEvents();
        card.grab().save(outDir + "/round11_card_long.png");
        qInfo().noquote() << "saved" << outDir << "/round11_card_long.png"
                          << "(height" << need << ")";
    }

    Q_UNUSED(header)
    Q_UNUSED(info)
    Q_UNUSED(awayTeamId)
    qInfo() << "round11 preview complete ->" << outDir;
    return 0;
}
