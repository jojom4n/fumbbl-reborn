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
#include <QPainterPath>
#include <QPair>
#include <QPaintEvent>
#include <QPixmap>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QTimer>
#include <QWheelEvent>

#include <cmath>

FieldView::FieldView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(390, 225); // 26:15 ratio at 15px cells
    setMouseTracking(true); // hover cards (Mockup F round 8)
    setCursor(Qt::OpenHandCursor);
    // Round 13 turn 4c: the user rejected the beige "light/clean" stadiums
    // and approved the dark-fantasy style of their reference — the dark
    // arena (Qwen i2i from the reference, see tools/mockup/r13_arena_gen.py
    // + stadium_cleanup.py) is now the default. medieval_arena stays
    // installed: stadiums are INTERCHANGEABLE (setStadium(name)).
    setStadium(QStringLiteral("dark_arena"));
    // Round 13: WASD pan — the field can take keyboard focus (clicking it or
    // zooming with the wheel grabs it; typing in the chat input still works
    // because that widget has the focus then).
    setFocusPolicy(Qt::StrongFocus);

    // Round 13 turn 5 (user): the EDGE PAN is REMOVED — pressing the cursor
    // against the window edge kept scrolling the zoomed field ("è rimasto
    // attivo uno scroll con il mouse"). Navigation is now exactly:
    // wheel = zoom, wheel-button click = reset, right-drag / WASD = pan.

    // Round 9: selection-halo pulse (~1.2 Hz breathing). The timer only
    // repaints the selected token's rect — never the whole field.
    m_pulseTimer = new QTimer(this);
    m_pulseTimer->setInterval(33); // ~30 fps
    connect(m_pulseTimer, &QTimer::timeout, this, [this]() {
        m_pulsePhase += 2.0 * M_PI * 1.2 * 0.033;
        if (m_pulsePhase > 2.0 * M_PI)
            m_pulsePhase -= 2.0 * M_PI;
        if (m_selectedPlayer.isEmpty() && m_blockTarget.isEmpty())
            return;
        // Round 13 turn 4: while a BLOCK glow is on, a full field update is
        // cheap enough (the glow is short-lived); the selection alone keeps
        // the efficient token-rect repaint.
        if (!m_blockTarget.isEmpty()) {
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

void FieldView::setHomeTeamName(const QString &name)
{
    m_homeName = name;
    update();
}

void FieldView::setAwayTeamName(const QString &name)
{
    m_awayName = name;
    update();
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

    // Round 13 (user): per-square MOVEMENT ANIMATION. Build the new on-field
    // model layout; every player that MOVED gets a glide through the grid
    // (old cell -> new cell, one square per leg), like the official client.
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
            startTokenAnim(id, np);
        } else if (!m_tokenAnim.value(id).active) {
            m_visualPos.insert(id, QPointF(x + 0.5, y + 0.5));
        }
        m_prevBall = np;
        m_havePrevBall = true;
    } else {
        m_visualPos.remove(id);
        m_tokenAnim.remove(id);
        m_havePrevBall = false;
    }
    if (anyTokenAnimActive())
        ensureAnimTimer();
    update();
}

void FieldView::setTurn(int turn)
{
    m_turn = turn;
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
    rainConfigure(); // round 12: rain on/off starts/stops the animation
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
// Round 12 — animated rain (user request): droplet streaks falling over the
// field, a splash ripple where each drop lands. The puddles (water patches
// drawn on the grass) catch their own drops with a BIGGER, longer ripple.
// The puddle layout is stable (fixed fractions of the field) so the water
// patches do not jump around between frames/resizes.
// ---------------------------------------------------------------------------
// Round 13 (user): the FIXED puddle layout is gone — the puddles are now
// DYNAMIC (rainConfigure / stepPuddles): they accumulate over match time,
// are smaller, organic, and fade in as they appear.

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

    // Round 13 (user): puddles are DYNAMIC — they accumulate over match
    // time (stepPuddles), not all at once. Seed a couple of "pre-existing"
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
    // Round 13 (user): prettier, SMALLER, organic puddles that FADE IN as
    // they appear (they accumulate over match time, not all at once).
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
    // Round 13 turn 4 (user): TOP-DOWN droplets — the field is seen from
    // ABOVE. Each drop is a SHORT bright streak that GROWS as it descends
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

// Mowing stripes (round 13): every GRASS column (1..24; 0/25 are the TD
// columns) is tinted light or dark alternating — the same values the
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
// (round 13: code-drawn — the baked version blurred with the texture at
// zoom). Port of the mockup's narrow_td_zone (crimson 128,30,28 + white
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

    // Vector TOUCHDOWN running the FULL length of the TD zone (round 13
    // user: "le parole corrano lungo tutta la touchdown zone", keeping the
    // current vector sharpness). The word is long for a 1-column zone, so it
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
    // Round 13 turn 5b (user, FINAL): the words face OUTWARD, mirrored —
    // the LEFT word's letter-tops point LEFT (rotated 270 deg, reads
    // bottom->top), the RIGHT word's letter-tops point RIGHT (rotated 90
    // deg, reads top->bottom). (Turn 5a pointed both toward the center:
    // "hai fatto casino" — each rotated another 180 deg.)
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

void FieldView::setSelectedPlayer(const QString &playerId)
{
    if (m_selectedPlayer == playerId)
        return;
    m_selectedPlayer = playerId;
    // The pulse timer runs only while a selection is visible (round 9:
    // no idle repaints on the field).
    if (playerId.isEmpty())
        m_pulseTimer->stop();
    else
        m_pulseTimer->start();
    update();
}

void FieldView::setBlockTarget(const QString &playerId)
{
    if (m_blockTarget == playerId)
        return;
    m_blockTarget = playerId;
    // Round 13 turn 4: the red block glow breathes with the same pulse
    // timer as the selection glow.
    if (m_selectedPlayer.isEmpty() && m_blockTarget.isEmpty())
        m_pulseTimer->stop();
    else
        m_pulseTimer->start();
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
    // SMOOTH WASD pan (round 13, user: "lo scroll da tastiera non è fluido").
    // Track the held keys and let a fast timer pan continuously — no more
    // jumpy auto-repeat steps. Only meaningful while zoomed (at zoom 1 the
    // letterboxed scene cannot pan).
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

void FieldView::setMoveTrace(const QVector<QVector<QPoint>> &paths)
{
    if (paths == m_moveTrace)
        return;
    m_moveTrace = paths;
    update();
}

// ---------------------------------------------------------------------------
// Round 13 (user): per-square movement animation. A moved token glides
// through the grid (old cell -> new cell, one square per leg) instead of
// jumping — fast and pleasant, like the official client. The model is
// authoritative; this is pure render state.
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
    static const qreal kLegDur = 0.085; // 85 ms per square (fast + pleasant)
    const qreal dt = 0.016;             // 16 ms frame
    bool any = false;
    for (auto it = m_tokenAnim.begin(); it != m_tokenAnim.end(); ++it) {
        TokenAnim &a = it.value();
        if (!a.active)
            continue;
        any = true;
        a.t += dt / kLegDur;
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

// The actor's CUMULATIVE move trail (round 13): one polyline per move
// action, through the FIELD CELL CENTERS (the line follows the grid — a
// center-to-center segment per stepped square, never a straight line from
// start to end), with an arrow at the end of each path. White over a dark
// under-stroke so it reads on both the grass and the crimson TD. Drawn in
// the zoomed scene space, under the tokens (the trail belongs to the field,
// the player walks on top of it).
void FieldView::drawMoveTrail(QPainter &p)
{
    if (m_moveTrace.isEmpty())
        return;
    const QRectF f = fieldRect();
    if (f.isEmpty())
        return;
    const qreal cellW = f.width() / kFieldWidth;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    for (const QVector<QPoint> &path : m_moveTrace) {
        if (path.size() < 2)
            continue; // a single square: nothing to trace yet
        // Defensive (round 13): a path with NO real movement (every cell
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
        // Round 13 (user): the trail must leave a SMALL GAP before the
        // player box, end in an ARROW, and the arrow must NOT invade the
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
        p.setPen(QPen(QColor(255, 255, 255, 235), cellW * 0.075,
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(linePts.constData(), linePts.size());
        // Arrowhead at the end of the trail, pointing along the last step.
        const QPointF perp(-u.y(), u.x());
        QPolygonF tri;
        tri << tip << (base + perp * arrowLen * 0.62)
            << (base - perp * arrowLen * 0.62);
        p.setBrush(QColor(255, 255, 255, 240));
        p.setPen(QPen(QColor(24, 24, 30, 200), qMax(1.5, cellW * 0.05)));
        p.drawPolygon(tri);
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
        // Left button = SELECT / show the info card (the round-12 behavior,
        // kept). An EMPTY id (clicked an empty square) is emitted too: the
        // caller uses it to HIDE the card. Emitted on the press, as before.
        const PlayerToken *t = tokenAt(event->pos());
        emit playerClicked(t ? t->playerId : QString());
        setFocus(Qt::MouseFocusReason); // WASD pan right after a click
    } else if (event->button() == Qt::MiddleButton) {
        // Round 13 (user): the view RESET moved to the MIDDLE click (the
        // right click no longer resets — the right button is the drag-pan).
        resetView();
    } else if (event->button() == Qt::RightButton) {
        // Round 13 (user): right button + DRAG = pan (brought back). Record
        // the press so mouseMove can pan and mouseRelease can tell a drag
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
    // Right button + DRAG = pan (round 13, user): move the view with the
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
        // Round 13 (user): a right DRAG was a pan (nothing to do on release).
        // A right CLICK (no drag) in play mode on a player = action wheel.
        // The reset moved to the MIDDLE click, so a plain right click no
        // longer resets the view.
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
    // Round 13 (user, FINAL mapping): WHEEL = ZOOM (anchored on the cursor).
    // Panning is right-button+DRAG or WASD (smooth). MIDDLE click resets.
    // A plain wheel used to PAN, which at zoom=1 did nothing visible — hence
    // "non funziona più lo zoom". Now the wheel always zooms in/out.
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
    // Round 13 (user): the SCENE is the STADIUM image, COVER-fitted to the
    // widget (fill it, preserve aspect). The anchor is the FIELD PIT CENTER
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
    // ROUND 13 TURN 4 FIX ("stadium shifted right + small", "cannot pan to
    // the right side when zoomed"): the rect is anchored at the ORIGIN
    // (0,0) — the centering is clampPan()'s job. Centering it HERE too
    // double-applied the letterbox offset: at zoom 1 the stadium was drawn
    // shifted right by the whole left margin, and the zoom-clamp range was
    // computed as if the scene started at 0, so the stadium's right side
    // could never be panned to the screen edge.
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
    // (round 13 turn 5, user: "il campo dovrebbe coprire tutto... espanso al
    // massimo, senza lasciare spazi vuoti" — the flat brown apron strips
    // above/below the field were rejected). The cells take the pit's own
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

// Dugout sections (round 12): the box sections, shared by the slot painter
// and the hover hit-test. State bases from ffb-common PlayerState:
// RESERVE = 9, KO = 5, INJURED = 6..8,10.
namespace {
// Round 13 turn 5 (user): the dugout is a SINGLE bench section holding up
// to 16 players (the physical game's 3-section split is replaced by a small
// STATE BADGE on each face ring, like the number badges on field tokens):
//   * reserves (9)            : plain face, no badge
//   * KO (5)                  : little cartoon "dizzy" stars (top-left)
//   * injured (6,7) / banned (13) / missing (10): red medical cross
//   * dead (8)                : a small skull
// The badge sits TOP-LEFT of the ring (user: "così non coprono il loro
// numero") while the number stays bottom-right. The wood panel is a
// SEPARATE asset MOUNTED on the manifest's dugout rect (user's idea): the
// dugout is UNIVERSAL — any future stadium ships its rect, the same panel
// mounts on it.
constexpr int kDugoutMaxFaces = 16;

struct DugoutLayout {
    qreal faceD = 0;
    QVector<QPointF> centers; // one per face, in entries order
};

// The bench faces: 1 row for <=8 players, 2 rows of 8 above — the faces
// stay readable even with a full 16-man bench (user: "le facce dei
// giocatori non siano troppo piccole"). Shared by boxSlots (hit-test) and
// paintBoxStrip (painter) so the two never drift.
DugoutLayout dugoutFaceLayout(const QRectF &R, int count)
{
    DugoutLayout out;
    count = qBound(0, count, kDugoutMaxFaces);
    if (count == 0)
        return out;
    const qreal marginX = R.width() * 0.04;
    const qreal marginY = R.height() * 0.09;
    const qreal availW = R.width() - 2 * marginX;
    const qreal availH = R.height() - 2 * marginY;
    const int perRow = qMin(count, 8);
    const int rows = qMax(1, (count + perRow - 1) / perRow);
    out.faceD = qMin(qMin(availH / rows * 0.94, availW / perRow * 0.92),
                     R.height() * 0.80);
    for (int i = 0; i < count; ++i) {
        const int row = i / perRow;
        const int col = i % perRow;
        const int inRow = qMin(perRow, count - row * perRow);
        const qreal rowW = inRow * out.faceD;
        const qreal x0 = R.center().x() - rowW / 2;
        const qreal cy = rows == 1
            ? R.center().y()
            : (row == 0 ? R.center().y() - availH / 4 : R.center().y() + availH / 4);
        out.centers.append(QPointF(x0 + col * out.faceD + out.faceD / 2, cy));
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

// The little STATE badge on the dugout face ring (TOP-LEFT of the ring,
// so it never covers the number badge at the bottom-right). Vector —
// QPainter is the right tool for small crisp badges (turn-5 rule).
void drawDugoutStateBadge(QPainter &p, int state, const QRectF &ring)
{
    const int st = state & 0xFF;
    if (st != 5 && st != 6 && st != 7 && st != 8 && st != 10 && st != 13)
        return; // reserves (and any other state): plain face
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const qreal br = ring.width() * 0.27; // badge radius
    // Top-left 45 deg, straddling the ring's edge (like the number badge).
    const QPointF c = ring.center()
                       + QPointF(-ring.width() * 0.37, -ring.height() * 0.37);
    if (st == 5) { // KO: cartoon dizzy stars
        drawStar(p, c + QPointF(-br * 0.55, -br * 0.35), br * 0.40, 0.3,
                 QColor(255, 214, 64, 255));
        drawStar(p, c + QPointF(br * 0.50, -br * 0.45), br * 0.28, -0.2,
                 QColor(255, 214, 64, 255));
        drawStar(p, c + QPointF(br * 0.15, br * 0.50), br * 0.32, 0.8,
                 QColor(255, 214, 64, 255));
    } else if (st == 8) { // dead: small skull
        p.setPen(QPen(QColor(25, 25, 30, 210), 0.8));
        p.setBrush(QColor(236, 233, 226, 255));
        p.drawEllipse(QRectF(c.x() - br * 0.72, c.y() - br * 0.85,
                             br * 1.44, br * 1.44));
        p.drawRoundedRect(QRectF(c.x() - br * 0.38, c.y() + br * 0.35,
                                 br * 0.76, br * 0.42), 1, 1); // jaw
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(28, 28, 33, 255));
        p.drawEllipse(c + QPointF(-br * 0.30, -br * 0.15), br * 0.19, br * 0.21);
        p.drawEllipse(c + QPointF(br * 0.30, -br * 0.15), br * 0.19, br * 0.21);
        QPolygonF nose;
        nose << c + QPointF(0, br * 0.03) << c + QPointF(-br * 0.11, br * 0.28)
             << c + QPointF(br * 0.11, br * 0.28);
        p.drawPolygon(nose);
        p.setPen(QPen(QColor(28, 28, 33, 220), 0.8));
        p.drawLine(QPointF(c.x() - br * 0.18, c.y() + br * 0.60),
                   QPointF(c.x() - br * 0.18, c.y() + br * 0.76));
        p.drawLine(QPointF(c.x() + br * 0.18, c.y() + br * 0.60),
                   QPointF(c.x() + br * 0.18, c.y() + br * 0.76));
    } else { // injured (6,7) / missing (10) / banned (13): red medical cross
        p.setPen(QPen(QColor(255, 255, 255, 255), 1.0));
        p.setBrush(QColor(255, 255, 255, 245));
        p.drawEllipse(c, br, br);
        const qreal arm = br * 0.52;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(204, 34, 30, 255));
        p.drawRoundedRect(QRectF(c.x() - arm / 2, c.y() - arm * 1.15,
                                 arm, arm * 2.3), 1, 1);
        p.drawRoundedRect(QRectF(c.x() - arm * 1.15, c.y() - arm / 2,
                                 arm * 2.3, arm), 1, 1);
    }
    p.restore();
}
} // namespace

// Round 12: the bench slots live on the DUGOUT PANEL (outside the TD
// columns). Each of the 3 image bands (RESERVE / KO / INJURED) holds a
// 2-column x 4-row grid of slots below the baked-in band label. Field space;
// shared by painter + hover so the two never drift apart.
QVector<QPair<QRectF, const FieldView::BoxEntry*>> FieldView::boxSlots(
    const QVector<BoxEntry> &entries, bool leftSide) const
{
    // Round 13 turn 5: ONE section, all bench players in entries order
    // (1 row for <=8, 2 rows of 8 above) — the hit-test shares the exact
    // layout with the painter (dugoutFaceLayout).
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

// The dugout slot under a WIDGET point (round 12: the dugouts are inside
// the zoomed scene, so the point is mapped to field space first).
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

// Dugout bench (round 13): the team's top-band pavilion. Three horizontal
// section rows (RESERVE / KO / INJURED), each with a GW-style symbol badge
// (left gutter) + a run of team-ringed faces (only the players actually
// present — no empty rings). A thin team-color bar under the pavilion says
// whose dugout it is.
void FieldView::paintBoxStrip(QPainter &p, const QColor &accent,
                              const QVector<BoxEntry> &entries, bool leftSide) const
{
    const QRectF R = dugoutRect(leftSide); // the team's top-band pavilion
    if (R.isEmpty())
        return;

    // 1) The WOODEN PANEL — a separate asset MOUNTED on the manifest rect
    //    (round 13 turn 5, user's idea: the dugout is universal across
    //    stadiums, the manifest carries the coordinates). The baked-in
    //    pavilions of the stadium image stay underneath as a fallback.
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

    // 2) The bench faces (team-ringed, number bottom-right, STATE badge
    //    top-left of the ring — round 13 turn 5, user design).
    const DugoutLayout lay =
        dugoutFaceLayout(R, qMin(entries.size(), kDugoutMaxFaces));
    for (int i = 0; i < lay.centers.size(); ++i) {
        const BoxEntry *e = &entries[i];
        const QPointF c = lay.centers[i];
        const qreal faceD = lay.faceD;
        const QRectF ring(c.x() - faceD / 2, c.y() - faceD / 2, faceD, faceD);
        const QPixmap pm = assets::playerFace(e->positionName, e->race);
        if (!pm.isNull()) {
            const qreal fr = faceD * 0.44;
            const QRectF faceR(c.x() - fr, c.y() - fr, fr * 2, fr * 2);
            p.save();
            QPainterPath clip;
            clip.addEllipse(faceR);
            p.setClipPath(clip);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawPixmap(faceR, pm, QRectF(0, 0, pm.width(), pm.height()));
            p.restore();
        }
        p.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), 235),
                      qMax(1.0, faceD * 0.09)));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(ring);

        // Number badge (bottom-right of the ring, like the field tokens).
        if (e->number > 0) {
            const qreal br = faceD * 0.26;
            const QPointF bc(c.x() + faceD * 0.27, c.y() + faceD * 0.29);
            p.setPen(QPen(QColor(0xd8, 0xb4, 0x4a), 1.0));
            p.setBrush(QColor(0x1e, 0x1e, 0x26));
            p.drawEllipse(bc, br, br);
            p.setPen(Qt::white);
            QFont nf = p.font();
            nf.setPointSizeF(qMax(4.5, br * 1.4));
            nf.setBold(true);
            p.setFont(nf);
            p.drawText(QRectF(bc.x() - br, bc.y() - br, br * 2, br * 2),
                       Qt::AlignCenter, QString::number(e->number));
        }

        // State badge (TOP-LEFT of the ring — never covers the number).
        drawDugoutStateBadge(p, e->state, ring);
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

    // Dark apron behind the scene (round 13): the stadium scene is drawn in
    // the zoomed/panned space below; this only shows if the scene ever fails
    // to cover (no stadium art) — a dark stadium floor.
    p.fillRect(QRectF(0, 0, width(), height()), QColor(40, 38, 36));

    // Field navigation (round 8): the WHOLE scene (dugouts, texture, grid,
    // tokens, rain) lives in a zoomed/panned space. Round 12: the dugout
    // panels are part of the scene too (they zoom with the field).
    p.save();
    p.translate(m_pan);
    p.scale(m_zoom, m_zoom);

    const QRectF s = sceneRect();
    const qreal cellW = f.width() / kFieldWidth;
    const qreal cellH = f.height() / kFieldHeight;

    // --- Stadium (round 13, user): the SCENE is the STADIUM image (an
    //     interchangeable top-down backdrop + manifest). Drawn COVER (fills
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
        // Dugout benches (team-ringed faces + GW section symbols) on the
        // stadium's top-band pavilions.
        paintBoxStrip(p, m_homeColor, m_homeBox, true);
        paintBoxStrip(p, m_awayColor, m_awayBox, false);
    }

    // --- Field texture (round 13): the Qwen art is PURE GRASS now (the
    //     baked grid/TD/TOUCHDOWN came out blurred and duplicated at zoom —
    //     user round 12/13). Everything structural is drawn in CODE over it:
    //     mowing stripes, crimson 1-column TD zones + vector TOUCHDOWN,
    //     the exact grid. The texture only supplies the grass detail, which
    //     stays soft even when zoomed (fine detail, tolerable) while lines
    //     and text stay razor-sharp at any zoom/HiDPI (user point 15).
    const QPixmap tex = assets::fieldTexture();
    if (!tex.isNull()) {
        // ROUND 13 TURN 4 (user): NO empty grass apron — the 26x15 pitch is
        // drawn over its rect ONLY. The "no black band" rule is satisfied by
        // the STADIUM itself: each stadium's field pit is designed at the
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
    // Round 11: the WIDE ZONE boundary lines (what the mockup called the
    // missing "side zone" lines). Ground truth (ffb-common
    // FieldCoordinateBounds): UPPER wide zone = y 0..3, LOWER = y 11..14,
    // centre field = y 4..10 — so the emphasized lines run at y=4 and y=11,
    // plus a slightly stronger sideline at y=0 / y=14 (the pitch edges).
    // ROUND 13 TURN 4 (user): the wide-zone lines must NOT continue into the
    // TOUCHDOWN zones — they start/stop at the TD line (x=1 / x=25), not at
    // the end of the pitch.
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

    // --- Team logos: per-match watermarks centered on each half.
    // Round 8: 25% — the user found 19% still too transparent.
    // Round 13 (user): both logos the SAME size, and they must NOT overflow
    // into the WIDE ZONES (the y 0..3 / y 11..14 bands). The box is fitted to
    // the central field (y 4..10) and the logo is drawn aspect-preserved (no
    // stretch) inside it, so both crests read the same size.
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

    // --- Puddles (round 12): water patches on the grass, drawn UNDER the
    //     tokens/ball. Present only when it rains — the animated rain's
    //     drops splash into them.
    if (m_raining)
        drawPuddles(p, f);

    // --- The actor's cumulative MOVE TRAIL (round 13): drawn on the field,
    //     under the tokens — the player walks on top of his own trail.
    drawMoveTrail(p);

    // --- Ball on an empty square -------------------------------------------
    const auto atBall = [this](const QVector<PlayerToken> &team) {
        for (const PlayerToken &t : team)
            if (t.x == m_ballX && t.y == m_ballY)
                return true;
        return false;
    };
    const bool ballOccupied = atBall(m_homePlayers) || atBall(m_awayPlayers);
    if (m_ballInPlay && !ballOccupied) {
        // Round 13 (user): the ball glides through the grid too — draw it at
        // its animated VISUAL position (cell coords), not the model cell.
        const QPointF bvc = m_visualPos.value(QLatin1String("__ball__"),
                                              QPointF(m_ballX + 0.5, m_ballY + 0.5));
        const QPointF bctr(f.x() + bvc.x() * cellW, f.y() + bvc.y() * cellH);
        const QRectF ballRect(bctr.x() - cellW * 0.22, bctr.y() - cellH * 0.16,
                              cellW * 0.44, cellH * 0.32);
        p.setPen(QPen(QColor(0x40, 0x26, 0x12), 1));
        p.setBrush(QColor(0x8b, 0x5a, 0x2b));
        p.drawEllipse(ballRect);
    }

    // --- Target brackets + action wheel (drawn UNDER the tokens, like the
    //     mockup). Round 9: NO LONGER follows the FFB acting-player state
    //     (t.isActing) nor the click selection — only an EXPLICIT target
    //     (setActionWheelTarget, set by a right-click on a player in play
    //     mode). In spectator mode the target stays empty: no wheel. The
    //     wheel + bracket code is kept "meso da parte" for the play mode.
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
        const QColor team = sideHome ? m_homeColor : m_awayColor;
        const QColor teamDark = team.darker(150);
        const qreal s = qMin(cellW, cellH) * 0.92;
        for (const PlayerToken &t : players) {
            if (t.x < 0 || t.x >= kFieldWidth || t.y < 0 || t.y >= kFieldHeight)
                continue; // box coordinate / off field
            // Round 13 (user): the token is drawn at its ANIMATED visual
            // position (cell coords) so it glides square-by-square while
            // moving; at rest it sits exactly on the model cell center.
            const QPointF vpos = tokenVisualPos(t);
            const QPointF ctr(f.x() + vpos.x() * cellW, f.y() + vpos.y() * cellH);

            // Team-colored circle behind the face. Round 11: DOWN players
            // keep the FULL round token — round 8 stretched the face into a
            // flat ellipse and the face came out "schiacciata" (user
            // report). Prone/stunned is shown by the state mark instead.
            const QRectF circ(ctr.x() - s * 0.5, ctr.y() - s * 0.5, s, s);
            p.setPen(QPen(teamDark, 1.6));
            p.setBrush(team);
            p.drawEllipse(circ);

            // Face on top (slightly inset from the circle). Custom-named
            // star pieces fall back to the side's race representative. The
            // face assets are square (RGB, no alpha), so the face is clipped
            // to the team circle — the mockup does the same with a mask.
            const QString race = sideHome ? m_homeRace : m_awayRace;
            const QPixmap pm = assets::playerFace(t.positionName, race);
            if (!pm.isNull()) {
                const qreal fs = s * 0.78;
                const QRectF face(ctr.x() - fs / 2, ctr.y() - fs / 2, fs, fs);
                p.save();
                QPainterPath clip;
                clip.addEllipse(circ.adjusted(fs * 0.06, fs * 0.06, -fs * 0.06, -fs * 0.06));
                p.setClipPath(clip);
                p.drawPixmap(face, pm, QRectF(0, 0, pm.width(), pm.height()));
                p.restore();
            }

            // Round 10: player state mark over the face — prone = "/",
            // stunned = "X" (white with a dark under-stroke for contrast).
            const int base = t.state & 0xFF;
            if (base == 3 || base == 4) {
                const qreal mw = s * 0.46;
                const QRectF mr(ctr.x() - mw / 2, ctr.y() - mw / 2, mw, mw);
                QPen under(QColor(24, 24, 30, 210), qMax(3.0, s * 0.15));
                under.setCapStyle(Qt::RoundCap);
                QPen over(QColor(255, 255, 255, 235), qMax(1.5, s * 0.08));
                over.setCapStyle(Qt::RoundCap);
                const auto diag = [&](const QPointF &a, const QPointF &b) {
                    p.setPen(under);
                    p.drawLine(a, b);
                    p.setPen(over);
                    p.drawLine(a, b);
                };
                if (base == 3)
                    diag(mr.bottomLeft(), mr.topRight()); // "/"  (prone)
                else {
                    diag(mr.topLeft(), mr.bottomRight()); // "\"
                    diag(mr.bottomLeft(), mr.topRight()); // "/"  -> "X" (stunned)
                }
            }

            // Number badge (bottom-right of the token — follows it while it
            // glides, not the model cell).
            const qreal br = qMin(cellW, cellH) * 0.24;
            const QPointF bc(ctr.x() + s * 0.28, ctr.y() + s * 0.30);
            p.setPen(QPen(QColor(0xd8, 0xb4, 0x4a), 1.2));
            p.setBrush(QColor(0x1e, 0x1e, 0x26));
            p.drawEllipse(bc, br, br);
            p.setPen(Qt::white);
            QFont numFont = p.font();
            numFont.setPointSizeF(qMax(5.0, br * 1.5));
            numFont.setBold(true);
            p.setFont(numFont);
            p.drawText(QRectF(bc.x() - br, bc.y() - br, br * 2, br * 2), Qt::AlignCenter,
                       t.number > 0 ? QString::number(t.number) : QStringLiteral("?"));

            // Ball carrier indicator (small football, top-left of the token)
            if (t.hasBall) {
                const QRectF br2(ctr.x() - s * 0.5, ctr.y() - s * 0.44,
                                 cellW * 0.34, cellH * 0.22);
                p.setPen(QPen(QColor(0x40, 0x26, 0x12), 1));
                p.setBrush(QColor(0xc7, 0x83, 0x3b));
                p.drawEllipse(br2);
            }
        }
    };

    drawTeam(m_homePlayers, true);
    drawTeam(m_awayPlayers, false);

    // --- Selection: BLUE RIM GLOW (round 9: blue radial + ring; round 10:
    //     the glow now hugs the token's RIM only — transparent over the
    //     face, a soft blue band just outside the team circle, fading out —
    //     with a crisp blue ring. Pulsing: the band's alpha breathes
    //     sinusoidally (~1.2 Hz, m_pulseTimer). Drawn last in field space
    //     so it is always visible and follows zoom/pan.
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
            // Round 11: round 10's subtle ring was INVISIBLE on the bright
            // green field (user report) — a clearly visible rim glow, still
            // limited to the border (the face stays uncovered).
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

    // --- Round 13 turn 4 (user): RED GLOW on the player being BLOCKED —
    //     same shape as the selection's blue rim glow, in the reserved red
    //     (injury/failure/block). Armed by the model on the block report,
    //     cleared when the block resolves (state change / new turn / new
    //     actor). Follows the token's animated glide.
    if (!m_blockTarget.isEmpty()) {
        const auto find = [this](const QVector<PlayerToken> &team) -> const PlayerToken * {
            for (const PlayerToken &t : team)
                if (t.playerId == m_blockTarget)
                    return &t;
            return nullptr;
        };
        const PlayerToken *blk = find(m_homePlayers) ? find(m_homePlayers) : find(m_awayPlayers);
        if (blk) {
            const QPointF vpos = tokenVisualPos(*blk);
            const QPointF ctr(f.x() + vpos.x() * cellW, f.y() + vpos.y() * cellH);
            const qreal breath = 0.5 - 0.5 * std::cos(m_pulsePhase);
            const qreal minDim = qMin(cellW, cellH);
            const qreal ringR = minDim * (0.58 + 0.025 * breath);
            const qreal glowR = minDim * (0.95 + 0.08 * breath);
            QRadialGradient halo(ctr, glowR);
            const int rimA = int(150 + 90 * breath); // 150..240
            halo.setColorAt(0.0, QColor(225, 64, 54, 0));
            halo.setColorAt((minDim * 0.5) / glowR, QColor(225, 64, 54, 0));
            halo.setColorAt(ringR / glowR, QColor(225, 64, 54, rimA));
            halo.setColorAt(1.0, QColor(225, 64, 54, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(ctr, glowR, glowR);
            QPen ringPen(QColor(255, 110, 100, 255), qMax(2.5, minDim * 0.075));
            p.setPen(ringPen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(ctr, ringR, ringR);
        }
    }

    // --- Animated rain (round 12): droplet streaks + splash ripples, in the
    //     zoomed field space so they belong to the field scene. Stepped by
    //     m_rainTimer (see rainConfigure). Replaces the old static specks.
    if (m_raining)
        drawRain(p);

    // --- Bench box slots on the dugout panels (left = home, right = away).
    //     Round 12: the dugouts are part of the scene, so the slots are drawn
    //     in the zoomed scene space (the hover hit-test uses the same field
    //     space via toFieldSpace).
    paintBoxStrip(p, m_homeColor, m_homeBox, true);
    paintBoxStrip(p, m_awayColor, m_awayBox, false);

    p.restore(); // end of the zoomed field space

}
