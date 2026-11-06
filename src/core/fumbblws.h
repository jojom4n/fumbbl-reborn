// =============================================================================
// FumbblWs — FUMBBL command WebSocket client
//
// Faithful port of src/services/fumbblWebSocket.ts. Protocol facts (verified
// against the official ffbclient / ffb Java server — see memory-bank):
//
//   * URL: ws://fumbbl.com:22223/command  (22224 = prod)
//   * ALL frames are LZString-UTF16 compressed (compressToUTF16 out,
//     decompressFromUTF16 in; if decompression yields empty, treat the
//     payload as raw JSON — same fallback as the TS client).
//   * Ping: clientPing every 2000 ms, FIRST PING IMMEDIATE on open
//     (official client.ini: client.ping.interval=2000; the server's
//     SessionTimeoutTask kills sessions silent for 10 s).
//   * Login sequence (official coach+password flow): clientRequestVersion
//     (once) -> clientPasswordChallenge {coach} -> server sends
//     serverPasswordChallenge {challenge: hex} -> client computes
//     R = HMAC-MD5(MD5(password), challenge) and sends clientJoin
//     {coach, password: hex(R)} -> serverJoin. NEVER send clientReady
//     (does not exist in the protocol). A re-opened socket re-runs the
//     whole sequence (fresh challenge, fresh join) — nothing is replayed.
//   * Reconnect: exponential backoff 3000 ms * 2^(n-1), capped at 2^5,
//     unlimited attempts; a clean close (1000) does not reconnect.
// =============================================================================
#pragma once

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkReply;
class QTimer;
class QWebSocket;

class FumbblWs : public QObject
{
    Q_OBJECT

public:
    explicit FumbblWs(QObject *parent = nullptr);
    ~FumbblWs() override;

    static QString defaultUrl(); // "ws://fumbbl.com:22223/command"

    // Configure the login that fires automatically right after open:
    // clientRequestVersion -> clientPasswordChallenge -> (server challenge)
    // -> clientJoin with the computed challenge response.
    // pwdMd5: 16-byte binary MD5 of the clear-text password (the HMAC key;
    // the plaintext is never held here).
    void configureJoin(const QString &coach, const QByteArray &pwdMd5, int gameId,
                       bool spectator);
    // Re-run the challenge on the CURRENT open socket (join retry): the
    // server issues a fresh challenge and a new join follows.
    void retryLogin();
    void connectTo();
    void disconnectFromServer();
    // Immediate, synchronous teardown (game switching): abort() the socket so
    // it is Unconnected at once — a subsequent connectTo() then actually
    // connects instead of returning early against the old close handshake.
    void abortFromServer();

    bool isOpen() const;
    bool isUnconnected() const;

    // Outgoing commands (LZString-compressed internally).
    void sendVersionRequest();
    void sendChat(const QString &message); // clientTalk
    void sendAction(const QString &action, const QStringList &params = {}); // clientAction
    void sendCloseSession();

signals:
    void opened();
    void closed(int code, const QString &reason);
    void reconnectScheduled(int attempt);
    void errorOccurred(const QString &message);

    // Raw protocol visibility (for the Protocol Log dock / debugging).
    void sending(const QString &netCommandId, const QJsonObject &payload);
    void received(const QString &netCommandId, const QJsonObject &payload);

    // A serverStatus arrived (e.g. when the join's challenge response was
    // rejected). The session layer turns this into a visible error + a
    // retry with a fresh challenge.
    void statusReceived(const QString &status, const QString &message);

    // Routed server commands (subset relevant to a client; the rest only
    // surface via `received`).
    void serverVersionReceived(const QJsonObject &payload);
    void userSettingsReceived(const QJsonObject &payload);
    void joined(const QJsonObject &payload);
    void left(const QJsonObject &payload);
    void gameStateReceived(const QJsonObject &game);
    void modelSyncReceived(const QJsonArray &changes, const QJsonArray &reports);
    void gameTimeReceived(qint64 gameTimeMs, qint64 turnTimeMs);
    void talkReceived(const QJsonObject &payload);
    void soundReceived(const QJsonObject &payload);
    void adminMessageReceived(const QJsonObject &payload);
    void socketClosedByServer(const QJsonObject &payload);
    void pongReceived(qint64 timestamp);

private:
    void handleOpen();
    void handleClose();
    void handleMessage(const QString &raw);
    void scheduleReconnect();
    void sendRaw(const QJsonObject &message);
    void startPingTimer();
    void stopPingTimer();
    void sendPing();
    void sendLoginSequence();      // version request + password challenge
    void sendPasswordChallenge();  // clientPasswordChallenge
    void sendJoinWithResponse(const QString &response); // clientJoin

    QWebSocket *m_ws = nullptr;
    QTimer *m_pingTimer = nullptr;
    QTimer *m_reconnectTimer = nullptr;

    // Configured login (mirrors FumbblWebSocket fields).
    QString m_coach;
    QByteArray m_pwdMd5; // binary MD5 of the password (HMAC key)
    int m_gameId = 0;
    bool m_spectator = true;
    // True once clientRequestVersion was sent on the CURRENT socket. The
    // protocol requires it before the login exchange; it must go out once
    // per socket (reset in handleOpen).
    bool m_versionSent = false;
    bool m_manualDisconnect = false;
    int m_reconnectAttempts = 0;

    QElapsedTimer m_openedAt;
};
