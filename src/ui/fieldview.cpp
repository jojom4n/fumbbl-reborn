// =============================================================================
// FieldView — implementation
// =============================================================================
#include "ui/fieldview.h"

#include "core/assets.h"

#include <QCursor>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <functional>
#include <QPainterPath>
#include <QPair>
#include <QPaintEvent>
#include <QPixmap>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QTimer>
#include <QWheelEvent>

#include <cmath>

// TEAM RING colors. Home team (left in the header) = BLUE, away team
// (right in the header) = RED. Drawn INSIDE the token so the ring never
// overflows the cell, with a thin dark edge so it reads clearly against
// the face and the grass.
namespace {
inline QColor teamRingColor(bool home)
{
    return home ? QColor(0x2f, 0x6f, 0xe8) : QColor(0xe0, 0x28, 0x1e);
}
inline void drawTeamRing(QPainter &p, const QPointF &c, qreal d, bool home)
{
    // Ring width: kept thin so the face stays large.
    const qreal w = d * 0.07;
    const qreal rr = d * 0.5 - w / 2;     // outer edge = token edge (no overflow)
    QPen rp(teamRingColor(home), w);
    p.setPen(rp);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, rr, rr);
    QPen edge(QColor(18, 18, 24, 220), qMax(1.0, w * 0.30));
    p.setPen(edge);
    const qreal eo = rr + w / 2 - edge.widthF() / 2;
    p.drawEllipse(c, eo, eo);
}
} // namespace

FieldView::FieldView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(390, 225); // 26:15 ratio at 15px cells
    setMouseTracking(true); // hover cards
    setCursor(Qt::OpenHandCursor);
    // Default stadium: the dark arena (Qwen i2i from the reference, see
    // tools/mockup/r13_arena_gen.py + stadium_cleanup.py). medieval_arena
    // stays installed: stadiums are INTERCHANGEABLE (setStadium(name)).
    setStadium(QStringLiteral("dark_arena"));
    // WASD pan — the field can take keyboard focus (clicking it or zooming
    // with the wheel grabs it; typing in the chat input still works because
    // that widget has the focus then).
    setFocusPolicy(Qt::StrongFocus);

    // Selection-halo pulse (~1.2 Hz breathing). The timer only repaints the
    // selected token's rect — never the whole field.
    m_pulseTimer = new QTimer(this);
    m_pulseTimer->setInterval(33); // ~30 fps
    // BALL FLIGHT settle: while the ball steps through the grid after a
    // throw, every step resets this timer; when it fires (the ball has been
    // still for a moment) the flight ends and the ball size goes back to
    // the resting size.
    m_ballSettleTimer = new QTimer(this);
    m_ballSettleTimer->setSingleShot(true);
    // 900ms could fire BETWEEN two wire bounce steps of a kickoff (server
    // ticks ~0.5-1s) and chop the flight — 1600ms is safe.
    m_ballSettleTimer->setInterval(1600);
    connect(m_ballSettleTimer, &QTimer::timeout, this, [this]() {
        m_ballFlightSteps = 0;
        m_ballMoving = false;
        m_ballFlightBaseDist = 0; // the bounce profile restarts
        m_kickoffKickerId.clear(); // the kicker glow ends with it
        update();
    });
    connect(m_pulseTimer, &QTimer::timeout, this, [this]() {
        m_pulsePhase += 2.0 * M_PI * 1.2 * 0.033;
        if (m_pulsePhase > 2.0 * M_PI)
            m_pulsePhase -= 2.0 * M_PI;
        // The ball's gold glow breathes on its OWN slower phase (~0.25 Hz,
        // one breath every ~4 s) — faster would be too distracting; the
        // other glows keep the 1.2 Hz pulse.
        m_ballPulsePhase += 2.0 * M_PI * 0.25 * 0.033;
        if (m_ballPulsePhase > 2.0 * M_PI)
            m_ballPulsePhase -= 2.0 * M_PI;
        // The ball's gold glow must be repainted on EVERY tick, even when
        // no selection/block/pass glow is armed: an early-return here would
        // advance the phase but never schedule a repaint, and the glow
        // would sit FROZEN at the phase of the last unrelated paint. Full
        // field update while the ball is visible (on a square, gliding in
        // flight, or in a carrier's hand) — always correct, and cheap
        // (same cost the block/pass glows already pay).
        if (ballGlowVisible()) {
            update();
            return;
        }
        if (m_selectedPlayer.isEmpty() && m_blockTarget.isEmpty()
            && m_passTarget.isEmpty())
            return;
        // While a BLOCK glow is on, a full field update is cheap enough
        // (the glow is short-lived); the selection alone keeps the
        // efficient token-rect repaint. The white pass glow breathes the
        // same way.
        if (!m_blockTarget.isEmpty() || !m_passTarget.isEmpty()) {
            update();
            return;
        }
        const auto find = [this](const QVector<PlayerToken> &team) -> const PlayerToken * {
            for (const PlayerToken &t : team)
                if (t.playerId == m_selectedPlayer)
                    return &t;
            return nullptr;
        };
        const PlayerToken *sel = find(m_homePlayers) ? find(m_homePlayers) : find(m_awayPlayers);
        if (!sel)
            return;
        // The halo extends ~1.9 cells around the token center: repaint that
        // rect (mapped through the zoom/pan transform), nothing else.
        const QRectF c = cellRect(sel->x, sel->y);
        const qreal cellW = c.width(), cellH = c.height();
        QRectF r(c.center().x() - cellW * 1.1, c.center().y() - cellH * 1.1,
                 cellW * 2.2, cellH * 2.2);
        r = QRectF(r.x() * m_zoom + m_pan.x(), r.y() * m_zoom + m_pan.y(),
                   r.width() * m_zoom, r.height() * m_zoom);
        update(r.toAlignedRect().adjusted(-2, -2, 2, 2));
    });
}

QSize FieldView::sizeHint() const
{
    return QSize(780, 450);
}

void FieldView::setHomeColor(const QColor &color)
{
    m_homeColor = color;
    update();
}

void FieldView::setAwayColor(const QColor &color)
{
    m_awayColor = color;
    update();
}

void FieldView::setHomeRace(const QString &race)
{
    m_homeRace = race;
}

void FieldView::setAwayRace(const QString &race)
{
    m_awayRace = race;
}

void FieldView::setPlayers(const QVector<PlayerToken> &home,
                           const QVector<PlayerToken> &away)
{
    m_homePlayers = home;
    m_awayPlayers = away;
    // The ball-in-hand marker (hasBall) carries the ball's animated gold
    // glow — keep the pulse timer in sync.
    if (needsPulse())
        m_pulseTimer->start();
    else
        m_pulseTimer->stop();

    // Per-square MOVEMENT ANIMATION. Build the new on-field model layout;
    // every player that MOVED gets a glide through the grid (old cell ->
    // new cell, one square per leg), like the official client.
    // New arrivals snap in; players that left the field (to the box) drop.
    QHash<QString, QPoint> newModel;
    const auto record = [&](const PlayerToken &t) {
        if (t.x < 0 || t.x >= kFieldWidth || t.y < 0 || t.y >= kFieldHeight)
            return; // box / off-field: not drawn on the pitch
        newModel.insert(t.playerId, QPoint(t.x, t.y));
    };
    for (const PlayerToken &t : home) record(t);
    for (const PlayerToken &t : away) record(t);

    for (auto it = newModel.constBegin(); it != newModel.constEnd(); ++it) {
        const QString &id = it.key();
        const QPoint np = it.value();
        auto pit = m_prevModel.constFind(id);
        if (pit == m_prevModel.constEnd()) {
            m_visualPos.insert(id, QPointF(np.x() + 0.5, np.y() + 0.5)); // snap in
            m_tokenAnim.remove(id);
        } else if (pit.value() != np) {
            startTokenAnim(id, np);
        } else if (!m_tokenAnim.value(id).active) {
            m_visualPos.insert(id, QPointF(np.x() + 0.5, np.y() + 0.5)); // rest
        }
    }
    for (auto it = m_prevModel.constBegin(); it != m_prevModel.constEnd(); ++it)
        if (!newModel.contains(it.key())) {
            m_visualPos.remove(it.key());
            m_tokenAnim.remove(it.key());
        }
    m_prevModel = newModel;

    if (anyTokenAnimActive())
        ensureAnimTimer();
    update();
}

void FieldView::setBoxes(const QVector<BoxEntry> &home, const QVector<BoxEntry> &away)
{
    m_homeBox = home;
    m_awayBox = away;
    update();
}

void FieldView::beginKickoffFlight(const QPoint &origin, const QString &kickerId)
{
    // Armed once per kickoff (the model bumps its seq). The actual
    // animation starts on the landing setBall, which is
    // the model's ball coordinate (the wire sends landing + bounces only).
    m_kickoffOrigin = origin;
    m_kickoffArmed = true;
    m_kickoffKickerId = kickerId; // the blue glow starts immediately
    m_ballFlightBaseDist = 0;
    // Any leftover ball animation (e.g. a pass still settling) must not
    // hijack the kickoff flight: the flight path is started by the landing
    // setBall, from this origin.
    m_tokenAnim.remove(QStringLiteral("__ball__"));
    update();
}

void FieldView::setBall(int x, int y, bool inPlay)
{
    m_ballX = x;
    m_ballY = y;
    m_ballInPlay = inPlay;
    // The ball glides too (same mechanism, reserved id, separate prev so the
    // player cleanup in setPlayers never touches it).
    const QString id = QLatin1String("__ball__");
    const bool onField = (x >= 0 && x < kFieldWidth && y >= 0 && y < kFieldHeight);
    if (onField) {
        const QPoint np(x, y);
        if (!m_havePrevBall) {
            m_visualPos.insert(id, QPointF(x + 0.5, y + 0.5));
            m_tokenAnim.remove(id);
        } else if (m_prevBall != np) {
            // KICKOFF — the model jumped the ball straight to the landing
            // cell; synthesize the flight from the
            // kickoff origin (the model's 30-yd line or the previous
            // on-field position) to here, with the bouncing size profile
            // (see the painter). The kickoff leg counts as a MULTI-step
            // flight (steps >= 2) so the profile is enabled.
            const bool kickoffLeg = m_kickoffArmed;
            if (kickoffLeg) {
                m_kickoffArmed = false;
                m_prevBall = m_kickoffOrigin;
                m_ballFlightBaseDist = 0;
            }
            startTokenAnim(id, np);
            // The ball GROWS along the pass trajectory — every step since
            // the flight started adds a little size (capped
            // in the painter). A 1-step fumble hop starts the flight at 1
            // step = NO growth (the painter only grows from step 2 up).
            if (kickoffLeg) {
                m_ballFlightSteps = 2;
            } else if (!m_ballMoving) {
                m_ballFlightSteps = 1;
            } else {
                ++m_ballFlightSteps;
                // A step arrived while a flight was running — the previous
                // leg is DONE: fold its length into the base so the
                // bounce profile phase is continuous across wire steps.
                m_ballFlightBaseDist += qAbs(np.x() - m_prevBall.x())
                    + qAbs(np.y() - m_prevBall.y());
            }
            m_ballMoving = true;
            m_ballSettleTimer->start();
        } else if (!m_tokenAnim.value(id).active) {
            m_visualPos.insert(id, QPointF(x + 0.5, y + 0.5));
        }
        m_prevBall = np;
        m_havePrevBall = true;
    } else {
        m_visualPos.remove(id);
        m_tokenAnim.remove(id);
        m_havePrevBall = false;
        m_kickoffArmed = false; // the ball left the field: no flight
        m_ballFlightSteps = 0;
        m_ballMoving = false;
        m_ballFlightBaseDist = 0;
        m_kickoffKickerId.clear();
    }
    if (anyTokenAnimActive())
        ensureAnimTimer();
    // The ball's animated gold glow needs the pulse timer.
    if (needsPulse())
        m_pulseTimer->start();
    else
        m_pulseTimer->stop();
    update();
}

void FieldView::setTeamLogos(const QPixmap &home, const QPixmap &away)
{
    m_homeLogo = home;
    m_awayLogo = away;
    update();
}

void FieldView::setWeather(const QString &weather)
{
    if (m_weather == weather)
        return;
    m_weather = weather;
    rainConfigure(); // rain on/off starts/stops the animation
    update();
}

void FieldView::setStadium(const QString &name)
{
    if (m_stadiumName == name && m_stadium.valid)
        return;
    m_stadiumName = name;
    m_stadium = assets::stadiumManifest(name); // data-driven alignment
    update();
}

void FieldView::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    clampPan(); // re-center / re-clamp the letterboxed scene
    if (m_raining)
        rainConfigure(); // reseed the sim + puddles at the new size
}

// ---------------------------------------------------------------------------
// Animated rain: droplet streaks falling over the field, a splash ripple
// where each drop lands. The puddles (water patches drawn on the grass)
// catch their own drops with a BIGGER, longer ripple. The puddles are
// DYNAMIC (rainConfigure / stepPuddles): they accumulate over match time,
// are smaller, organic, and fade in as they appear (stable fractions of
// the field, so the water patches do not jump between frames/resizes).
// ---------------------------------------------------------------------------

void FieldView::rainConfigure()
{
    const bool rain = m_weather.toLower().contains(QLatin1String("rain"));
    m_raining = rain;
    if (!rain) {
        if (m_rainTimer)
            m_rainTimer->stop();
        m_puddles.clear();
        return;
    }
    const QRectF f = fieldRect();
    if (f.isEmpty())
        return;

    // Puddles are DYNAMIC — they accumulate over match time (stepPuddles),
    // not all at once. Seed a couple of "pre-existing"
    // ones (already faded in) so the field isn't bare, then let the rest
    // grow over time.
    m_puddles.clear();
    m_puddleClockMs = 0.0;
    for (int i = 0; i < 3; ++i) {
        Puddle pd;
        pd.fx = 0.10 + 0.80 * (QRandomGenerator::global()->bounded(1000) / 1000.0);
        pd.fy = 0.10 + 0.80 * (QRandomGenerator::global()->bounded(1000) / 1000.0);
        pd.fr = 0.024 + 0.020 * (QRandomGenerator::global()->bounded(1000) / 1000.0);
        pd.squash = 0.55 + 0.25 * (QRandomGenerator::global()->bounded(1000) / 1000.0);
        pd.rotDeg = QRandomGenerator::global()->bounded(360);
        pd.birthMs = -2000.0 - i * 400.0; // pre-existing: already faded in
        m_puddles.append(pd);
    }
    m_nextPuddleAt = 2500.0 + QRandomGenerator::global()->bounded(2500);

    // "Light Rain" (and friends) is sparser than a downpour.
    const int intensity =
        m_weather.toLower().contains(QLatin1String("light")) ? 45 : 85;
    m_rain.configure(f.width(), f.height(), puddleRects());
    m_rain.setIntensity(intensity);

    if (!m_rainTimer) {
        m_rainTimer = new QTimer(this);
        m_rainTimer->setInterval(33); // ~30 fps
        connect(m_rainTimer, &QTimer::timeout, this, [this]() {
            const float dt = float(m_rainClock.isValid() ? m_rainClock.restart()
                                                         : 33.0);
            const float dtc = qMin(dt, 100.0f);
            m_rain.step(dtc);
            stepPuddles(dtc);
            update();
        });
    }
    m_rainClock.start();
    m_rainTimer->start();
}

