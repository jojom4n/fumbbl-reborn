// =============================================================================
// FumbblAuth — implementation
// =============================================================================
#include "core/fumbblauth.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

QString FumbblAuth::defaultBaseUrl()
{
    return QStringLiteral("https://fumbbl.com");
}

FumbblAuth::FumbblAuth(QObject *parent)
    : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    connect(m_nam, &QNetworkAccessManager::finished, this, [this](QNetworkReply *reply) {
        if (reply == m_pendingReply) {
            if (m_sessionStep)
                onSessionReplyFinished();
            else
                onTokenReplyFinished();
        }
        reply->deleteLater();
    });
}

void FumbblAuth::authenticate(const QString &clientId, const QString &clientSecret)
{
    if (m_pendingReply)
        return;

    QNetworkRequest req(QUrl(defaultBaseUrl() + QStringLiteral("/api/oauth/token")));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));

    QByteArray form;
    form += "grant_type=client_credentials&";
    form += "client_id=" + QUrl::toPercentEncoding(clientId);
    form += "&client_secret=" + QUrl::toPercentEncoding(clientSecret);

    m_sessionStep = false;
    m_pendingReply = m_nam->post(req, form);
}

void FumbblAuth::fetchSessionToken()
{
    if (m_pendingReply)
        return;

    QNetworkRequest req(QUrl(defaultBaseUrl() + QStringLiteral("/api/auth/getToken")));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Authorization", "Bearer " + m_lastAccessToken.toUtf8());

    m_sessionStep = true;
    m_pendingReply = m_nam->post(req, QByteArray());
}

void FumbblAuth::onTokenReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    m_pendingReply = nullptr;
    if (!reply)
        return;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status >= 300) {
        emit failed(QStringLiteral("oauth"),
                    QStringLiteral("HTTP %1: %2").arg(status).arg(reply->readAll()));
        return;
    }

    const QJsonObject data = QJsonDocument::fromJson(reply->readAll()).object();
    m_lastAccessToken = data.value("access_token").toString();
    if (m_lastAccessToken.isEmpty()) {
        emit failed(QStringLiteral("oauth"), QStringLiteral("no access_token in response"));
        return;
    }
    emit accessTokenReady(m_lastAccessToken);
}

void FumbblAuth::onSessionReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    m_pendingReply = nullptr;
    if (!reply)
        return;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status >= 300) {
        emit failed(QStringLiteral("session-token"),
                    QStringLiteral("HTTP %1: %2").arg(status).arg(reply->readAll()));
        return;
    }

    // CRITICAL (from the TS client): the endpoint returns a JSON-encoded
    // string, e.g.  "ddec93be11e06d2e71838dd22a89bdf5"  (with quotes).
    // Must JSON-parse it; trimming would keep the quotes and the server
    // would answer "Wrong Password".
    const QByteArray body = reply->readAll();
    QString token;

    // The endpoint returns a JSON-encoded string ("ddec93be…"). Parse it the
    // same way the TS client does (JSON.parse); also stay compatible with an
    // object-shaped response ({ session_token: … }).
    QJsonParseError pe {};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    if (pe.error == QJsonParseError::NoError && doc.isObject())
        token = doc.object().value("session_token").toString();
    else
        token = QString::fromUtf8(body).trimmed();

    if (token.size() >= 2 && token.startsWith(QLatin1Char('"')) && token.endsWith(QLatin1Char('"')))
        token = token.mid(1, token.size() - 2); // drop JSON string quotes

    if (token.isEmpty()) {
        emit failed(QStringLiteral("session-token"),
                    QStringLiteral("empty session token: %1").arg(QString::fromUtf8(body)));
        return;
    }
    emit sessionTokenReady(token);
}
