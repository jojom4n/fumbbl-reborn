#include "ui/panel.h"

#include <QToolButton>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QVBoxLayout>

FloatingPanel::FloatingPanel(const QString &title, QWidget *content, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_content(content)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMinimumSize(kMinWidth, kMinHeight);
    setFocusPolicy(Qt::ClickFocus);

    m_content->setParent(this);

    m_close = new QToolButton(this);
    m_close->setText(QStringLiteral("\u00d7"));
    m_close->setFixedSize(18, 18);
    m_close->setCursor(Qt::PointingHandCursor);
    m_close->setAutoRaise(true);
    m_close->setStyleSheet(
        QStringLiteral("QToolButton { color: rgba(200, 210, 225, 160); background: transparent; "
                       "font-size: 13px; font-weight: bold; } "
                       "QToolButton:hover { color: white; }"));
    connect(m_close, &QToolButton::clicked, this,
            [this] { emit hideRequested(); });
}

void FloatingPanel::setAccent(const QColor &color)
{
    m_accent = color;
    update();
}

int FloatingPanel::snapCoord(int v)
{
    return (v + kGridStep / 2) / kGridStep * kGridStep;
}

int FloatingPanel::snapLen(int v, int minLen)
{
    const int snapped = v / kGridStep * kGridStep;
    if (snapped >= minLen)
        return snapped;
    // Grow to the next grid step that satisfies the minimum.
    return (minLen + kGridStep - 1) / kGridStep * kGridStep;
}

int FloatingPanel::edgeAt(const QPoint &pos) const
{
    const int zone = 6;
    const bool l = pos.x() < zone;
    const bool r = pos.x() > width() - zone;
    const bool b = pos.y() > height() - zone;
    int edge = EdgeNone;
    if (l) edge |= EdgeLeft;
    if (r) edge |= EdgeRight;
    if (b) edge |= EdgeBottom;
    // Top and top corners are the title strip (drag, not resize).
    return edge;
}

Qt::CursorShape FloatingPanel::cursorFor(int edge) const
{
    switch (edge) {
    case EdgeLeft:
        return Qt::SizeHorCursor;
    case EdgeRight:
        return Qt::SizeHorCursor;
    case EdgeBottom:
        return Qt::SizeVerCursor;
    case EdgeLeft | EdgeBottom:
        return Qt::SizeFDiagCursor;
    case EdgeRight | EdgeBottom:
        return Qt::SizeBDiagCursor;
    default:
        return Qt::ArrowCursor;
    }
}

void FloatingPanel::clampToParent()
{
    QWidget *parent = this->parentWidget();
    if (!parent) return;
    const int pw = parent->width();
    const int ph = parent->height();
    int x = geometry().x();
    int y = geometry().y();
    // Keep a 60px slice of the title reachable and stay inside the parent.
    x = qBound(-(geometry().width() - 60), x, pw - 60);
    y = qBound(0, y, ph - kTitleHeight);
    move(x, y);
}

void FloatingPanel::beginMove(const QPoint &globalPos)
{
    m_dragging = true;
    m_grabOffset = mapFromGlobal(globalPos);
    raise();
}

void FloatingPanel::beginResize(int edge, const QPoint &globalPos)
{
    m_resizing = true;
    m_edge = edge;
    m_startGeom = geometry();
    QWidget *parent = this->parentWidget();
    m_pressParentPos = parent ? parent->mapFromGlobal(globalPos) : QPoint();
    raise();
}

void FloatingPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    p.setPen(Qt::NoPen);
    // Blood Bowl 3 style: the panel is part of the scene — dark but clearly
    // translucent, so the field glows through it.
    p.setBrush(QColor(10, 14, 22, 150));
    p.drawRoundedRect(r, 10, 10);

    QPen border(QColor(255, 255, 255, 90));
    border.setWidthF(1.2);
    p.setPen(border);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 10, 10);

    // Accent line along the top edge.
    QPen accent(m_accent);
    accent.setWidthF(2.4);
    p.setPen(accent);
    p.drawLine(QPointF(r.left() + 10, r.top() + 1.2), QPointF(r.right() - 10, r.top() + 1.2));

    // Title text (the "title bar" is part of the translucent chrome).
    p.setPen(QColor(222, 230, 242, 235));
    QFont f = font();
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRectF(10, 0, width() - 36, kTitleHeight),
               Qt::AlignVCenter | Qt::AlignLeft, m_title);
}

void FloatingPanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QPoint pos = event->pos();
    if (pos.y() < kTitleHeight) {
        if (m_close->geometry().contains(pos))
            return; // let the label handle it
        beginMove(event->globalPosition().toPoint());
        return;
    }
    const int edge = edgeAt(pos);
    if (edge != EdgeNone)
        beginResize(edge, event->globalPosition().toPoint());
    raise();
}

void FloatingPanel::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint global = event->globalPosition().toPoint();
    if (m_dragging) {
        QWidget *parent = this->parentWidget();
        if (parent) {
            const QPoint inParent = parent->mapFromGlobal(global);
            // Live snap to the invisible grid (Blood Bowl 3 style): the panel
            // moves in kGridStep increments while being carried.
            move(snapCoord(inParent.x() - m_grabOffset.x()),
                 snapCoord(inParent.y() - m_grabOffset.y()));
            clampToParent();
        }
        return;
    }
    if (m_resizing) {
        QWidget *parent = this->parentWidget();
        if (!parent) return;
        const QPoint inParent = parent->mapFromGlobal(global);
        const int dx = inParent.x() - m_pressParentPos.x();
        const int dy = inParent.y() - m_pressParentPos.y();
        QRect g = m_startGeom;
        if (m_edge & EdgeLeft) {
            const int maxDx = m_startGeom.width() - kMinWidth;
            g.setLeft(qBound(m_startGeom.left(), m_startGeom.left() + dx,
                             m_startGeom.left() + maxDx));
        }
        if (m_edge & EdgeRight)
            g.setRight(qBound(m_startGeom.left() + kMinWidth,
                              m_startGeom.right() + dx, parent->width()));
        if (m_edge & EdgeBottom)
            g.setBottom(qBound(m_startGeom.top() + kMinHeight,
                               m_startGeom.bottom() + dy, parent->height()));
        setGeometry(g);
        return;
    }
    setCursor(cursorFor(edgeAt(event->pos())));
}

void FloatingPanel::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (m_dragging || m_resizing) {
        // On release: position AND size align to the invisible grid.
        QRect g = geometry();
        g.setWidth(snapLen(g.width(), kMinWidth));
        g.setHeight(snapLen(g.height(), kMinHeight));
        g.moveLeft(snapCoord(g.left()));
        g.moveTop(snapCoord(g.top()));
        setGeometry(g);
        clampToParent();
    }
    m_dragging = false;
    m_resizing = false;
    m_edge = EdgeNone;
    setCursor(Qt::ArrowCursor);
}

void FloatingPanel::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    if (m_content)
        m_content->setGeometry(6, kTitleHeight, width() - 12, height() - kTitleHeight - 6);
    if (m_close)
        m_close->move(width() - 26, (kTitleHeight - m_close->height()) / 2);
}