QRectF FieldView::puddleRect(const Puddle &pd, const QRectF &f) const
{
    const QPointF c(f.x() + pd.fx * f.width(), f.y() + pd.fy * f.height());
    const qreal rx = pd.fr * f.height();
    const qreal ry = rx * pd.squash;
    // Bounding box of the (rotated) ellipse ~ a square on the max axis.
    const qreal m = qMax(rx, ry);
    return QRectF(c.x() - m, c.y() - m, m * 2, m * 2);
}

QVector<QRectF> FieldView::puddleRects() const
{
    const QRectF f = fieldRect();
    QVector<QRectF> rects;
    rects.reserve(m_puddles.size());
    for (const Puddle &pd : m_puddles)
        rects.append(puddleRect(pd, f));
    return rects;
}

void FieldView::stepPuddles(float dtMs)
{
    m_puddleClockMs += dtMs;
    const int maxPuddles = 9;
    if (m_puddles.size() < maxPuddles && m_puddleClockMs >= m_nextPuddleAt) {
        Puddle pd;
        pd.fx = 0.05 + 0.90 * (QRandomGenerator::global()->bounded(1000) / 1000.0);
        pd.fy = 0.08 + 0.84 * (QRandomGenerator::global()->bounded(1000) / 1000.0);
        pd.fr = 0.022 + 0.020 * (QRandomGenerator::global()->bounded(1000) / 1000.0);
        pd.squash = 0.55 + 0.25 * (QRandomGenerator::global()->bounded(1000) / 1000.0);
        pd.rotDeg = QRandomGenerator::global()->bounded(360);
        pd.birthMs = m_puddleClockMs;
        m_puddles.append(pd);
        m_rain.setPuddles(puddleRects()); // bigger ripples on the new water
        m_nextPuddleAt = m_puddleClockMs + 2500.0
                         + QRandomGenerator::global()->bounded(2500);
    }
}

void FieldView::drawPuddles(QPainter &p, const QRectF &f) const
{
    // Smaller, organic puddles that FADE IN as they appear (they
    // accumulate over match time, not all at once).
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    for (const Puddle &pd : m_puddles) {
        const qreal fadeIn =
            qBound(0.0, (m_puddleClockMs - pd.birthMs) / 1800.0, 1.0);
        if (fadeIn <= 0.0)
            continue;
        const QPointF c(f.x() + pd.fx * f.width(), f.y() + pd.fy * f.height());
        const qreal rx = pd.fr * f.height();
        const qreal ry = rx * pd.squash;
        p.save();
        p.translate(c);
        p.rotate(pd.rotDeg);
        p.setOpacity(fadeIn);
        // Soft water: a radial gradient (darker heart, wet edge) — much
        // prettier than the old flat fill + rim.
        QRadialGradient g(QPointF(-rx * 0.25, -ry * 0.30), rx * 1.15);
        g.setColorAt(0.0, QColor(58, 92, 120, 115));
        g.setColorAt(0.7, QColor(38, 66, 92, 95));
        g.setColorAt(1.0, QColor(22, 44, 66, 128));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(QPointF(0, 0), rx, ry);
        // Wet rim.
        p.setPen(QPen(QColor(18, 38, 58, 120), qMax(1.0, rx * 0.06)));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(0, 0), rx, ry);
        // Small glint (top-left).
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(172, 206, 228, 55));
        p.drawEllipse(QPointF(-rx * 0.30, -ry * 0.34), rx * 0.32, ry * 0.26);
        p.restore();
    }
    p.restore();
}

void FieldView::drawRain(QPainter &p)
{
    // The sim is in PITCH space (0..pitch); translate into scene space.
    const QRectF f = fieldRect();
    p.save();
    p.translate(f.x(), f.y());
    p.setRenderHint(QPainter::Antialiasing, true);
    // TOP-DOWN droplets — the field is seen from ABOVE. Each drop is a
    // SHORT bright streak that GROWS as it descends
    // (top-down pseudo-depth: small/high -> big/low), with a consistent
    // wind slant. The sim spawns/lands them uniformly over the WHOLE field
    // (the old "fall from the top" left rain only on the upper band and the
    // dots read as a flat platform layer).
    p.setPen(QPen(QColor(226, 238, 252, 190), 1.4, Qt::SolidLine,
                  Qt::RoundCap));
    for (const RainSim::Drop &d : m_rain.drops()) {
        const float t = qMax(0.0f, qMin(1.0f, d.lifeMs / d.maxLifeMs));
        const float grow = 0.35f + 0.65f * t;      // descends = grows
        const float len = d.len * grow;
        const QPointF head(d.x, d.y);
        const QPointF tail(d.x - std::cos(d.angle) * len,
                           d.y - std::sin(d.angle) * len);
        p.setPen(QPen(QColor(226, 238, 252, int(120 + 90 * t)), 1.4,
                      Qt::SolidLine, Qt::RoundCap));
        p.drawLine(tail, head);
    }

    // Splash ripples: CIRCULAR rings on the ground (top-down water), bigger
    // and longer on the puddles.
    p.setBrush(Qt::NoBrush);
    for (const RainSim::Splash &s : m_rain.splashes()) {
        const float maxAge = s.onPuddle ? 700.0f : 420.0f;
        const float t = qMin(1.0f, s.ageMs / maxAge);
        const float r = (s.onPuddle ? 2.5f : 1.8f) +
                        t * (s.onPuddle ? 8.0f : 4.0f);
        const int a = int((1.0 - t) * (s.onPuddle ? 200 : 150));
        p.setPen(QPen(QColor(228, 240, 252, a), 1.3));
        p.drawEllipse(QPointF(s.x, s.y), r, r);
        if (s.onPuddle) {
            p.setPen(QPen(QColor(228, 240, 252, a / 2), 1.0));
            p.drawEllipse(QPointF(s.x, s.y), r * 0.55, r * 0.55);
        }
    }
    p.restore();
}

// Mowing stripes: every GRASS column (1..24; 0/25 are the TD columns) is
// tinted light or dark alternating — the same values the
// mockup baked (tools/mockup/compose_f.py grid_mow_tint: even x1.252, odd
// x0.884). Drawn as a white/black overlay in code so it stays perfectly
// grid-aligned and crisp at any zoom.
void FieldView::drawMowStripes(QPainter &p, const QRectF &f) const
{
    const qreal cellW = f.width() / kFieldWidth;
    p.save();
    p.setPen(Qt::NoPen);
    for (int i = 1; i < kFieldWidth - 1; ++i) {
        const QRectF col(f.x() + i * cellW, f.y(), cellW, f.height());
        p.setBrush(i % 2 == 0 ? QColor(255, 255, 255, 42) : QColor(0, 0, 0, 32));
        p.fillRect(col, p.brush().color());
    }
    p.restore();
}

// The crimson 1-column TOUCHDOWN zones + the vertical vector "TOUCHDOWN"
// (code-drawn — the baked version blurred with the texture at zoom).
// Port of the mockup's narrow_td_zone (crimson 128,30,28 + white
// 900-weight text, rotated 90 deg, reads top-to-bottom, centered in the
// column).
void FieldView::drawTouchdownZones(QPainter &p, const QRectF &f) const
{
    const qreal cellW = f.width() / kFieldWidth;
    const QColor crimson(128, 30, 28);
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(crimson);
    p.fillRect(QRectF(f.x(), f.y(), cellW, f.height()), crimson);
    p.fillRect(QRectF(f.right() - cellW, f.y(), cellW, f.height()), crimson);

    // Vector TOUCHDOWN running the FULL length of the TD zone (keeping the
    // vector sharpness). The word is long for a 1-column zone, so it
    // is set to a size where its advance == the target run, then condensed
    // (scaled) horizontally into the column width — a tall stretched
    // end-zone word, still razor-sharp (pure vector transform, no raster).
    QFont tf = p.font();
    tf.setWeight(QFont::Black);
    tf.setPointSizeF(20.0);
    QFontMetricsF fm(tf);
    const QString text = QStringLiteral("TOUCHDOWN");
    const qreal targetLen = 0.94 * f.height(); // run the whole zone height
    const qreal colW = cellW * 0.86;           // fit inside the 1-col zone
    const qreal ratio = targetLen / fm.horizontalAdvance(text);
    tf.setPointSizeF(tf.pointSizeF() * ratio);
    fm = QFontMetricsF(tf);
    const qreal adv = fm.horizontalAdvance(text);    // ~ targetLen (vertical)
    const qreal scaleY = colW / qMax(1.0, fm.capHeight()); // condense width
    p.setFont(tf);
    p.setPen(QColor(255, 255, 255, 235));
    // The words face OUTWARD, mirrored: the LEFT word's letter-tops point
    // LEFT (rotated 270 deg, reads bottom->top), the RIGHT word's
    // letter-tops point RIGHT (rotated 90 deg, reads top->bottom).
    const QPair<qreal, qreal> cols[2] = {
        { f.x() + cellW * 0.5, 270.0 },
        { f.right() - cellW * 0.5, 90.0 },
    };
    for (const QPair<qreal, qreal> &col : cols) {
        p.save();
        p.translate(col.first, f.center().y());
        p.rotate(col.second);
        p.scale(1.0, scaleY); // condense the thickness into the column
        p.drawText(QPointF(-adv / 2, fm.capHeight() / 2), text);
        p.restore();
    }
    p.restore();
}

// The ball's gold glow breathes, so the pulse timer must run while the
// ball is
// visible ANYWHERE: on an empty field square (m_ballInPlay), gliding in
// flight (the __ball__ token anim), or as the ball-in-hand marker on a
// token (hasBall). No idle repaints: when the ball is gone (and no other
// glow is armed) the timer stops.
bool FieldView::ballGlowVisible() const
{
    if (m_ballInPlay)
        return true;
    if (m_tokenAnim.value(QLatin1String("__ball__")).active)
        return true;
    for (const PlayerToken &t : m_homePlayers)
        if (t.hasBall)
            return true;
    for (const PlayerToken &t : m_awayPlayers)
        if (t.hasBall)
            return true;
    return false;
}

bool FieldView::needsPulse() const
{
    return !m_selectedPlayer.isEmpty() || !m_blockTarget.isEmpty()
           || !m_passTarget.isEmpty() || ballGlowVisible();
}

void FieldView::setSelectedPlayer(const QString &playerId)
{
    if (m_selectedPlayer == playerId)
        return;
    m_selectedPlayer = playerId;
    // The pulse timer runs only while something animated is visible
    // (no idle repaints on the field).
    if (needsPulse())
        m_pulseTimer->start();
    else
        m_pulseTimer->stop();
    update();
}

void FieldView::setBlockMarker(const QString &attackerId, const QString &defenderId,
                               int dieFace)
{
    if (m_blockTarget == defenderId && m_blockAttacker == attackerId
        && m_blockDieFace == dieFace)
        return;
    m_blockTarget = defenderId;
    m_blockAttacker = attackerId;
    m_blockDieFace = dieFace;
    // The red block glow breathes with the same pulse timer as the
    // selection glow (+ the ball's gold glow).
    if (needsPulse())
        m_pulseTimer->start();
    else
        m_pulseTimer->stop();
    update();
}

void FieldView::setActedPlayers(const QSet<QString> &ids)
{
    if (m_actedPlayers == ids)
        return;
    m_actedPlayers = ids;
    update();
}

void FieldView::setBlitzingPlayer(const QString &playerId)
{
    if (m_blitzingPlayer == playerId)
        return;
    m_blitzingPlayer = playerId;
    update();
}

void FieldView::setPassTarget(const QString &playerId)
{
    if (m_passTarget == playerId)
        return;
    m_passTarget = playerId;
    // The white receiver glow breathes with the same pulse timer (+ the
    // ball's gold glow).
    if (needsPulse())
        m_pulseTimer->start();
    else
        m_pulseTimer->stop();
    update();
}

void FieldView::setActionWheelTarget(const QString &playerId)
{
    if (m_wheelTarget == playerId)
        return;
    m_wheelTarget = playerId;
    update();
}

void FieldView::setPlayMode(bool play)
{
    m_playMode = play;
}

