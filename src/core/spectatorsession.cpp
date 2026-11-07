// =============================================================================
// SpectatorSession — implementation
// =============================================================================
#include "core/spectatorsession.h"

#include "core/fumbblws.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

SpectatorSession::SpectatorSession(QObject *parent)
    : QObject(parent)
{
    m_ws = new FumbblWs(this);

    connect(m_ws, &FumbblWs::opened, this, [this]() {
        progressIfLive(QStringLiteral("WebSocket open (ws://fumbbl.com:22223/command)"));
    });
    connect(m_ws, &FumbblWs::statusReceived, this, [this](const QString &status, const QString &message) {
        emit statusReceived(status, message);
        progressIfLive(QStringLiteral("serverStatus: %1 %2").arg(status, message));
        // A serverStatus while not joined = the join was REJECTED by the
        // server ("Wrong Password", "Unknown Coach", "Unknown Game Id", ...
        // see ServerStatus.java in ffb-common). The login plan (04/nov)
        // forbids auto-retrying credentials — the server limits repeated
        // failures — so a rejection stops immediately and is fatal: the
        // user re-enters in the Spectate dialog. Only the SILENT watchdog
        // timeout (no status at all) still retries with a fresh challenge.
        if (!m_joined) {
            m_joinWatchdog->stop();
            const QString why = message.isEmpty() ? status
                                                  : status + QStringLiteral(" — ") + message;
            // Report the rejection FIRST: the fatal consumes the in-flight
            // verification state (MainWindow shows the message and clears
            // m_authenticating). Only then kill the socket: the server's
            // close for this connection (a 1000, usually in the same burst
            // as the status) is processed RE-ENTRANTLY by abort() — if the
            // fatal had not consumed the state first, that close would be
            // reported as "Connection closed" and cancel the verification
            // the user is about to retry.
            emit fatal(QStringLiteral("The server rejected the join: %1. Check your FUMBBL "
                                      "credentials (Spectate Game… dialog) and try again.")
                           .arg(why));
            m_ws->abortFromServer();
        }
    });
    // Watchdog: login sent but no serverJoin (the server-side FUMBBL auth
    // check can fail or hang silently).
    m_joinWatchdog = new QTimer(this);
    m_joinWatchdog->setSingleShot(true);
    connect(m_joinWatchdog, &QTimer::timeout, this, &SpectatorSession::onJoinWatchdog);
    connect(m_ws, &FumbblWs::sending, this, [this](const QString &id, const QJsonObject &)
            { if (id == QLatin1String("clientJoin") && !m_joined) m_joinWatchdog->start(kJoinWatchdogMs); });
    connect(m_ws, &FumbblWs::serverVersionReceived, this,
            [this](const QJsonObject &p) {
                progressIfLive(QStringLiteral("serverVersion: %1")
                                 .arg(p.value("serverVersion").toString()));
                emit serverVersionReceived(p);
            });
    connect(m_ws, &FumbblWs::joined, this, [this](const QJsonObject &p) {
        m_joined = true;
        m_joinRetries = 0;
        m_joinWatchdog->stop();
        progressIfLive(QStringLiteral("serverJoin: coach=%1 mode=%2 game=%3")
                           .arg(p.value("coach").toString())
                           .arg(p.value("clientMode").toString())
                           .arg(p.value("gameId").toVariant().toString()));
        emit joined(p);
    });
    connect(m_ws, &FumbblWs::gameStateReceived, this, [this](const QJsonObject &game) {
        const QJsonObject home = game.value("teamHome").toObject();
        const QJsonObject away = game.value("teamAway").toObject();
        progressIfLive(QStringLiteral(
                           "serverGameState: %1 (%2) vs %3 (%4), half=%5, playing=%6")
                           .arg(home.value("teamName").toString())
                           .arg(home.value("race").toString())
                           .arg(away.value("teamName").toString())
                           .arg(away.value("race").toString())
                           .arg(game.value("half").toVariant().toString())
                           .arg(game.value("homePlaying").toVariant().toString()));
        emit gameStateReceived(game);
    });
    connect(m_ws, &FumbblWs::modelSyncReceived, this, [this](const QJsonArray &changes,
                                                             const QJsonArray &reports) {
        if (changes.size() + reports.size() > 0)
            progressIfLive(QStringLiteral("serverModelSync: %1 changes, %2 reports")
                               .arg(changes.size()).arg(reports.size()));
        emit modelSyncReceived(changes, reports);
    });
    connect(m_ws, &FumbblWs::gameTimeReceived, this, [this](qint64 gameMs, qint64 turnMs) {
        // turnTime is ELAPSED ms (0..30000); UI shows 30 - elapsed.
        const int remaining = qMax(0, 30 - static_cast<int>(turnMs / 1000));
        progressIfLive(QStringLiteral("serverGameTime: game=%1ms turn elapsed=%2ms (remaining "
                                      "%3s)")
                           .arg(gameMs)
                           .arg(turnMs)
                           .arg(remaining));
        emit gameTimeReceived(gameMs, turnMs);
    });
    connect(m_ws, &FumbblWs::talkReceived, this, [this](const QJsonObject &p) {
        progressIfLive(QStringLiteral("serverTalk [%1]: %2")
                           .arg(p.value("player").toString())
                           .arg(p.value("message").toString()));
        emit talkReceived(p);
    });
    connect(m_ws, &FumbblWs::pongReceived, this, [this](qint64 ts) {
        emit pongReceived(ts);
    });
    connect(m_ws, &FumbblWs::sending, this, [this](const QString &id, const QJsonObject &p) {
        emit protocolSending(id, p);
    });
    connect(m_ws, &FumbblWs::received, this, [this](const QString &id, const QJsonObject &p) {
        emit protocolReceived(id, p);
    });
    connect(m_ws, &FumbblWs::closed, this, [this](int code, const QString &reason) {
        // Client-initiated close (logout / login teardown): the UI already
        // set its own status — "WebSocket closed: 1000" would clobber it.
        if (!m_clientClosed)
            emit progress(QStringLiteral("WebSocket closed: %1 %2").arg(code).arg(reason));
        emit wsClosed(code, reason);
    });
    connect(m_ws, &FumbblWs::reconnectScheduled, this, [this](int attempt) {
        emit progress(QStringLiteral("Reconnect scheduled (attempt %1)").arg(attempt));
    });
    connect(m_ws, &FumbblWs::errorOccurred, this,
            [this](const QString &e) { emit progress(QStringLiteral("WS error: %1").arg(e)); });
}

