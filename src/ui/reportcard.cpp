// =============================================================================
// ReportCardDialog — full-time summary (R0 v1)
// =============================================================================
#include "ui/reportcard.h"

#include "core/gamemodel.h"

#include <QBrush>
#include <QColor>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
// The SPP text for one player: the base (PRE-MATCH value from fumbbl.com
// when available, else the stream's first sight) + the in-match gain in
// red ("5 (+2)").
QString sppText(const GameModel *model, const QHash<QString, int> &preMatch,
                const QString &playerId, int *gainOut)
{
    const int pre = preMatch.value(playerId, -1);
    const int base = pre >= 0 ? pre : model->sppBase(playerId);
    const int gain = qMax(0, model->sppCurrent(playerId) - base);
    if (gainOut)
        *gainOut = gain;
    return gain > 0 ? QStringLiteral("%1 (+%2)").arg(base).arg(gain)
                    : QString::number(base);
}
} // namespace

ReportCardDialog::ReportCardDialog(const GameModel *model,
                                   const QHash<QString, int> &preMatchSpp,
                                   QWidget *parent)
    : QDialog(parent), m_model(model), m_preMatchSpp(preMatchSpp)
{
    const QString home = model->teamHomeName();
    const QString away = model->teamAwayName();
    setWindowTitle(tr("Full time — %1 vs %2").arg(home, away));
    setMinimumWidth(900);

    auto *root = new QVBoxLayout(this);

    auto *title = new QLabel(QStringLiteral("FULL TIME"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QStringLiteral("font-size:15px; font-weight:800; color:#687280; "
                       "letter-spacing:4px;"));
    root->addWidget(title);

    // Final score: names + big numbers, coach/race underneath.
    auto *score = new QLabel(this);
    score->setAlignment(Qt::AlignCenter);
    score->setText(QStringLiteral(
                       "<span style=\"font-size:18px;font-weight:800;\">%1</span>"
                       "  <span style=\"font-size:26px;font-weight:900;\">%2 : %3</span>"
                       "  <span style=\"font-size:18px;font-weight:800;\">%4</span><br>"
                       "<span style=\"font-size:11px;color:#687280;\">%5 · %6 "
                       "&nbsp;&nbsp;|&nbsp;&nbsp; %7 · %8</span>")
                       .arg(home)
                       .arg(model->scoreHome())
                       .arg(model->scoreAway())
                       .arg(away)
                       .arg(model->teamHomeCoach())
                       .arg(model->teamHomeRace())
                       .arg(model->teamAwayCoach())
                       .arg(model->teamAwayRace()));
    score->setTextFormat(Qt::RichText);
    root->addWidget(score);

    // Per-team roster tables side by side (head + table share ONE column,
    // so each team gets half the width).
    auto *teams = new QHBoxLayout;
    auto addTeam = [this, model, teams](const QString &name, const QString &coach,
                                        const QString &race,
                                        const QVector<FFBPlayer> &roster) {
        auto *head = new QLabel(this);
        head->setAlignment(Qt::AlignCenter);
        head->setText(QStringLiteral(
                           "<span style=\"font-size:13px;font-weight:800;\">%1</span><br>"
                           "<span style=\"font-size:11px;color:#687280;\">%2 · %3</span>")
                          .arg(name, coach, race));
        head->setTextFormat(Qt::RichText);

        auto *table = new QTableWidget(0, 5, this);
        table->setHorizontalHeaderLabels(
            { QStringLiteral("#"), QStringLiteral("PLAYER"),
              QStringLiteral("POS"), QStringLiteral("SPP"), QStringLiteral("STATE") });
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setAlternatingRowColors(true);
        // PLAYER absorbs the width; the other columns stay tight (the card
        // is two tables side by side — SPP/STATE must never scroll out of
        // sight).
        table->horizontalHeader()->setStretchLastSection(false);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        table->setColumnWidth(0, 34);
        table->setColumnWidth(2, 115);
        table->setColumnWidth(3, 56);
        table->setColumnWidth(4, 60);

        for (const FFBPlayer &p : roster) {
            const int row = table->rowCount();
            table->insertRow(row);
            auto *numItem = new QTableWidgetItem(
                p.number > 0 ? QString::number(p.number) : QString());
            table->setItem(row, 0, numItem);
            table->setItem(row, 1, new QTableWidgetItem(p.name));
            // Position without the race prefix (the table header says the race).
            QString pos = p.position;
            if (!race.isEmpty() && pos.startsWith(race, Qt::CaseInsensitive))
                pos = pos.mid(race.size()).trimmed();
            table->setItem(row, 2, new QTableWidgetItem(pos));

            // SPP: base (pre-match / first sight), in-match gain in red.
            int gain = 0;
            table->setItem(
                row, 3,
                new QTableWidgetItem(sppText(model, m_preMatchSpp, p.playerId, &gain)));
            if (gain > 0)
                table->item(row, 3)->setForeground(QColor(0xc0, 0x39, 0x2b));

            // Final state: the injury/box labels (empty = still on the field).
            const QString st = GameModel::stateLabel(p.state);
            table->setItem(row, 4, new QTableWidgetItem(st));
            // Register the SPP cell for the LIVE recompute (post-match
            // awards arrive after endGame).
            m_rows.append({table, row, p.playerId});
        }
        teams->addWidget(head);
        teams->addWidget(table, 1);
    };
    addTeam(home, model->teamHomeCoach(), model->teamHomeRace(),
            model->rosterHome());
    addTeam(away, model->teamAwayCoach(), model->teamAwayRace(),
            model->rosterAway());
    root->addLayout(teams, 1);

    // LIVE SPP: the post-match awards (playerResultSetCurrentSpps in the
    // end-game steps) emit model updates AFTER endGame — recompute the SPP
    // cells in place instead of the card staying stale.
    connect(model, &GameModel::stateUpdated, this, [this]() {
        rebuildSppCells();
    });

    resize(920, 540);
}

void ReportCardDialog::refreshSpp(const QHash<QString, int> &preMatchSpp)
{
    m_preMatchSpp = preMatchSpp;
    rebuildSppCells();
}

void ReportCardDialog::rebuildSppCells()
{
    for (const Row &r : m_rows) {
        if (!r.table || !r.table->item(r.row, 3))
            continue;
        int gain = 0;
        r.table->item(r.row, 3)
            ->setText(sppText(m_model, m_preMatchSpp, r.playerId, &gain));
        if (gain > 0)
            r.table->item(r.row, 3)->setForeground(QColor(0xc0, 0x39, 0x2b));
        else
            r.table->item(r.row, 3)->setForeground(QBrush()); // palette default
    }
}