void FieldView::keyPressEvent(QKeyEvent *event)
{
    // SMOOTH WASD pan: track the held keys and let a fast timer pan
    // continuously — no jumpy auto-repeat steps. Only meaningful while
    // zoomed (at zoom 1 the letterboxed scene cannot pan).
    const int key = event->key();
    const bool wasd = key == Qt::Key_W || key == Qt::Key_A ||
                      key == Qt::Key_S || key == Qt::Key_D;
    if (wasd && m_zoom > 1.0001) {
        m_panKeys.insert(key);
        if (!m_panTimer) {
            m_panTimer = new QTimer(this);
            m_panTimer->setInterval(16); // ~60 fps, smooth
            connect(m_panTimer, &QTimer::timeout, this, &FieldView::smoothPanTick);
        }
        if (!m_panTimer->isActive())
            m_panTimer->start();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void FieldView::keyReleaseEvent(QKeyEvent *event)
{
    m_panKeys.remove(event->key());
    if (m_panTimer && m_panKeys.isEmpty() && m_panTimer->isActive())
        m_panTimer->stop();
    QWidget::keyReleaseEvent(event);
}

void FieldView::smoothPanTick()
{
    if (m_zoom <= 1.0001 || m_panKeys.isEmpty())
        return;
    const qreal speed = 2.0; // widget px per 16ms frame (~125 px/s)
    QPointF d;
    if (m_panKeys.contains(Qt::Key_W)) d += QPointF(0.0, speed);   // look up
    if (m_panKeys.contains(Qt::Key_S)) d += QPointF(0.0, -speed);  // look down
    if (m_panKeys.contains(Qt::Key_A)) d += QPointF(speed, 0.0);   // look left
    if (m_panKeys.contains(Qt::Key_D)) d += QPointF(-speed, 0.0);  // look right
    if (d.isNull())
        return;
    m_pan += d;
    clampPan();
    update();
}

void FieldView::setMoveTrace(const QVector<GameModel::MoveSegment> &segments,
                             const QVector<GameModel::BlockStepMark> &marks)
{
    // (No cheap equality check — the segments carry numbering state the
    // cell-only view can't express; the re-set is harmless.)
    if (qEnvironmentVariableIsSet("FUMBBL_DEBUG_BADGES"))
        fprintf(stderr, "SET_TRACE segs=%d marks=%d\n", segments.size(),
                marks.size());
    m_moveSegments = segments;
    m_blockMarks = marks;
    update();
}

// ---------------------------------------------------------------------------
// Per-square movement animation. A moved token glides through the grid
// (old cell -> new cell, one square per leg) instead of jumping — fast
// and pleasant, like the official client. The model is authoritative;
// this is pure render state.
// ---------------------------------------------------------------------------
void FieldView::startTokenAnim(const QString &id, const QPoint &to)
{
    const QPoint from = m_prevModel.value(id,
                       id == QLatin1String("__ball__") ? m_prevBall : to);
    if (from == to)
        return;
    TokenAnim a;
    a.active = true;
    a.leg = 0;
    a.t = 0.0;
    a.legMs = (id == QLatin1String("__ball__") && m_kickoffArmed)
            ? 48 : FieldView::kLegDur; // kickoff: a fast, long loft
    a.waypoints.append(QPointF(from.x() + 0.5, from.y() + 0.5));
    // Grid path from `from` to `to`: step one square at a time, preferring the
    // axis with the larger remaining distance (axis-aligned moves come out as
    // a clean straight run of squares).
    int cx = from.x(), cy = from.y();
    while (cx != to.x() || cy != to.y()) {
        const bool doX = (cx != to.x())
            && (cy == to.y() || qAbs(to.x() - cx) >= qAbs(to.y() - cy));
        if (doX) cx += (to.x() > cx ? 1 : -1);
        else     cy += (to.y() > cy ? 1 : -1);
        a.waypoints.append(QPointF(cx + 0.5, cy + 0.5));
    }
    m_tokenAnim.insert(id, a);
    m_visualPos.insert(id, a.waypoints.first());
    ensureAnimTimer();
}

void FieldView::ensureAnimTimer()
{
    if (!m_animTimer) {
        m_animTimer = new QTimer(this);
        m_animTimer->setInterval(16); // ~60 fps
        connect(m_animTimer, &QTimer::timeout, this, &FieldView::animTick);
    }
    if (!m_animTimer->isActive())
        m_animTimer->start();
}

bool FieldView::anyTokenAnimActive() const
{
    for (const TokenAnim &a : m_tokenAnim)
        if (a.active)
            return true;
    return false;
}

void FieldView::animTick()
{
    const qreal dt = 0.016; // 16 ms frame
    bool any = false;
    for (auto it = m_tokenAnim.begin(); it != m_tokenAnim.end(); ++it) {
        TokenAnim &a = it.value();
        if (!a.active)
            continue;
        any = true;
        a.t += dt * 1000.0 / qreal(qMax(1, a.legMs));
        while (a.t >= 1.0) {
            if (a.leg + 1 < a.waypoints.size()) {
                a.t -= 1.0;
                ++a.leg;
            } else {
                a.t = 1.0;
                break;
            }
        }
        if (a.leg + 1 >= a.waypoints.size()) {
            m_visualPos.insert(it.key(), a.waypoints.last());
            a.active = false;
        } else {
            const QPointF &p0 = a.waypoints[a.leg];
            const QPointF &p1 = a.waypoints[a.leg + 1];
            m_visualPos.insert(it.key(), p0 + (p1 - p0) * a.t);
        }
    }
    if (any)
        update();
    else if (m_animTimer)
        m_animTimer->stop();
}

QPointF FieldView::tokenVisualPos(const PlayerToken &t) const
{
    auto it = m_visualPos.constFind(t.playerId);
    if (it != m_visualPos.constEnd())
        return it.value();
    return QPointF(t.x + 0.5, t.y + 0.5);
}

QPointF FieldView::cellCenter(int x, int y) const
{
    const QRectF c = cellRect(x, y);
    return c.center();
}

// The actor's CUMULATIVE move trail: one polyline per move action,
// through the FIELD CELL CENTERS (the line follows the grid — a
// center-to-center segment per stepped square, never a straight line from
// start to end), with an arrow at the end of each path. White over a dark
// under-stroke so it reads on both the grass and the crimson TD. Drawn in
// the zoomed scene space, under the tokens (the trail belongs to the field,
// the player walks on top of it).
void FieldView::drawMoveTrail(QPainter &p)
{
    if (m_moveSegments.isEmpty() && m_blockMarks.isEmpty())
        return;
    const QRectF f = fieldRect();
    if (f.isEmpty())
        return;
    const qreal cellW = f.width() / kFieldWidth;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    for (const GameModel::MoveSegment &seg : m_moveSegments) {
        const QVector<QPoint> &path = seg.cells;
        // The go-for-it extra move is drawn RED.
        const bool gfi = seg.goForIt;
        if (path.size() < 2)
            continue; // a single square: nothing to trace yet
        // Defensive: a path with NO real movement (every cell
        // identical, e.g. a server re-placement) would draw a degenerate
        // zero-length arrow. Skip it.
        bool anyMove = false;
        for (int i = 1; i < path.size(); ++i)
            if (path[i] != path[i - 1]) {
                anyMove = true;
                break;
            }
        if (!anyMove)
            continue;
        QVector<QPointF> pts;
        pts.reserve(path.size());
        for (const QPoint &c : path)
            pts << cellCenter(c.x(), c.y());
        const QPointF b = pts.last();          // the token's cell center
        const QPointF a = pts[pts.size() - 2]; // previous stepped cell
        const QPointF dir(b.x() - a.x(), b.y() - a.y());
        const qreal len = std::hypot(dir.x(), dir.y());
        const QPointF u(len > 1e-6 ? QPointF(dir.x() / len, dir.y() / len)
                                   : QPointF(1.0, 0.0));
        // The trail must leave a SMALL GAP before the player box, end in
        // an ARROW, and the arrow must NOT invade the
        // selection glow. Pull the end back from the token: just outside the
        // glow when the trail's endpoint is the selected player, otherwise a
        // small gap beyond the token.
        bool endpointSelected = false;
        {
            const QPoint ec = path.last();
            const auto isSel = [this, ec](const QVector<PlayerToken> &team) {
                for (const PlayerToken &t : team)
                    if (t.x == ec.x() && t.y == ec.y()
                        && t.playerId == m_selectedPlayer)
                        return true;
                return false;
            };
            endpointSelected = isSel(m_homePlayers) || isSel(m_awayPlayers);
        }
        const qreal minDim = qMin(cellW, f.height() / kFieldHeight);
        const qreal arrowLen = minDim * 0.28;
        const qreal desiredPull = endpointSelected ? minDim * 1.05 : minDim * 0.60;
        const qreal pull = qMin(desiredPull, len - arrowLen);
        const QPointF tip = b - u * pull;      // arrow tip (in the gap)
        const QPointF base = tip - u * arrowLen;
        // Dark under-stroke + white main stroke (rounded joins: the path
        // turns along the grid). The line runs through every stepped center
        // up to the previous cell, then out to the arrow base — it stops in
        // the gap, it does NOT reach the token.
        QVector<QPointF> linePts;
        linePts.reserve(pts.size());
        for (int i = 0; i < pts.size() - 1; ++i)
            linePts << pts[i];
        linePts << base;
        p.setPen(QPen(QColor(24, 24, 30, 200), cellW * 0.16,
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(linePts.constData(), linePts.size());
        p.setPen(QPen(gfi ? QColor(226, 44, 44, 235) : QColor(255, 255, 255, 235),
                      cellW * 0.075, Qt::SolidLine, Qt::RoundCap,
                      Qt::RoundJoin));
        p.drawPolyline(linePts.constData(), linePts.size());
        // Arrowhead at the end of the trail, pointing along the last step.
        const QPointF perp(-u.y(), u.x());
        if (qEnvironmentVariableIsSet("FUMBBL_DEBUG_BADGES"))
            fprintf(stderr, "ARROW tip=(%.0f,%.0f) base=(%.0f,%.0f) gfi=%d last=(%d,%d)\n",
                    tip.x(), tip.y(), base.x(), base.y(), gfi ? 1 : 0,
                    path.last().x(), path.last().y());
        QPolygonF tri;
        tri << tip << (base + perp * arrowLen * 0.62)
            << (base - perp * arrowLen * 0.62);
        p.setBrush(gfi ? QColor(226, 44, 44, 240) : QColor(255, 255, 255, 240));
        p.setPen(QPen(QColor(24, 24, 30, 200), qMax(1.5, cellW * 0.05)));
        p.drawPolygon(tri);
    }

    // BETWEEN-CELLS move numbers: the step number is painted BETWEEN two
    // consecutive cells of the route — never on a cell, never
    // on the trail line. A route of N steps shows 1..N: number k sits on
    // the shared edge of step k's two cells, offset perpendicular to the
    // line so it never touches or overlaps the trail (dark under-stroke
    // included). Consequences:
    //   * the LAST cell is numbered too: the last number of the move
    //     appears between the second-to-last and the last cell;
    //   * a 1-SQUARE move shows its "1" between the two cells (before:
    //     nothing — the server's only TrackNumber for that move is the 0
    //     on the starting cell, which was not painted);
    //   * a BLITZ and a follow are move routes too: same numbers.
    // The numbers live at the shared border of the two cells of the step
    // (the border line is perpendicular to the trail), slid along the
    // border away from the trail's crossing point so they never overlap
    // the line. Two tiers:
    //   * intermediate steps (endpoint cell EMPTY): 0.26*minDim from the
    //     crossing — clear of the dark under-stroke (half-width 0.08), tight
    //     to the "between the cells" spot;
    //   * the FINAL step (endpoint cell = the actor's resting cell): the
    //     actor's selection halo (r ~0.95-1.03) swallows the whole shared
    //     border of that cell, so ANY border number reads as "sul token"
    //     (three explicit rejections of border placements: 0.30-slide,
    //     corner vertex, 0.15-hug). The final
    //     number therefore sits in the PENULTIMATE (empty) cell, 0.30
    //     off the trail's last leg, anchored at that cell's center: the
    //     only spot that is beside the trail AND outside the token's halo
    //     (badge center 1.044 from the token center). It marks the last
    //     step exactly like the other numbers mark theirs.
    // The side is "left of the direction of travel" (an orderly row along
    // the route); a number whose endpoint cell is occupied takes whichever
    // side clears the occupant's corner badges (number/ball/check/
    // lightning/'?'/disc) and the other placed numbers best. Badge = the
    // approved token-number-badge language: opaque white disc, dark rim,
    // bold black digit.
    {
        const qreal cellH = f.height() / kFieldHeight;
        const qreal minDim = qMin(cellW, cellH);
        // The endpoint cell may be occupied (the FINAL cell is the actor's
        // resting cell). Its corner badges, in minDim units relative to the
        // token center: number (bottom-right, always), ball (top-left),
        // green check (top-right, acted this turn), lightning (top-left,
        // blitzed), "?" (bottom-left, confused), and the token disc.
        struct CornerBadge { QPointF c; qreal r; };
        const auto occupantBadges = [this](const PlayerToken &t) {
            QVector<CornerBadge> v;
            v.append({QPointF(0.276, 0.294), 0.15}); // jersey number
            if (t.hasBall)
                v.append({QPointF(-0.313, -0.350), 0.16}); // ball-in-hand
            if (m_actedPlayers.contains(t.playerId))
                v.append({QPointF(0.239, -0.258), 0.15}); // green check
            if (t.playerId == m_blitzingPlayer)
                v.append({QPointF(-0.239, -0.258), 0.15}); // lightning
            if (t.state & 0x00200)
                v.append({QPointF(-0.239, 0.258), 0.15}); // confused "?"
            v.append({QPointF(0.0, 0.0), 0.46}); // the token disc
            return v;
        };
        const auto occupantAt = [this](int x, int y) -> const PlayerToken * {
            for (const auto &team : { m_homePlayers, m_awayPlayers })
                for (const PlayerToken &t : team)
                    if (t.x == x && t.y == y)
                        return &t;
            return nullptr;
        };
        // One entry per numbered segment of every route.
        struct Num {
            int n = 0;          // the digit (segment index + 1)
            QPointF mid;        // the shared border's midpoint
            QPointF cStart;     // the penultimate cell's center (a)
            QPointF cEnd;       // the endpoint cell's center (b)
            QPointF u;          // unit travel direction (a -> b)
            QPointF perp;       // unit vector ALONG the border, left of travel
            QPoint end;         // endpoint cell (may hold the resting token)
            bool isBlock = false; // block mark (no closing arrow beside it)
        };
        QVector<Num> nums;
        // The numbers come from the MODEL (segment.stepNumbers — continuous
        // across the whole action: sideways step 1, block 2, follow 3,4,5...
        // frenzy block next, GFI move continues).
        for (const GameModel::MoveSegment &seg : m_moveSegments) {
            for (int i = 0;
                 i + 1 < seg.cells.size() && i < seg.stepNumbers.size();
                 ++i) {
                if (seg.cells[i] == seg.cells[i + 1])
                    continue; // degenerate no-op segment: no number
                const QPointF a = cellCenter(seg.cells[i].x(), seg.cells[i].y());
                const QPointF b =
                    cellCenter(seg.cells[i + 1].x(), seg.cells[i + 1].y());
                const QPointF dir = b - a;
                const qreal segLen = std::hypot(dir.x(), dir.y());
                if (segLen < 1e-6)
                    continue;
                const QPointF u(dir.x() / segLen, dir.y() / segLen);
                Num num;
                num.n = seg.stepNumbers[i];
                num.mid = (a + b) / 2.0;
                num.cStart = a;
                num.cEnd = b;
                num.u = u;
                num.perp = QPointF(u.y(), -u.x()); // left of the travel
                num.end = seg.cells[i + 1];
                nums.append(num);
            }
        }
        // The block COSTS A MOVEMENT — its number is painted between the
        // attacker's cell and the defender's cell, like the other
        // between-cells numbers (the endpoint cell is the defender's — the
        // placement logic anchors it in the (empty) attacker cell, off the
        // defender's glow/badges).
        for (const GameModel::BlockStepMark &mk : m_blockMarks) {
            const QPointF a = cellCenter(mk.from.x(), mk.from.y());
            const QPointF b = cellCenter(mk.to.x(), mk.to.y());
            const QPointF dir = b - a;
            const qreal segLen = std::hypot(dir.x(), dir.y());
            if (segLen < 1e-6)
                continue;
            const QPointF u(dir.x() / segLen, dir.y() / segLen);
            Num num;
            num.n = mk.number;
            num.mid = (a + b) / 2.0;
            num.cStart = a;
            num.cEnd = b;
            num.u = u;
            num.perp = QPointF(u.y(), -u.x()); // left of the travel
            num.end = mk.to;
            num.isBlock = true;
            nums.append(num);
        }
        // The FINAL number's anchor is 0.35 ALONG the last leg from its
        // start — beside the
        // MIDDLE of the closing arrowhead (which spans the first 0.72 of
        // the leg), not beside its base. Block marks (no arrow) stay at the
        // attacker cell's center.
        const auto finalAnchor = [minDim](const Num &num) {
            return num.isBlock ? num.cStart
                               : num.cStart + num.u * (minDim * 0.35);
        };
        // Pass 1: the default side (left of travel) at the tiered distance.
        QVector<QPointF> pos(nums.size());
        for (int k = 0; k < nums.size(); ++k) {
            const bool occupied =
                occupantAt(nums[k].end.x(), nums[k].end.y()) != nullptr;
            if (occupied)
                pos[k] = finalAnchor(nums[k]) + nums[k].perp * minDim * 0.28;
            else
                pos[k] = nums[k].mid + nums[k].perp * minDim * 0.26;
        }
        // Pass 2: a number whose endpoint cell is occupied (the actor's
        // resting cell) takes whichever side clears the occupant's corner
        // badges and the other placed numbers best.
        for (int k = 0; k < nums.size(); ++k) {
            const PlayerToken *occ =
                occupantAt(nums[k].end.x(), nums[k].end.y());
            if (!occ)
                continue;
            const auto clearance = [&](const QPointF &c) {
                qreal m = 1e9;
                for (const CornerBadge &bd : occupantBadges(*occ)) {
                    const QPointF d = c - (nums[k].cEnd + bd.c * minDim);
                    m = qMin(m, std::hypot(d.x(), d.y()) - bd.r * minDim);
                }
                for (int j = 0; j < nums.size(); ++j) {
                    if (j == k)
                        continue;
                    m = qMin(m, std::hypot(pos[j].x() - c.x(),
                                           pos[j].y() - c.y())
                               - minDim * 0.33); // two badges (r 0.165 each)
                }
                return m;
            };
            const QPointF anchor = finalAnchor(nums[k]);
            const QPointF candA = anchor + nums[k].perp * minDim * 0.28;
            const QPointF candB = anchor - nums[k].perp * minDim * 0.28;
            if (clearance(candB) > clearance(candA))
                pos[k] = candB;
        }
        for (int k = 0; k < nums.size(); ++k) {
            const int n = nums[k].n;
            const QPointF at = pos[k];
            if (qEnvironmentVariableIsSet("FUMBBL_DEBUG_BADGES"))
                fprintf(stderr, "BADGE n=%d at=(%.2f,%.2f) end=(%d,%d) occupied=%d\n",
                        n, at.x(), at.y(), nums[k].end.x(), nums[k].end.y(),
                        occupantAt(nums[k].end.x(), nums[k].end.y())
                            ? 1 : 0);
            // Badge radius 0.165 (the digit used to kiss the rim at r 0.13).
            const qreal br = minDim * 0.165;
            QFont nf = p.font();
            nf.setPointSizeF(qMax(4.0, minDim * (n > 9 ? 0.15 : 0.20)));
            nf.setBold(true);
            p.setFont(nf);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(250, 250, 246, 240));
            p.drawEllipse(at, br, br);
            p.setPen(QPen(QColor(46, 40, 34, 200), qMax(1.0, minDim * 0.02)));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(at, br, br);
            const QRectF box(at.x() - br, at.y() - br, br * 2, br * 2);
            p.setPen(QColor(20, 20, 24, 255));
            p.drawText(box, Qt::AlignCenter, QString::number(n));
        }
    }
    p.restore();
}

// ---------------------------------------------------------------------------
// Click handling
// ---------------------------------------------------------------------------
const FieldView::PlayerToken *FieldView::tokenAt(const QPoint &pos) const
{
    const QRectF f = fieldRect();
    if (f.isEmpty())
        return nullptr;
    const QPointF w = toFieldSpace(pos);
    if (!f.contains(w))
        return nullptr;
    const qreal cellW = f.width() / kFieldWidth;
    const qreal cellH = f.height() / kFieldHeight;
    const int x = int((w.x() - f.x()) / cellW);
    const int y = int((w.y() - f.y()) / cellH);
    if (x < 0 || x >= kFieldWidth || y < 0 || y >= kFieldHeight)
        return nullptr;

    // Away is drawn last (on top), so it wins the click.
    const auto find = [this, x, y](const QVector<PlayerToken> &team) -> const PlayerToken * {
        for (const PlayerToken &t : team)
            if (t.x == x && t.y == y)
                return &t;
        return nullptr;
    };
    return find(m_awayPlayers) ? find(m_awayPlayers) : find(m_homePlayers);
}

void FieldView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Left button = SELECT / show the info card. An EMPTY id (clicked
        // an empty square) is emitted too: the
        // caller uses it to HIDE the card. Emitted on the press, as before.
        const PlayerToken *t = tokenAt(event->pos());
        emit playerClicked(t ? t->playerId : QString());
        setFocus(Qt::MouseFocusReason); // WASD pan right after a click
    } else if (event->button() == Qt::MiddleButton) {
        // The view RESET is on the MIDDLE click (the right button is the
        // drag-pan).
        resetView();
    } else if (event->button() == Qt::RightButton) {
        // Right button + DRAG = pan. Record the press so mouseMove can pan
        // and mouseRelease can tell a drag
        // from a (play-mode, on-player) action-wheel click.
        m_rightPressPos = event->pos();
        m_rightDragLast = event->pos();
        m_rightDragging = false;
        const PlayerToken *t = tokenAt(event->pos());
        m_rightPressPlayerId = t ? t->playerId : QString();
    }
    QWidget::mousePressEvent(event);
}

void FieldView::mouseMoveEvent(QMouseEvent *event)
{
    // Right button + DRAG = pan: move the view with the
    // cursor while the right button is held. A move under the threshold is
    // still a click (no pan) so the play-mode action wheel keeps working.
    if (event->buttons() & Qt::RightButton) {
        QPoint delta = event->pos() - m_rightDragLast;
        if (!m_rightDragging) {
            const int manhattan = qAbs(event->pos().x() - m_rightPressPos.x()) +
                                  qAbs(event->pos().y() - m_rightPressPos.y());
            if (manhattan > 4)
                m_rightDragging = true; // crossed the click/drag threshold
            else
                delta = QPoint();       // still a click: no pan yet
        }
        if (m_rightDragging && delta != QPoint()) {
            m_pan += delta;
            clampPan();
            update();
        }
        m_rightDragLast = event->pos();
    }
    // Hover: field tokens first, then bench faces. The hover "key" (token
    // id, or the box-entry name) changes only when the target changes.
    const PlayerToken *t = tokenAt(event->pos());
    PlayerToken tok;
    QString key;
    if (t) {
        tok = *t;
        for (const PlayerToken &h : m_homePlayers)
            if (h.playerId == t->playerId) { tok.sideHome = true; break; }
        key = t->playerId;
    } else {
        bool side = true;
        const BoxEntry *be = boxEntryAt(event->pos(), side);
        if (be) {
            tok.name = be->name;
            tok.positionName = be->positionName;
            tok.state = be->state;
            tok.sideHome = side;
            tok.playerId = QString(); // resolved by name in the model
            key = QStringLiteral("box:") + be->name;
        }
    }
    if (key != m_hoverId) {
        m_hoverId = key;
        emit playerHovered(tok);
    }
}

void FieldView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Left click already fired on the press (see mousePressEvent); the
        // release does nothing.
    } else if (event->button() == Qt::RightButton) {
        // A right DRAG was a pan (nothing to do on release). A right CLICK
        // (no drag) in play mode on a player = action wheel; a plain right
        // click does not reset the view (the reset is the MIDDLE click).
        if (!m_rightDragging && m_playMode && !m_rightPressPlayerId.isEmpty()) {
            const PlayerToken *tRel = tokenAt(event->pos());
            if (tRel && tRel->playerId == m_rightPressPlayerId) {
                setActionWheelTarget(m_rightPressPlayerId);
                emit actionWheelRequested(m_rightPressPlayerId);
            }
        }
        m_rightPressPlayerId.clear();
        m_rightDragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void FieldView::leaveEvent(QEvent *)
{
    if (!m_hoverId.isEmpty()) {
        m_hoverId.clear();
        emit playerHovered(PlayerToken());
    }
}

void FieldView::wheelEvent(QWheelEvent *event)
{
    // WHEEL = ZOOM (anchored on the cursor). Panning is right-button+DRAG
    // or WASD (smooth). MIDDLE click resets.
    const QPoint delta = event->angleDelta();
    const int dy = delta.y();
    if (dy == 0) {
        event->accept();
        return;
    }
    const qreal factor = dy > 0 ? 1.15 : (1.0 / 1.15);
    const qreal newZoom = qBound(1.0, m_zoom * factor, 3.0);
    if (qFuzzyCompare(newZoom, m_zoom)) {
        event->accept();
        return;
    }
    const QPointF cursor = event->position();
    const QPointF world = toFieldSpace(cursor);
    m_zoom = newZoom;
    m_pan = cursor - world * m_zoom;
    if (m_zoom <= 1.0001)
        m_pan = QPointF();
    clampPan();
    update();
    setFocus(Qt::OtherFocusReason);
    event->accept();
}

void FieldView::resetView()
{
    m_zoom = 1.0;
    m_pan = QPointF();
    clampPan(); // center the letterboxed scene
    update();
}

QPointF FieldView::toFieldSpace(const QPointF &pos) const
{
    return (pos - m_pan) / m_zoom;
}

void FieldView::clampPan()
{
    // Keep the (zoomed) scene covering the widget. On an axis where the
    // zoomed scene is SMALLER than the widget (always true at zoom=1 for at
    // least one axis: the letterbox), the scene is centered on that axis.
    const QRectF s = sceneRect();
    const qreal zw = s.width() * m_zoom, zh = s.height() * m_zoom;
    const qreal W = width(), H = height();
    if (zw <= W)
        m_pan.setX((W - zw) / 2.0);
    else
        m_pan.setX(qBound(W - zw, m_pan.x(), 0.0));
    if (zh <= H)
        m_pan.setY((H - zh) / 2.0);
    else
        m_pan.setY(qBound(H - zh, m_pan.y(), 0.0));
}

QRectF FieldView::sceneRect() const
{
    // The SCENE is the STADIUM image, COVER-fitted to the widget (fill it,
    // preserve aspect). The anchor is the FIELD PIT CENTER
    // (not the image center): the field is the visual centerpiece, so it sits
    // in the middle of the widget and the stands + top-band dugout sit at a
    // predictable offset around it (data-driven, per the manifest). Returns
    // the stadium's drawn rect in WIDGET space (zoom 1); it may be LARGER
    // than the widget on one axis (the overflow is the crop). No stadium art
    // -> the widget rect (fallback, so the field still renders).
    if (width() <= 0 || height() <= 0)
        return QRectF();
    const QPixmap st = assets::stadiumImage(m_stadiumName);
    if (st.isNull())
        return QRectF(0, 0, width(), height());
    // CONTAIN fit: the WHOLE stadium (stands + top-band dugout + pit) is
    // always visible — never cropped, so the dugout and the field are always
    // on screen at any window size. The leftover (the widget's wider
    // dimension) is the dark stadium apron (filled in paintEvent).
    //
    // The rect is anchored at the ORIGIN (0,0) — the centering is
    // clampPan()'s job. Centering it HERE too double-applied the letterbox
    // offset: at zoom 1 the stadium was drawn shifted right by the whole
    // left margin, and the zoom-clamp range (computed as if the scene
    // started at 0) never let the right side reach the screen edge.
    const qreal scale = qMin(qreal(width()) / st.width(),
                             qreal(height()) / st.height());
    return QRectF(0.0, 0.0, st.width() * scale, st.height() * scale);
}

QRectF FieldView::mapNormRect(const QRectF &norm) const
{
    // Map a normalized (0..1, image-space) rect into scene (widget) space
    // using the cover transform. This is the heart of the data-driven
    // alignment: every stadium's manifest rects map through its own cover
    // rect, so the field + dugouts land correctly on ANY stadium.
    const QRectF s = sceneRect();
    if (s.isEmpty())
        return QRectF();
    return QRectF(s.x() + norm.x() * s.width(),
                  s.y() + norm.y() * s.height(),
                  norm.width() * s.width(),
                  norm.height() * s.height());
}

QRectF FieldView::fieldRect() const
{
    // The PITCH: the 26x15 field FILLING the stadium's FIELD PIT EXACTLY
    // (no empty apron strips above/below the field). The cells take the
    // pit's own
    // width/height (cellW = pitW/26, cellH = pitH/15): when the pit's
    // aspect is close to 26:15 (the stadiums are generated that way) the
    // cells stay (near-)square and the field covers 100% of the pit — no
    // apron, at ANY stadium size. This is also what makes the field/stadium
    // pairing UNIVERSAL: any pit, any size, is fully covered.
    const QRectF pit = m_stadium.valid
        ? mapNormRect(m_stadium.fieldPit)
        : QRectF(0, 0, width(), height()); // fallback: the whole widget
    return pit;
}

QRectF FieldView::dugoutRect(bool home) const
{
    if (!m_stadium.valid)
        return QRectF();
    return mapNormRect(home ? m_stadium.dugoutHome : m_stadium.dugoutAway);
}

QRectF FieldView::fieldRectPx() const
{
    const QRectF r = fieldRect();
    if (r.isEmpty())
        return QRectF();
    return QRectF(m_pan + r.topLeft() * m_zoom,
                  m_pan + r.bottomRight() * m_zoom);
}

QRectF FieldView::dugoutRectPx(bool home) const
{
    const QRectF r = dugoutRect(home);
    if (r.isEmpty())
        return QRectF();
    return QRectF(m_pan + r.topLeft() * m_zoom,
                  m_pan + r.bottomRight() * m_zoom);
}

QPoint FieldView::cellCenterPx(int x, int y) const
{
    // Widget-space center of cell (x,y) under the current zoom/pan (zoom 1 /
    // pan 0 = the fitted scene). Tests use this instead of hard-coding the
    // scene geometry.
    const QPointF c = m_pan + cellCenter(x, y) * m_zoom;
    return QPoint(int(c.x()), int(c.y()));
}

QRectF FieldView::cellRect(int x, int y) const
{
    const QRectF f = fieldRect();
    if (f.isEmpty())
        return QRectF();
    const qreal cellW = f.width() / kFieldWidth;
    const qreal cellH = f.height() / kFieldHeight;
    return QRectF(f.x() + x * cellW, f.y() + y * cellH, cellW, cellH);
}

// Dugout sections: the box sections, shared by the slot painter and the
// hover hit-test. State bases from ffb-common PlayerState: RESERVE = 9,
// KO = 5, INJURED = 6..8,10.
namespace {
// The dugout is a SINGLE bench section holding up to 16 players (the
// physical game's 3-section split is replaced by a small STATE BADGE on
// each face ring, like the number badges on field tokens):
//   * reserves (9)            : plain face, no badge
//   * KO (5)                  : little cartoon "dizzy" stars (top-left)
//   * injured (6,7) / banned (13) / missing (10): red medical cross
//   * dead (8)                : a small skull
// The badge sits TOP-LEFT of the ring (so it never covers the number,
// which stays bottom-right). The wood panel is a SEPARATE asset MOUNTED on
// the manifest's dugout rect: the dugout is UNIVERSAL — any stadium ships
// its rect, the same panel mounts on it.
constexpr int kDugoutMaxFaces = 16;

struct DugoutLayout {
    qreal faceD = 0;
    QVector<QPointF> centers; // one per face, in entries order
};

// The face size is FIXED — sized by the 16-slot grid, NEVER by the bench
// count. The grid is 2 rows x 8 slots = room for 16 per
// dugout; players fill the slots in order (left->right, top->bottom) and
// ONLY the present players are drawn ("non voglio empty slot"). Wide, even
// spacing ("spazia di più"). Shared by boxSlots (hit-test) and
// paintBoxStrip (painter) so the two never drift.
DugoutLayout dugoutFaceLayout(const QRectF &R, int count)
{
    DugoutLayout out;
    count = qBound(0, count, kDugoutMaxFaces);
    if (count == 0)
        return out;
    constexpr int kCols = 8, kRows = 2; // 16 slots
    const qreal marginX = R.width() * 0.03;
    const qreal cellW = (R.width() - 2 * marginX) / kCols;
    out.faceD = cellW * 0.78;              // FIXED, 16-slot sized
    const qreal gap = cellW - out.faceD;   // generous, even
    const qreal blockH = kRows * out.faceD + (kRows - 1) * gap;
    const qreal x0 = R.x() + marginX;
    const qreal y0 = R.center().y() - blockH / 2;
    for (int i = 0; i < count; ++i) {
        const int row = i / kCols;
        const int col = i % kCols;
        out.centers.append(QPointF(x0 + (col + 0.5) * cellW,
                                   y0 + (row + 0.5) * out.faceD + row * gap));
    }
    return out;
}

// A tiny 5-point star (the cartoon "dizzy" one).
void drawStar(QPainter &p, const QPointF &c, qreal r, qreal rot,
              const QColor &color)
{
    QPolygonF poly;
    for (int i = 0; i < 10; ++i) {
        const qreal rr = (i % 2 == 0) ? r : r * 0.45;
        const qreal a = rot + i * M_PI / 5.0 - M_PI / 2;
        poly << c + QPointF(std::cos(a) * rr, std::sin(a) * rr);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPolygon(poly);
}

// The little STATE badge on the dugout face ring: TOP-RIGHT of the ring,
// made EXACTLY LIKE THE NUMBER BADGE (dark disc + gold rim) but SLIGHTLY
// MORE INTERNAL and smaller. Vector — QPainter is the right tool for
// small crisp badges.
// Comic-style bandage (the KO badge): a capsule rotated ~35°, tan body,
// lighter center pad with tiny dots, bold dark outline (cartoon look).
void drawComicBandage(QPainter &p, const QPointF &c, qreal br)
{
    p.save();
    p.translate(c);
    p.rotate(-35);
    const qreal w = br * 1.72, h = br * 0.88, r = h / 2;
    // Body (tan) with a bold comic outline.
    p.setPen(QPen(QColor(58, 40, 24, 255), qMax(1.0, br * 0.10)));
    p.setBrush(QColor(233, 197, 148, 255));
    p.drawRoundedRect(QRectF(-w / 2, -h / 2, w, h), r, r);
    // Center pad (lighter) + the little ventilation dots.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(248, 228, 199, 255));
    const qreal pw = w * 0.42, ph = h * 0.62;
    p.drawRoundedRect(QRectF(-pw / 2, -ph / 2, pw, ph), ph * 0.25, ph * 0.25);
    p.setBrush(QColor(196, 158, 112, 255));
    for (int i = 0; i < 4; ++i)
        p.drawEllipse(QPointF((i % 2 ? 0.22 : -0.22) * pw, (i < 2 ? -0.25 : 0.25) * ph),
                      br * 0.05, br * 0.05);
    p.restore();
}

void drawDugoutStateBadge(QPainter &p, int state, const QPointF &c, qreal faceD)
{
    const int st = state & 0xFF;
    if (st != 5 && st != 6 && st != 7 && st != 8 && st != 10 && st != 13)
        return; // reserves (and any other state): plain face
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    // Number badge ref: center (0.27, 0.29)*faceD, r 0.26*faceD.
    // Status badge: top-right, slightly more internal (0.33 vs 0.41 faceD
    // from center) and smaller (0.21 vs 0.26 faceD).
    const qreal br = faceD * 0.21;
    // The badge center sits ON the face ring (ring radius = 0.33 faceD).
    const QPointF bc = c + QPointF(faceD * 0.26, -faceD * 0.22);
    // ALL state badges sit on a WHITE disc (ambulance / medical-service
    // style: red cross on white, and bandage + skull on white too). The
    // gold rim keeps the badge family look (number-badge style).
    p.setPen(QPen(QColor(0xd8, 0xb4, 0x4a), 1.0));
    p.setBrush(QColor(255, 255, 255, 255));
    p.drawEllipse(bc, br, br);
    if (st == 5) {
        // KO: comic bandage on the white disc.
        drawComicBandage(p, bc, br);
    } else if (st == 8) {
        // Dead: skull on white — DARK skull, eyes/nose cut out as white.
        const qreal k = br * 0.72;
        p.setPen(QPen(QColor(25, 25, 30, 230), 0.8));
        p.setBrush(QColor(42, 42, 48, 255));
        p.drawEllipse(QRectF(bc.x() - k * 0.70, bc.y() - k * 0.88,
                             k * 1.40, k * 1.40));
        p.drawRoundedRect(QRectF(bc.x() - k * 0.36, bc.y() + k * 0.38,
                                 k * 0.72, k * 0.40), 1, 1); // jaw
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 255)); // white eye sockets
        p.drawEllipse(bc + QPointF(-k * 0.29, -k * 0.16), k * 0.19, k * 0.21);
        p.drawEllipse(bc + QPointF(k * 0.29, -k * 0.16), k * 0.19, k * 0.21);
        QPolygonF nose;
        nose << bc + QPointF(0, k * 0.02) << bc + QPointF(-k * 0.10, k * 0.26)
             << bc + QPointF(k * 0.10, k * 0.26);
        p.drawPolygon(nose);
    } else {
        // Injured (6,7) / missing (10) / banned (13): the RED CROSS on
        // white — ambulance / medical-service style. Geneva cross
        // proportions: arm thickness = 1/6 of the full length.
        const qreal L = br * 0.62;  // half-length
        const qreal t = L / 3.0;    // half-thickness (= 1/6 of 2L)
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(206, 24, 28, 255));
        p.drawRoundedRect(QRectF(bc.x() - t, bc.y() - L, t * 2, L * 2), t * 0.4, t * 0.4);
        p.drawRoundedRect(QRectF(bc.x() - L, bc.y() - t, L * 2, t * 2), t * 0.4, t * 0.4);
    }
    p.restore();
}

// ---------------------------------------------------------------------------
// The BLOCK MARKER — drawn between attacker and defender. While the die
// is not chosen yet: an AMERICAN FOOTBALL HELMET; after the choice: the
// CHOSEN DICE'S SYMBOL ONLY. Both are vector (QPainter is the right tool
// for crisp 4K glyphs).
// ---------------------------------------------------------------------------
// ALL block symbols share ONE style:
//   * a WHITE disc (the "die face") with a thin dark rim, and
//   * a GOLD-AMBER symbol with a thin dark outline.
// The pending state draws the Qwen-Image helmet (grey monochrome) on the
// SAME white disc. No mixed backgrounds or colors: an arrow face clearly
// reads as a symbol on the shared die face.
const QColor kMarkGold(224, 158, 0, 255);
const QColor kMarkInk(46, 40, 34, 255);
const QColor kMarkDisc(252, 252, 248, 245);

// The shared white disc (radius r at c).
inline void markerDisc(QPainter &p, const QPointF &c, qreal r)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(kMarkDisc);
    p.drawEllipse(c, r, r);
    const qreal rim = qMax(1.0, r * 0.09);
    QPen rp(kMarkInk, rim);
    p.setPen(rp);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, r - rim / 2, r - rim / 2);
}

