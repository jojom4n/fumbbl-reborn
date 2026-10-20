// =============================================================================
// ReportParser — implementation (see reportparser.h for references)
// =============================================================================
#include "core/reportparser.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>

namespace {

// Palette (mirrors the TS tailwind classes, adapted to the dark theme).
const QColor kGreen(0x6f, 0xbf, 0x73);   // success
const QColor kRed(0xd8, 0x7a, 0x7a);     // failure
const QColor kYellow(0xd8, 0xc0, 0x7a);  // partial
const QColor kGray(0xa0, 0xa0, 0xa0);    // system
const QColor kLightGray(0xb8, 0xb8, 0xb8);
const QColor kOrange(0xd8, 0x9a, 0x5a);  // foul / armor broken
const QColor kCyan(0x6f, 0xc3, 0xd8);    // weather / leader / skill
const QColor kBlue(0x7a, 0x9f, 0xd8);    // blitz / reroll
const QColor kPurple(0xb0, 0x8f, 0xd8);  // fans

QString enumName(const QJsonValue &v)
{
    if (v.isObject())
        return v.toObject().value(QStringLiteral("name")).toString();
    if (v.isString())
        return v.toString();
    return QString();
}

QString statLabel(const QJsonObject &r)
{
    const QJsonValue s = r.value(QStringLiteral("statBasedRollModifier"));
    if (s.isObject()) {
        const QJsonObject so = s.toObject();
        const QString stat = so.value(QStringLiteral("stat")).isString()
                                 ? so.value(QStringLiteral("stat")).toString()
                                 : enumName(so.value(QStringLiteral("stat")));
        if (stat.isEmpty())
            return QString();
        return QStringLiteral("%1: %2").arg(stat).arg(so.value(QStringLiteral("value")).toInt());
    }
    return QString();
}

QString modifierLabel(const QJsonObject &r)
{
    const QJsonArray m = r.value(QStringLiteral("rollModifiers")).toArray();
    if (m.isEmpty())
        return QString();
    QStringList names;
    for (const QJsonValue &x : m)
        names << (x.isObject() ? x.toObject().value(QStringLiteral("name")).toString() : x.toString());
    return QStringLiteral(" (%1)").arg(names.join(QStringLiteral(", ")));
}

// "✓ [3] vs 5 [RR]"
QString formatRoll(int roll, int minRoll, bool ok, bool reRolled)
{
    QString s;
    s += ok ? QStringLiteral("✓ [") : QStringLiteral("✗ [");
    s += QString::number(roll);
    s += QStringLiteral("] vs ");
    s += QString::number(minRoll);
    if (reRolled)
        s += QStringLiteral(" [RR]");
    return s;
}

// Appends "  ST: 4 (modifier)" style stat/modifier suffix.
QString suffix(const QJsonObject &r)
{
    const QString sl = statLabel(r);
    const QString fm = modifierLabel(r);
    if (sl.isEmpty() && fm.isEmpty())
        return QString();
    return QStringLiteral("%1%2").arg(sl).arg(fm);
}

QString pid(const QJsonObject &r) { return r.value(QStringLiteral("playerId")).toString(); }
QString did(const QJsonObject &r) { return r.value(QStringLiteral("defenderId")).toString(); }
QString tid(const QJsonObject &r) { return r.value(QStringLiteral("choosingTeamId")).toString(); }

int roll(const QJsonObject &r) { return r.value(QStringLiteral("roll")).toInt(); }
int minRoll(const QJsonObject &r) { return r.value(QStringLiteral("minimumRoll")).toInt(); }
bool ok(const QJsonObject &r) { return r.value(QStringLiteral("successful")).toBool(); }
bool reRolled(const QJsonObject &r) { return r.value(QStringLiteral("reRolled")).toBool(); }

} // namespace

