// =============================================================================
// FumbblWs — implementation
// =============================================================================
#include "core/fumbblws.h"

#include "core/lzstring.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketProtocol>

QString FumbblWs::defaultUrl()
{
    return QStringLiteral("ws://fumbbl.com:22223/command");
}

FumbblWs::FumbblWs(QObject *parent)
    : QObject(parent)
{
    m_ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_pingTimer = new QTimer(this);
    m_pingTimer->setInterval(2000); // official client.ini value (5x under the 10s timeout)
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);

    connect(m_ws, &QWebSocket::connected, this, &FumbblWs::handleOpen);
    connect(m_ws, &QWebSocket::disconnected, this, &FumbblWs::handleClose);
    connect(m_ws, &QWebSocket::textMessageReceived, this, &FumbblWs::handleMessage);
    connect(m_ws, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit errorOccurred(m_ws->errorString());
    });
    connect(m_pingTimer, &QTimer::timeout, this, &FumbblWs::sendPing);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]() { connectTo(); });
}

FumbblWs::~FumbblWs() = default;

void FumbblWs::configureJoin(const QString &coach, const QString &sessionToken, int gameId,
                             bool spectator)
{
    m_coach = coach;
    m_sessionToken = sessionToken;
    m_gameId = gameId;
    m_spectator = spectator;
}

void FumbblWs::connectTo()
{
    if (m_ws->state() != QAbstractSocket::UnconnectedState)
        return;
    m_manualDisconnect = false;
    m_reconnectTimer->stop();
    m_ws->open(QUrl(defaultUrl()));
}

void FumbblWs::disconnectFromServer()
{
    m_manualDisconnect = true;
    m_reconnectTimer->stop();
    stopPingTimer();
    if (m_ws->state() != QAbstractSocket::UnconnectedState)
        m_ws->close();
}

bool FumbblWs::isOpen() const
{
    return m_ws->state() == QAbstractSocket::ConnectedState;
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------
void FumbblWs::handleOpen()
{
    m_openedAt.start();
    m_reconnectAttempts = 0;
    emit opened();

    // CRITICAL: the server kills sessions silent for 10 s. Start the ping
    // timer (2 s interval) and send the FIRST ping immediately, exactly like
    // the official Java client's Timer.schedule(task, 0, interval).
    startPingTimer();
    sendPing();

    sendJoinInternal();
}

void FumbblWs::handleClose()
{
    stopPingTimer();
    const int code = static_cast<int>(m_ws->closeCode());
    const QString reason = m_ws->closeReason();
    emit closed(code, reason);

    // Mirrors the TS client: reconnect unless the close was clean (1000) or
    // we asked for it ourselves.
    if (!m_manualDisconnect && code != static_cast<int>(QWebSocketProtocol::CloseCodeNormal))
        scheduleReconnect();
}

void FumbblWs::scheduleReconnect()
{
    // reconnectDelay 3000 ms * 2^(attempt-1), capped at 2^5, unlimited
    // attempts (TS: maxReconnectAttempts = -1).
    ++m_reconnectAttempts;
    const int delayMs = 3000 * (1 << qMin(m_reconnectAttempts - 1, 5));
    emit reconnectScheduled(m_reconnectAttempts);
    m_reconnectTimer->start(delayMs);
}

void FumbblWs::startPingTimer()
{
    stopPingTimer();
    m_pingTimer->start();
}

void FumbblWs::stopPingTimer()
{
    m_pingTimer->stop();
}

void FumbblWs::sendPing()
{
    if (!isOpen())
        return;
    // Official client: clientPing with System.currentTimeMillis().
    sendRaw(QJsonObject{{"netCommandId", QStringLiteral("clientPing")},
                        {"timestamp", QDateTime::currentMSecsSinceEpoch()}});
}

// -----------------------------------------------------------------------------
// Outgoing commands
// -----------------------------------------------------------------------------
void FumbblWs::sendVersionRequest()
{
    sendRaw(QJsonObject{{"netCommandId", QStringLiteral("clientRequestVersion")}});
}

void FumbblWs::sendJoin()
{
    if (m_sessionToken.isEmpty() || m_gameId == 0) {
        emit errorOccurred(QStringLiteral("cannot join: missing session token or game id"));
        return;
    }

    // Mirrors fumbblWebSocket.ts sendJoin() field-for-field, including the
    // empty gameName/teamName strings.
    QJsonObject cmd;
    cmd["netCommandId"] = QStringLiteral("clientJoin");
    cmd["clientMode"] = m_spectator ? QStringLiteral("spectator") : QStringLiteral("player");
    cmd["coach"] = m_coach;
    cmd["password"] = m_sessionToken;
    cmd["gameId"] = m_gameId;
    cmd["gameName"] = QStringLiteral("");
    cmd["teamId"] = QStringLiteral(""); // spectator mode
    cmd["teamName"] = QStringLiteral("");
    sendRaw(cmd);
}

void FumbblWs::sendJoinInternal()
{
    if (m_sessionToken.isEmpty() || m_gameId == 0)
        return; // nothing configured yet

    // CRITICAL: clientRequestVersion before clientJoin (official pattern),
    // sent only once per socket.
    if (!m_versionSent) {
        m_versionSent = true;
        sendVersionRequest();
    }
    sendJoin();
}

void FumbblWs::sendChat(const QString &message)
{
    sendRaw(QJsonObject{{"netCommandId", QStringLiteral("clientTalk")},
                        {"talk", message}});
}

void FumbblWs::sendAction(const QString &action, const QStringList &params)
{
    QJsonObject cmd;
    cmd["netCommandId"] = QStringLiteral("clientAction");
    cmd["action"] = action;
    if (!params.isEmpty())
        cmd["params"] = QJsonArray::fromStringList(params);
    sendRaw(cmd);
}

void FumbblWs::sendCloseSession()
{
    sendRaw(QJsonObject{{"netCommandId", QStringLiteral("clientCloseSession")}});
}

void FumbblWs::sendRaw(const QJsonObject &message)
{
    if (!isOpen()) {
        emit errorOccurred(QStringLiteral("cannot send, socket not open"));
        return;
    }
    const QString json = QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact));
    // CRITICAL: the server only understands LZString-UTF16 frames.
    const QString compressed = fumbbl::lzstring::compressToUTF16(json);
    m_ws->sendTextMessage(compressed);
    emit sending(message.value("netCommandId").toString(), message);
}