// A soft GOLD halo under the ball (field + carrier), so it pops on the
// grass and on the token alike. The halo BREATHES — radius and alpha pulse
// sinusoidally with the shared pulse timer (same language as the
// selection/block/receiver glows). breath is the 0..1 breathing factor
// computed by the painter.
inline void drawBallGlow(QPainter &p, const QPointF &c, qreal radius, qreal breath)
{
    const qreal r = radius * (1.0 + 0.30 * breath);
    QRadialGradient g(c, r);
    g.setColorAt(0.0, QColor(255, 190, 40, int(100 + 130 * breath))); // 100..230
    g.setColorAt(0.55, QColor(255, 190, 40, int(50 + 70 * breath)));  // 50..120
    g.setColorAt(1.0, QColor(255, 190, 40, 0));
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(c, r, r);
    p.restore();
}

// The vector american football, shared by the ball on the field AND the
// ball-in-hand indicator on the token.
inline void drawFootball(QPainter &p, const QPointF &c, qreal bw, qreal bh,
                         qreal rotDeg)
{
    p.save();
    p.translate(c);
    p.rotate(rotDeg);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF body(-bw / 2, -bh / 2, bw, bh);
    QRadialGradient ballGrad(QPointF(-bw * 0.18, -bh * 0.22), bw * 0.62);
    ballGrad.setColorAt(0.0, QColor(0xb5, 0x7a, 0x3e));
    ballGrad.setColorAt(0.75, QColor(0x8b, 0x5a, 0x2b));
    ballGrad.setColorAt(1.0, QColor(0x5f, 0x3a, 0x18));
    p.setBrush(ballGrad);
    p.setPen(QPen(QColor(0x3a, 0x22, 0x0e), qMax(1.0, bw * 0.045)));
    p.drawEllipse(body);
    // laces: a white strip + cross ticks across the middle
    const qreal lw = bw * 0.46, lh = bh * 0.16;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(245, 242, 232, 255));
    p.drawRoundedRect(QRectF(-lw / 2, -lh / 2, lw, lh), lh * 0.4, lh * 0.4);
    QPen lace(QColor(0x3a, 0x22, 0x0e), qMax(1.0, bh * 0.06));
    p.setPen(lace);
    for (int i = 1; i <= 4; ++i) {
        const qreal lx = -lw / 2 + lw * i / 5.0;
        p.drawLine(QPointF(lx, -lh * 0.42), QPointF(lx, lh * 0.42));
    }
    p.restore();
}

