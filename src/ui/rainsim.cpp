// =============================================================================
// RainSim — implementation (top-down rain)
// =============================================================================
#include "ui/rainsim.h"

#include <QPointF>
#include <QRandomGenerator>

#include <cmath>

void RainSim::configure(float width, float height, const QVector<QRectF> &puddles)
{
    m_w = width;
    m_h = height;
    m_puddles = puddles;
    m_landingsGrass = 0;
    m_landingsPuddle = 0;
    m_splashes.clear();

    // Drop count scales with the intensity (light drizzle -> deluge).
    const int n = 24 + m_intensity * 2;
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

void RainSim::seed(qint64 s)
{
    m_seed = s;
    if (!m_rng)
        m_rng = new QRandomGenerator;
    m_rng->seed(static_cast<quint32>(s & 0xFFFFFFFF));
    m_hasRng = true;
}

void RainSim::spawnDrop(Drop &d, bool anywhere)
{
    QRandomGenerator *rng = m_hasRng ? m_rng : QRandomGenerator::global();
    // TOP-DOWN: the drop appears anywhere in the WHOLE field (this is what
    // makes the rain cover the entire pitch at all times — the old
    // "spawn at the top and fall" left a bare upper-bands-only rain).
    d.x = float(rng->bounded(int(m_w)));
    d.y = float(rng->bounded(int(m_h)));
    d.maxLifeMs = 90.0f + float(rng->bounded(130)); // 90..220 ms
    d.len = 4.0f + float(rng->bounded(10));          // 4..14 px streak
    // A consistent wind slant (slightly down-right) with per-drop jitter,
    // so the streaks read as rain, not as scattered dots.
    d.angle = 1.25f + float(rng->bounded(16) - 8) * 0.017f; // ~72deg +/- 0.14
    // A tiny top-down drift while the drop "falls" (barely visible).
    const float spd = 0.01f + float(rng->bounded(10)) * 0.002f;
    d.driftX = float(std::cos(d.angle) * spd);
    d.driftY = float(std::sin(d.angle) * spd);
    if (anywhere)
        d.lifeMs = float(rng->bounded(int(d.maxLifeMs))); // stagger the starts
    else
        d.lifeMs = 0.0f;
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
    QRandomGenerator *rng = m_hasRng ? m_rng : QRandomGenerator::global();
    for (Drop &d : m_drops) {
        d.lifeMs += dtMs;
        d.x += d.driftX * dtMs;
        d.y += d.driftY * dtMs;
        if (d.lifeMs >= d.maxLifeMs) {
            // LANDING: a splash ripple at the drop's spot.
            Splash s;
            s.x = qBound(0.0f, d.x, float(m_w));
            s.y = qBound(0.0f, d.y, float(m_h));
            s.ageMs = 0.0f;
            s.onPuddle = inPuddle(QPointF(s.x, s.y));
            m_splashes.append(s);
            if (s.onPuddle)
                ++m_landingsPuddle;
            else
                ++m_landingsGrass;
            // Respawn immediately at a NEW random spot of the whole field
            // (the rain is continuous everywhere).
            spawnDrop(d, /*anywhere=*/false);
        }
    }
    // Age the splashes; puddle ripples live longer than grass ones.
    const float kGrassLife = 350.0f;
    const float kPuddleLife = 700.0f;
    for (int i = m_splashes.size() - 1; i >= 0; --i) {
        Splash &s = m_splashes[i];
        s.ageMs += dtMs;
        const float life = s.onPuddle ? kPuddleLife : kGrassLife;
        if (s.ageMs >= life)
            m_splashes.removeAt(i);
    }
}