// -----------------------------------------------------------------------------
// Incoming messages
// -----------------------------------------------------------------------------
void FumbblWs::handleMessage(const QString &raw)
{
    if (raw.size() > 64 * 1024) { // TS: maxMessageSize
        emit errorOccurred(QStringLiteral("message too large"));
        return;
    }

    // CRITICAL: decompress; if the result is empty fall back to raw text
    // (the TS client does exactly this).
    QString text = fumbbl::lzstring::decompressFromUTF16(raw);
    if (text.isEmpty())
        text = raw;

    QJsonParseError parseErr {};
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        emit errorOccurred(QStringLiteral("JSON parse error: %1").arg(parseErr.errorString()));
        return;
    }

    const QJsonObject msg = doc.object();
    const QString commandId = msg.value("netCommandId").toString();
    if (commandId.isEmpty())
        return;

    emit received(commandId, msg);

    // Route the commands a client needs; everything else stays visible via
    // `received` (sketches, zap, local markers, ...).
    if (commandId == QLatin1String("serverVersion"))
        emit serverVersionReceived(msg);
    else if (commandId == QLatin1String("serverUserSettings"))
        emit userSettingsReceived(msg);
    else if (commandId == QLatin1String("serverJoin"))
        emit joined(msg);
    else if (commandId == QLatin1String("serverLeave"))
        emit left(msg);
    else if (commandId == QLatin1String("serverGameState"))
        emit gameStateReceived(msg.value("game").toObject());
    else if (commandId == QLatin1String("serverModelSync")) {
        const QJsonObject modelChangeList = msg.value("modelChangeList").toObject();
        const QJsonArray changes = modelChangeList.value("modelChangeArray").toArray();
        const QJsonArray reports = msg.value("reportList").toObject().value("reports").toArray();
        emit modelSyncReceived(changes, reports);
    } else if (commandId == QLatin1String("serverGameTime")) {
        emit gameTimeReceived(msg.value("gameTime").toVariant().toLongLong(),
                               msg.value("turnTime").toVariant().toLongLong());
    } else if (commandId == QLatin1String("serverTalk"))
        emit talkReceived(msg);
    else if (commandId == QLatin1String("serverSound"))
        emit soundReceived(msg);
    else if (commandId == QLatin1String("serverAdminMessage"))
        emit adminMessageReceived(msg);
    else if (commandId == QLatin1String("serverSocketClosed"))
        emit socketClosedByServer(msg);
    else if (commandId == QLatin1String("serverPong"))
        emit pongReceived(msg.value("timestamp").toVariant().toLongLong());
    // serverPing and the remaining commands: no action for a spectator.
}