// Gold stroke with a thin dark under-outline (readable on white).
inline void goldStroke(QPainter &p, const QPolygonF &path, qreal width, bool closed)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(kMarkInk, width * 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (closed)
        p.drawPolygon(path);
    else
        p.drawPolyline(path);
    p.setPen(QPen(kMarkGold, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (closed)
        p.drawPolygon(path);
    else
        p.drawPolyline(path);
}

inline void goldFill(QPainter &p, const QPolygonF &path)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(kMarkInk, path.boundingRect().width() * 0.06,
                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(kMarkGold);
    p.drawPolygon(path);
}

// Face 3/4 arrows (same geometry as the official faces, gold on the
// shared disc). The push symbol is the die's STRAIGHT chunky arrow — one
// filled polygon (shaft + big triangular head), a thin dark rim, rotated
// to the actual push direction.
void drawPushArrow(QPainter &p, const QPointF &c, qreal k, qreal angleDeg)
{
    p.save();
    p.translate(c);
    p.rotate(angleDeg);
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal w = k * 0.15; // half-width of the shaft
    // Single closed outline: shaft left end -> head base -> head tip -> back.
    QPolygonF arrow;
    arrow << QPointF(-0.46 * k, -w)
          << QPointF(0.16 * k, -w)
          << QPointF(0.16 * k, -0.38 * k)
          << QPointF(0.54 * k, 0)
          << QPointF(0.16 * k, 0.38 * k)
          << QPointF(0.16 * k, w)
          << QPointF(-0.46 * k, w);
    QPen rim(kMarkInk, k * 0.07);
    rim.setJoinStyle(Qt::RoundJoin);
    p.setPen(rim);
    p.setBrush(kMarkGold);
    p.drawPolygon(arrow);
    p.restore();
}

// Skull glyph, color-parametrized (face 1 = gold on the disc; face 2 =
// disc-white on the gold burst).
void drawSkull(QPainter &p, const QPointF &c, qreal r, const QColor &body,
               const QColor &cavity, bool outlined)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    if (outlined) {
        p.setPen(QPen(kMarkInk, r * 0.10, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(body);
    } else {
        p.setPen(Qt::NoPen);
        p.setBrush(body);
    }
    p.drawEllipse(c, r, r * 0.92);
    const QPointF jaw(c.x(), c.y() + r * 0.55);
    p.drawRoundedRect(QRectF(c.x() - r * 0.5, jaw.y(), r, r * 0.55), r * 0.18, r * 0.18);
    // eye sockets + nose: the cavity color shows through.
    p.setPen(Qt::NoPen);
    p.setBrush(cavity);
    p.drawEllipse(QPointF(c.x() - r * 0.42, c.y() - r * 0.15), r * 0.30, r * 0.32);
    p.drawEllipse(QPointF(c.x() + r * 0.42, c.y() - r * 0.15), r * 0.30, r * 0.32);
    QPolygonF nose;
    nose << QPointF(c.x(), c.y() + r * 0.10)
         << QPointF(c.x() - r * 0.22, c.y() + r * 0.45)
         << QPointF(c.x() + r * 0.22, c.y() + r * 0.45);
    p.drawPolygon(nose);
}

// The official spiky burst (star) glyph — faces 2/6 (and the badge rim
// look). Sized to fill most of the white disc.
QPolygonF burstPolygon(const QPointF &c, qreal k, qreal scale)
{
    QPolygonF burst;
    const int n = 12;
    for (int i = 0; i < n * 2; ++i) {
        const qreal rr = (i % 2 == 0 ? 0.58 * k : 0.28 * k) * scale;
        const qreal a = M_PI * i / n - M_PI / 2;
        burst << QPointF(c.x() + std::cos(a) * rr, c.y() + std::sin(a) * rr);
    }
    return burst;
}

void drawBlockDieSymbol(QPainter &p, const QPointF &c, qreal k, int face,
                        qreal arrowAngleDeg = -45)
{
    // k = marker radius. WHITE DISC + gold symbol, every face.
    // arrowAngleDeg: where the push arrow (faces 3/4) points — on the FIELD
    // marker it is the actual attacker -> defender direction; the bare
    // default matches the die faces (3 up-right, 4 down-right).
    p.save();
    markerDisc(p, c, 0.62 * k);
    // GROUND TRUTH — official GW block die (BB2025, verified on official
    // photos, see tools/mockup/compose_f.py::block_dice):
    //   1 skull (player down)   2 skull IN A BURST (BOTH DOWN)
    //   3 push arrow            4 push arrow (same symbol)
    //   5 '!' in a burst (STUMBLE)  6 burst (POW)
    switch (face) {
    case 5: { // '!' (stumble): the exclamation mark is inscribed in the
              // burst (star) TOO, like the block5 die face: gold burst, the
              // '!' knocked out in the disc color.
        goldFill(p, burstPolygon(c, k, 0.92));
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(kMarkDisc);
        QPolygonF bar;
        bar << QPointF(c.x() - 0.09 * k, c.y() - 0.32 * k)
            << QPointF(c.x() + 0.09 * k, c.y() - 0.32 * k)
            << QPointF(c.x() + 0.07 * k, c.y() + 0.05 * k)
            << QPointF(c.x() - 0.07 * k, c.y() + 0.05 * k);
        p.drawPolygon(bar);
        p.drawEllipse(QPointF(c.x(), c.y() + 0.25 * k), 0.10 * k, 0.10 * k);
        break;
    }
    case 6: // burst (POW)
        goldFill(p, burstPolygon(c, k, 1.0));
        break;
    case 3: // push arrow (official faces 3/4) — on the field marker both
    case 4: // faces point along the real push (attacker -> defender); the
            // parameter default only matters for a bare die-face replica.
        drawPushArrow(p, c, k, arrowAngleDeg);
        break;
    case 1: { // skull (player down). On the official die face 1 the skull
              // fills the face — sized to match.
        drawSkull(p, c, 0.46 * k, kMarkGold, kMarkDisc, true);
        break;
    }
    case 2: { // BOTH DOWN. Official face (block2 asset): skull +
              // explosion burst, both gold on the white disc. Scaled up to
              // match face 1/6 (barely visible at small cells otherwise).
        drawSkull(p, QPointF(c.x() - 0.20 * k, c.y() + 0.08 * k), 0.30 * k,
                  kMarkGold, kMarkDisc, true);
        goldFill(p, burstPolygon(QPointF(c.x() + 0.22 * k, c.y() - 0.18 * k),
                                 k * 0.80, 1.0));
        break;
    }
    default:
        break;
    }
    p.restore();
}

// The Qwen-generated grey helmet asset on the shared white disc. Vector
// fallback when the asset is missing.
void drawFootballHelmet(QPainter &p, const QPointF &c, qreal k, bool facingLeft)
{
    p.save();
    markerDisc(p, c, 0.62 * k);
    const QPixmap helm = assets::blockHelmet();
    if (helm.isNull()) {
        // --- vector fallback (recycled original helmet, recolored dark) ---
        p.translate(c);
        if (facingLeft)
            p.scale(-1, 1);
        const qreal w = 0.86 * k, h = 0.78 * k;
        QRectF dome(-w * 0.5, -h * 0.55, w * 0.95, h * 0.95);
        p.setPen(QPen(QColor(30, 30, 36, 255), qMax(1.0, k * 0.09)));
        p.setBrush(QColor(96, 100, 108, 255));
        p.drawRoundedRect(dome, h * 0.48, h * 0.48);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(30, 30, 36, 255));
        p.drawRoundedRect(QRectF(w * 0.16, -h * 0.30, w * 0.26, h * 0.22), 2, 2);
        p.drawEllipse(QPointF(-w * 0.18, -h * 0.05), w * 0.09, w * 0.09);
        QPen mask(QColor(30, 30, 36, 255), qMax(1.0, k * 0.075));
        mask.setCapStyle(Qt::RoundCap);
        p.setPen(mask);
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < 3; ++i) {
            const qreal y0 = -h * 0.22 + i * h * 0.16;
            p.drawArc(QRectF(w * 0.30, y0 - k * 0.02, w * 0.42, h * 0.16 + k * 0.04),
                      -60 * 16, 120 * 16);
        }
        p.setPen(QPen(QColor(30, 30, 36, 255), qMax(1.0, k * 0.07)));
        p.drawLine(QPointF(w * 0.52, -h * 0.26), QPointF(w * 0.52, h * 0.10));
        p.restore();
        return;
    }
    // --- the Qwen grey helmet, aspect-fit into the disc -------------------
    const qreal d = 1.06 * k; // disc diameter = 2 * 0.53k... keep it inside
    const qreal sc = qMin(d / helm.width(), d / helm.height());
    const QRectF r(c.x() - helm.width() * sc / 2, c.y() - helm.height() * sc / 2,
                   helm.width() * sc, helm.height() * sc);
    if (facingLeft) {
        p.translate(2 * c.x(), 0);
        p.scale(-1, 1);
    }
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawPixmap(r, helm, QRectF());
    p.restore();
}
} // namespace

