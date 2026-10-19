// =============================================================================
// SpectateDialog — implementation
// =============================================================================
#include "ui/spectatedialog.h"

#include "core/fumbblauth.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
// Same file the TS client (Tauri Store) and spectator_smoke read/write.
QString credentialsPath()
{
    return QDir::homePath()
        + QStringLiteral("/.local/share/com.saruman.fumbbl-reborn/fumbbl-credentials.json");
}

QJsonObject loadCredentialsObject()
{
    QFile f(credentialsPath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    QJsonObject c = root.value(QStringLiteral("fumbbl_credentials")).toObject();
    if (c.isEmpty())
        c = root.value(QStringLiteral("credentials")).toObject();
    return c;
}
} // namespace

SpectateDialog::SpectateDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Spectate a FUMBBL match"));
    resize(860, 560);

    // --- Filter bar ---------------------------------------------------------
    m_textFilter = new QLineEdit(this);
    m_textFilter->setPlaceholderText(tr("Filter by team or coach name…"));
    m_textFilter->setClearButtonEnabled(true);
    m_raceFilter = new QComboBox(this);
    m_raceFilter->setMinimumWidth(170);
    m_divisionFilter = new QComboBox(this);
    m_divisionFilter->setMinimumWidth(130);
    m_refreshBtn = new QPushButton(tr("Refresh"), this);

    auto *filterRow = new QHBoxLayout();
    filterRow->addWidget(m_textFilter, 1);
    filterRow->addWidget(m_raceFilter);
    filterRow->addWidget(m_divisionFilter);
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

    // --- Credentials (collapsible) ------------------------------------------
    // A plain QGroupBox toggled by a checkable button (a checkable group box
    // only DISABLES its children on some styles, it does not collapse them).
    m_credToggle = new QPushButton(tr("OAuth credentials  ▸"), this);
    m_credToggle->setCheckable(true);
    m_credToggle->setFlat(true);
    m_credToggle->setStyleSheet(QStringLiteral("text-align: left; padding: 2px 4px;"));
    m_credGroup = new QGroupBox(tr("OAuth credentials"), this);
    m_clientId = new QLineEdit(m_credGroup);
    m_clientSecret = new QLineEdit(m_credGroup);
    m_clientSecret->setEchoMode(QLineEdit::Password);
    m_username = new QLineEdit(m_credGroup);
    m_remember = new QCheckBox(tr("Remember credentials"), m_credGroup);
    m_remember->setChecked(true);
    m_loadBtn = new QPushButton(tr("Load matches"), m_credGroup);
    auto *credForm = new QHBoxLayout(m_credGroup);
    credForm->addWidget(new QLabel(tr("Client ID:")), 0);
    credForm->addWidget(m_clientId, 1);
    credForm->addWidget(new QLabel(tr("Secret:")), 0);
    credForm->addWidget(m_clientSecret, 1);
    credForm->addWidget(new QLabel(tr("Username:")), 0);
    credForm->addWidget(m_username, 1);
    auto *credRow2 = new QHBoxLayout();
    credRow2->addWidget(m_remember);
    credRow2->addStretch(1);
    credRow2->addWidget(m_loadBtn);
    auto *credLayout = new QVBoxLayout(m_credGroup);
    credLayout->addLayout(credForm);
    credLayout->addLayout(credRow2);

    connect(m_credToggle, &QPushButton::toggled, m_credGroup, &QWidget::setVisible);
    connect(m_credToggle, &QPushButton::toggled, this, [this](bool on) {
        m_credToggle->setText(on ? tr("OAuth credentials  ▾")
                                 : tr("OAuth credentials  ▸"));
    });

    // --- Manual game id + status + buttons ----------------------------------
    m_manualGameId = new QSpinBox(this);
    m_manualGameId->setRange(0, 2000000000);
    m_manualGameId->setSpecialValueText(tr("(pick a match above)"));
    m_manualGameId->setPrefix(tr("Game ID: "));
    m_manualGameId->setSuffix(tr("   (overrides selection)"));
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
    root->addLayout(filterRow);
    root->addWidget(m_table, 1);
    root->addWidget(m_credToggle);
    root->addWidget(m_credGroup);
    root->addWidget(m_status);
    root->addLayout(btnRow);

    // --- Wiring --------------------------------------------------------------
    connect(m_textFilter, &QLineEdit::textChanged, this, &SpectateDialog::applyFilters);
    connect(m_raceFilter, &QComboBox::currentTextChanged, this, &SpectateDialog::applyFilters);
    connect(m_divisionFilter, &QComboBox::currentTextChanged, this, &SpectateDialog::applyFilters);
    connect(m_refreshBtn, &QPushButton::clicked, this, &SpectateDialog::refreshList);
    connect(m_loadBtn, &QPushButton::clicked, this, &SpectateDialog::refreshList);
    connect(m_spectateBtn, &QPushButton::clicked, this, &SpectateDialog::spectate);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_auth = new FumbblAuth(this);
    m_matchList = new MatchListClient(this);
    connect(m_auth, &FumbblAuth::accessTokenReady, this, &SpectateDialog::onAccessTokenReady);
    connect(m_auth, &FumbblAuth::failed, this, &SpectateDialog::onAuthFailed);
    connect(m_matchList, &MatchListClient::matchesReceived,
            this, &SpectateDialog::onMatchesReceived);
    connect(m_matchList, &MatchListClient::fetchFailed, this, &SpectateDialog::onFetchFailed);

    loadCredentials();

    connect(m_manualGameId, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this](int) { this->spectateBtnState(); });

    // Credentials collapsed by default; expanded when they're missing.
    const bool hasCreds = currentCredentials().valid();
    m_credToggle->setChecked(!hasCreds);
    m_credGroup->setVisible(!hasCreds);

    // Auto-load the list when valid credentials are present.
    if (hasCreds)
        refreshList();
    else
        m_status->setText(tr("No saved credentials — fill them in below (or use a manual Game ID)."));
    spectateBtnState();
}

