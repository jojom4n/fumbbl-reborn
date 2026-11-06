// =============================================================================
// CredentialStore — implementation
// =============================================================================
// IMPORTANT: the GLib/libsecret headers must be included BEFORE any Qt
// header. Qt defines the `signals` macro by default (unless
// QT_NO_KEYWORDS), which breaks glib's gdbusintrospection.h — a struct
// member literally named `signals`. Once glib is parsed (headers are
// guarded), Qt's macro is harmless.
#include <QtGlobal> // Q_OS_* defines, needed by the platform block below

#if defined(Q_OS_LINUX) && defined(FUMBBL_HAS_SECRET)
// Distros split the header layout: Debian/CI ship <secret/secret.h> (cflags
// -I/usr/include/secret), Arch ships <libsecret/secret.h>.
#if __has_include(<secret/secret.h>)
#include <secret/secret.h>
#else
#include <libsecret/secret.h>
#endif
#endif

#include "core/credentialstore.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThreadPool>

#include <atomic>
#include <mutex>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <wincred.h>
#elif defined(Q_OS_LINUX) && defined(FUMBBL_HAS_SECRET)
#include <thread>
#endif

QByteArray CredentialStore::makePayload(const QString &coach, const QByteArray &passwordMd5)
{
    QJsonObject o;
    o.insert(QStringLiteral("coach"), coach);
    o.insert(QStringLiteral("passwordMd5"), QString::fromLatin1(passwordMd5.toHex()));
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

bool CredentialStore::parsePayload(const QByteArray &payload, QString &coach, QByteArray &passwordMd5)
{
    const QJsonObject o = QJsonDocument::fromJson(payload).object();
    coach = o.value(QStringLiteral("coach")).toString();
    passwordMd5 = QByteArray::fromHex(o.value(QStringLiteral("passwordMd5")).toString().toUtf8());
    return !coach.isEmpty() && passwordMd5.size() == 16;
}

CredentialStore::CredentialStore(QObject *parent)
    : QObject(parent)
{
}

bool CredentialStore::hasBackend()
{
#if defined(Q_OS_WIN) || (defined(Q_OS_LINUX) && defined(FUMBBL_HAS_SECRET))
    return true;
#else
    return false;
#endif
}

void CredentialStore::get(const QString &coach)
{
    if (!hasBackend()) {
        emit getFinished(coach, {}, false);
        return;
    }
    QThreadPool::globalInstance()->start([this, coach]() {
        QByteArray md5;
        QString error;
        const bool ok = backendGet(coach, md5, error);
        QMetaObject::invokeMethod(this, [this, coach, md5, ok]() {
            emit getFinished(coach, md5, ok);
        }, Qt::QueuedConnection);
    });
}

void CredentialStore::set(const QString &coach, const QByteArray &passwordMd5)
{
    if (!hasBackend()) {
        emit setFinished(coach, false, QStringLiteral("no credential store backend"));
        return;
    }
    QThreadPool::globalInstance()->start([this, coach, passwordMd5]() {
        QString error;
        const bool ok = backendSet(coach, passwordMd5, error);
        QMetaObject::invokeMethod(this, [this, coach, ok, error]() {
            emit setFinished(coach, ok, error);
        }, Qt::QueuedConnection);
    });
}

void CredentialStore::clear(const QString &coach)
{
    if (!hasBackend()) {
        emit clearFinished(coach, false);
        return;
    }
    QThreadPool::globalInstance()->start([this, coach]() {
        QString error;
        const bool ok = backendClear(coach, error);
        QMetaObject::invokeMethod(this, [this, coach, ok]() {
            emit clearFinished(coach, ok);
        }, Qt::QueuedConnection);
    });
}

// -----------------------------------------------------------------------------
// Windows backend — Credential Manager (generic credentials)
// -----------------------------------------------------------------------------
#if defined(Q_OS_WIN)
namespace {
QString winErrorText(DWORD code)
{
    LPWSTR buf = nullptr;
    const size_t n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                                      | FORMAT_MESSAGE_IGNORE_INSERTS,
                                    nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                    reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    QString msg;
    if (n > 0 && buf) {
        msg = QString::fromWCharArray(buf, static_cast<int>(n)).trimmed();
        LocalFree(buf);
    }
    if (msg.isEmpty())
        msg = QStringLiteral("Windows error %1").arg(code);
    return msg;
}

QByteArray credTarget(const QString &coach)
{
    return (QStringLiteral("fumbbl-reborn/") + coach).toUtf8();
}
} // namespace

bool CredentialStore::backendGet(const QString &coach, QByteArray &passwordMd5, QString &error)
{
    passwordMd5.clear();
    const QByteArray target = credTarget(coach);
    DWORD count = 0;
    LPSTRUCT_CREDENTIALA creds = nullptr;
    if (!CredReadA(target.constData(), CRED_TYPE_GENERIC, 0, &count, &creds) || count != 1) {
        if (GetLastError() == ERROR_NOT_FOUND)
            return true; // ok — simply no entry yet
        error = winErrorText(GetLastError());
        LocalFree(creds);
        return false;
    }
    const CREDENTIALA &c = creds[0];
    QString payloadCoach;
    QByteArray md5;
    const bool ok = c.CredentialBlobSize > 0
                    && parsePayload(QByteArray::fromRawData(
                           reinterpret_cast<const char *>(c.CredentialBlob),
                           static_cast<int>(c.CredentialBlobSize)),
                           payloadCoach, md5)
                    && payloadCoach == coach;
    LocalFree(creds);
    if (!ok) {
        error = QStringLiteral("malformed credential entry");
        return false;
    }
    passwordMd5 = md5;
    return true;
}

bool CredentialStore::backendSet(const QString &coach, const QByteArray &passwordMd5, QString &error)
{
    const QByteArray payload = makePayload(coach, passwordMd5);
    const QByteArray target = credTarget(coach);
    CREDENTIALA cred {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPSTR>(target.constData());
    cred.CredentialBlob = const_cast<LPBYTE>(payload.constData());
    cred.CredentialBlobSize = static_cast<DWORD>(payload.size());
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (!CredWriteA(&cred, 0)) {
        error = winErrorText(GetLastError());
        return false;
    }
    return true;
}

bool CredentialStore::backendClear(const QString &coach, QString &error)
{
    const QByteArray target = credTarget(coach);
    if (!CredFreeA(target.constData(), CRED_TYPE_GENERIC, 0)) {
        const DWORD code = GetLastError();
        if (code == ERROR_NOT_FOUND)
            return true;
        error = winErrorText(code);
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
// Linux backend — Secret Service (libsecret). Entries live in the default
// collection, keyed by the "username" attribute (the coach); the payload
// distinguishes this app from any other via the schema id.
// -----------------------------------------------------------------------------
#elif defined(Q_OS_LINUX) && defined(FUMBBL_HAS_SECRET)
namespace {
// libsecret's C API is synchronous D-Bus: the calling thread needs a
// GMainContext that is iterated while the call blocks (the first use can
// show a keyring-unlock dialog). Run the call here with a dedicated pump
// thread. (GLib >= 2.32 allows iterating a plain context from one thread;
// the removed g_main_context_*_thread helpers are not needed.)
template <typename Fn>
bool secretCall(Fn &&fn, QString &error)
{
    GMainContext *ctx = g_main_context_new();
    // Make this context the worker thread's default so any D-Bus
    // connection libsecret opens here is serviced by our pumper.
    g_main_context_push_thread_default(ctx);
    std::atomic<bool> done {false};
    std::thread pumper([ctx, &done]() {
        while (!done.load(std::memory_order_acquire))
            g_main_context_iteration(ctx, false);
    });
    bool ok = false;
    try {
        ok = fn();
    } catch (...) {
        ok = false;
        error = QStringLiteral("unexpected error in Secret Service call");
    }
    done.store(true, std::memory_order_release);
    pumper.join();
    g_main_context_pop_thread_default(ctx);
    g_main_context_unref(ctx);
    return ok;
}

// Schema with a single string attribute ("coach"); the fixed 32-slot array
// terminates at the first NULL name. (libsecret >= 0.3 API.)
static const SecretSchema kSchema = {
    "org.fumbbl.reborn.credentials",
    SECRET_SCHEMA_NONE,
    {{"coach", SECRET_SCHEMA_ATTRIBUTE_STRING}, {nullptr, (SecretSchemaAttributeType)0}},
};

// Best effort, once per process: on KDE/Niri the Secret Service name
// (org.freedesktop.Secret.Service) is owned by KWallet via the activatable
// org.kde.secretservicecompat, which nothing starts automatically. Ask
// D-Bus to activate it; on other desktops this simply fails and the
// libsecret calls report "no Secret Service" -> file fallback.
static void maybeActivateSecretService()
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        QDBusInterface dbus(QStringLiteral("org.freedesktop.DBus"),
                            QStringLiteral("/org/freedesktop/DBus"),
                            QStringLiteral("org.freedesktop.DBus"));
        QDBusReply<int> r = dbus.call(QStringLiteral("StartServiceByName"),
                                      QStringLiteral("org.kde.secretservicecompat"), 0);
        Q_UNUSED(r)
    });
}
} // namespace

bool CredentialStore::backendGet(const QString &coach, QByteArray &passwordMd5, QString &error)
{
    passwordMd5.clear();
    maybeActivateSecretService();
    return secretCall([&]() {
        GError *gerr = nullptr;
        const QByteArray coachUtf8 = coach.toUtf8();
        gchar *secret = secret_password_lookup_sync(&kSchema, nullptr, &gerr,
                                                    "coach", coachUtf8.constData(),
                                                    /* attributes end */ NULL);
        if (gerr) {
            error = QStringLiteral("Secret Service: %1").arg(QString::fromUtf8(gerr->message));
            g_clear_error(&gerr);
            return false;
        }
        if (!secret)
            return true; // ok — no entry yet
        const QByteArray payload = QByteArray::fromRawData(secret, static_cast<int>(strlen(secret)));
        g_free(secret);
        QString payloadCoach;
        if (parsePayload(payload, payloadCoach, passwordMd5) && payloadCoach == coach)
            return true;
        error = QStringLiteral("malformed credential entry");
        return false;
    }, error);
}

bool CredentialStore::backendSet(const QString &coach, const QByteArray &passwordMd5, QString &error)
{
    const QByteArray payload = makePayload(coach, passwordMd5);
    maybeActivateSecretService();
    return secretCall([&]() {
        GError *gerr = nullptr;
        const QByteArray coachUtf8 = coach.toUtf8();
        const bool ok = secret_password_store_sync(&kSchema, nullptr, "FUMBBL-Reborn",
                                                   payload.constData(), nullptr, &gerr,
                                                   "coach", coachUtf8.constData(),
                                                   /* attributes end */ NULL);
        if (gerr) {
            error = QStringLiteral("Secret Service: %1").arg(QString::fromUtf8(gerr->message));
            g_clear_error(&gerr);
            return false;
        }
        return ok;
    }, error);
}

bool CredentialStore::backendClear(const QString &coach, QString &error)
{
    Q_UNUSED(error)
    maybeActivateSecretService();
    return secretCall([&]() {
        GError *gerr = nullptr;
        const QByteArray coachUtf8 = coach.toUtf8();
        // Best effort: a missing entry is not an error for us.
        secret_password_clear_sync(&kSchema, nullptr, &gerr,
                                   "coach", coachUtf8.constData(),
                                   /* attributes end */ NULL);
        if (gerr)
            g_clear_error(&gerr);
        return true;
    }, error);
}

// -----------------------------------------------------------------------------
// No backend (macOS or Linux without libsecret): the file fallback applies.
// -----------------------------------------------------------------------------
#else
bool CredentialStore::backendGet(const QString &coach, QByteArray &passwordMd5, QString &error)
{
    Q_UNUSED(coach)
    Q_UNUSED(passwordMd5)
    Q_UNUSED(error)
    return false;
}

bool CredentialStore::backendSet(const QString &coach, const QByteArray &passwordMd5, QString &error)
{
    Q_UNUSED(coach)
    Q_UNUSED(passwordMd5)
    Q_UNUSED(error)
    return false;
}

bool CredentialStore::backendClear(const QString &coach, QString &error)
{
    Q_UNUSED(coach)
    Q_UNUSED(error)
    return false;
}
#endif
