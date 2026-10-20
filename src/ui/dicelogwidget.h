// =============================================================================
// DiceLogWidget — scrolling log of dice rolls / game events
// =============================================================================
#pragma once

#include <QPlainTextEdit>

class DiceLogWidget : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit DiceLogWidget(QWidget *parent = nullptr);

    // Append a colored entry. `color` may be transparent (default text color).
    void appendEntry(const QString &text, const QColor &color = QColor());
    void clearLog();

private:
    int m_entryCount = 0;
};
