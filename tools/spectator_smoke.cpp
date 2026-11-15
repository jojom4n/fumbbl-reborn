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

#include "core/credentialstore.h"
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

    QString credentialsPath = CredentialStore::credentialsPath();
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
    // The password is hashed in-process and NEVER printed. The file may
    // carry either password= (plaintext, hashed here) or passwordMd5=
    // (the 32-hex MD5 the protocol expects — the app's stored form).
    QString coach;
    QString password;
    QString storedMd5;
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
            else if (key == QLatin1String("passwordMd5")
                     || key == QLatin1String("password_md5"))
                storedMd5 = value;
        }
    }
    if (coach.isEmpty()) {
        logLine(QStringLiteral("FATAL: credentials file must contain coach= (and either "
                                "password= or an OS keychain entry)"));
        return 2;
    }
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

    // Password in the file? use it. Otherwise the file is coach-only (the
    // app migrated it): the MD5 comes from the OS keychain (async).
    QElapsedTimer watch;
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

    auto startSession = [&](const QByteArray &pwdMd5) {
        watch.start();
        session.connectAsSpectator(coach, pwdMd5, gameId);
        watchdog.start(durationSec * 1000);
    };

    const QByteArray fileMd5 = password.isEmpty()
                                   ? QByteArray()
                                   : fumbbl::PasswordChallenge::encodePassword(password);
    if (!fileMd5.isEmpty()) {
        startSession(fileMd5);
    } else if (!storedMd5.isEmpty()) {
        logLine(QStringLiteral("Using passwordMd5= from the credentials file."));
        startSession(storedMd5.toLatin1());
    } else if (CredentialStore::hasBackend()) {
        logLine(QStringLiteral("No password in the file — looking up the OS keychain …"));
        CredentialStore store;
        // Auto-connection queues to the main thread (the store calls back
        // from a worker thread).
        QObject::connect(&store, &CredentialStore::getFinished,
                         [&](const QString &, const QByteArray &md5, bool ok) {
                             if (ok && !md5.isEmpty()) {
                                 startSession(md5);
                                 return;
                             }
                             logLine(QStringLiteral("FATAL: no password in the file and no "
                                                    "keychain entry (store ok=%1)")
                                       .arg(ok));
                             app.exit(2);
                         });
        store.get(coach);
    } else {
        logLine(QStringLiteral("FATAL: no password in the file and no keychain backend"));
        return 2;
    }

    return app.exec();
}
