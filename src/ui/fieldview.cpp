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
    // Round 13: WASD pan — the field can take keyboard focus (clicking it or
    // zooming with the wheel grabs it; typing in the chat input still works
    // because that widget has the focus then).
    setFocusPolicy(Qt::StrongFocus);

    // Round 13: EDGE PAN — a fast timer polls the cursor position: pressed
    // against the window's edge the view scrolls in that direction (and the
    // cursor is clamped to the window, multi-monitor safe). Active only
    // while zoomed (see edgePanTick).
    m_edgePanTimer = new QTimer(this);
    m_edgePanTimer->setInterval(16); // ~60 fps
    connect(m_edgePanTimer, &QTimer::timeout, this, &FieldView::edgePanTick);
    m_edgePanTimer->start();

    // Round 9: selection-halo pulse (~1.2 Hz breathing). The timer only
    // repaints the selected token's rect — never the whole field.
    m_pulseTimer = new QTimer(this);
    m_pulseTimer->setInterval(33); // ~30 fps
    connect(m_pulseTimer, &QTimer::timeout, this, [this]() {
        m_pulsePhase += 2.0 * M_PI * 1.2 * 0.033;
        if (m_pulsePhase > 2.0 * M_PI)
            m_pulsePhase -= 2.0 * M_PI;
        if (m_selectedPlayer.isEmpty())
            return;
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
    // Round 13 (user): TOP-DOWN droplets — the field is seen from ABOVE, so a
    // falling drop reads as a small bright DOT (its cross-section), not a
    // side-view "platform" streak. The sim spawns/lands them all over the
    // whole field, so the rain covers it entirely.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(226, 238, 252, 150));
    for (const RainSim::Drop &d : m_rain.drops())
        p.drawEllipse(QPointF(d.x, d.y), 1.5, 1.5);

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
    for (const qreal colCenter : { f.x() + cellW * 0.5,
                                   f.right() - cellW * 0.5 }) {
        p.save();
        p.translate(colCenter, f.center().y());
        p.rotate(90);         // local +x now points DOWN (reads top->bottom)
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

// ---------------------------------------------------------------------------
// Round 13 — navigation (user spec, replaces the round-8 drag pan):
//   * EDGE PAN: cursor pressed against the window edge scrolls the view;
//   * WASD: W up, A left, S down, D right;
//   * no button-drag pan (left or right);
//   * left click = select, right click = reset (kept).
// ---------------------------------------------------------------------------
void FieldView::edgePanTick()
{
    // At zoom 1 the letterboxed scene cannot pan: never trap the mouse.
    if (m_zoom <= 1.0001)
        return;
    QWidget *top = window();
    if (!top)
        return;
    const QRect wr = top->frameGeometry(); // global (incl. title bar)
    const QPoint cur = QCursor::pos();
    if (!wr.contains(cur))
        return;
    const QPointF d = edgePanDelta(wr, cur, m_zoom);
    if (d.isNull())
        return;
    m_pan += d;
    clampPan();
    update();
    // MULTI-MONITOR (user request): while the user keeps the mouse against
    // the edge, clamp it INSIDE the window — it can never spill onto the
    // neighboring monitor, and the clamped cursor is still in the edge zone
    // so the pan keeps running.
    const QPoint clamped(qBound(wr.left() + 1, cur.x(), wr.right() - 1),
                         qBound(wr.top() + 1, cur.y(), wr.bottom() - 1));
    if (clamped != cur)
        QCursor::setPos(clamped);
}

QPointF FieldView::edgePanDelta(const QRect &wr, const QPoint &cursor, qreal zoom)
{
    if (zoom <= 1.0001 || !wr.contains(cursor))
        return QPointF();
    static const int kZone = 24;      // px from the window edge
    static const qreal kMax = 14.0;   // widget px per frame at full depth
    const auto depth = [](int dist) -> qreal {
        return qBound(0.0, 1.0 - qreal(dist) / kZone, 1.0);
    };
    // Looking toward an edge = the content moves toward that edge on screen:
    // left edge -> pan right (positive), top edge -> pan down (positive).
    qreal dx = 0.0, dy = 0.0;
    if (cursor.x() < wr.left() + kZone)
        dx = depth(cursor.x() - wr.left()) * kMax;
    else if (cursor.x() > wr.right() - kZone)
        dx = -depth(wr.right() - cursor.x()) * kMax;
    if (cursor.y() < wr.top() + kZone)
        dy = depth(cursor.y() - wr.top()) * kMax;
    else if (cursor.y() > wr.bottom() - kZone)
        dy = -depth(wr.bottom() - cursor.y()) * kMax;
    return QPointF(dx, dy);
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
    const QPoint delta = event->angleDelta();
    // Round 13 (user): a PLAIN wheel SCROLLS (pans) the view — "lo scroll
    // con il mouse verso l'alto non funzionava" (the wheel was zoom-only).
    // CTRL + wheel ZOOMS (anchored on the cursor, round-8 behavior kept).
    if (event->modifiers() & Qt::ControlModifier) {
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
        return;
    }
    // Plain wheel = SCROLL (pan). Scroll up (dy>0) reveals what's above
    // (content moves down); trackpad horizontal (dx) pans left/right. Only
    // meaningful while zoomed (at zoom 1 the letterboxed scene has nothing
    // to scroll and clampPan re-centers it).
    const int dx = delta.x();
    const int dy = delta.y();
    if (dx == 0 && dy == 0) {
        event->accept();
        return;
    }
    const qreal k = 0.5; // widget px per angleDelta unit
    m_pan += QPointF(-dx * k, dy * k);
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
    // The scene (round 13): [dugout | gap | pitch | gap | dugout] = 38x15
    // cells. The cells stay SQUARE (a Blood Bowl grid is square): the scene
    // is fitted into the widget and centered; the leftover is the dark
    // stadium apron (paintEvent fills the widget with it first). ORIGIN at
    // (0,0) — the m_pan transform (centering at zoom=1, user pan when
    // zoomed) places it on screen.
    if (width() <= 0 || height() <= 0)
        return QRectF();
    const qreal cell = qMin(width() / qreal(kSceneWidth), height() / qreal(kFieldHeight));
    return QRectF(0, 0, kSceneWidth * cell, kFieldHeight * cell);
}

QRectF FieldView::dugoutPanelRect(bool leftSide) const
{
    // Round 13: the panel is the FULL scene height (15 rows — the asset is
    // regenerated 1:3) — bigger slots, more recognizable faces (user).
    const QRectF s = sceneRect();
    if (s.isEmpty())
        return QRectF();
    const qreal cw = s.width() / kSceneWidth;
    const qreal x = leftSide ? s.x() : s.right() - kDugoutCells * cw;
    return QRectF(x, s.y(), kDugoutCells * cw, s.height());
}

QRectF FieldView::fieldRect() const
{
    // The PITCH (round 13): the middle 26 columns of the scene. The dugout
    // panels occupy the outer kDugoutCells columns; between each panel and
    // the pitch there is a visible GAP (kSceneGapCells) of dark stadium
    // apron — user round 13: the dugout must be separated HORIZONTALLY from
    // the field (the old baked wood frame ran "under" the field).
    const QRectF s = sceneRect();
    if (s.isEmpty())
        return QRectF();
    const qreal cw = s.width() / kSceneWidth;
    return QRectF(s.x() + (kDugoutCells + kSceneGapCells) * cw, s.y(),
                  kFieldWidth * cw, s.height());
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
struct DugoutSection { const char *label; QVector<int> codes; };
const DugoutSection kDugoutSections[] = {
    { "RESERVE", { 9 } },
    { "KO", { 5 } },
    { "INJURED", { 6, 7, 8, 10 } },
};
bool dugoutInSection(int st, const QVector<int> &codes)
{
    st &= 0xFF;
    return codes.contains(st);
}
} // namespace

// Round 12: the bench slots live on the DUGOUT PANEL (outside the TD
// columns). Each of the 3 image bands (RESERVE / KO / INJURED) holds a
// 2-column x 4-row grid of slots below the baked-in band label. Field space;
// shared by painter + hover so the two never drift apart.
QVector<QPair<QRectF, const FieldView::BoxEntry*>> FieldView::boxSlots(
    const QVector<BoxEntry> &entries, bool leftSide) const
{
    QVector<QPair<QRectF, const FieldView::BoxEntry*>> filled;
    const QRectF panel = dugoutPanelRect(leftSide);
    if (panel.isEmpty())
        return filled;

    // Round 13: full-height panel (15 rows -> 3 bands of 5 rows). The slots
    // are BIGGER than round 12 (user: "slot più grandi, volti più
    // riconoscibili"): RESERVE 2x2 (the reserve bench is short), KO and
    // INJURED 2x4 (down players accumulate).
    // Round 13 (user): grid sized for up to 16 players per section (4x4),
    // matching paintBoxStrip (keep the two in sync).
    constexpr int kCols = 4;
    const int kRows[] = { 4, 4, 4 };
    const qreal bandH = panel.height() / 3.0;
    for (int sec = 0; sec < 3; ++sec) {
        QVector<const BoxEntry *> mine;
        for (const BoxEntry &e : entries)
            if (dugoutInSection(e.state, kDugoutSections[sec].codes))
                mine.append(&e);
        const int rows = kRows[sec];
        const qreal slotTop = panel.y() + sec * bandH + bandH * 0.28; // below the label
        const qreal slotH = bandH * 0.68;
        const qreal colW = panel.width() / kCols;
        const qreal pitch = slotH / rows;
        const qreal r = qMin(colW, pitch) * 0.46;
        for (int i = 0; i < mine.size() && i < kCols * rows; ++i) {
            const int col = i % kCols, row = i / kCols;
            const qreal cx = panel.x() + (col + 0.5) * colW;
            const qreal cy = slotTop + row * pitch + pitch / 2;
            filled.append({ QRectF(cx - r, cy - r, r * 2, r * 2), mine[i] });
        }
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

// Dugout bench slots (round 12): faces in team-colored rings on the DUGOUT
// panel image (the soil art provides the RESERVE / KO / INJURED labels and
// the band structure). Empty slots are plain gray rings. A team-color bar on
// the panel edge facing the pitch says whose dugout it is.
void FieldView::paintBoxStrip(QPainter &p, const QColor &accent,
                              const QVector<BoxEntry> &entries, bool leftSide) const
{
    const QRectF panel = dugoutPanelRect(leftSide);
    if (panel.isEmpty())
        return;

    // Round 13: same (bigger) slot grid as boxSlots — keep the two in sync.
    // Round 13 (user): grid sized for up to 16 players per section (4x4),
    // matching boxSlots. NO empty "tondini" — only the players that are
    // actually in the section are drawn (empty slots stay empty, no rings).
    constexpr int kCols = 4;
    const int kRows[] = { 4, 4, 4 };
    const qreal bandH = panel.height() / 3.0;

    for (int sec = 0; sec < 3; ++sec) {
        QVector<const BoxEntry *> mine;
        for (const BoxEntry &e : entries)
            if (dugoutInSection(e.state, kDugoutSections[sec].codes))
                mine.append(&e);

        const int rows = kRows[sec];
        const qreal slotTop = panel.y() + sec * bandH + bandH * 0.28; // below the label
        const qreal slotH = bandH * 0.68;
        const qreal colW = panel.width() / kCols;
        const qreal pitch = slotH / rows;
        const qreal r = qMin(colW, pitch) * 0.46; // = boxSlots' radius

        for (int i = 0; i < mine.size() && i < kCols * rows; ++i) {
            const int col = i % kCols, row = i / kCols;
            const qreal cx = panel.x() + (col + 0.5) * colW;
            const qreal cy = slotTop + row * pitch + pitch / 2;
            const QRectF ring(cx - r, cy - r, r * 2, r * 2);
            const BoxEntry *e = mine[i];
            const QPixmap pm = assets::playerFace(e->positionName, e->race);
            if (!pm.isNull()) {
                const qreal fr = r * 0.78;
                const QRectF faceR(cx - fr, cy - fr, fr * 2, fr * 2);
                p.save();
                QPainterPath clip;
                clip.addEllipse(faceR);
                p.setClipPath(clip);
                p.setRenderHint(QPainter::SmoothPixmapTransform, true);
                p.drawPixmap(faceR, pm, QRectF(0, 0, pm.width(), pm.height()));
                p.restore();
            }
            p.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), 235),
                          qMax(1.0, r * 0.16)));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(ring);
        }
    }

    // Whose dugout: a team-color bar on the panel edge facing the pitch.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 215));
    const qreal barW = qMax(2.0, panel.width() * 0.035);
    const qreal barX = leftSide ? panel.right() - barW : panel.x();
    p.fillRect(QRectF(barX, panel.y() + panel.height() * 0.04,
                      barW, panel.height() * 0.92),
               QColor(accent.red(), accent.green(), accent.blue(), 215));
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

    // Stadium backdrop (round 13, user): the Qwen top-down FANTASY STADIUM
    // (packed medieval stands, inspired by Blood Bowl 3) fills the WHOLE
    // widget — replacing the flat grey apron the user found empty. The field
    // scene is drawn on top (it zooms/pans); the stands show around it and in
    // the apron gaps. Cover-scaled (preserve aspect, center-crop) so the
    // crowd is never distorted. Static backdrop: the field camera zooms over
    // the fixed stadium, like a TV shot.
    const QPixmap stands = assets::stadiumTexture();
    if (!stands.isNull()) {
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const qreal scale = qMax(qreal(width()) / stands.width(),
                                 qreal(height()) / stands.height());
        const QSize sz(int(stands.width() * scale) + 1, int(stands.height() * scale) + 1);
        p.drawPixmap(QRectF(width() / 2.0 - sz.width() / 2,
                            height() / 2.0 - sz.height() / 2,
                            sz.width(), sz.height()),
                     stands, QRectF(0, 0, stands.width(), stands.height()));
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    } else {
        p.fillRect(QRectF(0, 0, width(), height()), QColor(58, 56, 54)); // fallback
    }

    // Field navigation (round 8): the WHOLE scene (dugouts, texture, grid,
    // tokens, rain) lives in a zoomed/panned space. Round 12: the dugout
    // panels are part of the scene too (they zoom with the field).
    p.save();
    p.translate(m_pan);
    p.scale(m_zoom, m_zoom);

    const QRectF s = sceneRect();
    const qreal cellW = f.width() / kFieldWidth;
    const qreal cellH = f.height() / kFieldHeight;

    // --- Dugouts (round 13): team 1 left of the left TD column, team 2
    //     right of the right one. The panel is a SEPARATE soil-themed asset
    //     (identical for every field type), now FULL HEIGHT (the 1:3 art
    //     fits the 5x15 panel exactly). The kSceneGapCells column between
    //     panel and pitch stays dark stadium apron — the visible gap the
    //     user asked for (no more baked frame "under" the field).
    const auto drawDugout = [&](bool leftSide) {
        const QRectF panel = dugoutPanelRect(leftSide);
        const QPixmap dg = assets::dugoutTexture();
        if (!dg.isNull()) {
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawPixmap(panel, dg, QRectF(0, 0, dg.width(), dg.height()));
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        } else {
            p.fillRect(panel, QColor(122, 88, 52)); // fallback soil
        }
    };
    drawDugout(true);
    drawDugout(false);

    // --- Field texture (round 13): the Qwen art is PURE GRASS now (the
    //     baked grid/TD/TOUCHDOWN came out blurred and duplicated at zoom —
    //     user round 12/13). Everything structural is drawn in CODE over it:
    //     mowing stripes, crimson 1-column TD zones + vector TOUCHDOWN,
    //     the exact grid. The texture only supplies the grass detail, which
    //     stays soft even when zoomed (fine detail, tolerable) while lines
    //     and text stay razor-sharp at any zoom/HiDPI (user point 15).
    const QPixmap tex = assets::fieldTexture();
    if (!tex.isNull()) {
        // Cover scaling (preserve aspect, center-crop to the pitch).
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
    // full pitch length (end zone to end zone), plus a slightly stronger
    // sideline at y=0 / y=14 (the pitch edges).
    QPen wzPen(QColor(255, 255, 255, 210), qMax(2.0, cellW * 0.08));
    QPen sidePen(QColor(255, 255, 255, 160), qMax(1.5, cellW * 0.055));
    for (int y = 0; y <= kFieldHeight; ++y) {
        if (y == 4 || y == 11)
            p.setPen(wzPen);
        else if (y == 0 || y == kFieldHeight)
            p.setPen(sidePen);
        else
            p.setPen(linePen);
        p.drawLine(QPointF(f.x(), f.y() + y * cellH), QPointF(f.right(), f.y() + y * cellH));
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
        const QRectF bc = cellRect(m_ballX, m_ballY);
        const QRectF ballRect(bc.center().x() - cellW * 0.22, bc.center().y() - cellH * 0.16,
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
    const auto drawTeam = [this, &p, cellW, cellH](const QVector<PlayerToken> &players,
                                                   bool sideHome) {
        const QColor team = sideHome ? m_homeColor : m_awayColor;
        const QColor teamDark = team.darker(150);
        const qreal s = qMin(cellW, cellH) * 0.92;
        for (const PlayerToken &t : players) {
            if (t.x < 0 || t.x >= kFieldWidth || t.y < 0 || t.y >= kFieldHeight)
                continue; // box coordinate / off field
            const QRectF c = cellRect(t.x, t.y);
            const QPointF ctr = c.center();

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

            // Number badge (bottom-right corner)
            const qreal br = qMin(cellW, cellH) * 0.24;
            const QPointF bc(c.right() - cellW * 0.22, c.bottom() - cellH * 0.2);
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

            // Ball carrier indicator (small football, top-left corner)
            if (t.hasBall) {
                const QRectF br2(c.left() + cellW * 0.06, c.top() + cellH * 0.04,
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
            const QRectF c = cellRect(sel->x, sel->y);
            const QPointF ctr = c.center();
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