// The bench slots on the DUGOUT PANEL. Field space; shared by painter +
// hover so the two never drift apart.
QVector<QPair<QRectF, const FieldView::BoxEntry*>> FieldView::boxSlots(
    const QVector<BoxEntry> &entries, bool leftSide) const
{
    // ONE section, all bench players in entries order (1 row for <=8, 2
    // rows of 8 above) — the hit-test shares the exact layout with the
    // painter (dugoutFaceLayout).
    QVector<QPair<QRectF, const FieldView::BoxEntry*>> filled;
    const QRectF R = dugoutRect(leftSide); // the team's top-band pavilion
    if (R.isEmpty())
        return filled;
    const DugoutLayout lay =
        dugoutFaceLayout(R, qMin(entries.size(), kDugoutMaxFaces));
    for (int i = 0; i < lay.centers.size(); ++i) {
        const QPointF c = lay.centers[i];
        filled.append({ QRectF(c.x() - lay.faceD / 2, c.y() - lay.faceD / 2,
                               lay.faceD, lay.faceD),
                        &entries[i] });
    }
    return filled;
}

// The dugout slot under a WIDGET point (the dugouts are inside the
// zoomed scene, so the point is mapped to field space first).
const FieldView::BoxEntry *FieldView::boxEntryAt(const QPoint &pos, bool &sideHome) const
{
    const QPointF w = toFieldSpace(pos);
    sideHome = true;
    const auto hit = [this, w](const QVector<BoxEntry> &entries, bool left) -> const BoxEntry * {
        for (const auto &sl : boxSlots(entries, left))
            if (sl.first.contains(w))
                return sl.second;
        return nullptr;
    };
    const BoxEntry *e = hit(m_homeBox, true);
    if (e)
        return e;
    sideHome = false;
    return hit(m_awayBox, false);
}

// Dugout bench: the team's top-band pavilion — a wooden panel with a run
// of team-ringed faces (only the players actually present — no empty
// rings).
void FieldView::paintBoxStrip(QPainter &p, const QColor &accent,
                              const QVector<BoxEntry> &entries, bool leftSide) const
{
    const QRectF R = dugoutRect(leftSide); // the team's top-band pavilion
    if (R.isEmpty())
        return;

    // 1) The WOODEN PANEL — a separate asset MOUNTED on the manifest rect
    //    (the dugout is universal across stadiums; the manifest carries
    //    the coordinates). The baked-in pavilions of the stadium image
    //    stay underneath as a fallback.
    const QPixmap panel = assets::dugoutPanel();
    if (!panel.isNull()) {
        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawPixmap(R, panel, QRectF(0, 0, panel.width(), panel.height()));
        p.restore();
    } else {
        // Vector fallback (QPainter is allowed for structure): dark oak
        // planks + plank seams, so the bench never looks broken.
        p.save();
        p.setPen(QPen(QColor(30, 20, 10, 160), qMax(1.0, R.height() * 0.02)));
        p.setBrush(QColor(92, 62, 34, 235));
        p.drawRoundedRect(R, R.height() * 0.05, R.height() * 0.05);
        for (int i = 1; i < 6; ++i) {
            const qreal x = R.x() + R.width() * i / 6.0;
            p.drawLine(QPointF(x, R.y() + 2), QPointF(x, R.bottom() - 2));
        }
        p.restore();
    }

    // 2) The bench faces (team-ringed, FIXED size for 16, number badge
    //    bottom-right, STATE badge top-right).
    const DugoutLayout lay =
        dugoutFaceLayout(R, qMin(entries.size(), kDugoutMaxFaces));
    for (int i = 0; i < lay.centers.size(); ++i) {
        const BoxEntry *e = &entries[i];
        const QPointF c = lay.centers[i];
        const qreal faceD = lay.faceD;
        const QPixmap pm = assets::playerFace(e->positionName, e->race);
        if (!pm.isNull()) {
            const qreal fr = faceD * 0.33; // thinner ring -> bigger face
            const QRectF faceR(c.x() - fr, c.y() - fr, fr * 2, fr * 2);
            p.save();
            QPainterPath clip;
            clip.addEllipse(faceR);
            p.setClipPath(clip);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawPixmap(faceR, pm, QRectF(0, 0, pm.width(), pm.height()));
            p.restore();
        }
        // The SAME team ring as on the field (home blue / away red) — the
        // bench reads as the same team.
        drawTeamRing(p, c, faceD, leftSide);

        // Number badge (bottom-right of the ring, like the field tokens):
        // team color — blue disc (home) / red disc (away) with a white
        // number.
        if (e->number > 0) {
            const qreal br = faceD * 0.26;
            const QPointF bc(c.x() + faceD * 0.27, c.y() + faceD * 0.29);
            p.setPen(QPen(QColor(0x2a, 0x28, 0x22), qMax(1.0, br * 0.16)));
            p.setBrush(teamRingColor(leftSide));
            p.drawEllipse(bc, br, br);
            p.setPen(Qt::white);
            // The dugout's jersey number is the SAME SIZE as on the field
            // — the exact font formula of the field token badge, not a
            // face-relative one.
            const QRectF fr = fieldRect();
            const qreal minDimF = fr.isEmpty()
                                     ? faceD
                                     : qMin(fr.width() / kFieldWidth,
                                            fr.height() / kFieldHeight);
            const qreal fbr = minDimF * 0.15; // the field badge radius
            const int nd = e->number > 99 ? 3 : (e->number > 9 ? 2 : 1);
            QFont nf = p.font();
            nf.setPointSizeF(qMax(3.5, fbr * (nd == 1 ? 1.5 : 1.05)));
            nf.setBold(true);
            p.setFont(nf);
            p.drawText(QRectF(bc.x() - br, bc.y() - br, br * 2, br * 2),
                       Qt::AlignCenter, QString::number(e->number));
        }

        // State badge (TOP-RIGHT of the ring — never covers the number).
        drawDugoutStateBadge(p, e->state, c, faceD);
    }
}



// ---------------------------------------------------------------------------
// Action wheel (Mockup F): white disc around the acting player with the six
// core action icons (move, block, blitz, pass, foul, attack). Ported from
// the mockup compositor (tools/mockup/compose_f.py draw_wheel).
// ---------------------------------------------------------------------------
static void drawActionWheel(QPainter &p, const QPointF &ctr, qreal cell)
{
    const qreal R = cell * 1.28;
    const QColor INK(34, 40, 50, 255);

    // soft shadow, then the disc
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 26, 34, 70));
    p.drawEllipse(ctr + QPointF(2, 5), R, R);
    p.setBrush(QColor(252, 252, 254, 218));
    p.setPen(QPen(QColor(168, 176, 190, 255), 2.0));
    p.drawEllipse(ctr, R, R);

    const qreal ic = cell * 0.30;
    const qreal rr = R * 0.60;

    // move (up arrow)
    {
        p.setPen(QPen(INK, 3.0));
        p.drawLine(ctr + QPointF(0, -rr - ic * 0.5), ctr + QPointF(0, -rr + ic * 0.35));
        p.setPen(Qt::NoPen);
        p.setBrush(INK);
        QPolygonF tri;
        tri << ctr + QPointF(-ic * 0.42, -rr + ic * 0.05)
            << ctr + QPointF(ic * 0.42, -rr + ic * 0.05)
            << ctr + QPointF(0, -rr - ic * 0.55);
        p.drawPolygon(tri);
    }
    // block (two shields, top-right)
    {
        const QPointF b(ctr.x() + rr * 0.87, ctr.y() - rr * 0.5);
        p.setPen(Qt::NoPen);
        p.setBrush(INK);
        for (const qreal off : { -ic * 0.42, ic * 0.14 })
            p.drawRoundedRect(QRectF(b.x() + off - ic * 0.18, b.y() - ic * 0.3, ic * 0.36, ic * 0.6), 2, 2);
    }
    // blitz (zigzag, bottom-right)
    {
        const QPointF b(ctr.x() + rr * 0.87, ctr.y() + rr * 0.5);
        QPen zp(INK, 3.0);
        zp.setJoinStyle(Qt::RoundJoin);
        p.setPen(zp);
        p.drawPolyline(QPolygonF()
            << b + QPointF(-ic * 0.45, -ic * 0.35)
            << b + QPointF(-ic * 0.05, 0)
            << b + QPointF(-ic * 0.35, ic * 0.35)
            << b + QPointF(ic * 0.4, -ic * 0.1));
    }
    // pass (arc + arrowhead, bottom-left)
    {
        const QPointF b(ctr.x() - rr * 0.87, ctr.y() + rr * 0.5);
        p.setPen(QPen(INK, 3.0));
        p.drawArc(QRectF(b.x() - ic * 0.5, b.y() - ic * 0.5, ic, ic), 100 * 16, 160 * 16);
        p.setPen(Qt::NoPen);
        p.setBrush(INK);
        QPolygonF ah;
        ah << b + QPointF(ic * 0.32, -ic * 0.42)
           << b + QPointF(ic * 0.62, -ic * 0.18)
           << b + QPointF(ic * 0.18, -ic * 0.05);
        p.drawPolygon(ah);
    }
    // foul (crossed swords, top-left)
    {
        const QPointF b(ctr.x() - rr * 0.87, ctr.y() - rr * 0.5);
        p.setPen(QPen(INK, 3.0));
        p.drawLine(b + QPointF(-ic * 0.4, -ic * 0.4), b + QPointF(ic * 0.4, ic * 0.4));
        p.drawLine(b + QPointF(ic * 0.4, -ic * 0.4), b + QPointF(-ic * 0.4, ic * 0.4));
    }
    // attack (dagger, bottom)
    {
        const QPointF b(ctr.x(), ctr.y() + rr);
        p.setPen(Qt::NoPen);
        p.setBrush(INK);
        QPolygonF dg;
        dg << b + QPointF(0, ic * 0.5)
           << b + QPointF(-ic * 0.28, -ic * 0.3)
           << b + QPointF(0, -ic * 0.55)
           << b + QPointF(ic * 0.28, -ic * 0.3);
        p.drawPolygon(dg);
        p.setPen(QPen(INK, 2.0));
        p.drawLine(b + QPointF(-ic * 0.4, -ic * 0.35), b + QPointF(ic * 0.4, -ic * 0.35));
    }
}

