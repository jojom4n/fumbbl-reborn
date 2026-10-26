// =============================================================================
// FieldView — implementation
// =============================================================================
#include "ui/fieldview.h"

#include "core/assets.h"

#include <QEvent>
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
    update();
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
        const PlayerToken *t = tokenAt(event->pos());
        emit playerClicked(t ? t->playerId : QString());
    } else if (event->button() == Qt::RightButton) {
        // Right button: potential pan (round 8). A click without movement
        // resets the view; a drag pans (the move handler clamps). Round 9:
        // in PLAY mode a right-click that lands ON A PLAYER opens the
        // action wheel instead (remember who was under the button; the
        // release handler decides click vs drag).
        m_panning = true;
        m_panStart = event->pos();
        m_panStartOffset = m_pan;
        const PlayerToken *t = tokenAt(event->pos());
        m_rightPressPlayerId = t ? t->playerId : QString();
    }
    QWidget::mousePressEvent(event);
}

void FieldView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        if (m_zoom > 1.0001) {
            m_pan = m_panStartOffset + QPointF(event->pos() - m_panStart);
            clampPan();
            update();
        }
        return;
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
    if (event->button() == Qt::RightButton && m_panning) {
        m_panning = false;
        const bool wasClick = QLineF(m_panStart, event->pos()).length() < 4.0;
        if (wasClick) {
            if (m_playMode && !m_rightPressPlayerId.isEmpty()) {
                // Play mode: right-click ON A PLAYER = action wheel target
                // (the game logic shows the wheel menu from the signal).
                setActionWheelTarget(m_rightPressPlayerId);
                emit actionWheelRequested(m_rightPressPlayerId);
            } else {
                resetView(); // plain right click = reset zoom/pan
            }
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
    event->accept();
}

void FieldView::resetView()
{
    m_zoom = 1.0;
    m_pan = QPointF();
    update();
}

QPointF FieldView::toFieldSpace(const QPointF &pos) const
{
    return (pos - m_pan) / m_zoom;
}

void FieldView::clampPan()
{
    // Keep the (zoomed) field covering the widget.
    const qreal W = width(), H = height();
    m_pan.setX(qBound(W - W * m_zoom, m_pan.x(), 0.0));
    m_pan.setY(qBound(H - H * m_zoom, m_pan.y(), 0.0));
}

QRectF FieldView::fieldRect() const
{
    // The field stretches over the WHOLE widget (fullscreen HUD: no margins,
    // no letterboxing stripes around it). Cells follow the window aspect.
    if (width() <= 0 || height() <= 0)
        return QRectF();
    return QRectF(0, 0, width(), height());
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

// Layout of the filled slots of one strip: shared by the painter and the
// hover hit-test, so the two never drift apart. Scaled against the field
// height (the mockup was authored at a 739px field).
QVector<QPair<QRectF, const FieldView::BoxEntry*>> FieldView::boxSlots(
    const QRectF &f, const QVector<BoxEntry> &entries, bool leftSide) const
{
    Q_UNUSED(leftSide)
    QVector<QPair<QRectF, const BoxEntry*>> filled;
    if (f.isEmpty())
        return filled;
    const qreal s = f.height() / 739.0;
    const qreal stripW = 46 * s;
    const qreal x = leftSide ? f.x() + 8 * s : f.right() - 8 * s - stripW;
    const qreal y0 = f.y() + 120 * s;

    const auto inSection = [](int st, const QVector<int> &codes) {
        st &= 0xFF;
        return codes.contains(st);
    };
    const struct { const char *label; QVector<int> codes; } sections[] = {
        { "RESERVE", { 9 } },
        { "KO", { 5 } },
        { "INJURED", { 6, 7, 8, 10 } },
    };

    qreal y = y0 + 20 * s;
    for (const auto &sec : sections) {
        y += 13 * s; // section label row
        const QVector<int> &codes = sec.codes;
        QVector<const BoxEntry *> mine;
        for (const BoxEntry &e : entries)
            if (inSection(e.state, codes))
                mine.append(&e);
        const int n = qMax(2, mine.size());
        const qreal ringR = 12 * s;
        const qreal cx = x + 23 * s;
        for (int i = 0; i < n; ++i) {
            if (i < mine.size()) {
                const qreal sy = y + i * 24 * s + 12 * s;
                filled.append({ QRectF(cx - ringR, sy - ringR, ringR * 2, ringR * 2),
                               mine[i] });
            }
        }
        y += n * 24 * s + 6 * s;
    }
    return filled;
}

// The box-strip entry under a WIDGET point (screen space: the strips are HUD
// and stay put while the field zooms/pans).
const FieldView::BoxEntry *FieldView::boxEntryAt(const QPoint &pos, bool &sideHome) const
{
    const QRectF f(0, 0, width(), height());
    sideHome = true;
    const auto hit = [this, f, pos](const QVector<BoxEntry> &entries, bool left) -> const BoxEntry * {
        for (const auto &sl : boxSlots(f, entries, left))
            if (sl.first.contains(QPointF(pos)))
                return sl.second;
        return nullptr;
    };
    const BoxEntry *e = hit(m_homeBox, true);
    if (e)
        return e;
    sideHome = false;
    return hit(m_awayBox, false);
}

// Bench box strip (Mockup F): a thin translucent vertical card on the field
// edge with RESERVE / KO / INJURED sections. Filled slots show the player's
// face in a team-colored ring; empty slots are plain gray rings. Scaled
// against the field height (the mockup was authored at a 739px field).
void FieldView::paintBoxStrip(QPainter &p, const QRectF &f, const QColor &accent,
                              const QVector<BoxEntry> &entries, bool leftSide) const
{
    if (f.isEmpty())
        return;
    const qreal s = f.height() / 739.0;
    const qreal stripW = 46 * s;
    const qreal x = leftSide ? f.x() + 8 * s : f.right() - 8 * s - stripW;
    const qreal y0 = f.y() + 120 * s;
    const qreal y1 = f.y() + 470 * s;

    // Card background (translucent paper) + team-color top bar.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(250, 250, 253, 205));
    p.drawRoundedRect(QRectF(x, y0, stripW, y1 - y0), 12 * s, 12 * s);
    p.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 220));
    p.drawRoundedRect(QRectF(x + 4 * s, y0 + 4 * s, stripW - 8 * s, 6 * s), 3 * s, 3 * s);

    // Group entries by section (FFB PlayerState bases).
    const auto inSection = [](int st, const QVector<int> &codes) {
        st &= 0xFF;
        return codes.contains(st);
    };

    const QColor INK2(104, 114, 128);
    const QColor EMPTY(160, 166, 178);
    const struct { const char *label; QVector<int> codes; } sections[] = {
        { "RESERVE", { 9 } },
        { "KO", { 5 } },
        { "INJURED", { 6, 7, 8, 10 } },
    };

    qreal y = y0 + 20 * s;
    QFont base = font();
    for (const auto &sec : sections) {
        // Section label.
        QFont lf = base;
        lf.setPixelSize(qMax(6, int(7 * s))); // mockup: 7px at 739px field
        lf.setBold(true);
        p.setFont(lf);
        p.setPen(QColor(INK2.red(), INK2.green(), INK2.blue(), 220));
        p.drawText(QRectF(x + 8 * s, y, stripW - 12 * s, 13 * s),
                   Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8(sec.label));
        y += 13 * s;

        const QVector<int> &codes = sec.codes;
        QVector<const BoxEntry *> mine;
        for (const BoxEntry &e : entries)
            if (inSection(e.state, codes))
                mine.append(&e);
        const int n = qMax(2, mine.size());
        const qreal ringR = 12 * s; // ring radius
        const qreal cx = x + 23 * s;
        for (int i = 0; i < n; ++i) {
            const qreal sy = y + i * 24 * s + 12 * s;
            QRectF ring(cx - ringR, sy - ringR, ringR * 2, ringR * 2);
            if (i < mine.size()) {
                const BoxEntry *e = mine[i];
                const QPixmap pm = assets::playerFace(e->positionName, e->race);
                if (!pm.isNull()) {
                    const qreal fr = 10 * s; // face radius
                    QRectF faceR(cx - fr, sy - fr, fr * 2, fr * 2);
                    p.save();
                    QPainterPath clip;
                    clip.addEllipse(faceR);
                    p.setClipPath(clip);
                    p.drawPixmap(faceR, pm, QRectF(0, 0, pm.width(), pm.height()));
                    p.restore();
                }
                p.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), 230),
                              qMax(1.0, 2 * s)));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(ring);
            } else {
                p.setPen(QPen(QColor(EMPTY.red(), EMPTY.green(), EMPTY.blue(), 160),
                              qMax(1.0, 2 * s)));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(ring);
            }
        }
        y += n * 24 * s + 6 * s;
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

    // Field navigation (round 8): the WHOLE field (texture, grid, tokens,
    // rain) lives in a zoomed/panned space; the bench strips are HUD and are
    // drawn afterwards in screen space.
    p.save();
    p.translate(m_pan);
    p.scale(m_zoom, m_zoom);

    const qreal cellW = f.width() / kFieldWidth;
    const qreal cellH = f.height() / kFieldHeight;

    // --- Field texture (baked Mockup-F art: grass, mowing stripes, crimson
    //     1-column TD zones with vertical TOUCHDOWN) or flat fallback ------
    const QPixmap tex = assets::fieldTexture();
    if (!tex.isNull()) {
        // Cover scaling (preserve aspect, center-crop to the widget).
        const qreal scale = qMax(f.width() / tex.width(), f.height() / tex.height());
        const QSize sz(int(tex.width() * scale) + 1, int(tex.height() * scale) + 1);
        p.drawPixmap(QRectF(f.center().x() - sz.width() / 2, f.center().y() - sz.height() / 2,
                            sz.width(), sz.height()),
                     tex, QRectF(0, 0, tex.width(), tex.height()));
    } else {
        p.fillRect(f, QColor(0x3d, 0x6b, 0x35));
        p.fillRect(cellRect(0, 0).united(cellRect(0, 14)), QColor(0x80, 0x20, 0x1e));
        p.fillRect(cellRect(25, 0).united(cellRect(25, 14)), QColor(0x80, 0x20, 0x1e));
    }

    // --- Exact 26x15 grid over the WHOLE field: the TD columns show their
    //     15 cells (horizontal lines run full width), the side-zone rows
    //     (row 0 / row 14) get emphasized boundary lines. ------------------
    QPen linePen(QColor(255, 255, 255, 110));
    linePen.setWidthF(qMax(1.0, cellW * 0.04));
    p.setPen(linePen);
    for (int x = 0; x <= kFieldWidth; ++x)
        p.drawLine(QPointF(f.x() + x * cellW, f.y()), QPointF(f.x() + x * cellW, f.bottom()));
    for (int y = 0; y <= kFieldHeight; ++y) {
        if (y == 1 || y == kFieldHeight - 1) {  // side-zone boundaries
            QPen sz(QColor(255, 255, 255, 150), qMax(1.5, cellW * 0.06));
            p.setPen(sz);
        }
        p.drawLine(QPointF(f.x(), f.y() + y * cellH), QPointF(f.right(), f.y() + y * cellH));
        p.setPen(linePen);
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
        p.drawPixmap(QRectF(f.x() + col * cellW - s / 2, f.center().y() - s / 2, s, s), pm,
                     QRectF(0, 0, pm.width(), pm.height()));
        p.restore();
    };
    drawLogo(m_homeLogo, 6.5);
    drawLogo(m_awayLogo, 19.5);

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
            const bool down = (t.state == 3 || t.state == 4 || t.state == 5 || t.state == 11);
            const QPointF ctr = c.center();

            // Team-colored circle behind the face.
            QRectF circ(ctr.x() - s * 0.5, ctr.y() - s * 0.5, s, s);
            if (down) {
                circ = QRectF(ctr.x() - s * 0.55, ctr.y() - s * 0.28, s * 1.1, s * 0.56);
            }
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
                QRectF face(ctr.x() - fs / 2, ctr.y() - fs / 2, fs, fs);
                if (down)
                    face = QRectF(ctr.x() - fs * 0.52, ctr.y() - fs * 0.24, fs * 1.04, fs * 0.48);
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
                const qreal mh = down ? s * 0.30 : s * 0.46; // fit the squashed face
                const QRectF mr(ctr.x() - mw / 2, ctr.y() - mh / 2, mw, mh);
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
            const qreal ringR = minDim * (0.56 + 0.02 * breath);
            const qreal glowR = minDim * (0.85 + 0.06 * breath);
            // Rim-only radial: clear over the face (up to the team circle's
            // edge), peak band at the ring, fade to transparent.
            QRadialGradient halo(ctr, glowR);
            const int rimA = int(70 + 70 * breath); // 70..140, calmer than round 9
            halo.setColorAt(0.0, QColor(88, 168, 255, 0));
            halo.setColorAt((minDim * 0.5) / glowR, QColor(88, 168, 255, 0));
            halo.setColorAt(ringR / glowR, QColor(88, 168, 255, rimA));
            halo.setColorAt(1.0, QColor(88, 168, 255, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(halo);
            p.drawEllipse(ctr, glowR, glowR);
            // Crisp blue ring around the token.
            QPen ringPen(QColor(120, 190, 255, 230), qMax(2.0, minDim * 0.055));
            p.setPen(ringPen);
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(ctr, ringR, ringR);
        }
    }

    p.restore(); // end of the zoomed field space

    // --- Bench box strips (left = home, right = away; Mockup F). HUD: drawn
    //     in screen space so they stay visible while the field zooms/pans. --
    paintBoxStrip(p, f, m_homeColor, m_homeBox, true);
    paintBoxStrip(p, f, m_awayColor, m_awayBox, false);

    // --- Rain (MicroProse-Soccer style: fine top-down droplet specks) ------
    if (m_weather.toLower().contains(QLatin1String("rain"))) {
        QRandomGenerator rng(7);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 520; ++i) {
            const qreal x0 = rng.bounded(int(f.width()));
            const qreal y0 = rng.bounded(int(f.height()));
            const int a = 35 + rng.bounded(81);
            static constexpr qreal kRadii[] = { 0.9, 0.9, 1.1, 1.5 };
            const qreal r = kRadii[rng.bounded(4)];
            p.setBrush(QColor(235, 240, 250, a));
            p.drawEllipse(QPointF(x0, y0), r, r);
        }
        for (int i = 0; i < 90; ++i) {
            const qreal x0 = rng.bounded(int(f.width()));
            const qreal y0 = rng.bounded(int(f.height()));
            const qreal L = 2.5 + rng.bounded(40) / 10.0;
            const int a = 28 + rng.bounded(45);
            p.setPen(QPen(QColor(235, 240, 250, a), 1));
            p.drawLine(QPointF(x0, y0), QPointF(x0, y0 + L));
        }
    }

}
