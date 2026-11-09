// =============================================================================
// SpectateDialog — implementation
// =============================================================================
#include "ui/spectatedialog.h"

#include "core/matchlist.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabBar>
#include <QTableWidget>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <QSet>

SpectateDialog::SpectateDialog(QWidget *parent, bool autoLoadList) : QDialog(parent)
{
    setWindowTitle(tr("Spectate a FUMBBL match"));
    resize(860, 560);

    // --- Division tabs (R0): "All" + one tab per division -------------------
    m_divisionTabs = new QTabBar(this);
    m_divisionTabs->addTab(tr("All"));
    m_divisionTabs->setTabData(0, QString());

    // --- Filter bar ---------------------------------------------------------
    m_textFilter = new QLineEdit(this);
    m_textFilter->setPlaceholderText(tr("Filter by team or coach name…"));
    m_textFilter->setClearButtonEnabled(true);
    m_raceFilter = new QComboBox(this);
    m_raceFilter->setMinimumWidth(170);
    m_refreshBtn = new QPushButton(tr("Refresh"), this);

    auto *filterRow = new QHBoxLayout();
    filterRow->addWidget(m_textFilter, 1);
    filterRow->addWidget(m_raceFilter);
    filterRow->addWidget(m_refreshBtn);

    // --- Match table ---------------------------------------------------------
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({ tr("Status"), tr("Game"),
                                         tr("Home (coach — team)"), tr("Score"),
                                         tr("Away (coach — team)") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    m_table->setColumnWidth(0, 110);
    m_table->setColumnWidth(1, 90);
    m_table->setColumnWidth(2, 260);
    m_table->setColumnWidth(3, 70);

    connect(m_table, &QTableWidget::itemSelectionChanged, this,
            [this] { this->spectateBtnState(); });
    connect(m_table, &QTableWidget::cellDoubleClicked, this,
            [this](int, int) { this->spectate(); });

    // --- Manual game id + status + buttons ----------------------------------
    m_manualGameId = new QSpinBox(this);
    m_manualGameId->setRange(0, 2000000000);
    m_manualGameId->setSpecialValueText(tr("(pick a match above)"));
    m_manualGameId->setPrefix(tr("Game ID: "));
    m_manualGameId->setSuffix(tr("   (or enter a Game ID)"));
    m_status = new QLabel(this);
    m_status->setStyleSheet(QStringLiteral("color: #9fb0c4;"));

    m_spectateBtn = new QPushButton(tr("Spectate"), this);
    m_spectateBtn->setDefault(true);
    auto *cancelBtn = new QPushButton(tr("Cancel"), this);

    auto *btnRow = new QHBoxLayout();
    btnRow->addWidget(m_manualGameId, 1);
    btnRow->addStretch(1);
    btnRow->addWidget(m_spectateBtn);
    btnRow->addWidget(cancelBtn);

    auto *root = new QVBoxLayout(this);
    root->addWidget(m_divisionTabs);
    root->addLayout(filterRow);
    root->addWidget(m_table, 1);
    root->addWidget(m_status);
    root->addLayout(btnRow);

    // --- Wiring --------------------------------------------------------------
    connect(m_textFilter, &QLineEdit::textChanged, this, &SpectateDialog::applyFilters);
    connect(m_raceFilter, &QComboBox::currentTextChanged, this, &SpectateDialog::applyFilters);
    connect(m_divisionTabs, &QTabBar::currentChanged, this,
            [this](int) { this->applyFilters(); });
    connect(m_refreshBtn, &QPushButton::clicked, this, &SpectateDialog::refreshList);
    connect(m_spectateBtn, &QPushButton::clicked, this, &SpectateDialog::spectate);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_matchList = new MatchListClient(this);
    connect(m_matchList, &MatchListClient::matchesReceived,
            this, &SpectateDialog::onMatchesReceived);
    connect(m_matchList, &MatchListClient::fetchFailed, this, &SpectateDialog::onFetchFailed);

    const int lastGame = QSettings().value(QStringLiteral("connect/lastGameId")).toInt();
    if (lastGame > 0)
        m_manualGameId->setValue(lastGame);

    connect(m_manualGameId, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int v) {
                // Entering a manual id makes it the active pick: clear any
                // row selection so it can't be silently overridden later.
                if (v > 0)
                    m_table->clearSelection();
                this->spectateBtnState();
            });
    // Selecting a row makes it the active pick: clear the manual id (which
    // may still hold a stale lastGameId) so the clicked match wins.
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this](const QItemSelection &selected, const QItemSelection &)
            {
                if (!selected.isEmpty()) {
                    m_manualGameId->blockSignals(true);
                    m_manualGameId->setValue(0);
                    m_manualGameId->blockSignals(false);
                }
                this->spectateBtnState();
            });

    // The match list is public: always auto-load (tests opt out) and keep
    // it fresh with the R0 fixed 20 s polling (skipped while a fetch is
    // already in flight).
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(20000);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        if (!m_matchList->isBusy())
            refreshList();
    });

    if (autoLoadList) {
        refreshList();
        m_pollTimer->start();
    } else {
        m_status->setText(tr("Ready."));
    }
    spectateBtnState();
}

