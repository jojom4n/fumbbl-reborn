// =============================================================================
// reports_capturer — live raw-report capture
//
// The action log could not show subjects/dice on many live reports ("move",
// "stand up" without a name; numeric ids; no dice). Root cause suspected:
// the C++ parser was written against ffb-common field names that do not all
// match what the server actually sends. This tool removes the guesswork: it
// spectates a LIVE match as the client does (SpectatorSession:
// coach+password challenge -> LZString WS -> clientJoin) and dumps
//
//   <out>/game_<id>_<ts>.jsonl        one raw report JSON per line
//                                     (every object of serverModelSync
//                                      reportList.reports[])
//   <out>/game_<id>_<ts>_state.json   the full raw serverGameState payload
//                                     (team objects incl. logoUrl/baseIconPath)
//
// The .jsonl lines are the GROUND TRUTH for the reportparser: after a live
// capture, align reportparser.cpp field-by-field against this file.
//
// Usage:
//   ./reports_capturer --game <GAME_ID> [--duration <sec>] [--out <dir>]
//                      [--credentials <path>]
//
// Credentials default to the same file the client uses (keep 0600):
//   ~/.config/fumbbl-reborn/creds   (lines: coach=... / password=...)
//
// Exit code: 0 if joined + game state received, 1 otherwise.
// =============================================================================
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QJsonArray>
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

