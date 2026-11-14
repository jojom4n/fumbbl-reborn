#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QPainterPath>
#include <QPixmap>
#include <QTimer>
#include <QVector>
#include <QWidget>

// The TOUCHDOWN celebration: a large Qwen-Image banner (opaque 2:1 art
// with the word TOUCHDOWN painted into it — user decision: the text is
// part of the art) centered in belle evidenza on the screen, with
// ANIMATED falling colored ribbons drawn in vector (QPainter, crisp at
// 4K). It is an EVENT: shown for a few seconds on every touchdown,
// transparent to the mouse so the field stays interactive underneath.
class TouchdownBanner : public QWidget
{
    Q_OBJECT
public:
    explicit TouchdownBanner(QWidget *parent = nullptr);

    // Start (or restart) a celebration.
    void celebrate();
    // True while the celebration is on screen. Test seam.
    bool celebrating() const { return m_active; }
    // Celebration length (ms) — the field wipe after a TD is deferred by
    // about this long so the players stay while the banner celebrates.
    static constexpr qint64 durationMs() { return 4500; }

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    // A falling streamer ribbon, like the ones in the banner art: a thin
    // wavy curve or a small spiral curl. All motion is derived from the
    // elapsed time (deterministic, no per-frame state drift).
    struct Confetto {
        qreal x0 = 0;   // start x (fraction of the width)
        qreal y0 = 0;   // start y (fraction of the height, negative = above)
        qreal vy = 0;   // fall speed (fractions of the height per second)
        qreal sway = 0; // horizontal sway amplitude (fraction of the width)
        qreal freq = 0; // sway frequency (Hz)
        qreal phase = 0;
        qreal rot0 = 0; // initial rotation (rad)
        qreal rotV = 0; // rotation speed (rad/s)
        qreal len = 16; // streamer length (px)
        qreal amp = 3;  // wave amplitude (px)
        qreal penW = 2.5; // stroke width (px)
        qreal size = 6;   // spiral size (px)
        bool spiral = false;
        QColor color;
    };
    void spawnConfetti();
    static void paintStreamer(QPainter &p, const Confetto &c, double tSec,
                              double x, double y, double tumble);

    QVector<Confetto> m_confetti;
    bool m_active = false;
    QElapsedTimer m_clock;
    QTimer m_timer;
};