void SpectateDialog::refreshList()
{
    // Public endpoint: no credentials needed to list matches.
    m_status->setText(tr("Loading live matches…"));
    m_matchList->fetch();
}

void SpectateDialog::onFetchFailed(const QString &message)
{
    m_status->setText(tr("Could not load matches: %1").arg(message.left(200)));
}

void SpectateDialog::onMatchesReceived(QVector<FFBLiveMatch> matches)
{
    m_matches = matches;
    rebuildFilterCombos();
    applyFilters();
    m_status->setText(tr("%1 live matches · updated %2")
                         .arg(m_matches.size())
                         .arg(QTime::currentTime().toString("HH:mm:ss")));
    emit listLoaded();
}

void SpectateDialog::rebuildFilterCombos()
{
    // Division tabs: "All" + the divisions present in the list. The active
    // division survives a polling rebuild (the same division may vanish and
    // reappear between polls).
    const QString divKeep =
        m_divisionTabs->tabData(m_divisionTabs->currentIndex()).toString();
    m_divisionTabs->blockSignals(true);
    while (m_divisionTabs->count() > 0)
        m_divisionTabs->removeTab(0);
    m_divisionTabs->addTab(tr("All"));
    m_divisionTabs->setTabData(0, QString());
    QSet<QString> divs;
    for (const FFBLiveMatch &m : m_matches)
        divs.insert(m.division);
    QStringList dsorted = divs.values();
    std::sort(dsorted.begin(), dsorted.end());
    for (const QString &d : dsorted) {
        m_divisionTabs->addTab(d);
        m_divisionTabs->setTabData(m_divisionTabs->count() - 1, d);
    }
    int keep = 0;
    for (int i = 0; i < m_divisionTabs->count(); ++i)
        if (m_divisionTabs->tabData(i).toString() == divKeep)
            keep = i;
    m_divisionTabs->setCurrentIndex(keep);
    m_divisionTabs->blockSignals(false);

    const QString raceKeep = m_raceFilter->currentText();

    m_raceFilter->blockSignals(true);
    m_raceFilter->clear();
    m_raceFilter->addItem(tr("All races"), QString());
    QSet<QString> races;
    for (const FFBLiveMatch &m : m_matches) {
        races.insert(m.home.race);
        races.insert(m.away.race);
    }
    QStringList sorted = races.values();
    std::sort(sorted.begin(), sorted.end());
    for (const QString &r : sorted)
        m_raceFilter->addItem(r, r);
    const int ri = m_raceFilter->findData(raceKeep);
    m_raceFilter->setCurrentIndex(ri >= 0 ? ri : 0);
    m_raceFilter->blockSignals(false);
}

