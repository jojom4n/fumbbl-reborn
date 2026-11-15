#pragma once

#include <QColor>
#include <QWidget>

// Live toast overlay (R0): center ribbons for game milestones (CENTER
// TOUCHDOWN / 2ND HALF / EXTRA TIME / FULL TIME) and edge pills for event
// toasts (+N SPP, COMPLETION, ARMOUR BREAK, TURNOVER, ...). Display-only:
// transparent to the mouse, auto-dismissing with a short fade. The host
// (MainWindow) keeps it at the stage's rect.
class ToastOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit ToastOverlay(QWidget *parent = nullptr);

    // Center ribbon: top-center of the stage, large, visible `visibleMs`.
    void showCenter(const QString &text, int visibleMs = 2200);
    // Edge pill: stacked top-left (below the header row), visible
    // `visibleMs`. `accent` is the pill background.
    void showEdge(const QString &text, const QColor &accent, int visibleMs = 2800);

    // Number of live edge pills (test seam + stacking).
    int edgeCount() const { return m_edgeCount; }

private:
    void makeToast(const QString &text, const QColor &bg, int x, int y,
                   int visibleMs, bool edge);
    int m_edgeCount = 0;
};
