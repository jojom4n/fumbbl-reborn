#include "core/apppaths.h"
#include "core/matcharchive.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

#include <algorithm>

QString MatchArchive::s_root;

QString MatchArchive::root()
{
    if (s_root.isEmpty())
        s_root = appSaveRoot() + QStringLiteral("/matches");
    return s_root;
}

void MatchArchive::setRootForTest(const QString &r)
{
    s_root = r;
}

// The rulesVersion gameOption ("" when absent — store what the wire says).
static QString rulesVersionOf(const QJsonObject &game)
{
    const QJsonArray opts = game.value("gameOptions").toObject()
                                .value("gameOptionArray").toArray();
    for (const QJsonValue &v : opts) {
        const QJsonObject o = v.toObject();
        if (o.value("gameOptionId").toString() == QLatin1String("rulesVersion"))
            return o.value("gameOptionValue").toString();
    }
    return QString();
}

bool MatchArchive::begin(const QJsonObject &frame)
{
    const QJsonObject game = frame.value("game").toObject();
    const qint64 id = game.value("gameId").toVariant().toLongLong();
    if (id <= 0)
        return false; // no id: nothing to archive
    if (m_active && m_gameId == id)
        return true; // re-sync: keep appending to the open dir

    m_startedAt = QDateTime::currentDateTime();
    m_dir = root() + QStringLiteral("/")
         + QString::number(id) + QStringLiteral("_")
         + m_startedAt.toString(QStringLiteral("yyyyMMdd_HHmmss"));
    if (!QDir().mkpath(m_dir))
        return false;
    m_gameId = id;

    const QJsonObject home = game.value("teamHome").toObject();
    const QJsonObject away = game.value("teamAway").toObject();
    m_homeId = home.value("teamId").toVariant().toLongLong();
    m_awayId = away.value("teamId").toVariant().toLongLong();
    m_homeName = home.value("teamName").toString();
    m_awayName = away.value("teamName").toString();
    m_homeCoach = home.value("coach").toString();
    m_awayCoach = away.value("coach").toString();
    m_homeRace = home.value("race").toString();
    m_awayRace = away.value("race").toString();
    m_rulesVersion = rulesVersionOf(game);
    m_scoreHome = m_scoreAway = 0;
    m_ended = false;
    m_endedAt = QDateTime();

    m_streamFile.setFileName(m_dir + QStringLiteral("/stream.jsonl"));
    if (!m_streamFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    m_stream.setDevice(&m_streamFile);
    QJsonObject seed;
    seed.insert(QStringLiteral("seedKind"), QStringLiteral("initial"));
    seed.insert(QStringLiteral("game"), game);
    m_stream << QJsonDocument(seed).toJson(QJsonDocument::Compact)
             << QLatin1Char('\n');
    m_stream.flush();
    writeMatchJson();
    m_active = true;
    return true;
}

void MatchArchive::appendModelSync(const QJsonObject &frame)
{
    if (!m_active || m_ended)
        return;
    m_stream << QJsonDocument(frame).toJson(QJsonDocument::Compact)
             << QLatin1Char('\n');
    m_stream.flush();
}

void MatchArchive::endMatch(int scoreHome, int scoreAway)
{
    if (!m_active)
        return;
    m_scoreHome = scoreHome;
    m_scoreAway = scoreAway;
    m_ended = true;
    m_endedAt = QDateTime::currentDateTime();
    writeMatchJson();
    if (m_stream.device())
        m_stream.flush();
    m_streamFile.close();
    m_stream.setDevice(nullptr);
    m_active = false;
}

void MatchArchive::writeMatchJson()
{
    QJsonObject home;
    home.insert(QStringLiteral("id"), m_homeId);
    home.insert(QStringLiteral("name"), m_homeName);
    home.insert(QStringLiteral("coach"), m_homeCoach);
    home.insert(QStringLiteral("race"), m_homeRace);
    QJsonObject away;
    away.insert(QStringLiteral("id"), m_awayId);
    away.insert(QStringLiteral("name"), m_awayName);
    away.insert(QStringLiteral("coach"), m_awayCoach);
    away.insert(QStringLiteral("race"), m_awayRace);

    QJsonObject m;
    m.insert(QStringLiteral("gameId"), m_gameId);
    m.insert(QStringLiteral("seedKind"), QStringLiteral("initial"));
    m.insert(QStringLiteral("rulesVersion"), m_rulesVersion);
    m.insert(QStringLiteral("home"), home);
    m.insert(QStringLiteral("away"), away);
    m.insert(QStringLiteral("startedAt"),
             m_startedAt.toString(Qt::ISODateWithMs));
    m.insert(QStringLiteral("endedAt"),
             m_ended ? m_endedAt.toString(Qt::ISODateWithMs) : QJsonValue());
    m.insert(QStringLiteral("scoreHome"),
             m_ended ? m_scoreHome : QJsonValue());
    m.insert(QStringLiteral("scoreAway"),
             m_ended ? m_scoreAway : QJsonValue());
    m.insert(QStringLiteral("source"), QStringLiteral("live"));

    QFile f(m_dir + QStringLiteral("/match.json"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(m).toJson(QJsonDocument::Compact));
        f.close();
    }
}

QVector<MatchArchiveInfo> MatchArchive::listMatches()
{
    QVector<MatchArchiveInfo> out;
    const QDir rootDir(root());
    const QFileInfoList dirs =
        rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &d : dirs) {
        const QFileInfo mf(d.filePath() + QStringLiteral("/match.json"));
        if (!mf.exists())
            continue;
        QFile f(mf.filePath());
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject m = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        MatchArchiveInfo info;
        info.path = d.filePath();
        info.gameId = m.value("gameId").toVariant().toLongLong();
        info.homeName = m.value("home").toObject().value("name").toString();
        info.awayName = m.value("away").toObject().value("name").toString();
        info.homeCoach = m.value("home").toObject().value("coach").toString();
        info.awayCoach = m.value("away").toObject().value("coach").toString();
        info.rulesVersion = m.value("rulesVersion").toString();
        info.startedAt =
            QDateTime::fromString(m.value("startedAt").toString(), Qt::ISODateWithMs);
        info.ended = !m.value("endedAt").isNull();
        if (info.ended) {
            info.scoreHome = m.value("scoreHome").toVariant().toInt();
            info.scoreAway = m.value("scoreAway").toVariant().toInt();
        }
        out.append(info);
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const MatchArchiveInfo &a, const MatchArchiveInfo &b) {
                         return a.startedAt > b.startedAt;
                     });
    return out;
}
