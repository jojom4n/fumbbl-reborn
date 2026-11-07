// =============================================================================
// LoginDialog — implementation
// =============================================================================
#include "ui/logindialog.h"

#include "core/credentialstore.h"
#include "core/gamemodel.h" // fumbbl::PasswordChallenge
#include "core/passwordchallenge.h"

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {
// The sentinel standing in for a keychain entry: kStoredDots bullet
// characters. The field's echo mode already masks any text as dots, so
// the sentinel reads exactly like a saved password. The FIRST keystroke
// replaces it (standard field behavior) — after that it is just text.
QString storedPasswordSentinel()
{
    return QString(QChar(0x2022)).repeated(LoginDialog::kStoredDots);
}

// Shared credentials file (also used by MainWindow/Log out and the live
// tools; path resolved by CredentialStore::credentialsPath): plain lines
//
//     coach=...
//     password_md5=<hex>   (remembered credential; 0600)
//
// The file stores the MD5 of the site password — the protocol's own
// credential (the join sends HMAC-MD5(MD5(password), challenge), so the
// client never needs the clear text again; the keychain stores the very
// same value). NEVER plaintext, on any path.
// Returns {coach, passwordMd5}; empty when the file is missing/incomplete.
QPair<QString, QString> loadCredentialsPair()
{
    QString coach;
    QString passwordMd5;
    QFile f(CredentialStore::credentialsPath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QList<QByteArray> lines = f.readAll().split('\n');
    for (const QByteArray &raw : lines) {
        const int eq = raw.indexOf('=');
        if (eq <= 0)
            continue;
        const QString key = QString::fromUtf8(raw.left(eq)).trimmed();
        const QString value = QString::fromUtf8(raw.mid(eq + 1));
        if (key == QLatin1String("coach"))
            coach = value;
        else if (key == QLatin1String("password_md5"))
            passwordMd5 = value;
    }
    return {coach, passwordMd5};
}
} // namespace

LoginDialog::LoginDialog(QWidget *parent, bool autoLoadKeychain) : QDialog(parent)
{
    setWindowTitle(tr("FUMBBL Reborn — Login"));
    setMinimumWidth(420);

    auto *titleLabel = new QLabel(tr("Sign in to your FUMBBL account"), this);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: 600;"));

    m_coach = new QLineEdit(this);
    m_coach->setPlaceholderText(tr("Coach (site username)"));
    m_password = new QLineEdit(this);
    // ALWAYS dots: while typing and when a remembered entry is shown as
    // the sentinel. Plain QLineEdit::Password echo mode — the text is
    // never rendered in the clear.
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("Password"));

    m_remember = new QCheckBox(tr("Remember me"), this);
    m_remember->setChecked(true);

    m_status = new QLabel(this);
    m_status->setStyleSheet(QStringLiteral("color: #9fb0c4; font-size: 11px;"));
    m_status->setWordWrap(true);

    m_loginBtn = new QPushButton(tr("Login"), this);
    m_loginBtn->setDefault(true);
    auto *quitBtn = new QPushButton(tr("Quit"), this);

    auto *form = new QFormLayout();
    form->addRow(tr("Coach:"), m_coach);
    form->addRow(tr("Password:"), m_password);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    btnRow->addWidget(m_loginBtn);
    btnRow->addWidget(quitBtn);

    auto *root = new QVBoxLayout(this);
    root->addWidget(titleLabel);
    root->addLayout(form);
    root->addWidget(m_remember);
    root->addWidget(m_status);
    root->addLayout(btnRow);

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginDialog::login);
    connect(quitBtn, &QPushButton::clicked, this, &LoginDialog::quitRequested);
    // The button's enabled state must track the fields WHILE the user
    // types: it was only re-evaluated on the coach field's focus-out, so
    // a fully typed form could stay grey and unreachable.
    connect(m_coach, &QLineEdit::textChanged, this, &LoginDialog::loginBtnState);
    connect(m_password, &QLineEdit::textChanged, this, &LoginDialog::loginBtnState);

    m_store = new CredentialStore(this);
    connect(m_store, &CredentialStore::getFinished, this, &LoginDialog::onStoreGetFinished);
    connect(m_store, &CredentialStore::setFinished, this, &LoginDialog::onStoreSetFinished);

    loadCredentials();

    // Shared PC: the user switches to a different coach. The password
    // text and the stored MD5 belonged to the PREVIOUS coach — drop both,
    // then look the new one up in the keychain (a remembered coach's MD5
    // arrives via onStoreGetFinished, no retyping).
    connect(m_coach, &QLineEdit::editingFinished, this, [this]() {
        const QString coach = m_coach->text().trimmed();
        if (coach != m_lastCoach) {
            m_lastCoach = coach;
            m_password->clear();
            m_storedMd5.clear();
        }
        startKeychainLookup();
        loginBtnState();
    });

    // The match list is public and not needed here: the login verifies
    // against the server directly. The keychain lookup is the only
    // async load (tests opt out — they stay store-free).
    if (autoLoadKeychain)
        startKeychainLookup();
    else
        m_status->setText(tr("Enter your FUMBBL account to continue."));
    loginBtnState();
}

