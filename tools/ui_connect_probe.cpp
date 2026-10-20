// =============================================================================
// ui_connect_probe — headless reproduction of the REAL UI spectate path
//
// Builds a real MainWindow and drives it exactly like the "Spectate Game"
// dialog's lambda does (clearMatchUi + session()->connectAsSpectator), then
// prints the session progress + protocol for a few seconds. Used to hunt the
// "join sent but no serverJoin" problem that only happened from the UI.
//
//   ./ui_connect_probe --game 12345 [--seconds 25]
// =============================================================================
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <cstdio>

#include "ui/mainwindow.h"
#include "ui/spectatedialog.h"
#include "core/spectatorsession.h"

namespace {
QString credentialsPath()
{
    return QDir::homePath()
        + QStringLiteral("/.local/share/com.saruman.fumbbl-reborn/fumbbl-credentials.json");
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    int gameId = 0;
    int seconds = 25;
    bool viaDialog = false;
    for (int i = 1; i < argc; ++i) {
        const QString a = argv[i];
        if (a == QStringLiteral("--game") && i + 1 < argc)
            gameId = QString(argv[++i]).toInt();
        else if (a == QStringLiteral("--seconds") && i + 1 < argc)
            seconds = QString(argv[++i]).toInt();
        else if (a == QStringLiteral("--via-dialog"))
            viaDialog = true;
    }
    if (gameId <= 0) {
        std::fprintf(stderr, "usage: ui_connect_probe --game <id> [--seconds N]\n");
        return 2;
    }

    QFile f(credentialsPath());
    if (!f.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "no credentials file at %s\n", qPrintable(credentialsPath()));
        return 2;
    }
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    QJsonObject c = root.value(QStringLiteral("fumbbl_credentials")).toObject();
    if (c.isEmpty())
        c = root.value(QStringLiteral("credentials")).toObject();
    const QString clientId = c.value(QStringLiteral("clientId")).toString();
    const QString clientSecret = c.value(QStringLiteral("clientSecret")).toString();
    const QString username = c.value(QStringLiteral("username")).toString();
    if (clientId.isEmpty() || clientSecret.isEmpty() || username.isEmpty()) {
        std::fprintf(stderr, "credentials incomplete\n");
        return 2;
    }

    MainWindow w;
    w.show();

    bool joined = false;
    bool gameState = false;
    QObject::connect(w.session(), &SpectatorSession::joined,
                     &w, [&](const QJsonObject &) { joined = true; });
    QObject::connect(w.session(), &SpectatorSession::gameStateReceived,
                     &w, [&](const QJsonObject &) { gameState = true; });
    QObject::connect(w.session(), &SpectatorSession::progress, &w,
                     [](const QString &msg) { std::fprintf(stderr, "[progress] %s\n", qPrintable(msg)); });
    QObject::connect(w.session(), &SpectatorSession::protocolSending, &w,
                     [](const QString &id, const QJsonObject &) {
                         std::fprintf(stderr, ">> send %s\n", qPrintable(id));
                     });
    QObject::connect(w.session(), &SpectatorSession::protocolReceived, &w,
                     [](const QString &id, const QJsonObject &) {
                         std::fprintf(stderr, "<< recv %s\n", qPrintable(id));
                     });

    if (viaDialog) {
        // FULL UI: the real SpectateDialog (match list fetch + exec() +
        // spectate()). The dialog's connectRequested lambda drives the
        // session exactly like in production.
        std::fprintf(stderr, "=== driving the FULL UI path (real dialog) ===\n");
        SpectateDialog dialog(&w);
        QObject::connect(&dialog, &SpectateDialog::connectRequested, &w,
                         [&](const QString &cid, const QString &csec, const QString &user, int gid) {
                             std::fprintf(stderr, "[dialog] connectRequested game=%d user=%s\n", gid,
                                           qPrintable(user));
                             // Body of the production lambda (MainWindow::openSpectateDialog):
                             w.clearMatchUi();
                             w.setConnectionStatus(QStringLiteral("Connecting to game %1…").arg(gid), false);
                             w.session()->connectAsSpectator(cid, csec, user, gid);
                         });
        dialog.show();
        // Select the row for the requested game once the list is loaded, then
        // fire the spectate button — exactly what a double-click does.
        QObject::connect(&dialog, &SpectateDialog::listLoaded, &w,
                         [&]() {
                             std::fprintf(stderr, "[dialog] list loaded, selecting game %d and spectating\n", gameId);
                             QTimer::singleShot(200, &dialog, [&dialog, gameId, &app]() {
                                 if (!dialog.selectGame(gameId)) {
                                     std::fprintf(stderr, "[dialog] game %d NOT in list — not joining\n", gameId);
                                     app.quit();
                                     return;
                                 }
                                 dialog.spectate();
                             });
                         });
        dialog.exec();
    } else {
        std::fprintf(stderr, "=== driving the exact UI path (lambda body) ===\n");
        // This is literally the body of the connectRequested lambda in
        // MainWindow::openSpectateDialog():
        w.clearMatchUi();
        w.session()->connectAsSpectator(clientId, clientSecret, username, gameId);
    }

    QTimer::singleShot(seconds * 1000, &app, [&]() {
        std::fprintf(stderr, "=== after %ds: joined=%d gameState=%d ===\n", seconds,
                     int(joined), int(gameState));
        app.quit();
    });
    return app.exec();
}
