// =============================================================================
// FieldView — implementation
// =============================================================================
#include "ui/fieldview.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

FieldView::FieldView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(390, 225); // 26:15 ratio at 15px cells
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

void FieldView::setPlayers(const QVector<PlayerToken> &home,
                           const QVector<PlayerToken> &away)
{
    m_homePlayers = home;
    m_awayPlayers = away;
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

void FieldView::setSelectedPlayer(const QString &playerId)
{
    if (m_selectedPlayer == playerId)
        return;
    m_selectedPlayer = playerId;
    update();
}

// ---------------------------------------------------------------------------
// Click handling
// ---------------------------------------------------------------------------
const FieldView::PlayerToken *FieldView::tokenAt(const QPoint &pos) const
{
    const QRectF f = fieldRect();
    if (f.isEmpty() || !f.contains(pos))
        return nullptr;
    const qreal cellW = f.width() / kFieldWidth;
    const qreal cellH = f.height() / kFieldHeight;
    const int x = int((pos.x() - f.x()) / cellW);
    const int y = int((pos.y() - f.y()) / cellH);
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
    }
    QWidget::mousePressEvent(event);
}

QRectF FieldView::fieldRect() const
{
    const qreal margin = 10.0;
    const qreal availW = width() - 2 * margin;
    const qreal availH = height() - 2 * margin;
    if (availW <= 0 || availH <= 0)
        return QRectF();

    const qreal cell = qMin(availW / kFieldWidth, availH / kFieldHeight);
    const qreal w = cell * kFieldWidth;
    const qreal h = cell * kFieldHeight;
    return QRectF((width() - w) / 2.0, (height() - h) / 2.0, w, h);
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

void FieldView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF f = fieldRect();
    if (f.isEmpty())
        return;

    const qreal cellW = f.width() / kFieldWidth;
    const qreal cellH = f.height() / kFieldHeight;

    // --- Grass ------------------------------------------------------------
    p.fillRect(f, QColor(0x3d, 0x6b, 0x35));

    // --- End zones (column 0 = home TD, column 25 = away TD) -------------
    p.fillRect(cellRect(0, 0).united(cellRect(0, 14)), QColor(0x2e, 0x52, 0x28));
    p.fillRect(cellRect(25, 0).united(cellRect(25, 14)), QColor(0x54, 0x4a, 0x2e));

    // --- Grid lines --------------------------------------------------------
    QPen linePen(QColor(255, 255, 255, 60));
    linePen.setWidthF(1.0);
    p.setPen(linePen);
    for (int x = 0; x <= kFieldWidth; ++x) {
        p.drawLine(QPointF(f.x() + x * cellW, f.y()), QPointF(f.x() + x * cellW, f.bottom()));
    }
    for (int y = 0; y <= kFieldHeight; ++y) {
        p.drawLine(QPointF(f.x(), f.y() + y * cellH), QPointF(f.right(), f.y() + y * cellH));
    }

    // --- Center line (between columns 12 and 13) --------------------------
    QPen centerPen(QColor(0xd8, 0xc8, 0x6a, 160));
    centerPen.setWidthF(2.0);
    p.setPen(centerPen);
    const qreal cx = f.x() + 13 * cellW;
    p.drawLine(QPointF(cx, f.y()), QPointF(cx, f.bottom()));

    // --- End zone labels ----------------------------------------------------
    p.setPen(QColor(255, 255, 255, 140));
    QFont tdFont = p.font();
    tdFont.setPointSizeF(qMax(6.0, cellW / 2.5));
    tdFont.setBold(true);
    p.setFont(tdFont);
    const QRectF hz = cellRect(0, 7);
    p.drawText(hz, Qt::AlignCenter, QStringLiteral("TD"));
    const QRectF az = cellRect(25, 7);
    p.drawText(az, Qt::AlignCenter, QStringLiteral("TD"));

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

    // --- Player tokens -------------------------------------------------------
    const auto drawTeam = [this, &p, cellW, cellH](const QVector<PlayerToken> &players,
                                                   const QColor &color) {
        for (const PlayerToken &t : players) {
            if (t.x < 0 || t.x >= kFieldWidth || t.y < 0 || t.y >= kFieldHeight)
                continue; // box coordinate / off field
            const QRectF c = cellRect(t.x, t.y);
            const QRectF token(c.center().x() - cellW * 0.36, c.center().y() - cellH * 0.36,
                               cellW * 0.72, cellH * 0.72);

            // Prone / stunned / knocked out: flattened ellipse
            const bool down = (t.state == 3 || t.state == 4 || t.state == 5 || t.state == 11);
            p.setPen(QPen(QColor(0, 0, 0, 180), 1.5));
            p.setBrush(color);
            if (down)
                p.drawEllipse(QRectF(token.center().x() - token.width() * 0.55,
                                     token.center().y() - token.height() * 0.28,
                                     token.width() * 1.1, token.height() * 0.56));
            else
                p.drawEllipse(token);

            // Number
            p.setPen(Qt::white);
            QFont numFont = p.font();
            numFont.setPointSizeF(qMax(6.0, cellH * 0.55));
            numFont.setBold(true);
            p.setFont(numFont);
            p.drawText(token, Qt::AlignCenter,
                       t.number > 0 ? QString::number(t.number) : QStringLiteral("?"));

            // Ball carrier indicator (small football in the corner)
            if (t.hasBall) {
                const QRectF br(c.right() - cellW * 0.42, c.top() + cellH * 0.06,
                                cellW * 0.36, cellH * 0.24);
                p.setPen(QPen(QColor(0x40, 0x26, 0x12), 1));
                p.setBrush(QColor(0xc7, 0x83, 0x3b));
                p.drawEllipse(br);
            }
        }
    };

    drawTeam(m_homePlayers, QColor(0x4a, 0x7c, 0x3f));
    drawTeam(m_awayPlayers, QColor(0xc4, 0xa3, 0x5a));

    // --- Selection ring (drawn last so it is always visible) -----------------
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
            const QRectF ring(c.center().x() - cellW * 0.44, c.center().y() - cellH * 0.44,
                              cellW * 0.88, cellH * 0.88);
            QPen selPen(QColor(0xff, 0xd2, 0x4a), qMax(2.0, cellW * 0.09));
            p.setPen(selPen);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(ring, cellW * 0.12, cellW * 0.12);
        }
    }
}
