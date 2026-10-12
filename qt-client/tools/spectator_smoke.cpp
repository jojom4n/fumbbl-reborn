// =============================================================================
// spectator_smoke — headless live verification of the spectator flow
//
// Connects to fumbbl.com as a spectator (real network, real credentials)
// and prints the whole protocol exchange. This is the "debug as spectator"
// milestone: proves the C++ core respects the architecture of the working
// TS client (LZString frames, 2 s ping with immediate first ping,
// clientRequestVersion -> clientJoin -> serverGameState).
//
// Usage:
//   ./spectator_smoke --game <GAME_ID> [--credentials <path>] [--duration <sec>]
//
// Exit code: 0 if serverJoin + serverGameState were received, 1 otherwise.
// =============================================================================
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "core/spectatorsession.h"

#include <cstdio>

static QString timestamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
}

static void logLine(const QString &what)
{
    std::printf("[%s] %s\n", timestamp().toUtf8().constData(), what.toUtf8().constData());
    std::fflush(stdout);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("fumbbl-qt-spectator-smoke");

    QString credentialsPath = QDir::homePath() +
        QStringLiteral("/.local/share/com.saruman.fumbbl-reborn/fumbbl-credentials.json");
    int gameId = 0;
    int durationSec = 15;

    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QLatin1String("--game") && i + 1 < args.size())
            gameId = args[++i].toInt();
        else if (args[i] == QLatin1String("--credentials") && i + 1 < args.size())
            credentialsPath = args[++i];
        else if (args[i] == QLatin1String("--duration") && i + 1 < args.size())
            durationSec = args[++i].toInt();
    }

    if (gameId == 0) {
        std::printf("usage: spectator_smoke --game <GAME_ID> "
                    "[--credentials <path>] [--duration <sec>]\n");
        return 2;
    }

    // --- Load credentials (same file the Tauri app writes) -----------------
    QFile credsFile(credentialsPath);
    if (!credsFile.open(QIODevice::ReadOnly)) {
        logLine(QStringLiteral("FATAL: cannot read credentials file %1").arg(credentialsPath));
        return 2;
    }
    const QJsonObject creds = QJsonDocument::fromJson(credsFile.readAll()).object();
    QJsonObject c = creds.value("fumbbl_credentials").toObject();
    if (c.isEmpty())
        c = creds.value("credentials").toObject();
    const QString clientId = c.value("clientId").toString();
    const QString clientSecret = c.value("clientSecret").toString();
    const QString username = c.value("username").toString();
    if (clientId.isEmpty() || clientSecret.isEmpty() || username.isEmpty()) {
        logLine(QStringLiteral(
            "FATAL: credentials file must contain clientId/clientSecret/username"));
        return 2;
    }
    logLine(QStringLiteral("Using credentials for user '%1', game %2, watching %3 s")
                .arg(username)
                .arg(gameId)
                .arg(durationSec));

    // --- Session ------------------------------------------------------------
    SpectatorSession session;
    bool gotVersion = false;
    bool gotGameState = false;
    int modelSyncs = 0;
    int talks = 0;
    int pongs = 0;

    QObject::connect(&session, &SpectatorSession::progress,
                     [&](const QString &m) { logLine(QStringLiteral("PROGRESS  ") + m); });
    QObject::connect(&session, &SpectatorSession::fatal,
                     [&](const QString &m) { logLine(QStringLiteral("FATAL     ") + m); });
    QObject::connect(&session, &SpectatorSession::serverVersionReceived,
                     [&](const QJsonObject &) { gotVersion = true; });
    QObject::connect(&session, &SpectatorSession::gameStateReceived, [&](const QJsonObject &) {
        gotGameState = true;
    });
    QObject::connect(&session, &SpectatorSession::modelSyncReceived,
                     [&](const QJsonArray &, const QJsonArray &) { ++modelSyncs; });
    QObject::connect(&session, &SpectatorSession::talkReceived,
                     [&](const QJsonObject &) { ++talks; });
    QObject::connect(&session, &SpectatorSession::pongReceived,
                     [&](qint64) { ++pongs; });
    QObject::connect(&session, &SpectatorSession::protocolSending,
                     [&](const QString &id, const QJsonObject &) {
                         logLine(QStringLiteral(">> send  ") + id);
                     });
    QObject::connect(&session, &SpectatorSession::protocolReceived,
                     [&](const QString &id, const QJsonObject &p) {
                         // Keep the log readable: one line per command.
                         logLine(QStringLiteral("<< recv  ") + id);
                         Q_UNUSED(p)
                     });

    QElapsedTimer watch;
    watch.start();
    session.connectAsSpectator(clientId, clientSecret, username, gameId);

    // Watchdog: stop after the requested duration.
    QTimer watchdog;
    QObject::connect(&watchdog, &QTimer::timeout, [&]() {
        session.disconnect();

        std::printf("\n--- summary (%.1f s) ----------------------------------------\n",
                    watch.elapsed() / 1000.0);
        std::printf("  serverVersion      : %s\n", gotVersion ? "yes" : "no");
        std::printf("  serverJoin         : %s\n", session.isJoined() ? "yes" : "no");
        std::printf("  serverGameState    : %s\n", gotGameState ? "yes" : "no");
        std::printf("  serverModelSync    : %d\n", modelSyncs);
        std::printf("  serverTalk         : %d\n", talks);
        std::printf("  serverPong         : %d\n", pongs);

        const bool ok = gotVersion && session.isJoined() && gotGameState;
        std::printf("  RESULT: %s\n", ok ? "SPECTATOR FLOW OK" : "SPECTATOR FLOW INCOMPLETE");
        app.exit(ok ? 0 : 1);
    });
    watchdog.setSingleShot(true);
    watchdog.start(durationSec * 1000);

    return app.exec();
}
