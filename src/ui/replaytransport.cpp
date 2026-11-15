#include "ui/replaytransport.h"

#include "core/gamemodel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>

namespace {
// Official Java client speed table (slow -> fast), default index 2 = 1x.
const int kSpeedMs[7] = {800, 400, 200, 100, 50, 25, 10};
} // namespace

int ReplayTransport::speedMs(int i)
{
    return kSpeedMs[qBound(0, i, 6)];
}

ReplayTransport::ReplayTransport(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &ReplayTransport::tick);
}

QJsonObject ReplayTransport::frameAt(int index) const
{
    if (index < 1 || index > m_frames.size())
        return QJsonObject();
    return m_frames[index - 1];
}

bool ReplayTransport::load(const QString &dir)
{
    stop();
    m_frames.clear();
    m_marks.clear();
    m_meta = QJsonObject();
    m_seed = QJsonObject();
    m_seedKind = QStringLiteral("initial");
    m_next = 1;

    QFile mf(dir + QStringLiteral("/match.json"));
    if (mf.open(QIODevice::ReadOnly))
        m_meta = QJsonDocument::fromJson(mf.readAll()).object();

    QFile sf(dir + QStringLiteral("/stream.jsonl"));
    if (!sf.open(QIODevice::ReadOnly))
        return false;
    const QStringList lines = QString::fromUtf8(sf.readAll()).split(
        QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.isEmpty())
        return false;

    const QJsonObject l0 = QJsonDocument::fromJson(lines[0].toUtf8()).object();
    m_seedKind = l0.value(QStringLiteral("seedKind")).toString();
    m_seed = l0.value(QStringLiteral("game")).toObject();
    for (int i = 1; i < lines.size(); ++i)
        m_frames.append(QJsonDocument::fromJson(lines[i].toUtf8()).object());

    // Build the turn marks in a single model-only walk. "final" seeds reset to
    // kickoff in R1b (createReplaySeed); R1a local archives are always "initial"
    // (already the kickoff state), so initialize() straight through is correct.
    GameModel probe;
    probe.initialize(m_seed);
    bool sawEnd = false;
    for (int i = 0; i < m_frames.size(); ++i) {
        const QJsonObject &frame = m_frames[i];
        probe.applyModelChanges(frame.value(QStringLiteral("modelChangeList"))
                                    .toObject()
                                    .value(QStringLiteral("modelChangeArray"))
                                    .toArray());
        const QJsonArray reports =
            frame.value(QStringLiteral("reportList")).toObject()
                .value(QStringLiteral("reports")).toArray();
        for (const QJsonValue &rv : reports) {
            const QString rid = rv.toObject().value(QStringLiteral("reportId")).toString();
            if (rid == QLatin1String("turnEnd")) {
                m_marks.append(ReplayTurnMark{
                    i + 1,
                    QStringLiteral("Turn %1").arg(probe.turn())
                        + (probe.half() > 1 ? QStringLiteral(" (2nd half)") : QString())});
            } else if (rid == QLatin1String("startHalf")) {
                m_marks.append(ReplayTurnMark{i + 1, QStringLiteral("Half time")});
            } else if (rid == QLatin1String("kickoffResult")) {
                m_marks.append(ReplayTurnMark{i + 1, QStringLiteral("Kickoff")});
            }
        }
        if (!sawEnd && probe.turnMode() == QLatin1String("endGame")) {
            sawEnd = true;
            m_marks.append(ReplayTurnMark{i + 1, QStringLiteral("Full time")});
        }
    }
    std::sort(m_marks.begin(), m_marks.end(),
              [](const ReplayTurnMark &a, const ReplayTurnMark &b) {
                  return a.frameIndex < b.frameIndex;
              });
    emit loaded(m_meta, m_marks);
    return !m_frames.isEmpty() || !m_seed.isEmpty();
}

void ReplayTransport::setPlaying(bool playing)
{
    m_playing = playing;
    if (playing && m_next <= m_frames.size())
        m_timer.start(speedMs(m_speedIndex));
    else
        m_timer.stop();
}

void ReplayTransport::setSpeedIndex(int i)
{
    m_speedIndex = qBound(0, i, 6);
    if (m_playing)
        m_timer.setInterval(speedMs(m_speedIndex));
}

void ReplayTransport::start(int fromIndex)
{
    m_next = fromIndex;
    setPlaying(true);
}

void ReplayTransport::pause() { setPlaying(false); }

void ReplayTransport::resume()
{
    if (m_next <= m_frames.size())
        setPlaying(true);
}

void ReplayTransport::stop() { setPlaying(false); }

void ReplayTransport::seek(int index) { emit seekRequested(index); }

void ReplayTransport::setPosition(int index)
{
    m_next = index + 1; // the next frame to fire is the one AFTER index
    emit positionChanged(index);
}

void ReplayTransport::tick()
{
    if (m_next > m_frames.size()) {
        setPlaying(false);
        return;
    }
    const int i = m_next++;
    emit frameReady(i);
    emit positionChanged(i);
    if (m_next > m_frames.size())
        setPlaying(false);
}

void ReplayTransport::tickForTest()
{
    m_timer.stop(); // deterministic: no async timer firing in the test
    if (m_next <= m_frames.size()) {
        const int i = m_next++;
        emit frameReady(i);
        emit positionChanged(i);
        if (m_next > m_frames.size())
            m_playing = false;
    }
}
