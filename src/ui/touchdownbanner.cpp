#include "ui/touchdownbanner.h"

#include <QPaintEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QTimer>

#include "core/assets.h"

TouchdownBanner::TouchdownBanner(QWidget *parent)
    : QWidget(parent)
{
    // The celebration must never steal input from the field.
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    hide();

    m_timer.setInterval(33); // ~30 fps is plenty for confetti
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (m_clock.elapsed() >= durationMs()) {
            m_active = false;
            m_timer.stop();
            hide();
            update();
            return;
        }
        update();
    });
}

void TouchdownBanner::celebrate()
{
    m_active = true;
    spawnConfetti();
    m_clock.restart();
    show();
    raise();
    if (!m_timer.isActive())
        m_timer.start();
    update();
}

void TouchdownBanner::spawnConfetti()
{
    m_confetti.clear();
    QRandomGenerator *rng = QRandomGenerator::global();
    // The palette of the banner art: red, gold, blue, green, pink, orange,
    // white streamers.
    static const QColor kColors[] = {
        QColor(224, 48, 32),    // red
        QColor(255, 196, 40),   // gold
        QColor(48, 112, 224),   // blue
        QColor(64, 176, 96),    // green
        QColor(240, 120, 160),  // pink
        QColor(240, 140, 40),   // orange
        QColor(248, 248, 252),  // white
    };
    m_confetti.reserve(160);
    for (int i = 0; i < 160; ++i) {
        Confetto c;
        c.x0 = rng->bounded(1000) / 1000.0;
        c.y0 = -0.15 - 0.85 * (rng->bounded(1000) / 1000.0); // staggered start
        c.vy = 0.5 + 0.5 * (rng->bounded(1000) / 1000.0);
        c.sway = 0.01 + 0.05 * (rng->bounded(1000) / 1000.0);
        c.freq = 0.4 + 0.8 * (rng->bounded(1000) / 1000.0);
        c.phase = 2.0 * M_PI * (rng->bounded(1000) / 1000.0);
        c.rot0 = M_PI * (rng->bounded(1000) / 1000.0);
        c.rotV = (rng->bounded(1000) / 1000.0 - 0.5) * 6.0;
        c.len = 10 + rng->bounded(14);   // 10-23 px streamer
        c.amp = 2 + rng->bounded(25) / 10.0; // 2-4.5 px wave
        c.penW = 1.8 + rng->bounded(14) / 10.0; // 1.8-3.1 px stroke
        c.size = 4 + rng->bounded(5);    // 4-8 px spiral
        c.spiral = (i % 10) < 3;         // ~30% spiral curls
        c.color = kColors[rng->bounded(7)];
        m_confetti.append(c);
    }
}

void TouchdownBanner::paintStreamer(QPainter &p, const Confetto &c, double tSec,
                                    double x, double y, double tumble)
{
    p.save();
    p.translate(x, y);
    p.rotate(qRadiansToDegrees(c.rot0 + c.rotV * tSec));
    // Tumble: the streamer turns over — its apparent width oscillates.
    p.scale(tumble, 1.0);
    QPen pen(c.color, c.penW);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    if (c.spiral) {
        // A small spiral curl: r shrinks over ~2.25 turns.
        const double turns = 2.25 * M_PI;
        path.moveTo(c.size, 0.0);
        for (int s = 1; s <= 24; ++s) {
            const double th = turns * s / 24.0;
            const double r = c.size * (1.0 - th / (turns + 1.2));
            path.lineTo(r * qCos(th), r * qSin(th));
        }
        p.drawPath(path);
    } else {
        // A wavy streamer: two opposing humps.
        const double L = c.len;
        path.moveTo(-L / 2, 0.0);
        path.cubicTo(-L / 6, -c.amp, L / 6, c.amp, L / 2, 0.0);
        p.drawPath(path);
    }
    p.restore();
}

void TouchdownBanner::paintEvent(QPaintEvent *)
{
    if (!m_active)
        return;
    const double t = m_clock.elapsed();
    const int W = width();
    const int H = height();
    if (W <= 0 || H <= 0)
        return;

    // Fade in fast, out slow.
    qreal fade = 1.0;
    if (t < 250)
        fade = t / 250.0;
    else if (t > durationMs() - 900)
        fade = (durationMs() - t) / 900.0;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setOpacity(qBound(0.0, fade, 1.0));

    // --- The banner art: centered, 80% of the width (2:1 art). -----------
    const qreal bw = 0.80 * W;
    const QPixmap pm = assets::touchdownBanner();
    if (!pm.isNull()) {
        const qreal bh = pm.height() * (bw / pm.width());
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawPixmap(QRectF((W - bw) / 2, (H - bh) / 2, bw, bh), pm,
                     QRectF(pm.rect()));
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    } else {
        // Asset missing (dev machine): a plain festive ribbon so the
        // event still reads.
        const qreal bh = bw / 2;
        const QRectF r((W - bw) / 2, (H - bh) / 2, bw, bh);
        p.fillRect(r, QColor(128, 30, 28));
        p.setPen(QColor(255, 200, 40));
        QFont f = font();
        f.setPointSizeF(qMax(18.0, bw / 22.0));
        f.setWeight(QFont::Black);
        p.setFont(f);
        p.drawText(r, Qt::AlignCenter, QStringLiteral("TOUCHDOWN"));
    }

    // --- Animated falling streamers (vector: crisp at 4K). ---------------
    const double tSec = t / 1000.0;
    for (const Confetto &c : m_confetti) {
        const double y = c.y0 + c.vy * tSec; // fractions
        if (y > 1.15)
            continue; // fell out
        const double x = c.x0 + c.sway * qSin(2.0 * M_PI * c.freq * tSec
                                              + c.phase);
        const double tumble = 0.35 + 0.65 * qAbs(
                                   qSin(2.5 * tSec + c.phase * 1.7));
        paintStreamer(p, c, tSec, x * W, y * H, tumble);
    }
}
