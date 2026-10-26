// =============================================================================
// reports_capturer — live raw-report capture (Mockup F round 9, step A)
//
// The round-8 action log could not show subjects/dice on many live reports
// ("move", "stand up" without a name; numeric ids; no dice). Root cause
// suspected: the C++ parser was written against ffb-common field names that
// do not all match what the server actually sends. This tool removes the
// guesswork: it spectates a LIVE match as the client does (SpectatorSession:
// OAuth2 -> one-time token -> LZString WS -> clientJoin) and dumps
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
// Credentials default to the same file the client uses:
//   ~/.local/share/com.saruman.fumbbl-reborn/fumbbl-credentials.json
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
    app.setApplicationName("fumbbl-qt-reports-capturer");

    QString credentialsPath = QDir::homePath() +
        QStringLiteral("/.local/share/com.saruman.fumbbl-reborn/fumbbl-credentials.json");
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

    // --- Credentials (same file the client uses) ----------------------------
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

    // --- Output files ---------------------------------------------------------
    QDir().mkpath(outDir);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString jsonlPath = QStringLiteral("%1/game_%2_%3.jsonl").arg(outDir, QString::number(gameId), stamp);
    const QString statePath = QStringLiteral("%1/game_%2_%3_state.json").arg(outDir, QString::number(gameId), stamp);
    QFile jsonlFile(jsonlPath);
    if (!jsonlFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        logLine(QStringLiteral("FATAL: cannot write %1").arg(jsonlPath));
        return 2;
    }

    logLine(QStringLiteral("Capturing live reports for game %1 for %2 s")
                .arg(gameId)
                .arg(durationSec));
    logLine(QStringLiteral("  reports -> ") + jsonlPath);
    logLine(QStringLiteral("  state   -> ") + statePath);

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
    QObject::connect(&session, &SpectatorSession::modelSyncReceived,
                     [&](const QJsonArray &, const QJsonArray &reports) {
                         ++modelSyncs;
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
                         }
                         jsonlFile.flush();
                     });

    QElapsedTimer watch;
    watch.start();
    session.connectAsSpectator(clientId, clientSecret, username, gameId);

    QTimer watchdog;
    QObject::connect(&watchdog, &QTimer::timeout, [&]() {
        session.disconnect();

        std::printf("\n--- summary (%.1f s) ----------------------------------------\n",
                    watch.elapsed() / 1000.0);
        std::printf("  serverJoin         : %s\n", session.isJoined() ? "yes" : "no");
        std::printf("  serverGameState    : %s\n", gotGameState ? "yes" : "no");
        std::printf("  serverModelSync    : %d\n", modelSyncs);
        std::printf("  reports captured   : %d\n", reportCount);
        std::printf("  report id histogram:\n");
        for (auto it = reportCounts.cbegin(); it != reportCounts.cend(); ++it)
            std::printf("    %-28s %4d\n", it.key().toUtf8().constData(), it.value());
        const bool ok = session.isJoined() && gotGameState;
        std::printf("  RESULT: %s\n",
                    ok ? "CAPTURE OK" : "CAPTURE INCOMPLETE (join/state missing)");
        app.exit(ok ? 0 : 1);
    });
    watchdog.setSingleShot(true);
    watchdog.start(durationSec * 1000);

    return app.exec();
}
