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
namespace {
const QVector<QPointF> kPuddleCenters = {
    QPointF(0.155, 0.27), QPointF(0.335, 0.64), QPointF(0.465, 0.21),
    QPointF(0.575, 0.79), QPointF(0.665, 0.36), QPointF(0.815, 0.68),
    QPointF(0.895, 0.22), QPointF(0.275, 0.86),
};
const QVector<qreal> kPuddleWidths = {
    0.055, 0.07, 0.05, 0.08, 0.06, 0.05, 0.045, 0.065,
}; // fraction of the field width (full ellipse width)
} // namespace

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

    // Puddles: fixed layout in field space.
    m_puddles.clear();
    for (int i = 0; i < kPuddleCenters.size(); ++i) {
        const QPointF c = kPuddleCenters[i];
        const float rx = float(kPuddleWidths[i]) * float(f.width()) * 0.5f;
        const float ry = rx * 0.55f;
        m_puddles.append(QRectF(float(c.x()) * float(f.width()) - rx,
                                float(c.y()) * float(f.height()) - ry,
                                rx * 2.0f, ry * 2.0f));
    }

    // "Light Rain" (and friends) is sparser than a downpour.
    const int intensity =
        m_weather.toLower().contains(QLatin1String("light")) ? 45 : 85;
    m_rain.configure(float(f.width()), float(f.height()), m_puddles);
    m_rain.setIntensity(intensity);

    if (!m_rainTimer) {
        m_rainTimer = new QTimer(this);
        m_rainTimer->setInterval(33); // ~30 fps
        connect(m_rainTimer, &QTimer::timeout, this, [this]() {
            const float dt = float(m_rainClock.isValid() ? m_rainClock.restart()
                                                         : 33.0);
            m_rain.step(qMin(dt, 100.0f));
            update();
        });
    }
    m_rainClock.start();
    m_rainTimer->start();
}

void FieldView::drawPuddles(QPainter &p, const QRectF &f) const
{
    // The puddle rects are in PITCH space (0..pitch); the painter is in
    // scene space, so translate by the pitch origin.
    p.save();
    p.translate(f.x(), f.y());
    for (const QRectF &pd : m_puddles) {
        // Glossy water on the grass.
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(30, 58, 84, 95));
        p.drawEllipse(pd);
        // Darker rim.
        p.setPen(QPen(QColor(18, 38, 58, 110), qMax(1.0, pd.height() * 0.06)));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(pd);
        // Top-left glint.
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(150, 190, 215, 45));
        p.drawEllipse(pd.adjusted(pd.width() * 0.18, pd.height() * 0.22,
                                  -pd.width() * 0.30, -pd.height() * 0.52));
    }
    p.restore();
}

