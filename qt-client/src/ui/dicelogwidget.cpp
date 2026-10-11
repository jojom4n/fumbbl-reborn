// =============================================================================
// DiceLogWidget — implementation
// =============================================================================
#include "ui/dicelogwidget.h"

#include <QFont>

DiceLogWidget::DiceLogWidget(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(9);
    setFont(mono);
    setMaximumBlockCount(2000); // bound memory usage over long matches
}

void DiceLogWidget::appendEntry(const QString &text, const QColor &color)
{
    ++m_entryCount;
    if (color.isValid())
        appendHtml(QStringLiteral("<span style=\"color:%1;\">%2</span>")
                        .arg(color.name())
                        .arg(text.toHtmlEscaped()));
    else
        appendPlainText(text);
    // Keep the view scrolled to the newest entry.
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End);
    setTextCursor(cursor);
}

void DiceLogWidget::clearLog()
{
    clear();
    m_entryCount = 0;
}
