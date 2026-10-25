// =============================================================================
// field_preview — offscreen render of the FieldView for visual verification.
//
// Feeds a FieldView from a real serverGameState JSON fixture (the same format
// the client receives) and saves a PNG:
//
//   ./build/field_preview <fixture.json> [out.png] [weatherOverride]
//
// The weather override (e.g. "Pouring Rain") previews the rain overlay
// without waiting for a rainy match.
// =============================================================================
#include <QApplication>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPixmap>

#include "ui/fieldview.h"

#include <cstdio>

static QJsonObject gameObject(const QByteArray &raw)
{
    return QJsonDocument::fromJson(raw).object().value(QStringLiteral("game")).toObject();
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    if (argc < 2) {
        fprintf(stderr, "usage: field_preview <fixture.json> [out.png] [weatherOverride]\n");
        return 1;
    }
    QFile f(argv[1]);
    if (!f.open(QIODevice::ReadOnly)) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    const QJsonObject g = gameObject(f.readAll());
    const QJsonObject th = g.value(QStringLiteral("teamHome")).toObject();
    const QJsonObject ta = g.value(QStringLiteral("teamAway")).toObject();
    const QJsonObject fm = g.value(QStringLiteral("fieldModel")).toObject();

    // positionId -> position name (from each roster's positionArray)
    auto positionMap = [](const QJsonObject &team) {
        QHash<QString, QString> m;
        const QJsonArray positions =
            team.value(QStringLiteral("roster")).toObject()
                .value(QStringLiteral("positionArray")).toArray();
        for (const QJsonValue &pv : positions) {
            const QJsonObject pos = pv.toObject();
            m.insert(pos.value(QStringLiteral("positionId")).toString(),
                     pos.value(QStringLiteral("positionName")).toString());
        }
        return m;
    };
    const QHash<QString, QString> hpos = positionMap(th);
    const QHash<QString, QString> apos = positionMap(ta);

    // playerId -> display info (from both rosters)
    struct Info { QString name; int nr = 0; QString pos; };
    QHash<QString, Info> hinfo, ainfo;
    const auto fill = [](const QJsonObject &team, QHash<QString, Info> &info,
                         const QHash<QString, QString> &posMap) {
        for (const QJsonValue &pv : team.value(QStringLiteral("playerArray")).toArray()) {
            const QJsonObject p = pv.toObject();
            Info i;
            i.name = p.value(QStringLiteral("playerName")).toString();
            i.nr = p.value(QStringLiteral("playerNr")).toInt();
            i.pos = posMap.value(p.value(QStringLiteral("positionId")).toString());
            info.insert(p.value(QStringLiteral("playerId")).toString(), i);
        }
    };
    fill(th, hinfo, hpos);
    fill(ta, ainfo, apos);

    FieldView field;
    field.setHomeTeamName(th.value(QStringLiteral("teamName")).toString());
    field.setAwayTeamName(ta.value(QStringLiteral("teamName")).toString());
    field.setHomeColor(QColor(0x34, 0x6c, 0xb4));
    field.setAwayColor(QColor(0xc1, 0x98, 0x37));
    field.setHomeRace(th.value(QStringLiteral("race")).toString());
    field.setAwayRace(ta.value(QStringLiteral("race")).toString());

    const QString activeId = g.value(QStringLiteral("actingPlayer")).toObject()
                                  .value(QStringLiteral("playerId")).toString();

    QVector<FieldView::PlayerToken> home, away;
    QVector<FieldView::BoxEntry> homeBox, awayBox;
    for (const QJsonValue &dv : fm.value(QStringLiteral("playerDataArray")).toArray()) {
        const QJsonObject pd = dv.toObject();
        const QString id = pd.value(QStringLiteral("playerId")).toString();
        const QJsonArray coord = pd.value(QStringLiteral("playerCoordinate")).toArray();
        if (coord.size() < 2)
            continue;
        const int x = coord.at(0).toInt(), y = coord.at(1).toInt();
        const bool isHome = hinfo.contains(id);
        if (!isHome && !ainfo.contains(id))
            continue;
        // Box coordinate (x < 0 home box, x > 25 away box): bench strip entry.
        if (x < 0 || x > 25 || y < 0 || y > 14) {
            FieldView::BoxEntry e;
            e.state = pd.value(QStringLiteral("playerState")).toInt();
            if (isHome) {
                e.name = hinfo[id].name;
                e.positionName = hinfo[id].pos;
                e.race = th.value(QStringLiteral("race")).toString();
                homeBox.append(e);
            } else {
                e.name = ainfo[id].name;
                e.positionName = ainfo[id].pos;
                e.race = ta.value(QStringLiteral("race")).toString();
                awayBox.append(e);
            }
            continue;
        }
        FieldView::PlayerToken t;
        t.playerId = id;
        t.state = pd.value(QStringLiteral("playerState")).toInt();
        t.x = x;
        t.y = y;
        t.isActing = (id == activeId);
        if (isHome) {
            t.name = hinfo[id].name;
            t.number = hinfo[id].nr;
            t.positionName = hinfo[id].pos;
            home.append(t);
        } else {
            t.name = ainfo[id].name;
            t.number = ainfo[id].nr;
            t.positionName = ainfo[id].pos;
            away.append(t);
        }
    }
    field.setPlayers(home, away);
    field.setBoxes(homeBox, awayBox);

    const QJsonArray ball = fm.value(QStringLiteral("ballCoordinate")).toArray();
    if (ball.size() == 2)
        field.setBall(ball.at(0).toInt(), ball.at(1).toInt(),
                      fm.value(QStringLiteral("ballInPlay")).toBool(true));
    field.setTurn(g.value(QStringLiteral("turnDataHome")).toObject()
                      .value(QStringLiteral("turnNr")).toInt(1));
    field.setWeather(fm.value(QStringLiteral("weather")).toString());
    if (argc > 3)
        field.setWeather(QString::fromLocal8Bit(argv[3]));

    field.resize(1344, 734); // 26:15
    field.show();
    QApplication::processEvents();

    const QString out = argc > 2 ? QString::fromLocal8Bit(argv[2])
                                 : QStringLiteral("field_preview.png");
    QPixmap pm = field.grab();
    if (!pm.save(out)) {
        fprintf(stderr, "save failed: %s\n", out.toUtf8().constData());
        return 1;
    }
    printf("saved %s (%dx%d) home=%d away=%d ball=%d weather=%s\n",
           out.toUtf8().constData(), pm.width(), pm.height(),
           field.playerCount(),
           (int)(ball.size() == 2 ? 1 : 0),
           fm.value(QStringLiteral("weather")).toString().toUtf8().constData());
    return 0;
}
