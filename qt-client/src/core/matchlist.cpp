// =============================================================================
// matchlist — implementation
// =============================================================================
#include "core/matchlist.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

namespace {
constexpr auto kBaseUrl = "https://fumbbl.com";

FFBMatchTeam parseTeam(const QJsonObject &o)
{
    FFBMatchTeam t;
    t.name = o.value(QStringLiteral("name")).toString();
    t.coach = o.value(QStringLiteral("coach")).toString();
    t.race = o.value(QStringLiteral("race")).toString();
    t.rating = o.value(QStringLiteral("rating")).toString();
    t.score = o.value(QStringLiteral("score")).toInt();
    return t;
}
} // namespace

QVector<FFBLiveMatch> parseMatchList(const QByteArray &json)
{
    QVector<FFBLiveMatch> out;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray())
        return out;
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject m = v.toObject();
        FFBLiveMatch match;
        match.id = m.value(QStringLiteral("id")).toInt();
        match.half = m.value(QStringLiteral("half")).toInt();
        match.turn = m.value(QStringLiteral("turn")).toInt();
        match.division = m.value(QStringLiteral("division")).toString();
        match.scheduler = m.value(QStringLiteral("scheduler")).toString();
        bool homeSet = false;
        for (const QJsonValue &tv : m.value(QStringLiteral("teams")).toArray()) {
            const QJsonObject t = tv.toObject();
            if (t.value(QStringLiteral("side")).toString() == QLatin1String("home")) {
                match.home = parseTeam(t);
                homeSet = true;
            } else {
                match.away = parseTeam(t);
            }
        }
        // Some feeds order teams differently; fall back to array order.
        if (!homeSet && m.value(QStringLiteral("teams")).toArray().size() == 2)
            match.home = parseTeam(m.value(QStringLiteral("teams")).toArray().first().toObject());
        if (match.id > 0)
            out.push_back(match);
    }
    return out;
}

MatchListClient::MatchListClient(QObject *parent) : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    connect(m_nam, &QNetworkAccessManager::finished, this, &MatchListClient::onReplyFinished);
}

void MatchListClient::fetch(const QString &accessToken)
{
    if (m_reply)
        return;

    QNetworkRequest req(QUrl(QStringLiteral("https://fumbbl.com") + QStringLiteral("/api/match/current")));
    req.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());
    req.setRawHeader("Accept", "application/json");
    m_reply = m_nam->get(req);
}

void MatchListClient::onReplyFinished(QNetworkReply *reply)
{
    if (reply != m_reply)
        return; // unrelated (or stale) reply
    m_reply = nullptr;
    reply->deleteLater();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status >= 300) {
        emit fetchFailed(QStringLiteral("HTTP %1: %2")
                             .arg(status)
                             .arg(reply->readAll().left(300)));
        return;
    }
    emit matchesReceived(parseMatchList(reply->readAll()));
}