void SpectateDialog::applyFilters()
{
    const QString text = m_textFilter->text().trimmed().toLower();
    const QString race = m_raceFilter->currentData().toString();
    const QString div =
        m_divisionTabs->tabData(m_divisionTabs->currentIndex()).toString();

    // The user's selection must survive a polling rebuild.
    const int keepId = selectedGameId();

    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);

    int row = 0;
    for (const FFBLiveMatch &m : m_matches) {
        if (!race.isEmpty()
            && m.home.race != race && m.away.race != race)
            continue;
        if (!div.isEmpty() && m.division != div)
            continue;
        if (!text.isEmpty()) {
            const QString haystack =
                (m.home.name + ' ' + m.home.coach + ' ' + m.away.name + ' ' + m.away.coach)
                    .toLower();
            if (!haystack.contains(text))
                continue;
        }

        m_table->insertRow(row);
        auto *idItem = new QTableWidgetItem(QString::number(m.id));
        idItem->setData(Qt::UserRole, m.id);
        m_table->setItem(row, 0, new QTableWidgetItem(m.statusText()));
        m_table->setItem(row, 1, idItem);
        m_table->setItem(row, 2, new QTableWidgetItem(
            QStringLiteral("%1 — %2 (%3)").arg(m.home.coach, m.home.name, m.home.race)));
        m_table->setItem(row, 3, new QTableWidgetItem(
            QStringLiteral("%1 : %2").arg(m.home.score).arg(m.away.score)));
        m_table->setItem(row, 4, new QTableWidgetItem(
            QStringLiteral("%1 — %2 (%3)").arg(m.away.coach, m.away.name, m.away.race)));
        ++row;
    }
    m_table->setSortingEnabled(true);
    m_table->sortByColumn(1, Qt::AscendingOrder);

    // Restore the pre-rebuild selection (polling must not drop the pick).
    if (keepId > 0) {
        for (int r = 0; r < m_table->rowCount(); ++r)
            if (m_table->item(r, 1)
                && m_table->item(r, 1)->data(Qt::UserRole).toInt() == keepId) {
                m_table->setCurrentCell(r, 1);
                break;
            }
    }
}

int SpectateDialog::selectedGameId() const
{
    // The row the cursor is on (a click/double-click sets it) is the
    // authoritative pick; fall back to the first selected row otherwise.
    int r = m_table->currentRow();
    if (r < 0) {
        const auto sel = m_table->selectionModel()->selectedRows();
        if (sel.isEmpty())
            return -1;
        r = sel.first().row();
    }
    QTableWidgetItem *it = m_table->item(r, 1);
    return it ? it->data(Qt::UserRole).toInt() : -1;
}

QPair<QString, QString> SpectateDialog::selectedTeamLogos() const
{
    const int id = selectedGameId();
    for (const FFBLiveMatch &m : m_matches)
        if (m.id == id)
            return { m.home.logo, m.away.logo };
    return {};
}

int SpectateDialog::currentTableRow() const
{
    return m_table ? m_table->currentRow() : -1;
}

int SpectateDialog::tableRowCount() const
{
    return m_table ? m_table->rowCount() : 0;
}

void SpectateDialog::loadMatchesForTest(const QVector<FFBLiveMatch> &matches)
{
    onMatchesReceived(matches);
}

// Selects the row for the given game id (tools/tests; same effect as a
// click). Returns false when no visible row matches.
bool SpectateDialog::selectGame(int gameId)
{
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem *it = m_table->item(r, 1);
        if (it && it->data(Qt::UserRole).toInt() == gameId) {
            m_table->clearSelection();
            m_table->selectRow(r);
            m_table->setCurrentCell(r, 0);
            return true;
        }
    }
    return false;
}

bool SpectateDialog::selectFirstGame()
{
    if (m_table->rowCount() == 0)
        return false;
    // The table is sorted by game id: the first row is the lowest id.
    m_table->clearSelection();
    m_table->selectRow(0);
    m_table->setCurrentCell(0, 0);
    return true;
}

void SpectateDialog::spectateBtnState()
{
    // Enabled as soon as a match is picked (row or manual id). The
    // credentials are the client's login — always present here.
    const bool hasId = selectedGameId() > 0 || m_manualGameId->value() > 0;
    m_spectateBtn->setEnabled(hasId);
}

void SpectateDialog::spectate()
{
    const int gameId = m_manualGameId->value() > 0 ? m_manualGameId->value()
                                                   : selectedGameId();
    if (gameId <= 0) {
        m_status->setText(tr("Pick a match from the list (or enter a Game ID)."));
        return;
    }

    QSettings().setValue(QStringLiteral("connect/lastGameId"), gameId);

    emit connectRequested(gameId);
    accept();
}
