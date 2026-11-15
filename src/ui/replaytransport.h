// =============================================================================
// ReplayTransport — plays back an archived match's stream.jsonl (R1a)
//
// load() parses an archive dir: match.json (metadata) + stream.jsonl (line 0
// = the seed game, lines 1..N = raw serverModelSync frames). Playback advances
// one frame per timer tick; the UI dispatches each frameReady(index) frame
// through the live pipeline. Turn marks are built once at load() by a single
// model-only walk (a throwaway GameModel) so the transport bar can jump
// straight to a turn.
//
// Speed table (slow -> fast) mirrors the official Java client:
//   index  0      1      2      3      4      5      6
//   ms      800    400    200    100    50     25     10      (0.25x .. 16x)
// =============================================================================
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVector>

struct ReplayTurnMark {
    int frameIndex = 0; // 1-based: the frame that produced this mark
    QString label;      // "Kickoff", "Turn 5", "Turn 8 (2nd half)", "Full time"
};

class ReplayTransport : public QObject
{
    Q_OBJECT
public:
    explicit ReplayTransport(QObject *parent = nullptr);

    bool load(const QString &dir); // parse match.json + stream.jsonl
    int frameCount() const { return m_frames.size(); }
    QJsonObject frameAt(int index) const; // 1-based frame (raw serverModelSync)
    const QJsonObject &seedGame() const { return m_seed; }
    QString seedKind() const { return m_seedKind; }
    const QJsonObject &meta() const { return m_meta; }
    const QVector<ReplayTurnMark> &turnMarks() const { return m_marks; }

    void setPlaying(bool playing);
    bool playing() const { return m_playing; }
    void setSpeedIndex(int i);
    int speedIndex() const { return m_speedIndex; }

    void start(int fromIndex); // begin/resume from a 1-based frame
    void pause();
    void resume();
    void stop();
    void seek(int index);      // -> emit seekRequested(index)
    void setPosition(int index); // jump to index (next to fire is index+1)

    // Test seam: fire one frameReady synchronously (no timer) and stop it.
    void tickForTest();

    static int speedCount() { return 7; }
    static int speedMs(int i);

signals:
    void seekRequested(int index);
    void frameReady(int index); // 1..frameCount, fired on tick
    void positionChanged(int index); // frames played so far (0..frameCount)
    void loaded(const QJsonObject &meta, const QVector<ReplayTurnMark> &marks);

private:
    void tick();

    QVector<QJsonObject> m_frames; // 1..N serverModelSync frames
    QJsonObject m_seed;            // line 0 "game"
    QString m_seedKind;            // "initial" (R1a local); "final" (R1b remote)
    QJsonObject m_meta;            // match.json
    QVector<ReplayTurnMark> m_marks;
    int m_next = 1;                // next 1-based frame to fire
    bool m_playing = false;
    int m_speedIndex = 2;          // default 200 ms (1x)
    QTimer m_timer;
};
