// =============================================================================
// SpectatorSession — connects to a FUMBBL match as spectator
//
// Orchestrates the official coach+password login flow (same as the Java
// client's LoginLogicModule without JNLP token):
//
//   1. FumbblWs.connect() -> on open: ping(2s, immediate) ->
//      clientRequestVersion -> clientPasswordChallenge {coach}
//   2. server -> serverPasswordChallenge {challenge: hex}
//   3. client -> clientJoin {coach, password: hex(R), gameId, spectator}
//      where R = HMAC-MD5(MD5(password), challenge) (PasswordChallenge)
//   4. server -> serverJoin -> serverGameState ->
//      serverModelSync/serverGameTime/serverTalk...
//
// Reconnect and join retry re-run the challenge (fresh challenge, fresh
// response) — nothing is ever replayed.
// =============================================================================
#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class FumbblWs;
class QTimer;

class SpectatorSession : public QObject
{
    Q_OBJECT

public:
    explicit SpectatorSession(QObject *parent = nullptr);
    ~SpectatorSession() override;

    // Starts the full flow (async; progress via the signals below).
    // pwdMd5: 16-byte binary MD5 of the clear-text password.
    void connectAsSpectator(const QString &coach, const QByteArray &pwdMd5, int gameId);
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
    void joined(const QJsonObject &payload);
    void gameStateReceived(const QJsonObject &game);
    void modelSyncReceived(const QJsonArray &changes, const QJsonArray &reports);
    void gameTimeReceived(qint64 gameTimeMs, qint64 turnMs);
    void talkReceived(const QJsonObject &payload);
    void pongReceived(qint64 timestamp);
    void protocolSending(const QString &netCommandId, const QJsonObject &payload);
    void protocolReceived(const QString &netCommandId, const QJsonObject &payload);
    void wsClosed(int code, const QString &reason);

private slots:
    void onJoinWatchdog();
    void retryLogin(const QString &reason);

private:
    // Protocol narration for the status label, suppressed once WE
    // initiated the close (logout / login teardown): the UI already set
    // its own status and a late frame ("serverGameState …") must not
    // clobber it. The protocol LOG is unaffected (separate signals).
    void progressIfLive(const QString &message);

private:
    FumbblWs *m_ws = nullptr;
    int m_gameId = 0;
    bool m_joined = false;
    // True once WE initiated the close (logout / login-verification
    // teardown): the resulting "WebSocket closed" progress must not
    // clobber the status the UI already set ("Logged out", "Logged in
    // as …"). A SERVER-initiated close still reports as before.
    bool m_clientClosed = false;
    // Join watchdog: if the server never confirms the join (serverJoin) or
    // rejects it (serverStatus), retry with a FRESH challenge. The FUMBBL
    // auth check is async on the server and can fail/hang.
    QTimer *m_joinWatchdog = nullptr;
    int m_joinRetries = 0;
    static constexpr int kMaxJoinRetries = 3;
    static constexpr int kJoinWatchdogMs = 10000;
};