void LoginDialog::reject()
{
    // Window X / Esc: the gate cannot be bypassed — closing the dialog is
    // the same decision as the Quit button (MainWindow routes
    // quitRequested to qApp->quit).
    emit quitRequested();
    QDialog::reject();
}

bool LoginDialog::loginButtonEnabledForTest() const
{
    return m_loginBtn->isEnabled();
}

QString LoginDialog::serverMessageForTest() const
{
    return m_status->text();
}

void LoginDialog::loadCredentials()
{
    const QPair<QString, QString> c = loadCredentialsPair();
    if (!c.first.isEmpty()) {
        m_coach->setText(c.first);
        // The file holds the MD5 (the protocol's credential — the same
        // value the keychain stores): seed it exactly like a keychain
        // hit — sentinel in the field, MD5 ready for the join.
        if (!c.second.isEmpty()) {
            const QByteArray md5 = QByteArray::fromHex(c.second.toLatin1().constData());
            if (!md5.isEmpty()) {
                m_storedMd5 = md5;
                m_password->setText(storedPasswordSentinel());
            }
        }
    }
    // Baseline for "the user switched coach" (see the editingFinished
    // hook in the constructor).
    m_lastCoach = m_coach->text().trimmed();
}

void LoginDialog::startKeychainLookup()
{
    const QString coach = m_coach->text().trimmed();
    if (coach.isEmpty() || !CredentialStore::hasBackend())
        return; // fallback: the file keeps coach+MD5
    m_store->get(coach);
}

void LoginDialog::onStoreGetFinished(const QString &coach, const QByteArray &passwordMd5,
                                     bool ok)
{
    if (!ok || m_coach->text().trimmed() != coach)
        return; // store error or the coach field changed: keep the file as is
    if (!passwordMd5.isEmpty()) {
        // The keychain owns the password: show the sentinel (dots) and
        // use the stored MD5 — unless the user already typed a
        // replacement.
        m_storedMd5 = passwordMd5;
        if (m_password->text().isEmpty())
            m_password->setText(storedPasswordSentinel());
        stripPasswordFromFile();
        m_status->setText(tr("Remembered entry found — the password field shows the "
                             "stored entry. Type over it to use a different password."));
    } else if (!m_password->text().trimmed().isEmpty()
               && m_password->text().trimmed() != storedPasswordSentinel()
               && m_remember->isChecked()) {
        // First run: seed the store; the file strip happens once the write
        // is confirmed (onStoreSetFinished).
        m_store->set(coach,
                     fumbbl::PasswordChallenge::encodePassword(m_password->text().trimmed()));
    }
    loginBtnState();
}

void LoginDialog::onStoreSetFinished(const QString &coach, bool ok, const QString &error)
{
    if (m_coach->text().trimmed() != coach)
        return;
    if (ok) {
        m_storedMd5 =
            fumbbl::PasswordChallenge::encodePassword(m_password->text().trimmed());
        stripPasswordFromFile();
    } else {
        // Store write failed: the MD5 stays in the 0600 file (the safety
        // net written in acceptLogin) so the user loses nothing.
        m_status->setText(tr("Keychain unavailable (%1) — credentials kept in the local file.")
                              .arg(error.left(160)));
    }
    loginBtnState();
}

bool LoginDialog::storedPasswordUntouched() const
{
    return !m_storedMd5.isEmpty() && m_password->text() == storedPasswordSentinel();
}

void LoginDialog::loginBtnState()
{
    const bool hasCoach = !m_coach->text().trimmed().isEmpty();
    const bool hasPwd = !m_password->text().trimmed().isEmpty() || !m_storedMd5.isEmpty();
    m_loginBtn->setEnabled(hasCoach && hasPwd);
}

