// =============================================================================
// logo_probe — offscreen diagnostic for the team-logo fetch pipeline
//
// Feeds a real MainWindow a game state whose team logoUrl is forced to a
// KNOWN-GOOD live icon (i/785766), lets the network fetch run, then reports:
//   * every "logo ..." protocol-log line (request / HTTP error / decode)
//   * whether the field + header actually received non-null pixmaps
//
// This isolates WHERE the logo pipeline breaks (URL / HTTP / decode / draw).
// Run: QT_QPA_PLATFORM=offscreen ./build/logo_probe [fixture.json]
// =============================================================================
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QThread>

#include "core/gamemodel.h"
#include "ui/fieldview.h"
#include "ui/headerview.h"
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QString fixture = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                     : QStringLiteral("tests/liveGameState_fixture.json");
    QFile f(fixture);
    if (!f.open(QIODevice::ReadOnly))
        qFatal("cannot open %s", fixture.toUtf8().constData());
    QJsonObject game =
        QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("game")).toObject();

    // Force known-good live icons onto both teams (the fixture may carry null).
    for (const char *side : { "teamHome", "teamAway" }) {
        QJsonObject t = game.value(side).toObject();
        t.insert(QStringLiteral("logoUrl"), QStringLiteral("i/785766"));
        if (t.value(QStringLiteral("baseIconPath")).toString().isEmpty())
            t.insert(QStringLiteral("baseIconPath"), QStringLiteral("https://cdn.fumbbl.com/"));
        game.insert(side, t);
    }

    MainWindow w;
    w.resize(1200, 750);
    w.show();
    w.onGameState(game);

    // Let the async fetch run (real network on this box).
    for (int i = 0; i < 30; ++i) { // ~ up to 15 s
        QThread::msleep(500);
        QApplication::processEvents();
    }
    QApplication::processEvents();

    // Dump the protocol log (the logo trace lines live there).
    if (QPlainTextEdit *proto = w.findChild<QPlainTextEdit *>()) {
        const QString all = proto->toPlainText();
        for (const QString &line : all.split('\n'))
            if (line.contains(QStringLiteral("logo")) || line.contains(QStringLiteral("GameState")))
                qInfo().noquote() << "PROTO:" << line.trimmed();
    }

    // Did the widgets receive pixmaps? (field + header both hold copies.)
    FieldView *field = w.findChild<FieldView *>();
    HeaderView *header = w.findChild<HeaderView *>();
    qInfo().noquote()
        << "model.homeLogoUrl =" << w.model()->teamHomeLogoUrl();
    qInfo().noquote()
        << "model.awayLogoUrl =" << w.model()->teamAwayLogoUrl();

    // Grab the header to eyeball whether crests are painted.
    if (header)
        header->grab().save("/tmp/logo_header.png");
    qInfo() << "saved /tmp/logo_header.png";
    Q_UNUSED(field)
    return 0;
}
