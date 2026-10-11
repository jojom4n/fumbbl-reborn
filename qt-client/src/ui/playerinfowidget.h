// =============================================================================
// PlayerInfoWidget — details of the selected player (placeholder for now)
// =============================================================================
#pragma once

#include <QTextEdit>

class PlayerInfoWidget : public QTextEdit
{
    Q_OBJECT

public:
    explicit PlayerInfoWidget(QWidget *parent = nullptr);

    void showPlaceholder();
    // Simple text-based details view (rich rendering arrives with the game model).
    void showPlayerDetails(const QString &text);
};
