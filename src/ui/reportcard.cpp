// =============================================================================
// ReportCardDialog — full-time summary (R0 v1)
// =============================================================================
#include "ui/reportcard.h"

#include "core/gamemodel.h"

#include <QColor>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
// One team's roster table: # | PLAYER | POS | SPP | STATE.
QTableWidget *rosterTable(const QVector<FFBPlayer> &roster,
                          const QString &race, const GameModel *model)
{
    auto *table = new QTableWidget(0, 5);
    table->setHorizontalHeaderLabels(
        { QStringLiteral("#"), QStringLiteral("PLAYER"),
          QStringLiteral("POS"), QStringLiteral("SPP"), QStringLiteral("STATE") });
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    // PLAYER absorbs the width; the other columns stay tight (the card is
    // two tables side by side — SPP/STATE must never scroll out of sight).
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

        // SPP: base, in-match gain in red.
        const int base = model->sppBase(p.playerId);
        const int gain = qMax(0, model->sppCurrent(p.playerId) - base);
        auto *sppItem = new QTableWidgetItem(
            gain > 0
                ? QStringLiteral("%1 (+%2)").arg(base).arg(gain)
                : QString::number(base));
        if (gain > 0)
            sppItem->setForeground(QColor(0xc0, 0x39, 0x2b));
        table->setItem(row, 3, sppItem);

        // Final state: the injury/box labels (empty = still on the field).
        const QString st = GameModel::stateLabel(p.state);
        table->setItem(row, 4, new QTableWidgetItem(st));
    }
    return table;
}
} // namespace

ReportCardDialog::ReportCardDialog(const GameModel *model, QWidget *parent)
    : QDialog(parent)
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
        auto *col = new QVBoxLayout;
        auto *head = new QLabel(this);
        head->setAlignment(Qt::AlignCenter);
        head->setStyleSheet(QStringLiteral("font-size:12px; font-weight:700;"));
        head->setText(QStringLiteral("%1 — %2 (%3)").arg(name, coach, race));
        col->addWidget(head);
        col->addWidget(rosterTable(roster, race, model), 1);
        teams->addLayout(col, 1);
    };
    addTeam(home, model->teamHomeCoach(), model->teamHomeRace(),
            model->rosterHome());
    addTeam(away, model->teamAwayCoach(), model->teamAwayRace(),
            model->rosterAway());
    root->addLayout(teams, 1);

    resize(920, 540);
}
