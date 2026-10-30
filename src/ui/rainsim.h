// =============================================================================
// RainSim — the animated rain simulation (round 12, user request)
//
// TOP-DOWN rain (round 13 turn 4, user): the field is seen from ABOVE. There
// is no side-view "falling from the top" — a drop is a short bright streak
// that appears at a random spot of the WHOLE field, grows slightly as it
// descends (top-down pseudo-depth), and ends in a SPLASH ripple where it
// lands. Drops landing inside a puddle (the water patches drawn on the
// field) make a bigger, longer ripple.
//
// Round 13 turn 4 fix: the old sim spawned every new drop at the TOP of the
// field and let it "fall" a short distance — after a few seconds the rain
// only covered the upper band of the pitch. Now drops spawn/land uniformly
// over the entire field at all times.
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
        float x = 0.0f;       // position (px in field space)
        float y = 0.0f;
        float lifeMs = 0.0f;  // age of the current fall
        float maxLifeMs = 0.0f; // fall duration (short: 90..220 ms)
        float len = 0.0f;     // streak length at landing (px)
        float angle = 0.0f;   // streak direction (rad), slight wind slant
        float driftX = 0.0f;  // tiny top-down drift (px/ms)
        float driftY = 0.0f;
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

    // Advance the simulation: drops age and land (-> splash), splashes
    // expire, landed drops respawn at a NEW random spot of the whole field.
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
