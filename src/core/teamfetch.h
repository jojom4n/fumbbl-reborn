// =============================================================================
// teamfetch — FUMBBL pre-match player data (GET /api/team/get/<teamId>)
//
// The pre-match SPP SEED for the player card and the report card: the live
// stream only sends SPP when it CHANGES during the observed match, so a
// player who joined with SPP already earned would read 0. The endpoint
// (public, no auth) carries each player's record.spp + record.extra_spp —
// the base the match starts from. Verified live 15/nov:
//   { ..., "players": [ { "id": 10291788, "number": 1,
//                         "record": { "spp": 0, "extra_spp": 0, ... },
//                         "injuries": "", "skills": ["Mighty Blow"], ... } ] }
// Partial results are OK: a failed/absent team simply misses from the map
// and the UI degrades to stream-only SPP (never blocks the match UI).
// =============================================================================
#pragma once

#include <QHash>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVector>

struct TeamPlayerInfo
{
    qint64 id = 0;
    int number = 0;
    int spp = 0;      // record.spp
    int extraSpp = 0; // record.extra_spp
    QString injuries;
    QStringList skills;
};

class TeamFetch : public QObject
{
    Q_OBJECT

public:
    explicit TeamFetch(QObject *parent = nullptr);

    // Fetch BOTH teams of a match (one request each, 10 s timeout each).
    // `teamsReady` fires ONCE when both replies are settled (a failed team
    // is simply absent from the map — partial results are fine).
    void fetchTeams(const QList<qint64> &teamIds);

    bool isBusy() const { return !m_pending.isEmpty(); }

signals:
    void teamsReady(const QHash<qint64, QVector<TeamPlayerInfo>> &teams);

private:
    void onReplyFinished(QNetworkReply *reply);

    QNetworkAccessManager *m_nam = nullptr;
    QHash<qint64, QVector<TeamPlayerInfo>> m_results;
    QSet<qint64> m_pending;
};
