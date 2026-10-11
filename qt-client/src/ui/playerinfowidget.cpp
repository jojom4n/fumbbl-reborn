// =============================================================================
// PlayerInfoWidget — implementation
// =============================================================================
#include "ui/playerinfowidget.h"

#include <QFont>

PlayerInfoWidget::PlayerInfoWidget(QWidget *parent)
    : QTextEdit(parent)
{
    setReadOnly(true);
    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::TypeWriter);
    setFont(mono);
    showPlaceholder();
}

void PlayerInfoWidget::showPlaceholder()
{
    setPlainText(QStringLiteral(
        "Select a player on the field\nor in a roster to see details."));
}

void PlayerInfoWidget::showPlayerDetails(const QString &text)
{
    setPlainText(text);
}
