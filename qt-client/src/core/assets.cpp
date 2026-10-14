// =============================================================================
// Assets — implementation
// =============================================================================
#include "core/assets.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QPixmap>

namespace assets {

namespace {

QString s_dir;
bool s_dirResolved = false;
QHash<QString, QString> s_skillSlugs; // lowercased name -> slug
QHash<QString, QString> s_cardSlugs;
QHash<QString, QPixmap> s_cache; // file path -> pixmap

void loadMap(const QString &path, QHash<QString, QString> &map)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    QByteArray raw;
    while (!(raw = f.readLine()).isNull()) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty())
            continue;
        const QStringList parts = line.split(QLatin1Char('\t'));
        if (parts.size() == 2)
            map.insert(parts[0].toLower(), parts[1]);
    }
}

const QString &dir()
{
    if (!s_dirResolved) {
        s_dirResolved = true;
#ifdef FUMBBL_ASSETS_DIR
        {
            QDir d(QStringLiteral(FUMBBL_ASSETS_DIR));
            if (d.exists(QStringLiteral("players")))
                s_dir = d.absolutePath();
        }
#endif
        if (s_dir.isEmpty()) {
            const QStringList candidates = {
                QCoreApplication::applicationDirPath() + QStringLiteral("/../assets"),
                QDir::currentPath() + QStringLiteral("/assets"),
            };
            for (const QString &c : candidates) {
                QDir d(c);
                if (d.exists(QStringLiteral("players"))) {
                    s_dir = d.absolutePath();
                    break;
                }
            }
        }
    }
    return s_dir;
}

bool ensureMaps()
{
    static bool done = false;
    if (!done) {
        done = true;
        loadMap(dir() + QStringLiteral("/skill_names.txt"), s_skillSlugs);
        loadMap(dir() + QStringLiteral("/card_names.txt"), s_cardSlugs);
    }
    return true;
}

QPixmap loadPixmap(const QString &relPath)
{
    const QString path = dir() + QStringLiteral("/") + relPath;
    const auto it = s_cache.constFind(path);
    if (it != s_cache.constEnd())
        return it.value();
    QPixmap pm;
    if (!dir().isEmpty() && QFile::exists(path))
        pm.load(path);
    s_cache.insert(path, pm);
    return pm;
}

} // namespace

QString assetsDir()
{
    return dir();
}

QString skillIconSlug(const QString &skillName)
{
    ensureMaps();
    return s_skillSlugs.value(skillName.toLower(), QStringLiteral("generic"));
}

QString cardIconSlug(const QString &cardName)
{
    ensureMaps();
    return s_cardSlugs.value(cardName.toLower(), QStringLiteral("generic"));
}

QPixmap playerBust(int archetype, bool home)
{
    if (archetype < 0 || archetype > 4)
        archetype = 0;
    static const QStringList names = { QStringLiteral("blocker"), QStringLiteral("runner"),
                                       QStringLiteral("blitzer"), QStringLiteral("catcher"),
                                       QStringLiteral("thrower") };
    return loadPixmap(QStringLiteral("players/%1_%2.png")
                          .arg(names[archetype])
                          .arg(home ? QStringLiteral("home") : QStringLiteral("away")));
}

QPixmap skillPixmap(const QString &skillName)
{
    return loadPixmap(QStringLiteral("skills/%1.png").arg(skillIconSlug(skillName)));
}

QPixmap cardPixmap(const QString &cardName)
{
    return loadPixmap(QStringLiteral("cards/%1.png").arg(cardIconSlug(cardName)));
}

bool isAvailable()
{
    return !playerBust(0, true).isNull();
}

} // namespace assets
