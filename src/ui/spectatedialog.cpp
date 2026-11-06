// =============================================================================
// SpectateDialog — implementation
// =============================================================================
#include "ui/spectatedialog.h"

#include "core/credentialstore.h"
#include "core/passwordchallenge.h"

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
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
// Shared credentials file (also used by tools/spectator_smoke /
// tools/reports_capturer): plain lines
//
//     coach=...
//     password=...
//
// kept 0600 (site password of the user's FUMBBL account).
QString credentialsPath()
{
    return QDir::homePath() + QStringLiteral("/.config/fumbbl-reborn/creds");
}

// Returns {coach, password}; empty when the file is missing/incomplete.
QPair<QString, QString> loadCredentialsPair()
{
    QString coach;
    QString password;
    QFile f(credentialsPath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QList<QByteArray> lines = f.readAll().split('\n');
    for (const QByteArray &raw : lines) {
        const int eq = raw.indexOf('=');
        if (eq <= 0)
            continue;
        const QString key = QString::fromUtf8(raw.left(eq)).trimmed();
        const QString value = QString::fromUtf8(raw.mid(eq + 1)).trimmed();
        if (key == QLatin1String("coach"))
            coach = value;
        else if (key == QLatin1String("password"))
            password = value;
    }
    if (coach.isEmpty() || password.isEmpty())
        return {};
    return {coach, password};
}
} // namespace

SpectateDialog::SpectateDialog(QWidget *parent, bool autoLoadList) : QDialog(parent)
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
    m_credToggle = new QPushButton(tr("FUMBBL account  ▸"), this);
    m_credToggle->setCheckable(true);
    m_credToggle->setFlat(true);
    m_credToggle->setStyleSheet(QStringLiteral("text-align: left; padding: 2px 4px;"));
    m_credGroup = new QGroupBox(tr("FUMBBL account"), this);
    m_coach = new QLineEdit(m_credGroup);
    m_password = new QLineEdit(m_credGroup);
    m_password->setEchoMode(QLineEdit::Password);
    m_remember = new QCheckBox(tr("Remember credentials"), m_credGroup);
    m_remember->setChecked(true);
    m_loadBtn = new QPushButton(tr("Load matches"), m_credGroup);
    auto *credForm = new QHBoxLayout(m_credGroup);
    credForm->addWidget(new QLabel(tr("Coach:")), 0);
    credForm->addWidget(m_coach, 1);
    credForm->addWidget(new QLabel(tr("Password:")), 0);
    credForm->addWidget(m_password, 1);
    auto *credRow2 = new QHBoxLayout();
    credRow2->addWidget(m_remember);
    credRow2->addStretch(1);
    credRow2->addWidget(m_loadBtn);
    auto *credWarning = new QLabel(
        tr("Spectating uses the credentials of your FUMBBL account (the site's "
           "password). They are stored in your system keychain."),
        m_credGroup);
    credWarning->setStyleSheet(QStringLiteral("color: #9fb0c4; font-size: 11px;"));
    auto *credLayout = new QVBoxLayout(m_credGroup);
    credLayout->addLayout(credForm);
    credLayout->addLayout(credRow2);
    credLayout->addWidget(credWarning);

    connect(m_credToggle, &QPushButton::toggled, m_credGroup, &QWidget::setVisible);
    connect(m_credToggle, &QPushButton::toggled, this, [this](bool on) {
        m_credToggle->setText(on ? tr("FUMBBL account  ▾") : tr("FUMBBL account  ▸"));
    });

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

    m_matchList = new MatchListClient(this);
    connect(m_matchList, &MatchListClient::matchesReceived,
            this, &SpectateDialog::onMatchesReceived);
    connect(m_matchList, &MatchListClient::fetchFailed, this, &SpectateDialog::onFetchFailed);

    m_store = new CredentialStore(this);
    connect(m_store, &CredentialStore::getFinished, this, &SpectateDialog::onStoreGetFinished);
    connect(m_store, &CredentialStore::setFinished, this, &SpectateDialog::onStoreSetFinished);

    loadCredentials();

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
                    // The clicked match is now the active pick: drop the stale
                    // manual id so it cannot override the selection.
                    m_manualGameId->blockSignals(true);
                    m_manualGameId->setValue(0);
                    m_manualGameId->blockSignals(false);
                }
                this->spectateBtnState();
            });

    // Credentials collapsed by default; expanded when they're missing.
    const bool hasCreds = !m_coach->text().trimmed().isEmpty()
                          && !m_password->text().trimmed().isEmpty();
    m_credToggle->setChecked(!hasCreds);
    m_credGroup->setVisible(!hasCreds);

    // The match list is public: always auto-load (tests opt out, along
    // with the keychain lookup — tests stay network- and store-free).
    if (autoLoadList) {
        refreshList();
        startKeychainLookup();
    } else {
        m_status->setText(tr("Ready."));
    }
    if (!hasCreds)
        m_status->setText(tr("No saved credentials — fill in your FUMBBL account below."));
    spectateBtnState();
}

void SpectateDialog::loadCredentials()
{
    const QPair<QString, QString> c = loadCredentialsPair();
    if (!c.first.isEmpty()) {
        m_coach->setText(c.first);
        m_password->setText(c.second);
    }
    const int lastGame = QSettings().value(QStringLiteral("connect/lastGameId")).toInt();
    if (lastGame > 0)
        m_manualGameId->setValue(lastGame);
}

