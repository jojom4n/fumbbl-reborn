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
//   * Join sequence: clientRequestVersion (once) -> clientJoin -> serverJoin.
//     NEVER send clientReady (does not exist in the protocol).
//   * Session token is one-time: the caller must inject a fresh one per
//     connection (see SpectatorSession).
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

    // Configure the join that fires automatically right after open.
    void configureJoin(const QString &coach, const QString &sessionToken, int gameId,
                       bool spectator);
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
    void sendJoin(); // uses the configured join
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

    // A serverStatus arrived (e.g. ERROR_WRONG_PASSWORD when the join's
    // one-time session token was rejected). The session layer turns this
    // into a visible error + a retry with a fresh token.
    void statusReceived(const QString &status, const QString &message);
    // The socket (re)opened but the previous join already consumed its
    // one-time session token: the session layer must fetch a fresh token
    // and re-join (FumbblWs deliberately does NOT re-send the stale join).
    void reopenNeedsToken();

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
    void sendJoinInternal();

    QWebSocket *m_ws = nullptr;
    QTimer *m_pingTimer = nullptr;
    QTimer *m_reconnectTimer = nullptr;

    // Configured join (mirrors FumbblWebSocket fields).
    QString m_coach;
    QString m_sessionToken;
    int m_gameId = 0;
    bool m_spectator = true;
    // True once a join was sent. A later socket open is a RECONNECT:
    // re-sending the stored join would replay a CONSUMED one-time session
    // token (the server would reject it), so reopenNeedsToken() is emitted
    // instead.
    bool m_joinSent = false;
    // True once clientRequestVersion was sent on the CURRENT socket. The
    // protocol requires it before every clientJoin; it must go out once per
    // socket (reset in handleOpen).
    bool m_versionSent = false;
    bool m_manualDisconnect = false;
    int m_reconnectAttempts = 0;

    QElapsedTimer m_openedAt;
};
