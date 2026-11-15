// =============================================================================
// RainSim — implementation (top-down rain)
// =============================================================================
#include "ui/rainsim.h"

#include <QPointF>
#include <QRandomGenerator>

#include <cmath>

void RainSim::configure(float width, float height, const QVector<QRectF> &puddles,
                        Mode mode, const QRectF &fieldRect)
{
    m_w = width;
    m_h = height;
    m_puddles = puddles;
    m_mode = mode;
    m_fieldRect = fieldRect;
    m_landingsGrass = 0;
    m_landingsPuddle = 0;
    m_splashes.clear();

    // Drop count scales with the intensity (light drizzle -> deluge). Snow
    // is SOFTER: more, lighter flakes (a blizzard is a wall of white).
    const int n = mode == Mode::Snow ? 90 + m_intensity * 2
                                     : 24 + m_intensity * 2;
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

void RainSim::applyIntensity(int level)
{
    // Same drop-count formula as configure(): rain 24 + 2*level, snow
    // 90 + 2*level. Only the COUNT changes — no reseed (the running rain
    // keeps flowing), splashes are kept.
    m_intensity = qBound(0, level, 100);
    const int n = m_mode == Mode::Snow ? 90 + m_intensity * 2
                                       : 24 + m_intensity * 2;
    if (n > m_drops.size()) {
        for (int i = m_drops.size(); i < n; ++i) {
            Drop d;
            spawnDrop(d, /*anywhere=*/true); // staggered start
            m_drops.append(d);
        }
    } else if (n < m_drops.size()) {
        m_drops.resize(n); // chop the excess (drops are POD)
    }
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
    if (m_mode == Mode::Snow) {
        // A snowflake FLUTTERS: a slow, wandering fall with sideways sway
        // (the sway is the drift; the "streak" fields stay small — the
        // painter draws flakes as soft dots, not streaks).
        d.x = float(rng->bounded(int(m_w)));
        d.y = float(rng->bounded(int(m_h)));
        d.maxLifeMs = 500.0f + float(rng->bounded(600)); // 0.5..1.1 s
        d.len = 2.0f + float(rng->bounded(3));           // dot size hint
        d.angle = 1.5707f; // straight down (unused for dots)
        d.driftX = (float(rng->bounded(200)) - 100.0f) * 0.008f; // +/-0.4 px/ms
        d.driftY = 0.006f + float(rng->bounded(6)) * 0.0015f;    // 6..15 px/100ms
        if (anywhere)
            d.lifeMs = float(rng->bounded(int(d.maxLifeMs)));
        else
            d.lifeMs = 0.0f;
        return;
    }
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
            // LANDING: a splash ripple at the drop's spot (rain only —
            // snowflakes land silently; the drift keeps moving the flake).
            // ONLY on the PITCH: a drop that lands on the stadium surround
            // vanishes (m_fieldRect empty = the whole world is the field,
            // the direct-sim tests).
            if (m_mode == Mode::Rain) {
                const float sx = qBound(0.0f, d.x, float(m_w));
                const float sy = qBound(0.0f, d.y, float(m_h));
                const bool onField =
                    m_fieldRect.isEmpty()
                    || m_fieldRect.contains(QPointF(sx, sy));
                if (onField) {
                    Splash s;
                    s.x = sx;
                    s.y = sy;
                    s.ageMs = 0.0f;
                    s.onPuddle = inPuddle(QPointF(sx, sy));
                    m_splashes.append(s);
                    if (s.onPuddle)
                        ++m_landingsPuddle;
                    else
                        ++m_landingsGrass;
                }
            }
            // Respawn immediately at a NEW random spot of the whole scene
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