SpectatorSession::~SpectatorSession() = default;

void SpectatorSession::progressIfLive(const QString &message)
{
    if (m_clientClosed)
        return; // session ended on our side: don't narrate into the UI
    emit progress(message);
}

void SpectatorSession::connectAsSpectator(const QString &coach, const QByteArray &pwdMd5,
                                          int gameId)
{
    m_gameId = gameId;
    m_joined = false;
    m_clientClosed = false;
    m_joinRetries = 0;
    m_joinWatchdog->stop();

    // GAME SWITCH: tear down any existing session first. A still-open socket
    // would swallow the new login (FumbblWs::connectTo() only opens when the
    // socket is Unconnected), leaving the user on the OLD match. abort() is
    // synchronous, so the subsequent connectTo() is guaranteed to connect.
    if (!m_ws->isUnconnected())
        m_ws->abortFromServer();

    emit progress(QStringLiteral("Logging in as coach %1 (game %2): version -> challenge -> join …")
                      .arg(coach)
                      .arg(gameId));
    m_ws->configureJoin(coach, pwdMd5, gameId, /*spectator=*/true);
    // On open, FumbblWs sends clientRequestVersion + clientPasswordChallenge;
    // the join fires when the server's challenge arrives.
    m_ws->connectTo();
}

void SpectatorSession::disconnect()
{
    m_clientClosed = true;
    m_ws->disconnectFromServer();
}

void SpectatorSession::onJoinWatchdog()
{
    if (m_joined)
        return;
    retryLogin(QStringLiteral("join not confirmed after %1 ms").arg(kJoinWatchdogMs));
}

// The join was not confirmed (SILENT watchdog timeout only — a rejected
// join arrives as a serverStatus and is fatal, see the statusReceived
// handler): retry with a FRESH password challenge on the current socket.
// Bounded: after kMaxJoinRetries give up with a visible error.
void SpectatorSession::retryLogin(const QString &reason)
{
    if (m_joined)
        return;
    if (m_joinRetries >= kMaxJoinRetries) {
        // Report first, then kill the socket — see the rejection branch:
        // abort() re-entrantly processes the pending close, which must
        // land AFTER the fatal consumed the verification state.
        emit fatal(QStringLiteral("Cannot join game %1: the server did not confirm the join "
                                  "after %2 attempts (%3). Check your FUMBBL credentials and "
                                  "try again.")
                       .arg(m_gameId)
                       .arg(kMaxJoinRetries + 1)
                       .arg(reason));
        m_ws->abortFromServer();
        return;
    }
    ++m_joinRetries;
    emit progress(QStringLiteral("Join not confirmed (%1) — retry %2/%3 with a fresh challenge …")
                      .arg(reason)
                      .arg(m_joinRetries)
                      .arg(kMaxJoinRetries));
    m_joinWatchdog->start(kJoinWatchdogMs);
    m_ws->retryLogin();
}