void FieldView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF f = fieldRect();
    if (f.isEmpty())
        return;

    // Dark apron behind the scene: the stadium scene is drawn in the
    // zoomed/panned space below; this only shows if the scene ever fails to
    // cover (no stadium art) — a dark stadium floor.
    p.fillRect(QRectF(0, 0, width(), height()), QColor(40, 38, 36));

    // Field navigation: the WHOLE scene (dugouts, texture, grid, tokens,
    // rain) lives in a zoomed/panned space (the dugout panels are part of
    // the scene too — they zoom with the field).
    p.save();
    p.translate(m_pan);
    p.scale(m_zoom, m_zoom);

    const QRectF s = sceneRect();
    const qreal cellW = f.width() / kFieldWidth;
    const qreal cellH = f.height() / kFieldHeight;

    // --- Stadium: the SCENE is the STADIUM image (an interchangeable
    //     top-down backdrop + manifest). Drawn COVER (fills
    //     the widget at zoom 1, center-cropped) in scene space. The field is
    //     fitted into the manifest's pit (below); the dugout faces go into
    //     the manifest's dugout rects (TOP band: home LEFT, away RIGHT) —
    //     data-driven alignment, so any stadium aligns correctly.
    {
        const QPixmap st = assets::stadiumImage(m_stadiumName);
        if (!st.isNull()) {
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawPixmap(s, st, QRectF(0, 0, st.width(), st.height()));
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        } else {
            p.fillRect(s, QColor(58, 56, 54)); // fallback apron
        }
        // (The dugout benches are painted at the END of the zoomed pass,
        // once the field + tokens + rain are on top.)
    }

    // --- Field texture: the Qwen art is PURE GRASS (the baked
    //     grid/TD/TOUCHDOWN came out blurred and duplicated at zoom).
    //     Everything structural is drawn in CODE over it: mowing stripes,
    //     crimson 1-column TD zones + vector TOUCHDOWN, the exact grid. The
    //     texture only supplies the grass detail, which stays soft even
    //     when zoomed (fine detail, tolerable) while lines and text stay
    //     razor-sharp at any zoom/HiDPI.
    // The pit's inner corners are CURVED (the stone arch of the frame).
    // The field is clipped to a ROUNDED rect (the manifest's pitRadius) so
    // the grass/TD corners follow the arch instead of poking a sharp 90°
    // corner over it. Tokens/glow are drawn AFTER the restore, unclipped.
    p.save();
    {
        QPainterPath fieldClip;
        const qreal prx = m_stadium.pitRadiusX * s.width();
        const qreal pry = m_stadium.pitRadiusY * s.height();
        if (prx > 0.5 || pry > 0.5)
            fieldClip.addRoundedRect(f, prx, pry);
        else
            fieldClip.addRect(f);
        p.setClipPath(fieldClip);
    }
    const QPixmap tex = assets::fieldTexture();
    if (!tex.isNull()) {
        // NO empty grass apron — the 26x15 pitch is drawn over its rect
        // ONLY. The "no black band" rule is satisfied by the STADIUM
        // itself: each stadium's field pit is designed at the
        // field's aspect (26:15), so the pitch fills the pit exactly (see
        // the stadium generator prompts). Grass = the pitch, nothing else.
        const qreal scale = qMax(f.width() / tex.width(), f.height() / tex.height());
        const QSize sz(int(tex.width() * scale) + 1, int(tex.height() * scale) + 1);
        p.drawPixmap(QRectF(f.center().x() - sz.width() / 2, f.center().y() - sz.height() / 2,
                            sz.width(), sz.height()),
                     tex, QRectF(0, 0, tex.width(), tex.height()));
    } else {
        p.fillRect(f, QColor(0x3d, 0x6b, 0x35)); // flat fallback
    }
    drawMowStripes(p, f);
    drawTouchdownZones(p, f);

    // --- Exact 26x15 grid over the WHOLE field ----------------------------
    QPen linePen(QColor(255, 255, 255, 110));
    linePen.setWidthF(qMax(1.0, cellW * 0.04));
    p.setPen(linePen);
    for (int x = 0; x <= kFieldWidth; ++x)
        p.drawLine(QPointF(f.x() + x * cellW, f.y()), QPointF(f.x() + x * cellW, f.bottom()));
    // The WIDE ZONE boundary lines. Ground truth (ffb-common
    // FieldCoordinateBounds): UPPER wide zone = y 0..3, LOWER = y 11..14,
    // centre field = y 4..10 — so the emphasized lines run at y=4 and y=11,
    // plus a slightly stronger sideline at y=0 / y=14 (the pitch edges).
    // The wide-zone lines must NOT continue into the TOUCHDOWN zones —
    // they start/stop at the TD line (x=1 / x=25), not at the end of the
    // pitch.
    QPen wzPen(QColor(255, 255, 255, 210), qMax(2.0, cellW * 0.08));
    QPen sidePen(QColor(255, 255, 255, 160), qMax(1.5, cellW * 0.055));
    for (int y = 0; y <= kFieldHeight; ++y) {
        const qreal yPx = f.y() + y * cellH;
        if (y == 4 || y == 11) {
            p.setPen(wzPen);
            // From the home TD line to the away TD line (never across the
            // crimson TD columns).
            p.drawLine(QPointF(f.x() + cellW, yPx), QPointF(f.right() - cellW, yPx));
        } else if (y == 0 || y == kFieldHeight) {
            p.setPen(sidePen);
            p.drawLine(QPointF(f.x(), yPx), QPointF(f.right(), yPx));
        } else {
            p.setPen(linePen);
            p.drawLine(QPointF(f.x(), yPx), QPointF(f.right(), yPx));
        }
    }

    // --- Center line: SINGLE, emphasized (between columns 12 and 13) ------
    QPen centerPen(QColor(255, 255, 255, 240), qMax(3.0, cellW * 0.10));
    p.setPen(centerPen);
    const qreal cx = f.x() + 13 * cellW;
    p.drawLine(QPointF(cx, f.y()), QPointF(cx, f.bottom()));

    // --- Team logos: per-match watermarks centered on each half (25%
    //     opacity). Both logos the SAME size, and they must NOT overflow
    //     into the WIDE ZONES (the y 0..3 / y 11..14 bands). The box is
    //     fitted to the central field (y 4..10) and the logo is drawn
    //     aspect-preserved (no stretch) inside it, so both crests read the
    //     same size.
    const auto drawLogo = [&](const QPixmap &pm, qreal col) {
        if (pm.isNull())
            return;
        const qreal box = f.height() * 0.40; // fits the central field y 4..10
        const qreal cx = f.x() + col * cellW;
        const qreal cy = f.center().y();
        const qreal ar = qreal(pm.width()) / pm.height();
        qreal w = box, h = box;
        if (ar > 1.0)
            h = box / ar;
        else
            w = box * ar;
        p.save();
        p.setOpacity(0.25);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawPixmap(QRectF(cx - w / 2, cy - h / 2, w, h), pm,
                     QRectF(0, 0, pm.width(), pm.height()));
        p.restore();
    };
    drawLogo(m_homeLogo, 6.5);
    drawLogo(m_awayLogo, 18.5);

    // --- Puddles: water patches on the grass, drawn UNDER the
    //     tokens/ball. Present only when it rains — the animated rain's
    //     drops splash into them.
    if (m_raining)
        drawPuddles(p, f);

    // --- The actor's cumulative MOVE TRAIL: drawn on the field, under
    //     the tokens — the player walks on top of his own trail.
    drawMoveTrail(p);

    // End of the CLIPPED field (rounded pit corners). Everything after this
    // (tokens, ball, glows, rain) is unclipped.
    p.restore();

    // --- Ball on an empty square -------------------------------------------
    const auto atBall = [this](const QVector<PlayerToken> &team) {
        for (const PlayerToken &t : team)
            if (t.x == m_ballX && t.y == m_ballY)
                return true;
        return false;
    };
    const bool ballOccupied = atBall(m_homePlayers) || atBall(m_awayPlayers);
    if (m_ballInPlay && !ballOccupied) {
        // The ball glides through the grid too — draw it at its animated
        // VISUAL position (cell coords), not the model cell. The in-flight
        // size is a BOUNCE profile: the ball rises (bigger), lands (normal),
        // bounces smaller, ... across the trajectory (phase in cells
        // travelled since the flight started; arcs of ~3.2 cells, each
        // damped 0.70x). A 1-step fumble hop (steps == 1) stays flat (no
        // growth on fumble).
        qreal ballScale = 1.0;
        if (m_ballMoving && m_ballFlightSteps >= 2) {
            const TokenAnim &ba = m_tokenAnim.value(QLatin1String("__ball__"));
            const qreal d = m_ballFlightBaseDist + (ba.leg + ba.t);
            const qreal L = 3.2; // cells per bounce arc
            const qreal phase = d / L;
            ballScale = 1.0 + 0.55 * qAbs(qSin(M_PI * phase))
                                     * qPow(0.70, std::floor(phase));
        }
        const QPointF bvc = m_visualPos.value(QLatin1String("__ball__"),
                                              QPointF(m_ballX + 0.5, m_ballY + 0.5));
        const QPointF bctr(f.x() + bvc.x() * cellW, f.y() + bvc.y() * cellH);
        // A proper vector american football (drawFootball, shared with the
        // ball-in-hand marker).
        const qreal bw = cellW * 0.52 * ballScale;
        const qreal bh = cellH * 0.34 * ballScale;
        // A soft GOLD halo under the ball, at rest and in flight. The glow
        // is ANIMATED — it breathes with the pulse timer (the timer stays
        // armed while the ball is visible).
        const qreal breath = 0.5 - 0.5 * std::cos(m_ballPulsePhase);
        drawBallGlow(p, bctr, qMax(bw, bh) * 1.15, breath);
        drawFootball(p, bctr, bw, bh, -18);
    }

    // --- Target brackets + action wheel (drawn UNDER the tokens, like the
    //     mockup). Follows ONLY an EXPLICIT target (setActionWheelTarget,
    //     set by a right-click on a player in play mode) — never the FFB
    //     acting-player state nor the click selection. In spectator mode
    //     the target stays empty: no wheel. The wheel + bracket code is
    //     kept for the play mode.
    const PlayerToken *wheelOn = nullptr;
    if (!m_wheelTarget.isEmpty()) {
        const auto find = [this](const QVector<PlayerToken> &team) -> const PlayerToken * {
            for (const PlayerToken &t : team)
                if (t.playerId == m_wheelTarget)
                    return &t;
            return nullptr;
        };
        wheelOn = find(m_homePlayers) ? find(m_homePlayers) : find(m_awayPlayers);
    }
    if (wheelOn && wheelOn->x >= 0 && wheelOn->x < kFieldWidth
        && wheelOn->y >= 0 && wheelOn->y < kFieldHeight) {
        const PlayerToken *acting = wheelOn;
        // Target brackets: two empty cells adjacent to the acting player
        // (prefer towards the ball, then the cells ahead).
        const auto occupied = [this](int x, int y) {
            for (const PlayerToken &t : m_homePlayers)
                if (t.x == x && t.y == y)
                    return true;
            for (const PlayerToken &t : m_awayPlayers)
                if (t.x == x && t.y == y)
                    return true;
            return false;
        };
        const int bdir = m_ballInPlay
            ? (m_ballX < acting->x ? -1 : (m_ballX > acting->x ? 1 : 0)) : 0;
        QVector<QPair<int, int>> cand;
        if (bdir != 0) {
            cand.append({ acting->x + bdir, acting->y });
            cand.append({ acting->x - bdir, acting->y });
        } else {
            cand.append({ acting->x + 1, acting->y });
        }
        cand.append({ acting->x, acting->y - 1 });
        cand.append({ acting->x, acting->y + 1 });
        QPen brkPen(QColor(255, 255, 255, 215), qMax(2.0, cellW * 0.045));
        p.setPen(brkPen);
        int picks = 0;
        for (const auto &cc : cand) {
            if (picks == 2)
                break;
            const int tx = cc.first, ty = cc.second;
            if (tx < 0 || tx >= kFieldWidth || ty < 0 || ty >= kFieldHeight)
                continue;
            if (occupied(tx, ty))
                continue;
            const QRectF tr = cellRect(tx, ty);
            const qreal a = cellW * 0.16, m = cellW * 0.03;
            p.drawLine(tr.topLeft() + QPointF(m, m), tr.topLeft() + QPointF(m + a, m));
            p.drawLine(tr.topLeft() + QPointF(m, m), tr.topLeft() + QPointF(m, m + a));
            p.drawLine(tr.topRight() - QPointF(m, m), tr.topRight() - QPointF(m + a, m));
            p.drawLine(tr.topRight() - QPointF(m, m), tr.topRight() - QPointF(m, m + a));
            p.drawLine(tr.bottomLeft() + QPointF(m, -m), tr.bottomLeft() + QPointF(m + a, -m));
            p.drawLine(tr.bottomLeft() + QPointF(m, -m), tr.bottomLeft() + QPointF(m, -a));
            p.drawLine(tr.bottomRight() - QPointF(m, -m), tr.bottomRight() - QPointF(m + a, -m));
            p.drawLine(tr.bottomRight() - QPointF(m, -m), tr.bottomRight() - QPointF(m, -a));
            ++picks;
        }
        // The white action wheel (disc + six action icons).
        drawActionWheel(p, cellRect(acting->x, acting->y).center(), qMin(cellW, cellH));
    }

    // --- Player tokens -------------------------------------------------------
    // Team-colored circle (runtime, per home/away) + the piece's cartoon
    // face on top, with a number badge. A plain circle is the fallback when
    // the face assets are missing.
    const auto drawTeam = [this, &p, &f, cellW, cellH](const QVector<PlayerToken> &players,
                                                   bool sideHome) {
        const qreal s = qMin(cellW, cellH) * 0.92;
        for (const PlayerToken &t : players) {
            if (t.x < 0 || t.x >= kFieldWidth || t.y < 0 || t.y >= kFieldHeight)
                continue; // box coordinate / off field
            // The token is drawn at its ANIMATED visual position (cell
            // coords) so it glides square-by-square while moving; at rest
            // it sits exactly on the model cell center.
            const QPointF vpos = tokenVisualPos(t);
            const QPointF ctr(f.x() + vpos.x() * cellW, f.y() + vpos.y() * cellH);

            // DOWN players keep the FULL round token (a stretched ellipse
            // squashed the face); prone/stunned is shown by the state mark
            // instead. Home = BLUE ring, away = RED ring, inside the cell,
            // clearly readable around the human face: dark base disc ->
            // face -> team ring on the outer rim (outer edge = token edge,
            // no overflow).
            const QRectF circ(ctr.x() - s * 0.5, ctr.y() - s * 0.5, s, s);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0x23, 0x23, 0x2a));
            p.drawEllipse(circ);

            // Face on top (inset from the ring). Custom-named star pieces
            // fall back to the side's race representative. The face assets
            // are square (RGB, no alpha), so the face is clipped to a
            // circle — the mockup does the same with a mask.
            const QString race = sideHome ? m_homeRace : m_awayRace;
            const QPixmap pm = assets::playerFace(t.positionName, race);
            if (!pm.isNull()) {
                const qreal fs = s * 0.68; // thinner ring -> bigger face
                const QRectF face(ctr.x() - fs / 2, ctr.y() - fs / 2, fs, fs);
                p.save();
                QPainterPath clip;
                clip.addEllipse(face.adjusted(fs * 0.02, fs * 0.02, -fs * 0.02, -fs * 0.02));
                p.setClipPath(clip);
                p.drawPixmap(face, pm, QRectF(0, 0, pm.width(), pm.height()));
                p.restore();
            }

            // The thick TEAM ring (home blue / away red) on the outer rim.
            drawTeamRing(p, ctr, s, sideHome);

            // Player state mark over the face — prone = "/", stunned = "X"
            // (white with a dark under-stroke for contrast). ALL dark
            // under-strokes first, then all white over-strokes — otherwise
            // the second arm's dark rim paints over the first arm's white
            // and the X comes out broken.
            const int base = t.state & 0xFF;
            if (base == 3 || base == 4) {
                const qreal mw = s * 0.46;
                const QRectF mr(ctr.x() - mw / 2, ctr.y() - mw / 2, mw, mw);
                QPen under(QColor(24, 24, 30, 210), qMax(3.5, s * 0.16));
                under.setCapStyle(Qt::RoundCap);
                QPen over(QColor(255, 255, 255, 235), qMax(2.0, s * 0.10));
                over.setCapStyle(Qt::RoundCap);
                const auto arms = [&](const QPen &pen) {
                    p.setPen(pen);
                    if (base == 3)
                        p.drawLine(mr.bottomLeft(), mr.topRight()); // "/"  (prone)
                    else {
                        p.drawLine(mr.topLeft(), mr.bottomRight()); // "X" (stunned)
                        p.drawLine(mr.bottomLeft(), mr.topRight());
                    }
                };
                arms(under); // pass 1: every arm's rim
                arms(over);  // pass 2: every arm's white body
            }

            // Number badge (bottom-right of the token — follows it while it
            // glides, not the model cell). Same badge family as the
            // check/lightning/'?' ring badges (0.15 minDim) with a dark rim.
            // The badge is the TEAM COLOR — blue disc + white font for
            // home, red disc + white font for away.
            const qreal br = qMin(cellW, cellH) * 0.15;
            const QPointF bc(ctr.x() + s * 0.30, ctr.y() + s * 0.32);
            p.setPen(QPen(QColor(0x2a, 0x28, 0x22), qMax(1.0, br * 0.16)));
            p.setBrush(teamRingColor(sideHome));
            p.drawEllipse(bc, br, br);
            QFont numFont = p.font();
            const int ndigits = t.number > 99 ? 3 : (t.number > 9 ? 2 : 1);
            numFont.setPointSizeF(qMax(3.5, br * (ndigits == 1 ? 1.5 : 1.05)));
            numFont.setBold(true);
            p.setFont(numFont);
            p.setPen(Qt::white);
            p.drawText(QRectF(bc.x() - br, bc.y() - br, br * 2, br * 2), Qt::AlignCenter,
                       t.number > 0 ? QString::number(t.number) : QStringLiteral("?"));

            // Ball carrier indicator (top-left of the token): the SAME
            // vector football as the ball on the field, at badge scale.
            if (t.hasBall) {
                // The carrier ball GLOWS GOLD on the token too, ANIMATED —
                // it breathes with the pulse timer.
                const QPointF bc2(ctr.x() - s * 0.34, ctr.y() - s * 0.38);
                const qreal bw2 = cellW * 0.30, bh2 = cellH * 0.20;
                const qreal breath = 0.5 - 0.5 * std::cos(m_ballPulsePhase);
                drawBallGlow(p, bc2, qMax(bw2, bh2) * 1.25, breath);
                drawFootball(p, bc2, bw2, bh2, -18);
            }
        }
    };

    drawTeam(m_homePlayers, true);
    drawTeam(m_awayPlayers, false);

    // --- Selection: BLUE RIM GLOW — the glow hugs the token's RIM only
    //     (transparent over the face, a soft blue band just outside the
    //     team circle, fading out) with a crisp blue ring. Pulsing: the
    //     band's alpha breathes sinusoidally (~1.2 Hz, m_pulseTimer).
    //     Drawn last in field space so it is always visible and follows
    //     zoom/pan.
    if (!m_selectedPlayer.isEmpty()) {
        const auto find = [this](const QVector<PlayerToken> &team) -> const PlayerToken * {
            for (const PlayerToken &t : team)
                if (t.playerId == m_selectedPlayer)
                    return &t;
            return nullptr;
        };
        const PlayerToken *sel = find(m_homePlayers) ? find(m_homePlayers) : find(m_awayPlayers);
        if (sel) {
            // Follows the token's ANIMATED visual position (glides with it).
            const QPointF vpos = tokenVisualPos(*sel);
            const QPointF ctr(f.x() + vpos.x() * cellW, f.y() + vpos.y() * cellH);
            // Breathing factor 0..1 (sinusoid over the pulse phase).
            const qreal breath = 0.5 - 0.5 * std::cos(m_pulsePhase);
            const qreal minDim = qMin(cellW, cellH);
            const qreal ringR = minDim * (0.58 + 0.025 * breath);
            const qreal glowR = minDim * (0.95 + 0.08 * breath);
            // Rim-only radial: clear over the face (up to the team circle's
            // edge), peak band at the ring, fade to transparent.
            QRadialGradient halo(ctr, glowR);
            // A clearly visible rim glow, still limited to the border (the
            // face stays uncovered) — a subtle ring reads nothing on the
            // bright green field.
            const int rimA = int(150 + 90 * breath); // 150..240
            halo.setColorAt(0.0, QColor(88, 168, 255, 0));
            halo.setColorAt((minDim * 0.5) / glowR, QColor(88, 168, 255, 0));
            halo.setColorAt(ringR / glowR, QColor(88, 168, 255, rimA));
            halo.setColorAt(1.0, QColor(88, 168, 255, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(ctr, glowR, glowR);
            // Crisp bright ring around the token (full alpha, thicker).
            QPen ringPen(QColor(140, 200, 255, 255), qMax(2.5, minDim * 0.075));
            p.setPen(ringPen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(ctr, ringR, ringR);
        }
    }

    // --- RED GLOW on the player being BLOCKED — same shape as the
    //     selection's blue rim glow, in the reserved red (injury/failure/
    //     block). Armed by the model on the block report, cleared when the
    //     block resolves (state change / new turn / new actor). Follows the
    //     token's animated glide. The SAME shape in WHITE is the PASS
    //     RECEIVER's glow (fieldModelSetTargetSelectionState SELECTED).
    const auto drawGlow = [&](const QString &id, const QColor &col) {
        if (id.isEmpty())
            return;
        const auto find = [this, &id](const QVector<PlayerToken> &team) -> const PlayerToken * {
            for (const PlayerToken &t : team)
                if (t.playerId == id)
                    return &t;
            return nullptr;
        };
        const PlayerToken *tok = find(m_homePlayers) ? find(m_homePlayers)
                                                     : find(m_awayPlayers);
        if (!tok)
            return;
        const QPointF vpos = tokenVisualPos(*tok);
        const QPointF ctr(f.x() + vpos.x() * cellW, f.y() + vpos.y() * cellH);
        const qreal breath = 0.5 - 0.5 * std::cos(m_pulsePhase);
        const qreal minDim = qMin(cellW, cellH);
        const qreal ringR = minDim * (0.58 + 0.025 * breath);
        const qreal glowR = minDim * (0.95 + 0.08 * breath);
        QRadialGradient halo(ctr, glowR);
        const int rimA = int(150 + 90 * breath); // 150..240
        halo.setColorAt(0.0, QColor(col.red(), col.green(), col.blue(), 0));
        halo.setColorAt((minDim * 0.5) / glowR, QColor(col.red(), col.green(), col.blue(), 0));
        halo.setColorAt(ringR / glowR, QColor(col.red(), col.green(), col.blue(), rimA));
        halo.setColorAt(1.0, QColor(col.red(), col.green(), col.blue(), 0));
        p.setPen(Qt::NoPen);
        p.setBrush(halo);
        p.drawEllipse(ctr, glowR, glowR);
        QPen ringPen(col, 255);
        ringPen.setWidthF(qMax(2.5, minDim * 0.075));
        p.setPen(ringPen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(ctr, ringR, ringR);
    };
    drawGlow(m_blockTarget, QColor(225, 64, 54));   // red = block
    drawGlow(m_passTarget, QColor(255, 255, 255));  // white = pass receiver
    // The KICKER of a kickoff gets the usual blue glow while the ball is
    // in flight (cleared by the settle timer; the ballInPlay guard keeps
    // it off for a kickoff TIMEOUT where the ball never enters play).
    if (m_ballInPlay)
        drawGlow(m_kickoffKickerId, QColor(70, 160, 255));

    // The per-turn BADGES on the ring ("come su FUMBBL"), drawn AFTER the
    // glows (the actor's blue halo would otherwise cover the badge
    // corners). Corners: top-right = green check (already acted),
    // top-left = gold lightning (blitzed this turn), bottom-left = gold "?"
    // (confused, state bit 512). All white disc + gold symbol, EXCEPT the
    // check (green disc + white symbol).
    const auto drawTurnBadges = [&](const PlayerToken &t, const QPointF &ctr,
                                    qreal s) {
        // Badge radius: 0.15 minDim.
        const qreal tbr = qMin(cellW, cellH) * 0.15;
        const auto drawBadge = [&](const QPointF &c,
                                   const QColor &discCol,
                                   const std::function<void()> &glyph) {
            p.setPen(QPen(QColor(0xd8, 0xb4, 0x4a), 1.0));
            p.setBrush(discCol);
            p.drawEllipse(c, tbr, tbr);
            glyph();
        };
        const QPointF topR(ctr.x() + s * 0.26, ctr.y() - s * 0.28);
        const QPointF topL(ctr.x() - s * 0.26, ctr.y() - s * 0.28);
        const QPointF botL(ctr.x() - s * 0.26, ctr.y() + s * 0.28);
        if (m_actedPlayers.contains(t.playerId)) {
            drawBadge(topR, QColor(34, 160, 70), [&]() {
                QPen chk(Qt::white, qMax(1.6, tbr * 0.34));
                chk.setCapStyle(Qt::RoundCap);
                chk.setJoinStyle(Qt::RoundJoin);
                p.setPen(chk);
                p.setBrush(Qt::NoBrush);
                QPolygonF tick;
                tick << topR + QPointF(-tbr * 0.45, tbr * 0.02)
                     << topR + QPointF(-tbr * 0.10, tbr * 0.36)
                     << topR + QPointF(tbr * 0.50, -tbr * 0.34);
                p.drawPolyline(tick);
            });
        }
        if (!t.playerId.isEmpty() && t.playerId == m_blitzingPlayer) {
            drawBadge(topL, QColor(255, 255, 255), [&]() {
                QPolygonF bolt;
                bolt << topL + QPointF(tbr * 0.12, -tbr * 0.52)
                     << topL + QPointF(-tbr * 0.28, tbr * 0.10)
                     << topL + QPointF(-tbr * 0.02, tbr * 0.10)
                     << topL + QPointF(-tbr * 0.14, tbr * 0.52)
                     << topL + QPointF(tbr * 0.30, -tbr * 0.08)
                     << topL + QPointF(tbr * 0.04, -tbr * 0.08);
                p.setPen(QPen(QColor(46, 40, 34), qMax(1.0, tbr * 0.10),
                              Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p.setBrush(kMarkGold);
                p.drawPolygon(bolt);
            });
        }
        if (t.state & 0x00200) { // PlayerState._BIT_CONFUSED (ffb-common)
            drawBadge(botL, QColor(255, 255, 255), [&]() {
                // drawText uses the PEN, not the brush — the "?" would be
                // painted with NoPen and the disc would come out empty.
                QFont qf = p.font();
                qf.setPointSizeF(qMax(5.0, tbr * 1.6));
                qf.setBold(true);
                p.setFont(qf);
                p.setPen(QPen(kMarkInk, qMax(1.2, tbr * 0.18), Qt::SolidLine,
                              Qt::RoundCap, Qt::RoundJoin));
                p.drawText(QRectF(botL.x() - tbr, botL.y() - tbr, tbr * 2, tbr * 2),
                           Qt::AlignCenter, QStringLiteral("?"));
                p.setPen(kMarkGold);
                p.drawText(QRectF(botL.x() - tbr, botL.y() - tbr, tbr * 2, tbr * 2),
                           Qt::AlignCenter, QStringLiteral("?"));
            });
        }
    };
    const auto badgePass = [&](const QVector<PlayerToken> &team) {
        for (const PlayerToken &t : team) {
            const QPointF vpos = tokenVisualPos(t);
            const QPointF ctr(f.x() + vpos.x() * cellW, f.y() + vpos.y() * cellH);
            drawTurnBadges(t, ctr, qMin(cellW, cellH) * 0.92);
        }
    };
    badgePass(m_homePlayers);
    badgePass(m_awayPlayers);

    // --- The BLOCK MARKER between attacker and defender: a football
    //     helmet while the die is pending, the chosen die's SYMBOL after
    //     the choice. Follows the tokens' animated glide, so it travels
    //     with the follow step too.
    if (!m_blockTarget.isEmpty() && !m_blockAttacker.isEmpty()) {
        auto findTok = [this](const QString &id) -> const PlayerToken * {
            for (const auto &team : { m_homePlayers, m_awayPlayers })
                for (const PlayerToken &t : team)
                    if (t.playerId == id)
                        return &t;
            return nullptr;
        };
        const PlayerToken *att = findTok(m_blockAttacker);
        const PlayerToken *def = findTok(m_blockTarget);
        if (att && def) {
            const QPointF va = tokenVisualPos(*att);
            const QPointF vd = tokenVisualPos(*def);
            const QPointF mid(f.x() + (va.x() + vd.x()) * 0.5 * cellW,
                              f.y() + (va.y() + vd.y()) * 0.5 * cellH);
            const qreal k = qMin(cellW, cellH) * 0.45;
            if (m_blockDieFace >= 1 && m_blockDieFace <= 6) {
                // The push arrow points ALONG THE REAL PUSH (attacker ->
                // defender), like the die's straight arrow.
                const qreal ang =
                        qRadiansToDegrees(std::atan2(vd.y() - va.y(),
                                                     vd.x() - va.x()));
                drawBlockDieSymbol(p, mid, k, m_blockDieFace, ang);
            } else {
                drawFootballHelmet(p, mid, k, va.x() < vd.x());
            }
        }
    }

    // --- Animated rain: droplet streaks + splash ripples, in the zoomed
    //     field space so they belong to the field scene. Stepped by
    //     m_rainTimer (see rainConfigure).
    if (m_raining)
        drawRain(p);

    // --- Bench box slots on the dugout panels (left = home, right = away).
    //     The dugouts are part of the scene, so the slots are drawn in the
    //     zoomed scene space (the hover hit-test uses the same field space
    //     via toFieldSpace).
    paintBoxStrip(p, m_homeColor, m_homeBox, true);
    paintBoxStrip(p, m_awayColor, m_awayBox, false);

    p.restore(); // end of the zoomed field space

}
