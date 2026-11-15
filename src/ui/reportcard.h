#pragma once

#include <QDialog>
#include <QHash>
#include <QVector>

class GameModel;
class QTableWidget;

// Report card v1 (R0): the full-time summary. Shown automatically when the
// match ends (turnMode endGame) and closable — the ended match stays live
// behind it. Data: final score, per-team rosters with SPP (pre-match base
// from fumbbl.com + in-match gain, LIVE — the post-match awards arrive
// after endGame) and the final state (injury/box states from the model).
class ReportCardDialog : public QDialog
{
    Q_OBJECT

public:
    ReportCardDialog(const GameModel *model,
                     const QHash<QString, int> &preMatchSpp,
                     QWidget *parent = nullptr);

    // The SPP column is LIVE: the post-match SPP awards
    // (playerResultSetCurrentSpps in the end-game steps) arrive AFTER
    // endGame — recompute the SPP cells in place instead of staying stale.
    // Called on model updates and when the pre-match fetch settles.
    void refreshSpp(const QHash<QString, int> &preMatchSpp);

private:
    // One SPP row (table + row + the player it belongs to).
    struct Row {
        QTableWidget *table = nullptr;
        int row = 0;
        QString playerId;
    };
    void rebuildSppCells();

    const GameModel *m_model;
    QHash<QString, int> m_preMatchSpp;
    QVector<Row> m_rows;
};
