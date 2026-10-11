// =============================================================================
// SpectatorSession — connects to a FUMBBL match as spectator
//
// Orchestrates exactly the flow of the TS client (fumbblService.ts
// connectAsSpectator):
//
//   1. OAuth2 client_credentials        -> access token
//   2. POST /api/auth/getToken (fresh)  -> ONE-TIME session token
//   3. FumbblWs.connect() -> on open: ping(2s, immediate) ->
//      clientRequestVersion -> clientJoin(spectator) -> serverJoin ->
//      serverGameState -> serverModelSync/serverGameTime/serverTalk...
// =============================================================================
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class FumbblAuth;
class FumbblWs;

class SpectatorSession : public QObject
{
    Q_OBJECT

public:
    explicit SpectatorSession(QObject *parent = nullptr);
    ~SpectatorSession() override;

    // Starts the full flow (async; progress via the signals below).
    void connectAsSpectator(const QString &clientId, const QString &clientSecret,
                            const QString &username, int gameId);
    void disconnect();

    FumbblWs *ws() const { return m_ws; }
    bool joined() const { return m_joined; }

signals:
    void progress(const QString &message);
    void fatal(const QString &message);

    // Re-exports of the WebSocket protocol surface for UI/tools.
    void serverVersionReceived(const QJsonObject &payload);
    void joined(const QJsonObject &payload);
    void gameStateReceived(const QJsonObject &game);
    void modelSyncReceived(const QJsonArray &changes, const QJsonArray &reports);
    void gameTimeReceived(qint64 gameTimeMs, qint64 turnTimeMs);
    void talkReceived(const QJsonObject &payload);
    void pongReceived(qint64 timestamp);
    void protocolSending(const QString &netCommandId, const QJsonObject &payload);
    void protocolReceived(const QString &netCommandId, const QJsonObject &payload);
    void wsClosed(int code, const QString &reason);

private slots:
    void onAccessToken(const QString &token);
    void onSessionToken(const QString &token);
    void onAuthFailed(const QString &step, const QString &error);

private:
    FumbblAuth *m_auth = nullptr;
    FumbblWs *m_ws = nullptr;
    QString m_clientId;
    QString m_clientSecret;
    QString m_username;
    int m_gameId = 0;
    bool m_joined = false;
};
