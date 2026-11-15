#pragma once

#include <QDateTime>
#include <QFile>
#include <QJsonObject>
#include <QTextStream>
#include <QVector>

// Auto-archive of spectated matches (R1a). Every raw serverGameState /
// serverModelSync frame is written to a per-match directory so a match can be
// replayed offline. Remote replays (R1b) reuse the SAME canonical format:
//
//   <root>/<gameId>_<yyyyMMdd_HHmmss>/
//     match.json    metadata (teams, coaches, race, rulesVersion, score, timing)
//     stream.jsonl  line 0 = {"seedKind","game"}; then one RAW NetCommand JSON
//                   object per line, in order (the serverModelSync frames)
//
// seedKind "initial" = the state at join time (a live archive covers only what
// was watched — replay starts there); "final" = the full final state (a remote
// download — the viewer runs the kickoff reset, then the command log, to replay
// the whole match).
//
// The archive NEVER blocks the live UI: an IO error disables archiving for that
// match silently (the caller shows one status-bar warning).

struct MatchArchiveInfo {
    QString path; // absolute directory
    qint64 gameId = 0;
    QString homeName, awayName, homeCoach, awayCoach, rulesVersion;
    QDateTime startedAt;
    int scoreHome = 0, scoreAway = 0;
    bool ended = false;
};

class MatchArchive
{
public:
    // The raw serverGameState frame ({..., "game": {...}}). A new gameId opens
    // a fresh dir; a re-send of the SAME gameId (a full re-sync) keeps
    // appending to the open dir. A DIFFERENT gameId is the caller's job
    // (endMatch the previous one first). Returns true when the dir is open
    // (freshly or already) for this id.
    bool begin(const QJsonObject &frame);
    // A raw serverModelSync frame: appended as one line (flushed per line).
    void appendModelSync(const QJsonObject &frame);
    // endGame: record the final score, close the stream, mark finished.
    void endMatch(int scoreHome, int scoreAway);

    // Scan <root>/*/match.json (newest first); unreadable/incomplete skipped.
    static QVector<MatchArchiveInfo> listMatches();

    // Test seam: point the archive root at a temp dir (empty = the default
    // ~/.config/fumbbl-reborn/matches).
    static void setRootForTest(const QString &root);
    static QString root();

    bool archiving() const { return m_active; }
    qint64 gameId() const { return m_gameId; }

private:
    void writeMatchJson();

    static QString s_root;
    QString m_dir;
    bool m_active = false;
    bool m_ended = false;
    qint64 m_gameId = 0;
    qint64 m_homeId = 0, m_awayId = 0;
    QString m_homeName, m_awayName, m_homeCoach, m_awayCoach, m_homeRace,
        m_awayRace, m_rulesVersion;
    QDateTime m_startedAt;
    QDateTime m_endedAt;
    int m_scoreHome = 0, m_scoreAway = 0;
    QFile m_streamFile;
    QTextStream m_stream;
};
