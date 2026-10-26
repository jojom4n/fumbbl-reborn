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

    // Fills the common fields of a new line. `text` may still contain raw id
    // tokens: the UI resolves actorId/defenderId against the model and
    // substitutes the display names before rendering.
    auto push = [&](const QString &text, const QColor &color) {
        DiceLogLine l;
        l.text = text;
        l.color = color;
        l.actorId = pid(report); // may be empty for team-level reports
        out.append(l);
    };

    // Generic single-die skill roll: actor rolls a d6 against a target.
    auto sr = [&](const QString &name, const QColor &okColor, const QColor &failColor) {
        DiceLogLine l;
        const QString suf = suffix(report);
        l.text = suf.isEmpty() ? name
                               : QStringLiteral("%1  %2").arg(name, suf);
        l.color = ok(report) ? okColor : failColor;
        l.actorId = pid(report);
        l.d6Roll = roll(report);
        l.d6Target = minRoll(report);
        l.d6Ok = ok(report);
        l.d6ReRolled = reRolled(report);
        out.append(l);
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
            sr(QStringLiteral("Blitz"), kBlue, kBlue);
        } else if (id == QLatin1String("blockRoll")) {
            DiceLogLine l;
            l.text = QStringLiteral("chooses defender");
            l.color = kYellow;
            l.actorId = tid(report);
            l.defenderId = did(report);
            out.append(l);
        } else if (id == QLatin1String("block")) {
            // The six block dice: the star of the action log (Mockup F).
            DiceLogLine l;
            l.text = QStringLiteral("Block");
            l.color = kYellow;
            l.actorId = report.value(QStringLiteral("attackerId")).toString();
            l.defenderId = did(report);
            const QJsonArray d = report.value(QStringLiteral("blockRoll")).toArray();
            for (const QJsonValue &v : d)
                l.blockFaces << QString::number(v.toInt());
            out.append(l);
        } else if (id == QLatin1String("bribesRoll")) {
            DiceLogLine l;
            const QJsonArray d = report.value(QStringLiteral("rolls")).toArray();
            l.text = QStringLiteral("Bribes");
            l.color = kYellow;
            l.actorId = pid(report);
            l.d6Roll = d.isEmpty() ? 0 : d.at(0).toInt();
            l.d6Target = minRoll(report);
            l.d6Ok = ok(report);
            l.d6ReRolled = reRolled(report);
            out.append(l);
        }
        break;

    case 'p':
        if (id == QLatin1String("passRoll")) {
            DiceLogLine l;
            const QString suf = suffix(report);
            const QString pr = enumName(report.value(QStringLiteral("passResult")));
            QString extra;
            if (report.value(QStringLiteral("bomb")).toBool())
                extra += QStringLiteral(" [BOMB]");
            if (report.value(QStringLiteral("hailMaryPass")).toBool())
                extra += QStringLiteral(" [Hail Mary]");
            if (!pr.isEmpty())
                extra += QStringLiteral(" (%1)").arg(pr);
            l.text = QStringLiteral("Pass%1%2")
                         .arg(extra)
                         .arg(suf.isEmpty() ? QString() : QStringLiteral("  %1").arg(suf));
            l.color = ok(report) ? kGreen : kRed;
            l.actorId = pid(report);
            l.d6Roll = roll(report);
            l.d6Target = minRoll(report);
            l.d6Ok = ok(report);
            l.d6ReRolled = reRolled(report);
            out.append(l);
        } else if (id == QLatin1String("prayerRoll")) {
            DiceLogLine l;
            const QString pn = enumName(report.value(QStringLiteral("prayer")));
            l.text = QStringLiteral("Prayer: %1")
                         .arg(pn.isEmpty() ? QStringLiteral("?") : pn);
            l.color = ok(report) ? kGreen : kRed;
            l.actorId = tid(report); // team id
            l.d6Roll = roll(report);
            l.d6Target = minRoll(report);
            l.d6Ok = ok(report);
            l.d6ReRolled = reRolled(report);
            out.append(l);
        } else if (id == QLatin1String("pilingOn")) {
            DiceLogLine l;
            const QJsonArray d = report.value(QStringLiteral("rolls")).toArray();
            l.text = QStringLiteral("Piling On");
            l.color = kYellow;
            l.actorId = pid(report);
            l.d6Roll = d.isEmpty() ? 0 : d.at(0).toInt();
            l.d6Target = 0; // piling-on success has no printed threshold
            l.d6Ok = report.contains(QStringLiteral("successful")) ? ok(report) : true;
            l.d6ReRolled = reRolled(report);
            out.append(l);
        } else if (id == QLatin1String("playerAction")) {
            const QString an = enumName(report.value(QStringLiteral("playerAction")));
            push(QStringLiteral("%1").arg(an.isEmpty() ? QStringLiteral("acted") : an),
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
            auto victim = [&](DiceLogLine &l) { l.actorId = did(report); };
            if (!ar.isEmpty()) {
                DiceLogLine l;
                l.text = armorBroken ? QStringLiteral("Armor BROKEN!")
                                     : QStringLiteral("Armor holds");
                l.color = armorBroken ? kOrange : kGray;
                victim(l);
                out.append(l);
            }
            if (!ir.isEmpty()) {
                DiceLogLine l;
                l.text = iType.isEmpty() ? QStringLiteral("Injury")
                                         : QStringLiteral("Injury (%1)").arg(iType);
                l.color = kYellow;
                victim(l);
                out.append(l);
            }
            if (!cr.isEmpty()) {
                DiceLogLine l;
                l.text = QStringLiteral("is %1!%2")
                             .arg(ist.isEmpty() ? QStringLiteral("out") : ist)
                             .arg(si.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(si));
                l.color = kRed;
                victim(l);
                out.append(l);
            }
            if (ar.isEmpty() && ir.isEmpty() && cr.isEmpty()) {
                DiceLogLine l;
                l.text = QStringLiteral("is %1!%2")
                             .arg(ist.isEmpty() ? QStringLiteral("out") : ist)
                             .arg(si.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(si));
                l.color = kRed;
                victim(l);
                out.append(l);
            }
        }
        break;

    case 't':
        if (id == QLatin1String("turnEnd"))
            push(QStringLiteral("--- Turn ended ---"), kGray);
        else if (id == QLatin1String("startHalf"))
            push(QStringLiteral("--- Half %1 starts ---").arg(report.value(QStringLiteral("half")).toInt()),
                 kGray);
        else if (id == QLatin1String("throwIn"))
            push(QStringLiteral("Throw-In"), kGray);
        else if (id == QLatin1String("teamCaptainRoll")) {
            DiceLogLine l;
            l.text = QStringLiteral("Team Captain");
            l.color = kCyan;
            l.actorId = tid(report); // team id
            l.d6Roll = roll(report);
            l.d6Target = minRoll(report);
            l.d6Ok = ok(report);
            l.d6ReRolled = reRolled(report);
            out.append(l);
        } else if (id == QLatin1String("timeoutEnforced"))
            push(QStringLiteral("Timeout enforced for %1!").arg(tid(report)), kRed);
        break;

    case 'f':
        if (id == QLatin1String("foul"))
            push(QStringLiteral("commits a Foul!%1")
                     .arg(report.value(QStringLiteral("foulUsed")).toBool()
                              ? QStringLiteral(" (Foul used)")
                              : QString()),
                 kOrange);
        break;

    case 's':
        if (id == QLatin1String("skillUse")) {
            const QString sn = enumName(report.value(QStringLiteral("skill")));
            const QString su = enumName(report.value(QStringLiteral("skillUse")));
            push(QStringLiteral("uses %1%2")
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
            DiceLogLine l;
            const QString suf = suffix(report);
            l.text = suf.isEmpty() ? QStringLiteral("Confusion")
                                   : QStringLiteral("Confusion  %1").arg(suf);
            l.color = ok(report) ? kGreen : kRed;
            l.actorId = pid(report);
            l.d6Roll = roll(report);
            l.d6Target = minRoll(report);
            l.d6Ok = ok(report);
            l.d6ReRolled = reRolled(report);
            out.append(l);
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
            push(QStringLiteral("uses a Re-Roll"), kBlue);
        break;

    case 'l':
        if (id == QLatin1String("leader"))
            push(QStringLiteral("uses Leader"), kCyan);
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
