// =============================================================================
// teamfetch — implementation
// =============================================================================
#include "core/teamfetch.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

TeamFetch::TeamFetch(QObject *parent)
    : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    connect(m_nam, &QNetworkAccessManager::finished, this,
            &TeamFetch::onReplyFinished);
}

void TeamFetch::fetchTeams(const QList<qint64> &teamIds)
{
    m_results.clear();
    m_pending.clear();
    for (qint64 id : teamIds) {
        if (id <= 0 || m_pending.contains(id))
            continue;
        m_pending.insert(id);
        const QUrl url(QStringLiteral("https://fumbbl.com/api/team/get/%1")
                          .arg(id));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                       QStringLiteral("FUMBBL-Reborn"));
        req.setTransferTimeout(10000); // 10 s per team; failure = absent
        m_nam->get(req);
    }
    if (m_pending.isEmpty())
        emit teamsReady(QHash<qint64, QVector<TeamPlayerInfo>>());
}

void TeamFetch::onReplyFinished(QNetworkReply *reply)
{
    const QByteArray raw = reply->readAll();
    const QUrl url = reply->url();
    // The id is the last path segment of /api/team/get/<id>.
    const qint64 id = url.path().section(QLatin1Char('/'), -1).toLongLong();
    const bool ok = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();

    if (ok) {
        const QJsonObject obj = QJsonDocument::fromJson(raw).object();
        QVector<TeamPlayerInfo> players;
        const QJsonArray arr = obj.value(QStringLiteral("players")).toArray();
        for (const QJsonValue &pv : arr) {
            const QJsonObject p = pv.toObject();
            TeamPlayerInfo t;
            t.id = p.value(QStringLiteral("id")).toVariant().toLongLong();
            t.number = p.value(QStringLiteral("number")).toInt();
            const QJsonObject rec =
                p.value(QStringLiteral("record")).toObject();
            t.spp = rec.value(QStringLiteral("spp")).toInt();
            t.extraSpp = rec.value(QStringLiteral("extra_spp")).toInt();
            t.injuries = p.value(QStringLiteral("injuries")).toString();
            const QJsonArray skills =
                p.value(QStringLiteral("skills")).toArray();
            for (const QJsonValue &sv : skills) {
                if (sv.isString())
                    t.skills << sv.toString();
                else {
                    const QJsonObject so = sv.toObject();
                    if (!so.value(QStringLiteral("name")).toString().isEmpty())
                        t.skills
                            << so.value(QStringLiteral("name")).toString();
                }
            }
            players.append(t);
        }
        if (!players.isEmpty())
            m_results.insert(id, players);
    }
    m_pending.remove(id);
    if (m_pending.isEmpty())
        emit teamsReady(m_results);
}