void SpectateDialog::saveCredentials() const
{
    QFile f(credentialsPath());
    QDir().mkpath(QFileInfo(f).absolutePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(("coach=" + m_coach->text().trimmed() + "\n").toUtf8());
        f.write(("password=" + m_password->text().trimmed() + "\n").toUtf8());
    }
    // The file holds the site password: owner read/write only.
    QFile::setPermissions(credentialsPath(), QFile::ReadOwner | QFile::WriteOwner);
}



void SpectateDialog::refreshList()
{
    // Public endpoint: no credentials needed to list matches.
    m_status->setText(tr("Loading live matches…"));
    m_matchList->fetch();
}

void SpectateDialog::startKeychainLookup()
{
    const QString coach = m_coach->text().trimmed();
    if (coach.isEmpty() || !CredentialStore::hasBackend())
        return; // fallback: the file keeps coach+password
    m_store->get(coach);
}

void SpectateDialog::onStoreGetFinished(const QString &coach, const QByteArray &passwordMd5,
                                        bool ok)
{
    if (!ok || m_coach->text().trimmed() != coach)
        return; // store error or the coach field changed: keep the file as is
    if (!passwordMd5.isEmpty()) {
        // The keychain owns the password: use it, and drop the plaintext
        // line from the file (coach remembered, password in the store).
        m_storedMd5 = passwordMd5;
        stripPasswordFromFile();
    } else if (!m_password->text().trimmed().isEmpty() && m_remember->isChecked()) {
        // First run: seed the store; the file strip happens once the write
        // is confirmed (onStoreSetFinished).
        m_store->set(coach, fumbbl::PasswordChallenge::encodePassword(m_password->text().trimmed()));
    }
    spectateBtnState();
}

void SpectateDialog::onStoreSetFinished(const QString &coach, bool ok, const QString &error)
{
    if (m_coach->text().trimmed() != coach)
        return;
    if (ok) {
        m_storedMd5 = fumbbl::PasswordChallenge::encodePassword(m_password->text().trimmed());
        stripPasswordFromFile();
    } else {
        // Store write failed: keep the plaintext in the file (fallback) so
        // the user loses nothing.
        m_status->setText(tr("Keychain unavailable (%1) — credentials kept in the local file.")
                              .arg(error.left(160)));
    }
    spectateBtnState();
}

void SpectateDialog::stripPasswordFromFile() const
{
    QFile f(credentialsPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(("coach=" + m_coach->text().trimmed() + "\n").toUtf8());
    QFile::setPermissions(credentialsPath(), QFile::ReadOwner | QFile::WriteOwner);
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
    emit listLoaded();
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

void SpectateDialog::setCredentialsForTest(const QString &coach, const QString &password,
                                           bool remember)
{
    m_coach->setText(coach);
    m_password->setText(password);
    m_remember->setChecked(remember);
}

void SpectateDialog::setStoredMd5ForTest(const QByteArray &md5)
{
    m_storedMd5 = md5;
    spectateBtnState();
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
    if (m_table->rowCount() < 1)
        return false;
    // The table is sorted by game id (column 1) ascending; the first row
    // is the current top of the list.
    m_table->clearSelection();
    m_table->selectRow(0);
    m_table->setCurrentCell(0, 0);
    return true;
}

void SpectateDialog::spectateBtnState()
{
    // Enabled when a row is selected or a manual id is set, and login
    // material is present (typed password or a keychain entry).
    const bool hasId = selectedGameId() > 0 || m_manualGameId->value() > 0;
    const bool hasCreds = !m_coach->text().trimmed().isEmpty()
                          && (!m_password->text().trimmed().isEmpty() || !m_storedMd5.isEmpty());
    m_spectateBtn->setEnabled(hasId && hasCreds);
}

void SpectateDialog::spectate()
{
    const QString coach = m_coach->text().trimmed();
    const QString password = m_password->text().trimmed();
    if (coach.isEmpty() || (password.isEmpty() && m_storedMd5.isEmpty())) {
        m_status->setText(tr("Coach and password (your FUMBBL account) are required."));
        m_credToggle->setChecked(true);
        return;
    }

    int gameId = m_manualGameId->value() > 0 ? m_manualGameId->value() : selectedGameId();
    if (gameId <= 0) {
        m_status->setText(tr("Pick a match from the list (or enter a Game ID)."));
        return;
    }

    QByteArray pwdMd5 = m_storedMd5;
    if (!password.isEmpty() && pwdMd5.isEmpty())
        pwdMd5 = fumbbl::PasswordChallenge::encodePassword(password);

    if (m_remember->isChecked()) {
        if (CredentialStore::hasBackend()) {
            if (pwdMd5 != m_storedMd5)
                m_store->set(coach, pwdMd5); // remember / update in the keychain
            m_storedMd5 = pwdMd5;
            stripPasswordFromFile();
        } else {
            saveCredentials(); // fallback: the 0600 file keeps coach+password
        }
    }
    QSettings().setValue(QStringLiteral("connect/lastGameId"), gameId);

    emit connectRequested(coach, pwdMd5, gameId);
    accept();
}
