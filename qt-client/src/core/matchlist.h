// =============================================================================
// matchlist — FUMBBL live match list (GET /api/match/current)
//
// Powers the "Spectate Game" dialog: the list of matches currently in
// progress / scheduled, with per-team race / coach / team name / score,
// so the user can pick one and spectate it.
//
// Response shape (verified live, 13/ott):
//   [ { "id": 1954643, "half": 2, "turn": 0, "division": "Competitive",
//       "scheduler": "Blackbox",
//       "teams": [ { "side": "home", "name": "We Human", "coach": "Wotan83",
//                    "race": "Orc", "rating": "Mega-Star", "score": 0,
//                    "logo": "/i/486335", "tv": "1340000" }, ... ] } ]
//   half: 0 = not started, 1 = first half, 2 = second half
// =============================================================================
#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QVector>

struct FFBMatchTeam
{
    QString name;     // team name
    QString coach;    // coach (username)
    QString race;     // race name (may be a custom team race)
    QString rating;
    int score = 0;
};

struct FFBLiveMatch
{
    int id = 0;
    int half = 0; // 0 not started, 1 first half, 2 second half
    int turn = 0;
    QString division;
    QString scheduler;
    FFBMatchTeam home;
    FFBMatchTeam away;

    QString statusText() const
    {
        if (half <= 0) return QStringLiteral("—");
        if (half == 1) return turn > 0 ? QStringLiteral("1H, turn %1").arg(turn)
                                       : QStringLiteral("1H");
        return turn > 0 ? QStringLiteral("2H, turn %1").arg(turn) : QStringLiteral("2H");
    }
};

class MatchListClient : public QObject
{
    Q_OBJECT

public:
    explicit MatchListClient(QObject *parent = nullptr);

    // Fetch https://fumbbl.com/api/match/current with an OAuth access token.
    void fetch(const QString &accessToken);

    bool isBusy() const { return m_reply != nullptr; }

signals:
    void matchesReceived(QVector<FFBLiveMatch> matches);
    void fetchFailed(const QString &message);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_nam = nullptr;
    QNetworkReply *m_reply = nullptr;
};

QVector<FFBLiveMatch> parseMatchList(const QByteArray &json);
