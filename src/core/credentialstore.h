// =============================================================================
// CredentialStore — OS credential store for the FUMBBL account
//
// Stores {coach, passwordMd5}: ONLY the MD5 of the site password (it is the
// HMAC key the password-challenge login needs). The plaintext is NEVER
// persisted by the app (plan F2-A).
//
// Backends (compiled per platform):
//   * Linux   : Secret Service (libsecret) — GNOME Keyring, or KWallet via
//               the org.kde.secretservicecompat layer
//   * Windows : Credential Manager (CredRead/CredWrite, target
//               "fumbbl-reborn/<coach>")
//   * none    : hasBackend() == false; the caller falls back to the 0600
//               line file (coach remembered, password per session — see
//               SpectateDialog).
//
// All operations are async (the store may show an unlock dialog on first
// use). The synchronous store call runs on a worker thread with its own
// GMainContext pump (libsecret is synchronous D-Bus); results are delivered
// on the main thread via signals.
// =============================================================================
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class CredentialStore : public QObject
{
    Q_OBJECT

public:
    explicit CredentialStore(QObject *parent = nullptr);

    // A store backend is compiled in for this platform. Note: even with a
    // backend, a call can fail at runtime (no D-Bus session, no store
    // running) — the *Finished signals report that (ok == false).
    static bool hasBackend();

    // The 0600 line file the migration / no-backend path shares
    // (`~/.config/fumbbl-reborn/creds`, one `coach=<name>` line).
    // Single source of truth for the path: SpectateDialog, MainWindow
    // (Log out) and the live tools all resolve it here.
    static QString credentialsPath();

    // passwordMd5: 16 binary bytes (MD5 of the clear-text password).
    void get(const QString &coach);
    void set(const QString &coach, const QByteArray &passwordMd5);
    void clear(const QString &coach);

    // Payload stored by the backends: compact JSON {coach, passwordMd5(hex)}.
    static QByteArray makePayload(const QString &coach, const QByteArray &passwordMd5);
    static bool parsePayload(const QByteArray &payload, QString &coach, QByteArray &passwordMd5);

signals:
    // passwordMd5 empty = no entry for this coach (or store error, ok=false).
    void getFinished(const QString &coach, const QByteArray &passwordMd5, bool ok);
    void setFinished(const QString &coach, bool ok, const QString &error);
    void clearFinished(const QString &coach, bool ok);

private:
    // Synchronous backends (one per platform; no backend => !ok).
    static bool backendGet(const QString &coach, QByteArray &passwordMd5, QString &error);
    static bool backendSet(const QString &coach, const QByteArray &passwordMd5, QString &error);
    static bool backendClear(const QString &coach, QString &error);
};