void LoginDialog::login()
{
    const QString coach = m_coach->text().trimmed();
    const QString pwdText = m_password->text().trimmed();
    if (coach.isEmpty()) {
        m_status->setText(tr("The coach name (your FUMBBL username) is required."));
        return;
    }

    // A typed password ALWAYS wins over the stored MD5: the stored entry
    // can be stale (switched coach on a shared PC, changed site password).
    // The untouched sentinel means "use the keychain entry".
    QByteArray pwdMd5;
    if (storedPasswordUntouched())
        pwdMd5 = m_storedMd5;
    else if (!pwdText.isEmpty())
        pwdMd5 = fumbbl::PasswordChallenge::encodePassword(pwdText);
    if (pwdMd5.isEmpty()) {
        m_status->setText(tr("The password is required (or a remembered keychain entry)."));
        return;
    }

    m_status->setStyleSheet(QStringLiteral("color: #9fb0c4; font-size: 11px;"));
    m_status->setText(tr("Verifying with the FUMBBL server…"));
    emit loginRequested(coach, pwdMd5);
    // The dialog STAYS OPEN: MainWindow verifies (join attempt) and calls
    // acceptLogin() or showServerMessage().
}

void LoginDialog::showServerMessage(const QString &message)
{
    // Red until the next login attempt (login() resets the color).
    m_status->setStyleSheet(QStringLiteral("color: #c0392b; font-size: 11px;"));
    m_status->setText(message);
}

void LoginDialog::acceptLogin()
{
    // Persist per "remember me": {coach, MD5} — the MD5 is the
    // protocol's credential (the keychain and the 0600 file both store
    // it; the clear text is never written anywhere).
    const QString coach = m_coach->text().trimmed();
    if (m_remember->isChecked()) {
        if (CredentialStore::hasBackend()) {
            const QByteArray md5 = storedPasswordUntouched()
                                       ? m_storedMd5
                                       : fumbbl::PasswordChallenge::encodePassword(
                                             m_password->text().trimmed());
            if (md5 != m_storedMd5) {
                m_store->set(coach, md5); // remember / update in the keychain
                // SAFETY NET: the MD5 ALSO goes to the 0600 file — and
                // stays there until the store CONFIRMS the write. The
                // store is a child of this dialog, which dies right
                // after accept(), so a failed write (e.g. no Secret
                // Service on the bus) would otherwise leave the
                // credential in NEITHER place (the file was stripped and
                // the keychain never got it). Strips happen on
                // confirmation (onStoreSetFinished) or at the next
                // startup when the lookup finds the entry
                // (onStoreGetFinished).
                persistFile(coach, md5);
            }
            m_storedMd5 = md5;
            // md5 unchanged (the stored sentinel untouched): the keychain
            // already owns the entry; nothing to persist here.
        } else {
            persistFile(coach,
                        fumbbl::PasswordChallenge::encodePassword(
                            m_password->text().trimmed()));
        }
    }
    QSettings().setValue(QStringLiteral("connect/lastCoach"), coach);
    accept();
}

void LoginDialog::persistFile(const QString &coach, const QByteArray &passwordMd5)
{
    QFile f(CredentialStore::credentialsPath());
    QDir().mkpath(QFileInfo(f).absolutePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(("coach=" + coach + "\n").toUtf8());
        if (!passwordMd5.isEmpty())
            // hex: the file is text (the protocol's own hex convention);
            // raw bytes through QString would be UTF-8-mangled.
            f.write(("password_md5=" + QString::fromLatin1(passwordMd5.toHex()) + "\n")
                        .toUtf8());
    }
    // Owner read/write only (the MD5 of the site password lives here).
    QFile::setPermissions(f.fileName(), QFile::ReadOwner | QFile::WriteOwner);
}

void LoginDialog::stripPasswordFromFile() const
{
    persistFile(m_coach->text().trimmed(), QByteArray());
}

// -----------------------------------------------------------------------------
// Test / server-feedback seams
// -----------------------------------------------------------------------------
void LoginDialog::setCredentialsForTest(const QString &coach, const QString &password,
                                        bool remember)
{
    m_coach->setText(coach);
    m_password->setText(password);
    m_remember->setChecked(remember);
    m_lastCoach = coach;
}

void LoginDialog::setStoredMd5ForTest(const QByteArray md5)
{
    m_storedMd5 = md5;
    if (!md5.isEmpty() && m_password->text().isEmpty())
        m_password->setText(storedPasswordSentinel());
    loginBtnState();
}

void LoginDialog::loginForTest()
{
    login();
}