QVector<DiceLogLine> parseReport(const QJsonObject &report)
{
    QVector<DiceLogLine> out;
    const QString id = report.value(QStringLiteral("reportId")).toString();
    if (id.isEmpty())
        return out;

    auto push = [&](const QString &text, const QColor &color) {
        DiceLogLine l;
        l.text = text;
        l.color = color;
        l.playerId = pid(report); // may be empty for team-level reports
        out.append(l);
    };

    // Generic single-die skill roll: "<pid> <Name> ✓ [3] vs 5 [stat][mods]".
    auto sr = [&](const QString &name, const QColor &okColor, const QColor &failColor) {
        const QString s = suffix(report);
        const QString text = QStringLiteral("%1 %2 %3%4")
                                 .arg(pid(report))
                                 .arg(name)
                                 .arg(formatRoll(roll(report), minRoll(report), ok(report), reRolled(report)))
                                 .arg(s.isEmpty() ? QString() : QStringLiteral(" %1").arg(s));
        push(text, ok(report) ? okColor : failColor);
    };

    // The long list of plain skill rolls (TS reference).
    static const QHash<QString, std::pair<QString, int>> kSimpleRolls = {
        { "dodgeRoll", { "Dodge", 0 } },
        { "catchRoll", { "Catch", 0 } },
        { "escapeRoll", { "Escape", 0 } },
        { "leapRoll", { "Leap", 0 } },
        { "goForItRoll", { "Go For It", 0 } },
        { "pickUpRoll", { "Pick Up", 0 } },
        { "interceptionRoll", { "Intercept", 0 } },
        { "standUpRoll", { "Stand Up", 1 } },
        { "jumpUpRoll", { "Jump Up", 0 } },
        { "regenerationRoll", { "Regeneration", 1 } },
        { "rightStuffRoll", { "Right Stuff", 2 } },
        { "bloodLustRoll", { "Blood Lust", 1 } },
        { "dauntlessRoll", { "Dauntless", 0 } },
        { "solidDefenceRoll", { "Solid Defence", 2 } },
        { "quickSnapRoll", { "Quick Snap", 1 } },
        { "foulAppearanceRoll", { "Foul Appearance", 1 } },
        { "hypnoticGazeRoll", { "Hypnotic Gaze", 0 } },
        { "lookIntoMyEyesRoll", { "Look Into My Eyes", 0 } },
        { "steadyFootingRoll", { "Steady Footing", 0 } },
        { "dwarfenWisdomRoll", { "Dwarfen Wisdom", 3 } },
        { "gettingEvenRoll", { "Getting Even", 3 } },
        { "saboteurRoll", { "Saboteur", 4 } },
        { "chompRoll", { "Chomp", 4 } },
        { "dodgySnackRoll", { "Dodgy Snack", 4 } },
        { "weatherMageRoll", { "Weather Mage", 3 } },
        { "apothecaryRoll", { "Apothecary", 1 } },
    };
    // color kind: 0 green/red, 1 green/gray, 2 green/yellow, 3 cyan, 4 orange
    if (const auto it = kSimpleRolls.constFind(id); it != kSimpleRolls.constEnd()) {
        const QString &name = it.value().first;
        switch (it.value().second) {
        case 0: sr(name, kGreen, kRed); break;
        case 1: sr(name, kGreen, kGray); break;
        case 2: sr(name, kGreen, kYellow); break;
        case 3: sr(name, kCyan, kRed); break;
        case 4: sr(name, kOrange, kRed); break;
        }
        return out;
    }

    const char first = id.isEmpty() ? 0 : id.at(0).toLatin1();
    switch (first) {
    case 'b':
        if (id == QLatin1String("blitzRoll")) {
            const QString s = suffix(report);
            push(QStringLiteral("%1 Blitz %2%3")
                     .arg(pid(report))
                     .arg(formatRoll(roll(report), minRoll(report), ok(report), reRolled(report)))
                     .arg(s.isEmpty() ? QString() : QStringLiteral(" %1").arg(s)),
                 kBlue);
        } else if (id == QLatin1String("blockRoll")) {
            const QJsonArray d = report.value(QStringLiteral("blockRoll")).toArray();
            const QString t = QStringLiteral("%1 chooses defender%2")
                                  .arg(tid(report))
                                  .arg(did(report).isEmpty() ? QString()
                                                            : QStringLiteral(" → %1").arg(did(report)));
            push(t, kYellow);
        } else if (id == QLatin1String("block")) {
            const QString a = report.value(QStringLiteral("attackerId")).toString();
            push(QStringLiteral("Block: %1 vs %2").arg(a).arg(did(report)), kYellow);
        } else if (id == QLatin1String("bribesRoll")) {
            const QJsonArray d = report.value(QStringLiteral("rolls")).toArray();
            const int r0 = d.isEmpty() ? 0 : d.at(0).toInt();
            push(QStringLiteral("Bribes %1").arg(formatRoll(r0, minRoll(report), ok(report), reRolled(report))),
                 kYellow);
        }
        break;

    case 'p':
        if (id == QLatin1String("passRoll")) {
            const QString s = suffix(report);
            const QString pr = enumName(report.value(QStringLiteral("passResult")));
            QString extra;
            if (report.value(QStringLiteral("bomb")).toBool())
                extra += QStringLiteral(" [BOMB]");
            if (report.value(QStringLiteral("hailMaryPass")).toBool())
                extra += QStringLiteral(" [Hail Mary]");
            if (!pr.isEmpty())
                extra += QStringLiteral(" (%1)").arg(pr);
            push(QStringLiteral("%1 Pass %2%3%4")
                     .arg(pid(report))
                     .arg(formatRoll(roll(report), minRoll(report), ok(report), reRolled(report)))
                     .arg(s.isEmpty() ? QString() : QStringLiteral(" %1").arg(s))
                     .arg(extra),
                 ok(report) ? kGreen : kRed);
        } else if (id == QLatin1String("prayerRoll")) {
            const QString pn = enumName(report.value(QStringLiteral("prayer")));
            push(QStringLiteral("%1 Prayer: %2 %3")
                     .arg(tid(report))
                     .arg(pn.isEmpty() ? QStringLiteral("?") : pn)
                     .arg(formatRoll(roll(report), minRoll(report), ok(report), reRolled(report))),
                 ok(report) ? kGreen : kRed);
        } else if (id == QLatin1String("pilingOn")) {
            const QJsonArray d = report.value(QStringLiteral("rolls")).toArray();
            const int r0 = d.isEmpty() ? 0 : d.at(0).toInt();
            push(QStringLiteral("%1 Piling On %2")
                     .arg(pid(report))
                     .arg(formatRoll(r0, minRoll(report), report.contains("successful") ? ok(report) : true,
                                     reRolled(report))),
                 kYellow);
        } else if (id == QLatin1String("playerAction")) {
            const QString an = enumName(report.value(QStringLiteral("playerAction")));
            push(QStringLiteral("%1 %2").arg(pid(report)).arg(an.isEmpty() ? QStringLiteral("acted") : an),
                 kLightGray);
        }
        break;

    case 'i':
        if (id == QLatin1String("injury")) {
            const QString iType = enumName(report.value(QStringLiteral("injuryType")));
            const QJsonArray ar = report.value(QStringLiteral("armorRoll")).toArray();
            const QJsonArray ir = report.value(QStringLiteral("injuryRoll")).toArray();
            const QJsonArray cr = report.value(QStringLiteral("casualtyRoll")).toArray();
            const bool armorBroken = report.value(QStringLiteral("armorBroken")).toBool();
            const QString ist = enumName(report.value(QStringLiteral("injury")));
            const QString si = enumName(report.value(QStringLiteral("seriousInjury")));
            if (!ar.isEmpty())
                push(QStringLiteral("%1 Armor %2").arg(did(report)).arg(armorBroken ? QStringLiteral("BROKEN!") : QStringLiteral("holds")),
                     armorBroken ? kOrange : kGray);
            if (!ir.isEmpty()) {
                QString t = QStringLiteral("%1 Injury").arg(did(report));
                if (!iType.isEmpty())
                    t += QStringLiteral(" (%1)").arg(iType);
                push(t, kYellow);
            }
            if (!cr.isEmpty())
                push(QStringLiteral("%1 is %2!%3")
                         .arg(did(report))
                         .arg(ist.isEmpty() ? QStringLiteral("out") : ist)
                         .arg(si.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(si)),
                     kRed);
            if (ar.isEmpty() && ir.isEmpty() && cr.isEmpty())
                push(QStringLiteral("%1 is %2!%3")
                         .arg(did(report))
                         .arg(ist.isEmpty() ? QStringLiteral("out") : ist)
                         .arg(si.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(si)),
                     kRed);
        }
        break;

    case 't':
        if (id == QLatin1String("turnEnd"))
            push(QStringLiteral("--- Turn ended ---"), kGray);
        else if (id == QLatin1String("startHalf"))
            push(QStringLiteral("--- Half %1 starts ---").arg(report.value(QStringLiteral("half")).toInt()),
                 kGray);
        else if (id == QLatin1String("throwIn"))
            push(QStringLiteral("%1 Throw-In").arg(tid(report)), kGray);
        else if (id == QLatin1String("teamCaptainRoll"))
            push(QStringLiteral("%1 Team Captain %2")
                     .arg(tid(report))
                     .arg(formatRoll(roll(report), minRoll(report), ok(report), reRolled(report))),
                 kCyan);
        else if (id == QLatin1String("timeoutEnforced"))
            push(QStringLiteral("Timeout enforced for %1!").arg(tid(report)), kRed);
        break;

    case 'f':
        if (id == QLatin1String("foul"))
            push(QStringLiteral("%1 commits a Foul!%2")
                     .arg(pid(report))
                     .arg(report.value(QStringLiteral("foulUsed")).toBool() ? QStringLiteral(" (Foul used)") : QString()),
                 kOrange);
        break;

    case 's':
        if (id == QLatin1String("skillUse")) {
            const QString sn = enumName(report.value(QStringLiteral("skill")));
            const QString su = enumName(report.value(QStringLiteral("skillUse")));
            push(QStringLiteral("%1 uses %2%3")
                     .arg(pid(report))
                     .arg(sn.isEmpty() ? QStringLiteral("skill") : sn)
                     .arg(su.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(su)),
                 kCyan);
        } else if (id == QLatin1String("scatterBall")) {
            const QJsonObject c = report.value(QStringLiteral("coordinate")).toObject();
            push(QStringLiteral("Ball scattered to [%1, %2]")
                     .arg(c.value(QStringLiteral("x")).toInt())
                     .arg(c.value(QStringLiteral("y")).toInt()),
                 kGray);
        } else if (id == QLatin1String("spectators"))
            push(QStringLiteral("Spectators: %1").arg(report.value(QStringLiteral("spectators")).toInt()), kPurple);
        break;

    case 'c':
        if (id == QLatin1String("confusionRoll")) {
            const QString s = suffix(report);
            push(QStringLiteral("Confusion %1%2")
                     .arg(formatRoll(roll(report), minRoll(report), ok(report), reRolled(report)))
                     .arg(s.isEmpty() ? QString() : QStringLiteral(" %1").arg(s)),
                 ok(report) ? kGreen : kRed);
        } else if (id == QLatin1String("coinThrow"))
            push(QStringLiteral("Coin Toss: %1")
                     .arg(report.value(QStringLiteral("coinThrowHeads")).toBool() ? QStringLiteral("Heads") : QStringLiteral("Tails")),
                 kGray);
        break;

    case 'w':
        if (id == QLatin1String("weather") || id == QLatin1String("weatherMageResult")) {
            const QString w = enumName(report.value(QStringLiteral("weather")));
            push(QStringLiteral("Weather: %1").arg(w.isEmpty() ? QStringLiteral("Unknown") : w), kCyan);
        } else if (id == QLatin1String("winnings"))
            push(QStringLiteral("=== Game Over ==="), kGray);
        break;

    case 'r':
        if (id == QLatin1String("reRoll"))
            push(QStringLiteral("%1 uses a Re-Roll").arg(pid(report)), kBlue);
        break;

    case 'l':
        if (id == QLatin1String("leader"))
            push(QStringLiteral("%1 uses Leader").arg(pid(report)), kCyan);
        break;

    case 'g':
        if (id == QLatin1String("gameOver"))
            push(QStringLiteral("=== Game Over ==="), kGray);
        break;

    default:
        // Unknown report: keep it visible but neutral (nothing is silently lost).
        push(QStringLiteral("[%1] %2")
                 .arg(id)
                 .arg(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Compact)).mid(0, 200)),
             kGray);
        break;
    }

    return out;
}
