#pragma once

#include <QDialog>

class GameModel;

// Report card v1 (R0): the full-time summary. Shown automatically when the
// match ends (turnMode endGame) and closable — the ended match stays live
// behind it. Data: final score, per-team rosters with SPP (base + in-match
// gain) and the final state (injury/box states from the model).
class ReportCardDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReportCardDialog(const GameModel *model, QWidget *parent = nullptr);
};
