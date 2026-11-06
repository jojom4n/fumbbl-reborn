// =============================================================================
// spectator_smoke — headless live verification of the spectator flow
//
// Connects to fumbbl.com as a spectator (real network, real credentials)
// and prints the whole protocol exchange. This is the live verification of
// the official coach+password login: LZString frames, 2 s ping with
// immediate first ping, clientRequestVersion -> clientPasswordChallenge ->
// serverPasswordChallenge -> clientJoin(challenge response) ->
// serverGameState.
//
// Usage:
//   ./spectator_smoke --game <GAME_ID> [--credentials <path>] [--duration <sec>]
//                     [--dump-gamestate <file>]   # save the raw serverGameState JSON
//
// Credentials file (default ~/.config/fumbbl-reborn/creds, keep 0600):
//   coach=...
//   password=...
// The password is MD5-hashed in-process and never printed.
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

#include "core/passwordchallenge.h"
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

    QString credentialsPath = QDir::homePath() + QStringLiteral("/.config/fumbbl-reborn/creds");
    int gameId = 0;
    int durationSec = 15;
    QString dumpPath; // optional: write the raw serverGameState here

    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QLatin1String("--game") && i + 1 < args.size())
            gameId = args[++i].toInt();
        else if (args[i] == QLatin1String("--credentials") && i + 1 < args.size())
            credentialsPath = args[++i];
        else if (args[i] == QLatin1String("--duration") && i + 1 < args.size())
            durationSec = args[++i].toInt();
        else if (args[i] == QLatin1String("--dump-gamestate") && i + 1 < args.size())
            dumpPath = args[++i];
    }

    if (gameId == 0) {
        std::printf("usage: spectator_smoke --game <GAME_ID> "
                    "[--credentials <path>] [--duration <sec>]\n");
        return 2;
    }

    // --- Load credentials (shared line file, see credentialsPath) ---------
    // The password is hashed in-process and NEVER printed.
    QString coach;
    QString password;
    {
        QFile credsFile(credentialsPath);
        if (!credsFile.open(QIODevice::ReadOnly)) {
            logLine(QStringLiteral("FATAL: cannot read credentials file %1").arg(credentialsPath));
            return 2;
        }
        const QList<QByteArray> lines = credsFile.readAll().split('\n');
        for (const QByteArray &raw : lines) {
            const int eq = raw.indexOf('=');
            if (eq <= 0)
                continue;
            const QString key = QString::fromUtf8(raw.left(eq)).trimmed();
            const QString value = QString::fromUtf8(raw.mid(eq + 1)).trimmed();
            if (key == QLatin1String("coach"))
                coach = value;
            else if (key == QLatin1String("password"))
                password = value;
        }
    }
    if (coach.isEmpty() || password.isEmpty()) {
        logLine(QStringLiteral("FATAL: credentials file must contain coach=/password="));
        return 2;
    }
    const QByteArray pwdMd5 = fumbbl::PasswordChallenge::encodePassword(password);
    logLine(QStringLiteral("Using credentials for coach '%1', game %2, watching %3 s")
                .arg(coach)
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
                         if (!dumpPath.isEmpty() && id == QLatin1String("serverGameState")) {
                             QFile f(dumpPath);
                             if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                                 f.write(QJsonDocument(p).toJson(QJsonDocument::Indented));
                                 logLine(QStringLiteral("DUMPED    serverGameState -> ") + dumpPath);
                                 dumpPath.clear(); // one-shot
                             } else {
                                 logLine(QStringLiteral("DUMP FAIL ") + dumpPath);
                             }
                         }
                         Q_UNUSED(p)
                     });

    QElapsedTimer watch;
    watch.start();
    session.connectAsSpectator(coach, pwdMd5, gameId);

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
