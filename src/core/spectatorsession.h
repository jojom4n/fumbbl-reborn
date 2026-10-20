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
class QTimer;

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
    // Named isJoined() (NOT joined()) to avoid clashing with the joined()
    // signal below — an overloaded name breaks &SpectatorSession::joined
    // in connect().
    bool isJoined() const { return m_joined; }

signals:
    void progress(const QString &message);
    void fatal(const QString &message);

    // Re-exports of the WebSocket protocol surface for UI/tools.
    void serverVersionReceived(const QJsonObject &payload);
    // serverStatus re-export (visible errors, e.g. a rejected join).
    void statusReceived(const QString &status, const QString &message);
    // The socket re-opened after a reconnect: a fresh one-time token is
    // being fetched to re-join.
    void reopenScheduled();
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
    void onJoinWatchdog();
    void retryJoinWithFreshToken(const QString &reason);

private:
    FumbblAuth *m_auth = nullptr;
    FumbblWs *m_ws = nullptr;
    QString m_clientId;
    QString m_clientSecret;
    QString m_username;
    int m_gameId = 0;
    bool m_joined = false;
    // Session token received while the previous socket was still closing;
    // the join fires from the closed() handler.
    QString m_pendingToken;
    // Join watchdog: if the server never confirms the join (serverJoin) or
    // rejects it (serverStatus), retry with a FRESH one-time session token.
    // The FUMBBL auth check (fumbbl.com/xml:auth) is async on the server and
    // can fail/hang; a consumed token must never be replayed.
    QTimer *m_joinWatchdog = nullptr;
    int m_joinRetries = 0;
    static constexpr int kMaxJoinRetries = 3;
    static constexpr int kJoinWatchdogMs = 10000;
};
