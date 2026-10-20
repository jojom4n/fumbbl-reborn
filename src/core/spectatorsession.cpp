// =============================================================================
// SpectatorSession — implementation
// =============================================================================
#include "core/spectatorsession.h"

#include "core/fumbblauth.h"
#include "core/fumbblws.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

SpectatorSession::SpectatorSession(QObject *parent)
    : QObject(parent)
{
    m_auth = new FumbblAuth(this);
    m_ws = new FumbblWs(this);

    connect(m_auth, &FumbblAuth::accessTokenReady, this, &SpectatorSession::onAccessToken);
    connect(m_auth, &FumbblAuth::sessionTokenReady, this, &SpectatorSession::onSessionToken);
    connect(m_auth, &FumbblAuth::failed, this, &SpectatorSession::onAuthFailed);

    connect(m_ws, &FumbblWs::opened, this, [this]() {
        emit progress(QStringLiteral("WebSocket open (ws://fumbbl.com:22223/command)"));
    });
    connect(m_ws, &FumbblWs::statusReceived, this, [this](const QString &status, const QString &message) {
        emit statusReceived(status, message);
        emit progress(QStringLiteral("serverStatus: %1 %2").arg(status, message));
        // A status while not joined = the join was rejected (e.g. the
        // one-time token was already consumed or the server-side auth check
        // failed). Retry with a fresh token — never replay the old join.
        if (!m_joined)
            retryJoinWithFreshToken(QStringLiteral("serverStatus %1").arg(status));
    });
    connect(m_ws, &FumbblWs::reopenNeedsToken, this, [this]() {
        // The socket re-opened after a network blip; its join token was
        // already consumed. Fetch a fresh one and re-join.
        emit reopenScheduled();
        emit progress(QStringLiteral("Socket re-opened — fetching a fresh one-time token to re-join …"));
        m_auth->fetchSessionToken();
    });
    // Watchdog: clientJoin sent but no serverJoin (the server-side FUMBBL
    // auth check can fail or hang silently).
    m_joinWatchdog = new QTimer(this);
    m_joinWatchdog->setSingleShot(true);
    connect(m_joinWatchdog, &QTimer::timeout, this, &SpectatorSession::onJoinWatchdog);
    connect(m_ws, &FumbblWs::sending, this, [this](const QString &id, const QJsonObject &)
            { if (id == QLatin1String("clientJoin") && !m_joined) m_joinWatchdog->start(kJoinWatchdogMs); });
    connect(m_ws, &FumbblWs::serverVersionReceived, this,
            [this](const QJsonObject &p) {
                emit progress(QStringLiteral("serverVersion: %1")
                                  .arg(p.value("serverVersion").toString()));
                emit serverVersionReceived(p);
            });
    connect(m_ws, &FumbblWs::joined, this, [this](const QJsonObject &p) {
        m_joined = true;
        m_joinRetries = 0;
        m_joinWatchdog->stop();
        emit progress(QStringLiteral("serverJoin: coach=%1 mode=%2 game=%3")
                          .arg(p.value("coach").toString())
                          .arg(p.value("clientMode").toString())
                          .arg(p.value("gameId").toVariant().toString()));
        emit joined(p);
    });
    connect(m_ws, &FumbblWs::gameStateReceived, this, [this](const QJsonObject &game) {
        const QJsonObject home = game.value("teamHome").toObject();
        const QJsonObject away = game.value("teamAway").toObject();
        emit progress(QStringLiteral(
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
            emit progress(QStringLiteral("serverModelSync: %1 changes, %2 reports")
                              .arg(changes.size())
                              .arg(reports.size()));
        emit modelSyncReceived(changes, reports);
    });
    connect(m_ws, &FumbblWs::gameTimeReceived, this, [this](qint64 gameMs, qint64 turnMs) {
        // turnTime is ELAPSED ms (0..30000); UI shows 30 - elapsed.
        const int remaining = qMax(0, 30 - static_cast<int>(turnMs / 1000));
        emit progress(QStringLiteral("serverGameTime: game=%1ms turn elapsed=%2ms (remaining "
                                     "%3s)")
                          .arg(gameMs)
                          .arg(turnMs)
                          .arg(remaining));
        emit gameTimeReceived(gameMs, turnMs);
    });
    connect(m_ws, &FumbblWs::talkReceived, this, [this](const QJsonObject &p) {
        emit progress(QStringLiteral("serverTalk [%1]: %2")
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
        emit progress(QStringLiteral("WebSocket closed: %1 %2").arg(code).arg(reason));
        emit wsClosed(code, reason);
        // A game switch delivered its session token while this (old) socket
        // was still closing: join now that it is gone.
        if (!m_pendingToken.isEmpty()) {
            const QString token = m_pendingToken;
            m_pendingToken.clear();
            m_ws->connectTo(); // join was configured in onSessionToken()
            Q_UNUSED(token)
        }
    });
    connect(m_ws, &FumbblWs::reconnectScheduled, this, [this](int attempt) {
        emit progress(QStringLiteral("Reconnect scheduled (attempt %1)").arg(attempt));
    });
    connect(m_ws, &FumbblWs::errorOccurred, this,
            [this](const QString &e) { emit progress(QStringLiteral("WS error: %1").arg(e)); });
}

SpectatorSession::~SpectatorSession() = default;

void SpectatorSession::connectAsSpectator(const QString &clientId, const QString &clientSecret,
                                          const QString &username, int gameId)
{
    m_clientId = clientId;
    m_clientSecret = clientSecret;
    m_username = username;
    m_gameId = gameId;
    m_joined = false;
    m_pendingToken.clear();
    m_joinRetries = 0;
    m_joinWatchdog->stop();

    // GAME SWITCH: tear down any existing session first. A still-open socket
    // would swallow the new join (FumbblWs::connectTo() only opens when the
    // socket is Unconnected), leaving the user on the OLD match. abort() is
    // synchronous, so the subsequent connectTo() is guaranteed to connect.
    if (!m_ws->isUnconnected())
        m_ws->abortFromServer();

    emit progress(QStringLiteral("OAuth2 client_credentials -> /api/oauth/token …"));
    m_auth->authenticate(clientId, clientSecret);
}

void SpectatorSession::disconnect()
{
    m_ws->disconnectFromServer();
}

void SpectatorSession::onAccessToken(const QString &token)
{
    Q_UNUSED(token)
    emit progress(QStringLiteral("access_token received — fetching fresh session token "
                                 "(one-time use) …"));
    m_auth->fetchSessionToken();
}

void SpectatorSession::onSessionToken(const QString &token)
{
    emit progress(QStringLiteral("session_token received — joining as spectator (game %1) …")
                      .arg(m_gameId));
    // Fresh token + coach + game.
    m_ws->configureJoin(m_username, token, m_gameId, /*spectator=*/true);
    if (m_ws->isOpen()) {
        // Re-join on the ALREADY-OPEN socket (reconnect or join retry):
        // send the join now instead of opening a new socket.
        m_ws->sendJoin();
        return;
    }
    if (m_ws->isUnconnected()) {
        // First join: the WS fires clientRequestVersion -> clientJoin
        // automatically right after open.
        m_ws->connectTo();
    } else {
        // Defensive: the old socket is still draining. The closed() handler
        // fires the join once it is actually gone.
        m_pendingToken = token;
    }
}

// Joins were sent but the server never confirmed (or rejected) them: the
// one-time token may have been consumed or the server-side FUMBBL auth
// check may have failed/hung. Fetch a FRESH token and re-join. Bounded:
// after kMaxJoinRetries give up with a visible error.
void SpectatorSession::onJoinWatchdog()
{
    if (m_joined)
        return;
    retryJoinWithFreshToken(QStringLiteral("join not confirmed after %1 ms").arg(kJoinWatchdogMs));
}

void SpectatorSession::retryJoinWithFreshToken(const QString &reason)
{
    if (m_joined)
        return;
    if (m_joinRetries >= kMaxJoinRetries) {
        emit fatal(QStringLiteral("Cannot join game %1: the server did not confirm the join "
                                  "after %2 attempts (%3). Check the connection and try again.")
                       .arg(m_gameId)
                       .arg(kMaxJoinRetries + 1)
                       .arg(reason));
        return;
    }
    ++m_joinRetries;
    emit progress(QStringLiteral("Join not confirmed (%1) — retry %2/%3 with a fresh session token …")
                      .arg(reason)
                      .arg(m_joinRetries)
                      .arg(kMaxJoinRetries));
    m_auth->fetchSessionToken();
}

void SpectatorSession::onAuthFailed(const QString &step, const QString &error)
{
    emit fatal(QStringLiteral("Authentication failed (%1): %2").arg(step, error));
}
