// =============================================================================
// RainSim — the animated rain simulation (round 12, user request)
//
// When the weather is rain the field shows ANIMATED rain: streaks of
// droplets falling across the pitch, and a SPLASH where each drop lands.
// Drops that land inside one of the puddles (the water patches drawn on the
// field) make a bigger, longer ripple; drops on grass make a small one.
//
// Pure simulation (no painting, no widget): FieldView drives step() from a
// QTimer and draws the state; the unit tests drive it deterministically.
// =============================================================================
#pragma once

#include <QRectF>
#include <QVector>

#include <cstdint>

class QRandomGenerator;

class RainSim
{
public:
    struct Drop {
        float x = 0.0f;       // horizontal position (px in field space)
        float y = 0.0f;       // current vertical position
        float groundY = 0.0f; // where this drop lands (splash point)
        float speed = 0.0f;   // px per ms
        float len = 0.0f;     // streak length (px)
        float slant = 0.0f;   // horizontal slant of the streak (px)
    };
    struct Splash {
        float x = 0.0f;
        float y = 0.0f;
        float ageMs = 0.0f;   // elapsed since the drop landed
        bool onPuddle = false; // ripple on a puddle (bigger + longer)
    };

    // Puddle rectangles (field space). Drops landing inside make a
    // puddle ripple.
    void configure(float width, float height, const QVector<QRectF> &puddles);
    // Update the puddles WITHOUT resetting the drops (round 13: the puddles
    // are dynamic — they grow over match time). configure() reseeds; this
    // only swaps the water patches so the running rain keeps flowing.
    void setPuddles(const QVector<QRectF> &puddles);
    void setIntensity(int level); // 0..100 (more drops at higher levels)
    void seed(qint64 s);          // deterministic simulation (tests)

    // Advance the simulation; drops fall, landings become splashes,
    // splashes expire. dt in milliseconds.
    void step(float dtMs);

    const QVector<Drop> &drops() const { return m_drops; }
    const QVector<Splash> &splashes() const { return m_splashes; }
    const QVector<QRectF> &puddles() const { return m_puddles; }
    bool active() const { return !m_drops.isEmpty(); }

    // Landing counters (for tests / diagnostics).
    int landingsOnGrass() const { return m_landingsGrass; }
    int landingsOnPuddle() const { return m_landingsPuddle; }

private:
    void spawnDrop(Drop &d, bool anywhere);
    bool inPuddle(const QPointF &p) const;

    float m_w = 0.0f;
    float m_h = 0.0f;
    int m_intensity = 70;
    qint64 m_seed = 0;
    bool m_hasRng = false;
    QRandomGenerator *m_rng = nullptr; // owned

    QVector<Drop> m_drops;
    QVector<Splash> m_splashes;
    QVector<QRectF> m_puddles;

    int m_landingsGrass = 0;
    int m_landingsPuddle = 0;
};
