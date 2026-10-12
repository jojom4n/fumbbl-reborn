// =============================================================================
// ConnectDialog — implementation
// =============================================================================
#include "ui/connectdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>

namespace {
// Same file the TS client (Tauri Store) and spectator_smoke read/write.
QString credentialsPath()
{
    return QDir::homePath()
        + QStringLiteral("/.local/share/com.saruman.fumbbl-reborn/fumbbl-credentials.json");
}
} // namespace

ConnectDialog::ConnectDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Connect to a FUMBBL match"));
    setMinimumWidth(420);

    m_clientId = new QLineEdit(this);
    m_clientId->setPlaceholderText(tr("OAuth2 client id"));
    m_clientSecret = new QLineEdit(this);
    m_clientSecret->setPlaceholderText(tr("OAuth2 client secret"));
    m_clientSecret->setEchoMode(QLineEdit::Password);
    m_username = new QLineEdit(this);
    m_username->setPlaceholderText(tr("FUMBBL username (coach name)"));
    m_gameId = new QSpinBox(this);
    m_gameId->setRange(0, 2000000000);
    m_gameId->setSpecialValueText(tr("(current match)"));

    m_remember = new QCheckBox(tr("Remember credentials"), this);
    m_remember->setChecked(true);

    auto *form = new QFormLayout(this);
    form->addRow(tr("Client ID:"), m_clientId);
    form->addRow(tr("Client Secret:"), m_clientSecret);
    form->addRow(tr("Username:"), m_username);
    form->addRow(tr("Game ID:"), m_gameId);
    form->addRow(QString(), m_remember);

    auto *connectBtn = new QPushButton(tr("Connect"), this);
    auto *cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(connectBtn, &QPushButton::clicked, this, [this]() {
        if (m_clientId->text().trimmed().isEmpty() ||
            m_clientSecret->text().trimmed().isEmpty() ||
            m_username->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Missing information"),
                                 tr("Client ID, Client Secret and Username are all required."));
            return;
        }
        if (m_remember->isChecked())
            saveCredentials();
        QSettings().setValue(QStringLiteral("connect/lastGameId"), m_gameId->value());
        emit connectRequested(m_clientId->text().trimmed(), m_clientSecret->text().trimmed(),
                              m_username->text().trimmed(), m_gameId->value());
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::NoButton, this);
    buttons->addButton(connectBtn, QDialogButtonBox::AcceptRole);
    buttons->addButton(cancelBtn, QDialogButtonBox::RejectRole);
    form->addRow(buttons);

    loadSavedCredentials();
}

void ConnectDialog::loadSavedCredentials()
{
    QFile f(credentialsPath());
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        QJsonObject c = root.value(QStringLiteral("fumbbl_credentials")).toObject();
        if (c.isEmpty())
            c = root.value(QStringLiteral("credentials")).toObject();
        if (!c.isEmpty()) {
            m_clientId->setText(c.value(QStringLiteral("clientId")).toString());
            m_clientSecret->setText(c.value(QStringLiteral("clientSecret")).toString());
            m_username->setText(c.value(QStringLiteral("username")).toString());
        }
    }
    const int lastGame = QSettings().value(QStringLiteral("connect/lastGameId")).toInt();
    if (lastGame > 0)
        m_gameId->setValue(lastGame);
}

void ConnectDialog::saveCredentials() const
{
    QJsonObject c;
    c.insert(QStringLiteral("clientId"), m_clientId->text().trimmed());
    c.insert(QStringLiteral("clientSecret"), m_clientSecret->text().trimmed());
    c.insert(QStringLiteral("username"), m_username->text().trimmed());

    // Merge into the existing file so other keys (if any) survive.
    QJsonObject root;
    QFile f(credentialsPath());
    if (f.open(QIODevice::ReadOnly))
        root = QJsonDocument::fromJson(f.readAll()).object();
    root.insert(QStringLiteral("fumbbl_credentials"), c);

    QDir().mkpath(QFileInfo(f).absolutePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}