// The MODEL CHANGES are the ground truth for the move trail
// (actingPlayerSetPlayerId / actingPlayerSetPlayerAction /
// fieldModelSetPlayerCoordinate) and for the post-TD repositioning: every
// change object is dumped raw, one per line.
static void logChange(const QJsonObject &c)
{
    const QString id = c.value(QStringLiteral("modelChangeId")).toString();
    if (id == QLatin1String("actingPlayerSetPlayerId")
        || id == QLatin1String("actingPlayerSetPlayerAction")
        || id == QLatin1String("gameSetTurnMode")
        || id == QLatin1String("gameSetHalf")
        || id == QLatin1String("fieldModelSetPlayerState")
        || id.contains(QLatin1String("turnEnd")))
        logLine(QStringLiteral("CHANGE    ") + QJsonDocument(c).toJson(QJsonDocument::Compact));
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("fumbbl-qt-reports-capturer");

    QString credentialsPath = CredentialStore::credentialsPath();
    int gameId = 0;
    int durationSec = 300; // a full action needs ~1-2 min; default 5 min
    QString outDir = QDir::homePath() + QStringLiteral("/FUMBBL-live-captures");

    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QLatin1String("--game") && i + 1 < args.size())
            gameId = args[++i].toInt();
        else if (args[i] == QLatin1String("--credentials") && i + 1 < args.size())
            credentialsPath = args[++i];
        else if (args[i] == QLatin1String("--duration") && i + 1 < args.size())
            durationSec = args[++i].toInt();
        else if (args[i] == QLatin1String("--out") && i + 1 < args.size())
            outDir = args[++i];
    }

    if (gameId == 0) {
        std::printf("usage: reports_capturer --game <GAME_ID> "
                    "[--duration <sec>] [--out <dir>] [--credentials <path>]\n");
        return 2;
    }

    // --- Credentials (same line file the client uses; password hashed
    // in-process and never printed) -----------------------------------------
    QString coach;
    QString password;
    QByteArray fileMd5;
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
            else if (key == QLatin1String("password_md5")) {
                fileMd5 = QByteArray::fromHex(value.toLatin1());
                if (fileMd5.size() != 16)
                    fileMd5.clear();
            }
        }
    }
    if (coach.isEmpty()) {
        logLine(QStringLiteral("FATAL: credentials file must contain coach= (and either "
                                "password= or an OS keychain entry)"));
        return 2;
    }
    logLine(QStringLiteral("Using credentials for coach '%1', game %2")
                .arg(coach)
                .arg(gameId));

    // --- Output files ---------------------------------------------------------
    QDir().mkpath(outDir);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString jsonlPath = QStringLiteral("%1/game_%2_%3.jsonl").arg(outDir, QString::number(gameId), stamp);
    const QString statePath = QStringLiteral("%1/game_%2_%3_state.json").arg(outDir, QString::number(gameId), stamp);
    const QString changesPath = QStringLiteral("%1/game_%2_%3_changes.jsonl").arg(outDir, QString::number(gameId), stamp);
    QFile jsonlFile(jsonlPath);
    if (!jsonlFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        logLine(QStringLiteral("FATAL: cannot write %1").arg(jsonlPath));
        return 2;
    }
    QFile changesFile(changesPath);
    if (!changesFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        logLine(QStringLiteral("FATAL: cannot write %1").arg(changesPath));
        return 2;
    }

    logLine(QStringLiteral("Capturing live reports for game %1 for %2 s")
                .arg(gameId)
                .arg(durationSec));
    logLine(QStringLiteral("  reports -> ") + jsonlPath);
    logLine(QStringLiteral("  state   -> ") + statePath);
    logLine(QStringLiteral("  changes -> ") + changesPath);

    // --- Session ----------------------------------------------------------------
    SpectatorSession session;
    bool gotGameState = false;
    int modelSyncs = 0;
    int reportCount = 0;
    QHash<QString, int> reportCounts; // reportId -> count (the summary at the end)

    QObject::connect(&session, &SpectatorSession::progress,
                     [&](const QString &m) { logLine(QStringLiteral("PROGRESS  ") + m); });
    QObject::connect(&session, &SpectatorSession::fatal,
                     [&](const QString &m) { logLine(QStringLiteral("FATAL     ") + m); });
    QObject::connect(&session, &SpectatorSession::gameStateReceived,
                     [&](const QJsonObject &game) {
                         gotGameState = true;
                         QFile f(statePath);
                         if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                             f.write(QJsonDocument(game).toJson(QJsonDocument::Indented));
                             logLine(QStringLiteral("DUMPED    serverGameState -> ") + statePath);
                         } else {
                             logLine(QStringLiteral("DUMP FAIL ") + statePath);
                         }
                     });
    int changeCount = 0;
    QHash<QString, int> changeCounts; // modelChangeId -> count
    QObject::connect(&session, &SpectatorSession::modelSyncReceived,
                     [&](const QJsonArray &changes, const QJsonArray &reports) {
                         ++modelSyncs;
                         for (const QJsonValue &cv : changes) {
                             const QJsonObject change = cv.toObject();
                             if (change.isEmpty())
                                 continue;
                             // One RAW change per line — the ground truth for
                             // the move trail + the post-TD repositioning.
                             changesFile.write(QJsonDocument(change).toJson(QJsonDocument::Compact));
                             changesFile.write("\n");
                             ++changeCount;
                             const QString id = change.value(QStringLiteral("modelChangeId")).toString();
                             changeCounts[id]++;
                             logChange(change);
                         }
                         for (const QJsonValue &rv : reports) {
                             const QJsonObject report = rv.toObject();
                             if (report.isEmpty())
                                 continue;
                             // One RAW report per line — the ground truth for
                             // the parser. Compact JSON, no re-formatting.
                             jsonlFile.write(QJsonDocument(report).toJson(QJsonDocument::Compact));
                             jsonlFile.write("\n");
                             ++reportCount;
                             const QString id = report.value(QStringLiteral("reportId")).toString();
                             reportCounts[id]++;
                             if (id == QLatin1String("turnEnd") || id == QLatin1String("playerAction"))
                                 logLine(QStringLiteral("REPORT    ") + QJsonDocument(report).toJson(QJsonDocument::Compact));
                         }
                         jsonlFile.flush();
                         changesFile.flush();
                     });

    // Password in the file? use it. Otherwise the file is coach-only (the
    // app migrated it): the MD5 comes from the OS keychain (async).
    QElapsedTimer watch;
    QTimer watchdog;
    QObject::connect(&watchdog, &QTimer::timeout, [&]() {
        session.disconnect();

        std::printf("\n--- summary (%.1f s) ----------------------------------------\n",
                    watch.elapsed() / 1000.0);
        std::printf("  serverJoin         : %s\n", session.isJoined() ? "yes" : "no");
        std::printf("  serverGameState    : %s\n", gotGameState ? "yes" : "no");
        std::printf("  serverModelSync    : %d\n", modelSyncs);
        std::printf("  reports captured   : %d\n", reportCount);
        std::printf("  changes captured   : %d\n", changeCount);
        std::printf("  change id histogram:\n");
        for (auto it = changeCounts.cbegin(); it != changeCounts.cend(); ++it)
            std::printf("    %-34s %4d\n", it.key().toUtf8().constData(), it.value());
        std::printf("  report id histogram:\n");
        for (auto it = reportCounts.cbegin(); it != reportCounts.cend(); ++it)
            std::printf("    %-28s %4d\n", it.key().toUtf8().constData(), it.value());
        const bool ok = session.isJoined() && gotGameState;
        std::printf("  RESULT: %s\n",
                    ok ? "CAPTURE OK" : "CAPTURE INCOMPLETE (join/state missing)");
        app.exit(ok ? 0 : 1);
    });
    watchdog.setSingleShot(true);

    auto startSession = [&](const QByteArray &pwdMd5) {
        watch.start();
        session.connectAsSpectator(coach, pwdMd5, gameId);
        watchdog.start(durationSec * 1000);
    };

    const QByteArray md5 = password.isEmpty()
                              ? fileMd5
                              : fumbbl::PasswordChallenge::encodePassword(password);
    if (!md5.isEmpty()) {
        startSession(md5);
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
