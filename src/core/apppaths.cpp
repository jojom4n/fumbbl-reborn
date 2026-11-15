#include "core/apppaths.h"

#include <QDir>
#include <QtGlobal> // qgetenv

QString appSaveRoot()
{
#if defined(Q_OS_WIN)
    // Windows: all app files live in the standard "Saved Games" folder,
    // so the user finds them in one place (not buried in AppData).
    const QString user = qgetenv("USERPROFILE");
    const QString base = user.isEmpty() ? QDir::homePath() : user;
    return base + QStringLiteral("/Saved Games/FUMBBL Reborn");
#else
    return QDir::homePath() + QStringLiteral("/.config/fumbbl-reborn");
#endif
}
