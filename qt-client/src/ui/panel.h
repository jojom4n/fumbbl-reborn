// =============================================================================
// FloatingPanel — a translucent, draggable, resizable panel that floats over
// the central field (single-window "HUD" layout, phase 10).
//
// - WA_TranslucentBackground: the dark background is painted ~90% opaque, so
//   the field glows through slightly (user requirement: fully transparent
//   chrome, including the title bar).
// - Title strip: drag to move (clamped inside the parent), close (×) to hide.
// - 6px edge/cursor zones: resize on every edge and corner (min size 200x120,
//   clamped inside the parent).
// - The content widget is a plain child: it keeps focus, tab order, scrolling.
// =============================================================================
#pragma once

#include <QWidget>

class QToolButton;
class QVBoxLayout;

class FloatingPanel : public QWidget
{
    Q_OBJECT

public:
    FloatingPanel(const QString &title, QWidget *content, QWidget *parent);

    QWidget *contentWidget() const { return m_content; }
    QString panelTitle() const { return m_title; }
    void setAccent(const QColor &color);

    // Minimum allowed size (kept so the panel can't be resized away).
    static constexpr int kMinWidth = 200;
    static constexpr int kMinHeight = 120;
    static constexpr int kTitleHeight = 26;

signals:
    void hideRequested(); // the user clicked the × button

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum Edge {
        EdgeNone = 0x00,
        EdgeLeft = 0x01,
        EdgeRight = 0x02,
        EdgeTop = 0x04,
        EdgeBottom = 0x08,
    };

    int edgeAt(const QPoint &pos) const;
    Qt::CursorShape cursorFor(int edge) const;
    void beginMove(const QPoint &globalPos);
    void beginResize(int edge, const QPoint &globalPos);
    void clampToParent();

    QString m_title;
    QWidget *m_content = nullptr;
    QToolButton *m_close = nullptr;
    QColor m_accent{0x8f, 0xa6, 0xc0};
    bool m_dragging = false;
    bool m_resizing = false;
    int m_edge = EdgeNone;
    QPoint m_grabOffset;
    QRect m_startGeom;
    QPoint m_pressParentPos;
};