void FieldView::drawRain(QPainter &p)
{
    // The sim is in PITCH space (0..pitch); translate into scene space.
    const QRectF f = fieldRect();
    p.save();
    p.translate(f.x(), f.y());
    // Droplet streaks (slanted short lines). Bright + slightly thick so the
    // rain reads clearly over the grass at any zoom.
    p.setPen(QPen(QColor(224, 236, 250, 145), 1.5, Qt::SolidLine, Qt::RoundCap));
    for (const RainSim::Drop &d : m_rain.drops())
        p.drawLine(QPointF(d.x - d.slant, d.y - d.len), QPointF(d.x, d.y));

    // Splash ripples (flattened ellipses: water seen top-down).
    p.setBrush(Qt::NoBrush);
    for (const RainSim::Splash &s : m_rain.splashes()) {
        const float maxAge = s.onPuddle ? 650.0f : 380.0f;
        const float t = qMin(1.0f, s.ageMs / maxAge);
        const float r = (s.onPuddle ? 3.0f : 2.0f) +
                        t * (s.onPuddle ? 9.0f : 4.5f);
        const int a = int((1.0 - t) * (s.onPuddle ? 225 : 160));
        p.setPen(QPen(QColor(228, 240, 252, a), 1.4));
        p.drawEllipse(QPointF(s.x, s.y), r, r * 0.62);
        if (s.onPuddle) {
            p.setPen(QPen(QColor(228, 240, 252, a / 2), 1.1));
            p.drawEllipse(QPointF(s.x, s.y), r * 0.55, r * 0.55 * 0.62);
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

    // Vector TOUCHDOWN, rotated 90 deg (reads top-to-bottom) and centered in
    // the column. Font: the app font at Black weight (crisp at any zoom).
    QFont tf = p.font();
    tf.setWeight(QFont::Black);
    tf.setPointSizeF(14.0);
    QFontMetricsF fm(tf);
    const QString text = QStringLiteral("TOUCHDOWN");
    const qreal maxLen = 0.94 * f.height();
    const qreal maxCap = 0.62 * cellW; // cap height across the column width
    while ((fm.horizontalAdvance(text) > maxLen || fm.capHeight() > maxCap) &&
           tf.pointSizeF() > 2.0) {
        tf.setPointSizeF(tf.pointSizeF() * 0.97);
        fm = QFontMetricsF(tf);
    }
    p.setFont(tf);
    p.setPen(QColor(255, 255, 255, 235));
    const qreal adv = fm.horizontalAdvance(text);
    for (const qreal colCenter : { f.x() + cellW * 0.5,
                                   f.right() - cellW * 0.5 }) {
        p.save();
        p.translate(colCenter, f.center().y());
        p.rotate(90); // local +x now points DOWN: the text reads top->bottom
        p.drawText(QRectF(-adv / 2, -fm.ascent() / 2, adv, fm.ascent()),
                   Qt::AlignHCenter, text);
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
    // WASD pan (round 13). Only meaningful while zoomed (at zoom 1 the
    // scene is letterboxed and centered: panning would clamp right back).
    if (m_zoom > 1.0001) {
        const qreal step = 24.0; // widget px per key press (auto-repeat)
        QPointF d;
        switch (event->key()) {
        case Qt::Key_W: d = QPointF(0.0, step); break;   // look up
        case Qt::Key_S: d = QPointF(0.0, -step); break;  // look down
        case Qt::Key_A: d = QPointF(step, 0.0); break;   // look left
        case Qt::Key_D: d = QPointF(-step, 0.0); break;  // look right
        default:
            break;
        }
        if (!d.isNull()) {
            m_pan += d;
            clampPan();
            update();
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
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
        // Dark under-stroke + white main stroke (rounded joins: the path
        // turns along the grid).
        p.setPen(QPen(QColor(24, 24, 30, 200), cellW * 0.16,
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(pts.constData(), pts.size());
        p.setPen(QPen(QColor(255, 255, 255, 235), cellW * 0.075,
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(pts.constData(), pts.size());
        // Arrow at the end, pointing along the last stepped segment.
        const QPointF a = pts[pts.size() - 2];
        const QPointF b = pts.last();
        const QPointF dir(b.x() - a.x(), b.y() - a.y());
        const qreal len = std::hypot(dir.x(), dir.y());
        if (len < 1e-6)
            continue;
        const QPointF u(dir.x() / len, dir.y() / len);
        const qreal s = cellW * 0.24; // arrow size
        const QPointF base = b - u * s;
        const QPointF perp(-u.y(), u.x());
        QPolygonF tri;
        tri << b << (base + perp * s * 0.62) << (base - perp * s * 0.62);
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
        // kept — round 13 only retracted the DRAG-PAN, not the click). An
        // EMPTY id (clicked an empty square) is emitted too: the caller uses
        // it to HIDE the card. Emitted on the press, as before.
        const PlayerToken *t = tokenAt(event->pos());
        emit playerClicked(t ? t->playerId : QString());
        setFocus(Qt::MouseFocusReason); // WASD pan right after a click
    } else if (event->button() == Qt::RightButton) {
        // Round 13: right button no longer pans. A right CLICK resets the
        // view (or, in play mode on a player: the action wheel — remember
        // who was under the button; the release decides).
        const PlayerToken *t = tokenAt(event->pos());
        m_rightPressPlayerId = t ? t->playerId : QString();
    }
    QWidget::mousePressEvent(event);
}

void FieldView::mouseMoveEvent(QMouseEvent *event)
{
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
        // release does nothing (the round-13 drag-pan was retracted).
    } else if (event->button() == Qt::RightButton) {
        const PlayerToken *tRel = tokenAt(event->pos());
        if (m_playMode && !m_rightPressPlayerId.isEmpty()
            && tRel && tRel->playerId == m_rightPressPlayerId) {
            // Play mode: right-click ON A PLAYER = action wheel target
            // (the game logic shows the wheel menu from the signal).
            setActionWheelTarget(m_rightPressPlayerId);
            emit actionWheelRequested(m_rightPressPlayerId);
        } else {
            resetView(); // plain right click = reset zoom/pan
        }
        m_rightPressPlayerId.clear();
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
    // Zoom anchored on the cursor (round 8).
    const int dy = event->angleDelta().y();
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
    setFocus(Qt::OtherFocusReason); // round 13: WASD right after zooming
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
    constexpr int kCols = 2;
    const int kRows[] = { 2, 4, 4 };
    const qreal bandH = panel.height() / 3.0;
    for (int sec = 0; sec < 3; ++sec) {
        QVector<const BoxEntry *> mine;
        for (const BoxEntry &e : entries)
            if (dugoutInSection(e.state, kDugoutSections[sec].codes))
                mine.append(&e);
        const int rows = kRows[sec];
        const qreal slotTop = panel.y() + sec * bandH + bandH * 0.30; // below the label
        const qreal slotH = bandH * 0.66;
        const qreal pitch = slotH / rows;
        const qreal r = pitch * 0.46;
        for (int i = 0; i < mine.size() && i < kCols * rows; ++i) {
            const int col = i % kCols, row = i / kCols;
            const qreal cx = panel.center().x() + (col == 0 ? -1 : 1) * panel.width() * 0.21;
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
    constexpr int kCols = 2;
    const int kRows[] = { 2, 4, 4 };
    const QColor EMPTY(160, 166, 178);
    const qreal bandH = panel.height() / 3.0;

    for (int sec = 0; sec < 3; ++sec) {
        QVector<const BoxEntry *> mine;
        for (const BoxEntry &e : entries)
            if (dugoutInSection(e.state, kDugoutSections[sec].codes))
                mine.append(&e);

        const int rows = kRows[sec];
        const qreal slotTop = panel.y() + sec * bandH + bandH * 0.30; // below the label
        const qreal slotH = bandH * 0.66;
        const qreal pitch = slotH / rows;
        const qreal r = pitch * 0.46; // = boxSlots' radius (faces/hit-test)

        for (int i = 0; i < kCols * rows; ++i) {
            const int col = i % kCols, row = i / kCols;
            const qreal cx = panel.center().x() + (col == 0 ? -1 : 1) * panel.width() * 0.21;
            const qreal cy = slotTop + row * pitch + pitch / 2;
            const QRectF ring(cx - r, cy - r, r * 2, r * 2);
            if (i < mine.size()) {
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
            } else {
                p.setPen(QPen(QColor(EMPTY.red(), EMPTY.green(), EMPTY.blue(), 150),
                              qMax(1.0, r * 0.14)));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(ring);
            }
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

    // Stadium apron (round 12): the 36x15 scene keeps SQUARE cells and is
    // letterboxed inside the widget — everything outside it is dark stadium.
    p.fillRect(QRectF(0, 0, width(), height()), QColor(58, 56, 54));

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
    const auto drawLogo = [&](const QPixmap &pm, qreal col) {
        if (pm.isNull())
            return;
        const qreal s = f.height() * 0.52;
        p.save();
        p.setOpacity(0.25);
        // Round 11: smooth scaling — the 96/240px crests were scaled to
        // ~430px with fast scaling and came out grainy (user report).
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawPixmap(QRectF(f.x() + col * cellW - s / 2, f.center().y() - s / 2, s, s), pm,
                     QRectF(0, 0, pm.width(), pm.height()));
        p.restore();
    };
    drawLogo(m_homeLogo, 6.5);
    drawLogo(m_awayLogo, 19.5);

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
