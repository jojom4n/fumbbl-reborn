// =============================================================================
// Assets — implementation
// =============================================================================
#include "core/assets.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>

#include <algorithm>

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

QPixmap fieldTexture()
{
    return loadPixmap(QStringLiteral("field/field.png"));
}

QPixmap dugoutTexture()
{
    return loadPixmap(QStringLiteral("dugout/dugout.png"));
}

QPixmap dugoutSymbol(const QString &kind)
{
    return loadPixmap(QStringLiteral("dugout/sym_%1.png").arg(kind));
}

QPixmap dugoutPanel()
{
    return loadPixmap(QStringLiteral("dugout/panel.png"));
}

QPixmap blockHelmet()
{
    return loadPixmap(QStringLiteral("markers/helmet.png"));
}

QPixmap stadiumImage(const QString &name)
{
    // Prefer the 4K asset (stadium_4k.png, 3840px) — it stays crisp at any
    // zoom on 4K monitors; stadiums without one fall back to the base
    // resolution.
    const QString fourK = QStringLiteral("stadiums/%1/stadium_4k.png").arg(name);
    if (!dir().isEmpty() && QFile::exists(dir() + QStringLiteral("/") + fourK))
        return loadPixmap(fourK);
    return loadPixmap(QStringLiteral("stadiums/%1/stadium.png").arg(name));
}

StadiumManifest stadiumManifest(const QString &name)
{
    StadiumManifest m;
    const QString path = dir() + QStringLiteral("/stadiums/%1/stadium.json").arg(name);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return m;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return m;
    const QJsonObject o = doc.object();
    const auto readRect = [](const QJsonObject &o, const QString &key) -> QRectF {
        const QJsonObject r = o.value(key).toObject();
        return QRectF(r.value(QStringLiteral("x")).toDouble(),
                      r.value(QStringLiteral("y")).toDouble(),
                      r.value(QStringLiteral("w")).toDouble(),
                      r.value(QStringLiteral("h")).toDouble());
    };
    m.fieldPit = readRect(o, QStringLiteral("fieldPit"));
    m.dugoutHome = readRect(o, QStringLiteral("dugoutHome"));
    m.dugoutAway = readRect(o, QStringLiteral("dugoutAway"));
    const QJsonObject pr = o.value(QStringLiteral("pitRadius")).toObject();
    m.pitRadiusX = pr.value(QStringLiteral("x")).toDouble();
    m.pitRadiusY = pr.value(QStringLiteral("y")).toDouble();
    m.valid = !m.fieldPit.isNull();
    return m;
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

// ---------------------------------------------------------------------------
// keyBackground — generic flat-backdrop removal for team logos. The
// backdrop is NOT assumed white: it is estimated from the image border and
// removed by a border-connected flood fill, so it works for any flat color
// (or a user-uploaded logo with its own backdrop).
// ---------------------------------------------------------------------------
namespace {

struct Rgb { float r, g, b; };

float sqDist(const Rgb &a, const Rgb &b)
{
    const float dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}

bool imageHasAlpha(const QImage &im)
{
    // A real transparency channel: a meaningful fraction of pixels is not
    // fully opaque. Sampled (logos are small, but stay cheap on big ones).
    const int w = im.width(), h = im.height();
    int total = 0, transparent = 0;
    for (int y = 0; y < h; y += 3) {
        for (int x = 0; x < w; x += 3) {
            const QRgb px = im.pixel(x, y);
            ++total;
            if (qAlpha(px) < 250)
                ++transparent;
        }
    }
    return total > 0 && (transparent * 100) > total; // >10% sampled pixels
}

} // namespace

QImage keyBackground(QImage img)
{
    if (img.isNull())
        return img;
    // Already transparent (a real PNG alpha): nothing to key.
    if (imageHasAlpha(img))
        return img;

    img = img.convertToFormat(QImage::Format_ARGB32);
    const int w = img.width(), h = img.height();
    if (w < 4 || h < 4)
        return img;

    // 1) Estimate the backdrop color = MEDIAN of the border pixels (robust to
    //    a few outlier edge pixels). Border = the 4 outer edges.
    auto borderPix = [&](int x, int y) -> Rgb {
        const QRgb px = img.pixel(x, y);
        return Rgb{ float(qRed(px)), float(qGreen(px)), float(qBlue(px)) };
    };
    QVector<float> rs, gs, bs;
    auto push = [&](int x, int y) {
        const Rgb c = borderPix(x, y);
        rs.append(c.r); gs.append(c.g); bs.append(c.b);
    };
    for (int x = 0; x < w; ++x) { push(x, 0); push(x, h - 1); }
    for (int y = 0; y < h; ++y) { push(0, y); push(w - 1, y); }
    auto median = [](QVector<float> v) -> float {
        std::sort(v.begin(), v.end());
        return v.isEmpty() ? 0.0f : v[v.size() / 2];
    };
    const Rgb bg{ median(rs), median(gs), median(bs) };

    // 2) Guard: the border must be a UNIFORM flat color (a real backdrop),
    //    otherwise the logo likely touches the edges / it's a photo -> skip.
    const float uniformTol2 = 34.0f * 34.0f;
    const auto within = [&](int x, int y) { return sqDist(borderPix(x, y), bg) <= uniformTol2; };
    int uniformCount = 0, total = 0;
    for (int x = 0; x < w; ++x) { if (within(x, 0)) ++uniformCount; ++total; if (within(x, h - 1)) ++uniformCount; ++total; }
    for (int y = 0; y < h; ++y) { if (within(0, y)) ++uniformCount; ++total; if (within(w - 1, y)) ++uniformCount; ++total; }
    if (total == 0 || (uniformCount * 100) < (total * 60)) // <60% of border flat
        return img;

    // 3) Tolerance for the flood fill: a bit beyond the uniformity window.
    const float tol2 = 48.0f * 48.0f;

    // 4) Border-connected flood fill -> transparent.
    const int n = w * h;
    QVector<quint8> visited(n, 0);
    QVector<int> queue;
    queue.reserve(1024);
    auto seed = [&](int x, int y) {
        const int i = y * w + x;
        if (!visited[i] && within(x, y)) {
            visited[i] = 1;
            queue.append(i);
        }
    };
    for (int x = 0; x < w; ++x) { seed(x, 0); seed(x, h - 1); }
    for (int y = 0; y < h; ++y) { seed(0, y); seed(w - 1, y); }

    QRgb *bits = reinterpret_cast<QRgb *>(img.bits());
    for (int q = 0; q < queue.size(); ++q) {
        const int i = queue[q];
        const int x = i % w, y = i / w;
        bits[i] = bits[i] & 0x00FFFFFFu; // alpha = 0
        const int nx[4] = { x - 1, x + 1, x, x };
        const int ny[4] = { y, y, y - 1, y + 1 };
        for (int k = 0; k < 4; ++k) {
            const int a = nx[k], b = ny[k];
            if (a < 0 || a >= w || b < 0 || b >= h)
                continue;
            const int j = b * w + a;
            if (visited[j])
                continue;
            const QRgb px = bits[j];
            const Rgb c{ float(qRed(px)), float(qGreen(px)), float(qBlue(px)) };
            if (sqDist(c, bg) <= tol2) {
                visited[j] = 1;
                queue.append(j);
            }
        }
    }
    return img;
}

} // namespace assets
