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
struct RosterPiece {
    QString name;         // official BB2025 piece name (== faces.txt key)
    QStringList keywords; // roles + creature types ("Blitzer,Orc", ...)
};
QHash<QString, QVector<RosterPiece>> s_raceRoster; // lowercased race -> pieces
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

void loadRoster(const QString &path)
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
        if (parts.size() < 2)
            continue;
        RosterPiece piece;
        piece.name = parts[1];
        if (parts.size() >= 3)
            piece.keywords = parts[2].split(QLatin1Char(','), Qt::SkipEmptyParts);
        s_raceRoster[parts[0].toLower()].append(piece);
    }
}

// Race roster for the payload's race string, tolerant to singular/plural
// variants ("Amazon" <-> "Amazons").
const QVector<RosterPiece> &racePieces(const QString &race)
{
    static const QVector<RosterPiece> s_empty;
    const QString r = race.trimmed().toLower();
    auto it = s_raceRoster.constFind(r);
    if (it == s_raceRoster.constEnd() && !r.isEmpty()) {
        if (!r.endsWith(QLatin1Char('s')))
            it = s_raceRoster.constFind(r + QLatin1Char('s'));
        else
            it = s_raceRoster.constFind(r.chopped(1));
    }
    if (it == s_raceRoster.constEnd())
        return s_empty;
    return it.value();
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
        loadRoster(dir() + QStringLiteral("/race_roster.txt"));
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

// Exact face slug for an OFFICIAL piece name (faces.txt, or a slugified
// file). Empty when the name is unknown.
QString faceSlugExact(const QString &positionName)
{
    ensureMaps();
    const QString key = positionName.trimmed().toLower();
    if (key.isEmpty())
        return QString();
    const auto it = s_faceSlugs.constFind(key);
    if (it != s_faceSlugs.constEnd())
        return it.value();
    const QString slug = slugify(positionName);
    if (!slug.isEmpty() && QFile::exists(dir() + QStringLiteral("/faces/") + slug
                                                  + QStringLiteral(".png")))
        return slug;
    return QString();
}

QString faceSlug(const QString &positionName)
{
    const QString slug = faceSlugExact(positionName);
    return slug.isEmpty() ? QStringLiteral("human_lineman") : slug;
}

QPixmap playerFace(const QString &positionName, const QString &raceHint)
{
    const QString name = positionName.trimmed();
    // 1) Exact official piece name: "Orc Blitzer", "Dragon Prince", shared
    //    big guys ("Troll", "Ogre", "Minotaur", "Rat Ogre", ...).
    QString slug = faceSlugExact(name);
    // 2) Bare FUMBBL payload name ("Blitzer", "Zombie", "Wight", "Treeman",
    //    ...): resolve it against the authoritative BB2025 race roster
    //    (dadidimerda.it) for this team's race.
    if (slug.isEmpty() && !name.isEmpty() && !raceHint.trimmed().isEmpty()) {
        const QVector<RosterPiece> &pieces = racePieces(raceHint);
        const QString key = name.toLower();
        const RosterPiece *match = nullptr;
        // a) piece name or roster keyword equals the bare name (roster
        //    order: standard pieces come first).
        for (const RosterPiece &p : pieces) {
            if (p.name.toLower() == key || p.keywords.contains(key, Qt::CaseInsensitive)) {
                match = &p;
                break;
            }
        }
        // b) prefix fallback ("Wight" -> "Wight Blitzer").
        if (!match) {
            for (const RosterPiece &p : pieces) {
                if (p.name.toLower().startsWith(key)) {
                    match = &p;
                    break;
                }
            }
        }
        if (match)
            slug = faceSlugExact(match->name);
    }
    if (!slug.isEmpty())
        return loadPixmap(QStringLiteral("faces/%1.png").arg(slug));
    // 3) Custom-named star piece (not in any roster): the race's
    //    representative face.
    if (!raceHint.trimmed().isEmpty()) {
        const QString raceSlug = s_raceSlugs.value(raceHint.trimmed().toLower());
        if (!raceSlug.isEmpty())
            return loadPixmap(QStringLiteral("faces/%1.png").arg(raceSlug));
    }
    // 4) Last resort.
    return loadPixmap(QStringLiteral("faces/human_lineman.png"));
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
