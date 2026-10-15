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
QHash<QString, QString> s_faceSlugs; // lowercased position name -> slug
QHash<QString, QString> s_raceSlugs; // lowercased race name -> representative slug
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
            if (d.exists(QStringLiteral("faces")))
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
                if (d.exists(QStringLiteral("faces"))) {
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
        loadMap(dir() + QStringLiteral("/faces.txt"), s_faceSlugs);
        loadMap(dir() + QStringLiteral("/race_faces.txt"), s_raceSlugs);
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

// Slugify: lowercase, non-alphanumerics -> '_', trimmed. ('Ooligan -> ooligan,
// "Altern Forest Treeman" -> altern_forest_treeman)
QString slugify(const QString &name)
{
    QString out;
    bool pendingSep = false;
    for (const QChar ch : name.toLower()) {
        if (ch.isLetterOrNumber()) {
            if (!pendingSep && !out.isEmpty())
                out.append(QLatin1Char('_'));
            out.append(ch);
            pendingSep = false;
        } else {
            pendingSep = true;
        }
    }
    while (out.endsWith(QLatin1Char('_')))
        out.chop(1);
    return out;
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

QString faceSlug(const QString &positionName)
{
    ensureMaps();
    const QString key = positionName.trimmed().toLower();
    const auto it = s_faceSlugs.constFind(key);
    if (it != s_faceSlugs.constEnd())
        return it.value();
    const QString slug = slugify(positionName);
    if (!slug.isEmpty() && QFile::exists(dir() + QStringLiteral("/faces/") + slug
                                                  + QStringLiteral(".png")))
        return slug;
    return QStringLiteral("human_lineman"); // generic
}

QPixmap playerFace(const QString &positionName, const QString &raceHint)
{
    const QString slug = faceSlug(positionName);
    QPixmap pm = loadPixmap(QStringLiteral("faces/%1.png").arg(slug));
    if (pm.isNull() && !raceHint.trimmed().isEmpty()) {
        // Custom-named star piece: use the race's representative face.
        const QString raceSlug = s_raceSlugs.value(raceHint.trimmed().toLower());
        if (!raceSlug.isEmpty())
            pm = loadPixmap(QStringLiteral("faces/%1.png").arg(raceSlug));
    }
    if (pm.isNull() && slug != QStringLiteral("human_lineman"))
        pm = loadPixmap(QStringLiteral("faces/human_lineman.png"));
    return pm;
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
    return !playerFace(QStringLiteral("Ogre")).isNull();
}

} // namespace assets
