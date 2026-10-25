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

    // playerId -> display info (from both rosters)
    struct Info { QString name; int nr = 0; QString pos; };
    QHash<QString, Info> hinfo, ainfo;
    const auto fill = [](const QJsonObject &team, QHash<QString, Info> &info) {
        for (const QJsonValue &pv : team.value(QStringLiteral("playerArray")).toArray()) {
            const QJsonObject p = pv.toObject();
            Info i;
            i.name = p.value(QStringLiteral("playerName")).toString();
            i.nr = p.value(QStringLiteral("playerNr")).toInt();
            info.insert(p.value(QStringLiteral("playerId")).toString(), i);
        }
    };
    fill(th, hinfo);
    fill(ta, ainfo);

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
    for (const QJsonValue &dv : fm.value(QStringLiteral("playerDataArray")).toArray()) {
        const QJsonObject pd = dv.toObject();
        const QString id = pd.value(QStringLiteral("playerId")).toString();
        const QJsonArray coord = pd.value(QStringLiteral("playerCoordinate")).toArray();
        if (coord.size() < 2)
            continue;
        const int x = coord.at(0).toInt(), y = coord.at(1).toInt();
        if (x < 0 || x > 25 || y < 0 || y > 14)
            continue; // box coordinate
        FieldView::PlayerToken t;
        t.playerId = id;
        t.state = pd.value(QStringLiteral("playerState")).toInt();
        t.x = x;
        t.y = y;
        t.isActing = (id == activeId);
        if (auto it = hinfo.constFind(id); it != hinfo.constEnd()) {
            t.name = it->name;
            t.number = it->nr;
            home.append(t);
        } else if (auto ita = ainfo.constFind(id); ita != ainfo.constEnd()) {
            t.name = ita->name;
            t.number = ita->nr;
            away.append(t);
        }
    }
    field.setPlayers(home, away);

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
