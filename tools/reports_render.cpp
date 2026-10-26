// =============================================================================
// reports_render — feed captured raw reports (reports_capturer .jsonl)
// through the REAL parser + model and print the rendered action-log lines.
//
// Usage: ./reports_render <game_state.json> <reports.jsonl>
// =============================================================================
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include "core/gamemodel.h"
#include "core/reportparser.h"

static QString resolve(const GameModel *m, const QString &id)
{
    if (id.isEmpty() || !m)
        return id;
    if (const FFBPlayer *p = m->findPlayerById(id))
        return p->number > 0 ? QStringLiteral("%1 (%2)").arg(p->name).arg(p->number)
                             : p->name;
    if (id == m->teamHomeId())
        return m->teamHomeName();
    if (id == m->teamAwayId())
        return m->teamAwayName();
    bool numeric = !id.isEmpty();
    for (const QChar &c : id)
        numeric = numeric && c.isDigit();
    return numeric ? QString() : id; // same rule as MainWindow
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    if (argc < 3) {
        QTextStream out(stdout);
        out << "usage: reports_render <game_state.json> <reports.jsonl>\n";
        return 2;
    }
    QFile sf(argv[1]);
    if (!sf.open(QIODevice::ReadOnly))
        return 2;
    const QJsonObject game =
        QJsonDocument::fromJson(sf.readAll()).object();

    GameModel model;
    if (!model.initialize(game))
        return 2;

    QFile rf(argv[2]);
    if (!rf.open(QIODevice::ReadOnly))
        return 2;

    QTextStream out(stdout);
    int n = 0;
    while (!rf.atEnd()) {
        const QByteArray line = rf.readLine();
        const QJsonObject report = QJsonDocument::fromJson(line).object();
        if (report.isEmpty())
            continue;
        for (const DiceLogLine &l : parseReport(report)) {
            QString t = QStringLiteral("%1 %2").arg(resolve(&model, l.actorId),
                                                   l.text).trimmed();
            if (!l.defenderId.isEmpty())
                t += QStringLiteral(" -> %1").arg(resolve(&model, l.defenderId));
            QString dice;
            for (const QString &f : l.blockFaces)
                dice += QStringLiteral("[B%1]").arg(f);
            if (!l.d6Rolls.isEmpty())
                for (const QString &f : l.d6Rolls)
                    dice += QStringLiteral("[d%1]").arg(f);
            else if (l.d6Roll > 0)
                dice += QStringLiteral("[d%1]").arg(l.d6Roll);
            if (l.d6Target > 0)
                dice += QStringLiteral(" vs%1").arg(l.d6Target);
            if (l.d6ShowResult)
                dice += l.d6Ok ? " OK" : " FAIL";
            if (!dice.isEmpty())
                t += dice;
            out << t << '\n';
            ++n;
        }
    }
    out << QStringLiteral("---- %1 lines from %2 reports ----\n")
            .arg(n)
            .arg(argc > 3 ? argv[3] : QStringLiteral("?"));
    return 0;
}
