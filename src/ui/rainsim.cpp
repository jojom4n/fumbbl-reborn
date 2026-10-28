// =============================================================================
// RainSim — implementation (round 12)
// =============================================================================
#include "ui/rainsim.h"

#include <QRandomGenerator>

void RainSim::seed(qint64 s)
{
    m_seed = s;
    if (m_rng) {
        delete m_rng;
        m_rng = nullptr;
    }
    m_rng = new QRandomGenerator(s);
    m_hasRng = true;
}

void RainSim::configure(float width, float height, const QVector<QRectF> &puddles)
{
    m_w = width;
    m_h = height;
    m_puddles = puddles;
    m_landingsGrass = 0;
    m_landingsPuddle = 0;
    m_splashes.clear();

    if (!m_hasRng)
        seed(m_seed ? m_seed : 1234567);

    // Drop count scales with the area and the intensity.
    const qreal area = qreal(m_w) * qreal(m_h);
    const int base = int(area / 5200.0); // ~1 drop per 5200 px^2 at 100%
    const int n = qMax(0, base * m_intensity / 100);
    m_drops.resize(n);
    for (Drop &d : m_drops)
        spawnDrop(d, /*anywhere=*/true);
}

void RainSim::setPuddles(const QVector<QRectF> &puddles)
{
    m_puddles = puddles;
}

void RainSim::setIntensity(int level)
{
    m_intensity = qBound(0, level, 100);
}

void RainSim::spawnDrop(Drop &d, bool anywhere)
{
    QRandomGenerator *rng = m_hasRng ? m_rng : QRandomGenerator::global();
    d.x = float(rng->bounded(int(m_w)));
    d.speed = 0.22f + float(rng->bounded(180)) * 0.001f; // 0.22..0.40 px/ms
    d.len = 9.0f + float(rng->bounded(16));              // 9..24 px
    d.slant = 1.5f + float(rng->bounded(25)) * 0.05f;   // 1.5..4 px

    const float travel = 40.0f + float(rng->bounded(140)); // 40..180 px
    if (anywhere) {
        // Pre-populate: some drops mid-fall, some above the field.
        d.y = float(rng->bounded(int(m_h + 40))) - 40.0f;
    } else {
        d.y = -40.0f;
    }
    // The landing point stays inside the field (the streak is drawn above
    // the drop, so a drop near the bottom lands a bit lower but visible).
    d.groundY = qMin(d.y + travel, float(m_h) - 2.0f);
    if (d.groundY <= d.y)
        d.groundY = d.y + travel; // will re-wrap immediately on the next field
}

bool RainSim::inPuddle(const QPointF &p) const
{
    for (const QRectF &r : m_puddles)
        if (r.contains(p))
            return true;
    return false;
}

void RainSim::step(float dtMs)
{
    if (m_drops.isEmpty())
        return;

    // Drops fall.
    for (Drop &d : m_drops) {
        d.y += d.speed * dtMs;
        if (d.y >= d.groundY) {
            const QPointF land(d.x, d.groundY);
            Splash s;
            s.x = d.x;
            s.y = d.groundY;
            s.ageMs = 0.0f;
            s.onPuddle = inPuddle(land);
            if (s.onPuddle)
                ++m_landingsPuddle;
            else
                ++m_landingsGrass;
            m_splashes.append(s);
            spawnDrop(d, /*anywhere=*/false);
        }
    }

    // Splashes age and expire (puddle ripples live longer).
    for (int i = m_splashes.size() - 1; i >= 0; --i) {
        Splash &s = m_splashes[i];
        s.ageMs += dtMs;
        const float maxAge = s.onPuddle ? 650.0f : 380.0f;
        if (s.ageMs >= maxAge)
            m_splashes.removeAt(i);
    }
}