void SpectateDialog::loadCredentials()
{
    const QJsonObject c = loadCredentialsObject();
    if (!c.isEmpty()) {
        m_clientId->setText(c.value(QStringLiteral("clientId")).toString());
        m_clientSecret->setText(c.value(QStringLiteral("clientSecret")).toString());
        m_username->setText(c.value(QStringLiteral("username")).toString());
    }
    const int lastGame = QSettings().value(QStringLiteral("connect/lastGameId")).toInt();
    if (lastGame > 0)
        m_manualGameId->setValue(lastGame);
}

void SpectateDialog::saveCredentials() const
{
    QJsonObject c;
    c.insert(QStringLiteral("clientId"), m_clientId->text().trimmed());
    c.insert(QStringLiteral("clientSecret"), m_clientSecret->text().trimmed());
    c.insert(QStringLiteral("username"), m_username->text().trimmed());

    QJsonObject root;
    QFile f(credentialsPath());
    if (f.open(QIODevice::ReadOnly))
        root = QJsonDocument::fromJson(f.readAll()).object();
    root.insert(QStringLiteral("fumbbl_credentials"), c);

    QDir().mkpath(QFileInfo(f).absolutePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

SpectateDialog::Credentials SpectateDialog::currentCredentials() const
{
    return { m_clientId->text().trimmed(), m_clientSecret->text().trimmed(),
             m_username->text().trimmed() };
}

void SpectateDialog::refreshList()
{
    const Credentials c = currentCredentials();
    if (!c.valid()) {
        m_status->setText(tr("Client ID, Client Secret and Username are all required."));
        m_credToggle->setChecked(true);
        return;
    }
    m_status->setText(tr("Loading live matches…"));
    m_auth->authenticate(c.clientId, c.clientSecret);
}

void SpectateDialog::onAccessTokenReady(const QString &accessToken)
{
    m_matchList->fetch(accessToken);
}

void SpectateDialog::onAuthFailed(const QString &step, const QString &error)
{
    m_status->setText(tr("OAuth failed (%1): %2").arg(step, error.left(200)));
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
    m_status->setText(tr("%1 live match(es) found.").arg(m_matches.size()));
}

void SpectateDialog::rebuildFilterCombos()
{
    const QString raceKeep = m_raceFilter->currentText();
    const QString divKeep = m_divisionFilter->currentText();

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

    m_divisionFilter->blockSignals(true);
    m_divisionFilter->clear();
    m_divisionFilter->addItem(tr("All divisions"), QString());
    QSet<QString> divs;
    for (const FFBLiveMatch &m : m_matches)
        divs.insert(m.division);
    QStringList dsorted = divs.values();
    std::sort(dsorted.begin(), dsorted.end());
    for (const QString &d : dsorted)
        m_divisionFilter->addItem(d, d);
    const int di = m_divisionFilter->findData(divKeep);
    m_divisionFilter->setCurrentIndex(di >= 0 ? di : 0);
    m_divisionFilter->blockSignals(false);
}

void SpectateDialog::applyFilters()
{
    const QString text = m_textFilter->text().trimmed().toLower();
    const QString race = m_raceFilter->currentData().toString();
    const QString div = m_divisionFilter->currentData().toString();

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
}

int SpectateDialog::selectedGameId() const
{
    const auto sel = m_table->selectionModel()->selectedRows();
    if (sel.isEmpty())
        return -1;
    QTableWidgetItem *it = m_table->item(sel.first().row(), 1);
    return it ? it->data(Qt::UserRole).toInt() : -1;
}

void SpectateDialog::spectateBtnState()
{
    // Enabled when a row is selected or a manual id is set.
    const bool hasId = selectedGameId() > 0 || m_manualGameId->value() > 0;
    m_spectateBtn->setEnabled(hasId && currentCredentials().valid());
}

void SpectateDialog::spectate()
{
    const Credentials c = currentCredentials();
    if (!c.valid()) {
        m_status->setText(tr("Client ID, Client Secret and Username are all required."));
        m_credToggle->setChecked(true);
        return;
    }

    int gameId = m_manualGameId->value() > 0 ? m_manualGameId->value() : selectedGameId();
    if (gameId <= 0) {
        m_status->setText(tr("Pick a match from the list (or enter a Game ID)."));
        return;
    }

    if (m_remember->isChecked())
        saveCredentials();
    QSettings().setValue(QStringLiteral("connect/lastGameId"), gameId);

    emit connectRequested(c.clientId, c.clientSecret, c.username, gameId);
    accept();
}
