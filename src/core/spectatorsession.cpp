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
        emit progress(QStringLiteral("WebSocket open (ws://fumbbl.com:22223/command)"));
    });
    connect(m_ws, &FumbblWs::statusReceived, this, [this](const QString &status, const QString &message) {
        emit statusReceived(status, message);
        emit progress(QStringLiteral("serverStatus: %1 %2").arg(status, message));
        // A status while not joined = the join was rejected (e.g. the
        // challenge response was wrong). Retry with a FRESH challenge —
        // never replay the old join.
        if (!m_joined)
            retryLogin(QStringLiteral("serverStatus %1").arg(status));
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
                              .arg(changes.size()).arg(reports.size()));
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

void SpectatorSession::connectAsSpectator(const QString &coach, const QByteArray &pwdMd5,
                                          int gameId)
{
    m_gameId = gameId;
    m_joined = false;
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
    m_ws->disconnectFromServer();
}

void SpectatorSession::onJoinWatchdog()
{
    if (m_joined)
        return;
    retryLogin(QStringLiteral("join not confirmed after %1 ms").arg(kJoinWatchdogMs));
}

// The join was not confirmed (or was rejected): retry with a FRESH password
// challenge on the current socket. Bounded: after kMaxJoinRetries give up
// with a visible error.
void SpectatorSession::retryLogin(const QString &reason)
{
    if (m_joined)
        return;
    if (m_joinRetries >= kMaxJoinRetries) {
        emit fatal(QStringLiteral("Cannot join game %1: the server did not confirm the join "
                                  "after %2 attempts (%3). Check your FUMBBL credentials and "
                                  "try again.")
                       .arg(m_gameId)
                       .arg(kMaxJoinRetries + 1)
                       .arg(reason));
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
