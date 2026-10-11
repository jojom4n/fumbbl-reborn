// =============================================================================
// SpectatorSession — implementation
// =============================================================================
#include "core/spectatorsession.h"

#include "core/fumbblauth.h"
#include "core/fumbblws.h"

#include <QJsonArray>
#include <QJsonObject>

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
    connect(m_ws, &FumbblWs::serverVersionReceived, this,
            [this](const QJsonObject &p) {
                emit progress(QStringLiteral("serverVersion: %1")
                                  .arg(p.value("serverVersion").toString()));
                emit serverVersionReceived(p);
            });
    connect(m_ws, &FumbblWs::joined, this, [this](const QJsonObject &p) {
        m_joined = true;
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
    emit progress(QStringLiteral("session_token received — opening WebSocket and joining as "
                                 "spectator (game %1) …")
                      .arg(m_gameId));
    // Fresh token + coach + game: the WS fires clientRequestVersion ->
    // clientJoin automatically right after open.
    m_ws->configureJoin(m_username, token, m_gameId, /*spectator=*/true);
    m_ws->connectTo();
}

void SpectatorSession::onAuthFailed(const QString &step, const QString &error)
{
    emit fatal(QStringLiteral("Authentication failed (%1): %2").arg(step, error));
}
