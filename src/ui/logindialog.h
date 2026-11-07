// =============================================================================
// LoginDialog — client login (the gate to the full UI)
//
// Shown modal over the stadium at startup (and again after Log out). The
// user cannot click anything in the client — not even the menus — until
// the login is verified against the FUMBBL server.
//
// Credential rules (plan 0.2.2, product decision 06/nov):
//   * The password field ALWAYS shows dots — while typing AND when a
//     remembered entry is present (a fixed 8-dot sentinel text stands in
//     for the keychain entry; the first keystroke replaces it).
//   * "Remember me" (default on): on verified login the {coach, MD5}
//     goes to the OS keychain (never plaintext); without a keychain the
//     0600 line file keeps coach+password, as before.
//   * A typed password always wins over a stored MD5 (stale entry:
//     switched coach on a shared PC, changed site password).
//
// The dialog does NOT accept itself on Login: it emits loginRequested()
// and STAYS OPEN while MainWindow verifies the credentials with the
// server (a join attempt — the password is checked there). Success:
// acceptLogin(); failure: showServerMessage() and the user retries.
//
// The gate CANNOT be bypassed: closing the dialog (window X / Esc —
// both go through reject()) emits quitRequested() exactly like the Quit
// button — the client quits, the UI is never handed over un-gated.
// =============================================================================
#pragma once

#include <QDialog>

class CredentialStore;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr, bool autoLoadKeychain = true);

    // The keychain holds an entry for the current coach: the password
    // field shows the 8-dot sentinel and this MD5 is used unless the
    // user types a replacement.
    static const int kStoredDots = 8;

    // Test / server-feedback seams.
    void setCredentialsForTest(const QString &coach, const QString &password,
                               bool remember);
    void setStoredMd5ForTest(const QByteArray md5);
    // Fire the same path the Login button takes (Qt 6.12's QDialog no
    // longer exposes defaultButton() for the test to click).
    void loginForTest();
    // The server rejected the attempt (or an error happened): stay open.
    void showServerMessage(const QString &message);
    // The server verified the credentials: persist per "remember me" and
    // close.
    void acceptLogin();
    // True while the field still holds the untouched sentinel (the user
    // did not type a replacement).
    bool storedPasswordUntouched() const;
    // The live-typing contract: the Login button's enabled state must
    // track the fields as the user types (not only on focus-out).
    QLineEdit *coachFieldForTest() { return m_coach; }
    QLineEdit *passwordFieldForTest() { return m_password; }
    bool loginButtonEnabledForTest() const;
    // The status line exactly as the user reads it.
    QString serverMessageForTest() const;
    // Window X / Esc: the gate cannot be dismissed without a decision —
    // closing it is the same as the Quit button (quitRequested), never a
    // silent hand-over of the un-gated client. (QDialog::reject is public,
    // so the override stays public — tests drive the exact X path.)
    void reject() override;

signals:
    // Emitted on Login click. passwordMd5 is the MD5 of the typed
    // password, or the keychain entry when the field is the sentinel.
    void loginRequested(const QString &coach, const QByteArray &passwordMd5);
    // Quit button: close the client.
    void quitRequested();

private:
    void loadCredentials();
    void startKeychainLookup();
    void onStoreGetFinished(const QString &coach, const QByteArray &passwordMd5, bool ok);
    void onStoreSetFinished(const QString &coach, bool ok, const QString &error);
    void loginBtnState();
    void login();
    void stripPasswordFromFile() const;
    // The 0600 credentials file (the keychain's fallback / safety net):
    // "coach=…" line always, "password=…" only when a password is given
    // (an empty password = strip). Static: the path comes from
    // CredentialStore::credentialsPath().
    static void persistFile(const QString &coach, const QString &password);

    QLineEdit *m_coach = nullptr;
    QLineEdit *m_password = nullptr;
    QCheckBox *m_remember = nullptr;
    QPushButton *m_loginBtn = nullptr;
    QLabel *m_status = nullptr;
    CredentialStore *m_store = nullptr;
    // MD5 from the keychain (empty = nothing stored for this coach yet).
    QByteArray m_storedMd5;
    // The coach field's last value: a change means the user switched
    // account on a shared PC (previous password/MD5 are dropped).
    QString m_lastCoach;
};
