// =============================================================================
// FumbblAuth — OAuth2 client_credentials + session tokens
//
// Mirrors src/services/fumbblAuth.ts from the TS client:
//   1. POST https://fumbbl.com/api/oauth/token
//        form: grant_type=client_credentials, client_id, client_secret
//        -> { access_token, expires_in, ... }
//   2. POST https://fumbbl.com/api/auth/getToken  (Bearer access_token)
//        -> response body is a JSON-encoded string: "\"ddec93be...\""
//        -> JSON.parse it (CRITICAL: the raw text keeps the quotes)
//
// Session tokens are ONE-TIME USE: fetch a fresh one immediately before
// every WebSocket connection.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class FumbblAuth : public QObject
{
    Q_OBJECT

public:
    explicit FumbblAuth(QObject *parent = nullptr);

    static QString defaultBaseUrl(); // "https://fumbbl.com"

    // Step 1: exchange client credentials for an access token.
    void authenticate(const QString &clientId, const QString &clientSecret);
    // Step 2: fetch a fresh (one-time) session token. Requires authenticate().
    void fetchSessionToken();

signals:
    void accessTokenReady(const QString &accessToken);
    void sessionTokenReady(const QString &sessionToken);
    void failed(const QString &step, const QString &error);

private slots:
    void onTokenReplyFinished();
    void onSessionReplyFinished();

private:
    QNetworkAccessManager *m_nam = nullptr;
    QNetworkReply *m_pendingReply = nullptr;
    bool m_sessionStep = false; // which reply we are awaiting
    QString m_lastAccessToken;
};
