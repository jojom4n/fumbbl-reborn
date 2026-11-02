// =============================================================================
// ReportParser — implementation (see reportparser.h for the ffb-common
// references and the round-9 field alignment notes)
// =============================================================================
#include "core/reportparser.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

namespace {

// Palette (mirrors the TS tailwind classes, adapted to the light Mockup-F
// card where the UI darkens them).
const QColor kGreen(0x6f, 0xbf, 0x73);   // success
const QColor kRed(0xd8, 0x7a, 0x7a);     // failure
const QColor kYellow(0xd8, 0xc0, 0x7a);  // partial / choice
const QColor kGray(0xa0, 0xa0, 0xa0);    // system
const QColor kLightGray(0xb8, 0xb8, 0xb8);
const QColor kOrange(0xd8, 0x9a, 0x5a);  // foul / armor broken
const QColor kCyan(0x6f, 0xc3, 0xd8);    // weather / leader / skill
const QColor kBlue(0x7a, 0x9f, 0xd8);    // blitz / reroll
const QColor kPurple(0xb0, 0x8f, 0xd8);  // fans

// --- JSON helpers (key names per IJsonOption.java) --------------------------
QString str(const QJsonObject &r, const char *key)
{
    return r.value(QString::fromLatin1(key)).toString();
}
int num(const QJsonObject &r, const char *key)
{
    return r.value(QString::fromLatin1(key)).toInt();
}
bool flag(const QJsonObject &r, const char *key)
{
    return r.value(QString::fromLatin1(key)).toBool();
}
QJsonArray arr(const QJsonObject &r, const char *key)
{
    return r.value(QString::fromLatin1(key)).toArray();
}
// Enums on the wire are {"name": "..."} (JsonEnumWithNameOption) but plain
// strings in some payloads — accept both.
QString enumName(const QJsonValue &v)
{
    if (v.isObject())
        return v.toObject().value(QStringLiteral("name")).toString();
    if (v.isString())
        return v.toString();
    return QString();
}
// Coordinate objects {"x": n, "y": n}.
QString coordText(const QJsonObject &r, const char *key)
{
    const QJsonObject c = r.value(QString::fromLatin1(key)).toObject();
    if (c.isEmpty())
        return QString();
    return QStringLiteral("[%1, %2]").arg(c.value(QStringLiteral("x")).toInt())
                                .arg(c.value(QStringLiteral("y")).toInt());
}
// "camelCaseName" -> "Camel Case Name" (report/enum labels without JSON).
// Acronyms stay together ("POW" -> "Pow", "PUSHBACK" -> "Pushback",
// "POW/PUSH" -> "Pow/Push") — a space goes before an uppercase letter only
// when the previous is lowercase, or when the acronym ends (next is lower).
QString humanize(const QString &s)
{
    if (s.isEmpty())
        return QString();
    QString out;
    out.reserve(s.size() + 4);
    for (int i = 0; i < s.size(); ++i) {
        const QChar ch = s.at(i);
        if (ch.isUpper() && !out.isEmpty() && !out.endsWith(' ')) {
            const QChar prev = i > 0 ? s.at(i - 1) : QChar();
            const QChar next = i + 1 < s.size() ? s.at(i + 1) : QChar();
            const bool split = prev.isLower()
                || (next.isLower() && !prev.isUpper() && prev != QChar());
            if (split)
                out += QLatin1Char(' ');
        }
        out += ch;
    }
    out = out.simplified();
    out[0] = out[0].toUpper();
    return out;
}

// Stat/modifier suffix from ReportSkillRoll extras:
// statBasedRollModifier {name|stat, value}, rollModifiers[].
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
    const QJsonArray m = arr(r, "rollModifiers");
    if (m.isEmpty())
        return QString();
    QStringList names;
    for (const QJsonValue &x : m)
        names << (x.isObject() ? x.toObject().value(QStringLiteral("name")).toString() : x.toString());
    return QStringLiteral(" (%1)").arg(names.join(QStringLiteral(", ")));
}
QString suffix(const QJsonObject &report)
{
    const QString sl = statLabel(report);
    const QString fm = modifierLabel(report);
    if (sl.isEmpty() && fm.isEmpty())
        return QString();
    return QStringLiteral("%1%2").arg(sl).arg(fm);
}

// Fills a line's single-d6 fields from ReportSkillRoll (playerId/roll/
// minimumRoll/successful/reRolled).
void fillD6(DiceLogLine &l, const QJsonObject &r)
{
    l.d6Roll = num(r, "roll");
    l.d6Target = num(r, "minimumRoll");
    l.d6Ok = flag(r, "successful");
    l.d6ShowResult = r.contains(QStringLiteral("successful"));
    l.d6ReRolled = flag(r, "reRolled");
}
// Fills blockFaces from an int[] of 1..6.
void fillBlockFaces(DiceLogLine &l, const QJsonArray &d)
{
    for (const QJsonValue &v : d)
        l.blockFaces << QString::number(v.toInt());
}
// Fills d6Rolls (multi-die lines) from an int[].
void fillD6Rolls(DiceLogLine &l, const QJsonArray &d)
{
    for (const QJsonValue &v : d) {
        const int x = v.toInt();
        if (x >= 1 && x <= 6)
            l.d6Rolls << QString::number(x);
    }
    if (!l.d6Rolls.isEmpty())
        l.d6Roll = l.d6Rolls.first().toInt();
}

} // namespace

QVector<DiceLogLine> parseReport(const QJsonObject &report)
{
    QVector<DiceLogLine> out;
    const QString id = report.value(QStringLiteral("reportId")).toString();
    if (id.isEmpty())
        return out;

    // The subject of the report. ffb-common: most reports carry the acting
    // player in `playerId` (ReportSkillRoll), but ReportPlayerAction and
    // friends use `actingPlayerId`. Read both, `actingPlayerId` first.
    const QString actorId = !str(report, "actingPlayerId").isEmpty()
                                ? str(report, "actingPlayerId")
                                : str(report, "playerId");
    const QString defenderId = str(report, "defenderId");
    const QString teamId = !str(report, "teamId").isEmpty()
                               ? str(report, "teamId")
                               : str(report, "choosingTeamId");

    auto push = [&](const QString &text, const QColor &color,
                    const QString &actor = QString(), const QString &defender = QString()) {
        DiceLogLine l;
        l.text = text;
        l.color = color;
        l.actorId = actor;
        l.defenderId = defender;
        out.append(l);
    };

    // Generic single-die skill roll: actor rolls a d6 against a target.
    auto sr = [&](const QString &name, const QColor &okColor, const QColor &failColor) {
        DiceLogLine l;
        const QString suf = suffix(report);
        l.text = suf.isEmpty() ? name
                               : QStringLiteral("%1  %2").arg(name, suf);
        l.color = flag(report, "successful") ? okColor : failColor;
        l.actorId = actorId;
        fillD6(l, report);
        out.append(l);
    };

    // The long list of plain skill rolls (ReportSkillRoll subclasses —
    // verified field-by-field against ffb-common, round 9):
    // playerId, roll, minimumRoll, successful, reRolled, rollModifiers.
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
        { "confusionRoll", { "Confusion", 0 } }, // verified live: standard
        // ReportSkillRoll fields + confusionSkill
        { "dwarfenWisdomRoll", { "Dwarfen Wisdom", 3 } },
        { "gettingEvenRoll", { "Getting Even", 3 } },
        { "saboteurRoll", { "Saboteur", 4 } },
        { "chompRoll", { "Chomp", 4 } },
        { "dodgySnackRoll", { "Dodgy Snack", 4 } },
        { "weatherMageRoll", { "Weather Mage", 3 } },
        { "apothecaryRoll", { "Apothecary", 1 } },
        // Round 9 additions (all ReportSkillRoll subclasses in ffb-common):
        { "safeThrowRoll", { "Safe Throw", 0 } },
        { "weepingDaggerRoll", { "Weeping Dagger", 4 } },
        { "alwaysHungryRoll", { "Always Hungry", 4 } },
        { "animosityRoll", { "Animosity", 4 } },
        { "chainsawRoll", { "Chainsaw", 4 } },
        { "throwTeamMateRoll", { "Throw Team-mate", 0 } },
        { "swarmingPlayersRoll", { "Swarming Players", 4 } },
    };
    // color kind: 0 green/red, 1 green/gray, 2 green/yellow, 3 cyan/red, 4 orange/red
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

    // The big dispatch. Key names are the ffb-common IJsonOption names.
    if (id == QLatin1String("playerAction")) {
        // ReportPlayerAction: ACTING_PLAYER_ID + playerAction enum. THE
        // subject field is `actingPlayerId` (round-9 bug: the old parser
        // read `playerId`, so "move"/"stand up" had no subject).
        // Turn 15 (user: "riempi l'action log con le azioni del client
        // ufficiale"): the OFFICIAL client (PlayerActionMessage.render)
        // prints the PlayerAction enum's description string — use the same
        // wording (ffb-common PlayerAction.java, ground truth).
        const QString an = enumName(report.value(QStringLiteral("playerAction")));
        static const QHash<QString, QString> kActionText = {
            { QStringLiteral("move"),                QStringLiteral("moves") },
            { QStringLiteral("blitzMove"),           QStringLiteral("blitzes") },
            { QStringLiteral("block"),               QStringLiteral("blocks") },
            { QStringLiteral("standUp"),             QStringLiteral("stands up") },
            { QStringLiteral("standUpBlitz"),        QStringLiteral("stands up (Blitz)") },
            { QStringLiteral("passMove"),            QStringLiteral("passes") },
            { QStringLiteral("handOverMove"),        QStringLiteral("hands the ball over") },
            { QStringLiteral("foulMove"),            QStringLiteral("fouls") },
            { QStringLiteral("gazeMove"),            QStringLiteral("gazes") },
            { QStringLiteral("multipleBlock"),       QStringLiteral("multi-blocks") },
            { QStringLiteral("throwTeamMateMove"),   QStringLiteral("throws a team-mate") },
            { QStringLiteral("kickTeamMateMove"),    QStringLiteral("kicks a team-mate") },
            { QStringLiteral("throwBomb"),           QStringLiteral("throws a bomb") },
            { QStringLiteral("hailMaryBomb"),        QStringLiteral("hails Mary with a bomb") },
            { QStringLiteral("dumpOff"),             QStringLiteral("dumps off the bomb") },
            { QStringLiteral("swoop"),               QStringLiteral("swoops") },
            { QStringLiteral("removeConfusion"),     QStringLiteral("removes confusion") },
            { QStringLiteral("lookIntoMyEyes"),      QStringLiteral("looks into the eyes of") },
            { QStringLiteral("balefulHex"),          QStringLiteral("balefully hexes") },
            { QStringLiteral("allYouCanEat"),        QStringLiteral("all you can eats") },
            { QStringLiteral("putridRegurgitationMove"),  QStringLiteral("putridly regurgitates") },
            { QStringLiteral("putridRegurgitationBlock"), QStringLiteral("putridly regurgitates") },
            { QStringLiteral("putridRegurgitationBlitz"), QStringLiteral("putridly regurgitates") },
            { QStringLiteral("kickEmBlock"),         QStringLiteral("kick-em blocks") },
            { QStringLiteral("kickEmBlitz"),         QStringLiteral("kick-em blitzes") },
            { QStringLiteral("blackInk"),            QStringLiteral("uses Black Ink") },
            { QStringLiteral("catchOfTheDay"),       QStringLiteral("uses Catch of the Day") },
            { QStringLiteral("thenIStartedBlastin"), QStringLiteral("starts blastin'") },
            { QStringLiteral("theFlashingBlade"),    QStringLiteral("flashes the blade") },
            { QStringLiteral("viciousVines"),        QStringLiteral("uses Vicious Vines") },
            { QStringLiteral("furiousOutburst"),     QStringLiteral("has a furious outburst") },
            { QStringLiteral("secureTheBall"),       QStringLiteral("secures the ball") },
            { QStringLiteral("breatheFire"),         QStringLiteral("breathes fire") },
            { QStringLiteral("chainsaw"),            QStringLiteral("chainsaws") },
            { QStringLiteral("stab"),                QStringLiteral("stabs") },
            { QStringLiteral("projectileVomit"),     QStringLiteral("projectile vomits") },
        };
        const auto it = kActionText.constFind(an);
        push(it != kActionText.constEnd() ? it.value() : humanize(an),
             kLightGray, actorId);
    } else if (id == QLatin1String("block")) {
        // ReportBlock: ONLY defenderId = the player being blocked. No dice
        // here (the dice arrive in the `blockRoll` report — round-9 bug:
        // the old parser looked for them here and never found them).
        // Turn 15 (official wording, BlockMessage.render): the line is
        // "ATTACKER blocks DEFENDER: " — the report carries ONLY the
        // defender; the ATTACKER is the current acting player, so the
        // parser leaves actorId empty and MainWindow fills it from the
        // model (the block reports arrive while the attacker is the actor).
        push(QStringLiteral("blocks"), kYellow, QString(), defenderId);
    } else if (id == QLatin1String("blockRoll")) {
        // ReportBlockRoll: choosingTeamId + blockRoll[6] + defenderId.
        // The six block dice — the star of the action log.
        // Turn 16 (user: after a BLITZ the dice line was titled with the
        // blitzer's TEAM, not the blitzer): the actor is the ACTING PLAYER
        // (the blitzer / the blocking attacker) — actorId left empty, like
        // the `block` report; MainWindow fills it from the model.
        DiceLogLine l;
        l.text = QStringLiteral("Block roll");
        l.color = kYellow;
        l.actorId = QString();
        l.defenderId = defenderId;
        fillBlockFaces(l, arr(report, "blockRoll"));
        out.append(l);
    } else if (id == QLatin1String("blockChoice")) {
        // ReportBlockChoice: nrOfDice + blockRoll[6] + diceIndex +
        // blockResult + defenderId. BlockResult names are UPPERCASE with
        // spaces on the wire ("SKULL", "BOTH DOWN", "PUSHBACK", "POW/PUSH").
        const QString br = enumName(report.value(QStringLiteral("blockResult")));
        const QString brU = br.toUpper();
        DiceLogLine l;
        l.text = QStringLiteral("Block: %1").arg(br.isEmpty() ? QStringLiteral("?") : humanize(br));
        // From the choosing (attacking) team's point of view: PUSHBACK/POW
        // win, BOTH DOWN is partial, SKULL = the defender blocked.
        l.color = brU.startsWith(QLatin1String("SKULL")) ? kRed
                  : brU.contains(QLatin1String("BOTH")) ? kYellow
                  : kGreen;
        // Turn 16: same rule as blockRoll — the line is titled with the
        // acting player (blitzer / attacker), not the choosing team.
        l.actorId = QString();
        l.defenderId = defenderId;
        // Show the chosen die (blockRoll[diceIndex]) as a block face.
        const QJsonArray d = arr(report, "blockRoll");
        const int idx = num(report, "diceIndex");
        if (idx >= 0 && idx < d.size())
            l.blockFaces << QString::number(d.at(idx).toInt());
        out.append(l);
    } else if (id == QLatin1String("blockReRoll")) {
        // ReportBlockReRoll: blockRoll[6] + playerId + reRollSource.
        DiceLogLine l;
        l.text = QStringLiteral("Block re-roll (%1)")
                     .arg(humanize(enumName(report.value(QStringLiteral("reRollSource")))));
        l.color = kBlue;
        l.actorId = actorId;
        fillBlockFaces(l, arr(report, "blockRoll"));
        out.append(l);
    } else if (id == QLatin1String("bribesRoll")) {
        // ReportBribesRoll: playerId + successful + roll.
        sr(QStringLiteral("Bribes"), kGreen, kGray);
    } else if (id == QLatin1String("blitzRoll")) {
        sr(QStringLiteral("Blitz"), kBlue, kBlue);
    } else if (id == QLatin1String("passRoll")) {
        DiceLogLine l;
        const QString suf = suffix(report);
        const QString pr = enumName(report.value(QStringLiteral("passResult")));
        QString extra;
        if (flag(report, "bomb"))
            extra += QStringLiteral(" [BOMB]");
        if (flag(report, "hailMaryPass"))
            extra += QStringLiteral(" [Hail Mary]");
        if (!pr.isEmpty())
            extra += QStringLiteral(" (%1)").arg(humanize(pr));
        l.text = QStringLiteral("Pass%1%2")
                     .arg(extra)
                     .arg(suf.isEmpty() ? QString() : QStringLiteral("  %1").arg(suf));
        l.color = flag(report, "successful") ? kGreen : kRed;
        l.actorId = actorId;
        fillD6(l, report);
        out.append(l);
    } else if (id == QLatin1String("pilingOn")) {
        // ReportPilingOn: playerId + used + reRollInjury. NO dice on the
        // wire (the old parser invented a `rolls` array).
        push(flag(report, "used") ? QStringLiteral("uses Piling On")
                                  : QStringLiteral("Piling On available"),
             kCyan, actorId);
    } else if (id == QLatin1String("handOver")) {
        // ReportHandOver: catcherId (the player receiving the ball).
        push(QStringLiteral("Hand Over"), kLightGray, actorId, str(report, "catcherId"));
    } else if (id == QLatin1String("foul")) {
        // ReportFoul: defenderId = the player WHO GOT FOULED (the victim —
        // server StepInitFouling sets the game defender to the foul target
        // and the report carries game.getDefenderId()). The FOULER is the
        // current acting player, not in the report: actorId left empty and
        // filled from the model by MainWindow (same pattern as `block`).
        // Turn 16 user report: the old code titled the line with the victim
        // ("il fallatore era il giocatore fallato").
        push(QStringLiteral("commits a Foul!"), kOrange, QString(), defenderId);
    } else if (id == QLatin1String("pushback")) {
        // ReportPushback: defenderId + pushbackMode.
        push(QStringLiteral("is pushed back (%1)")
                 .arg(humanize(enumName(report.value(QStringLiteral("pushbackMode"))))),
             kLightGray, QString(), defenderId);
    } else if (id == QLatin1String("scatterPlayer")) {
        // ReportScatterPlayer: start/end coordinate + directions + rolls[].
        DiceLogLine l;
        const QString from = coordText(report, "startCoordinate");
        const QString to = coordText(report, "endCoordinate");
        l.text = QStringLiteral("scatters%1 -> %2")
                     .arg(from.isEmpty() ? QString() : from,
                          to.isEmpty() ? QStringLiteral("?") : to);
        l.color = kLightGray;
        fillD6Rolls(l, arr(report, "rolls"));
        out.append(l);
    } else if (id == QLatin1String("swoopPlayer")) {
        // ReportSwoopPlayer (bb2025): start/end coordinate + distance +
        // scatterDirection + outOfBounds.
        const QString to = coordText(report, "endCoordinate");
        push(QStringLiteral("Swoop: scatters to %1%2")
                 .arg(to.isEmpty() ? QStringLiteral("?") : to)
                 .arg(flag(report, "outOfBounds") ? QStringLiteral(" (out of bounds!)") : QString()),
             kLightGray);
    } else if (id == QLatin1String("scatterBall")) {
        // ReportScatterBall: directions + rolls + gustOfWind.
        DiceLogLine l;
        l.text = flag(report, "gustOfWind") ? QStringLiteral("Ball scattered (gust of wind)")
                                            : QStringLiteral("Ball scattered");
        l.color = kLightGray;
        fillD6Rolls(l, arr(report, "rolls"));
        out.append(l);
    } else if (id == QLatin1String("skillUse")) {
        // ReportSkillUse: playerId + skill + used + skillUse.
        const QString sn = enumName(report.value(QStringLiteral("skill")));
        const QString su = enumName(report.value(QStringLiteral("skillUse")));
        push(QStringLiteral("%1s %2%3")
                 .arg(flag(report, "used") ? QStringLiteral("use") : QStringLiteral("try to use"))
                 .arg(sn.isEmpty() ? QStringLiteral("a skill") : humanize(sn))
                 .arg(su.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(humanize(su))),
             kCyan, actorId);
    } else if (id == QLatin1String("skillUseOtherPlayer")) {
        const QString sn = enumName(report.value(QStringLiteral("skill")));
        push(QStringLiteral("uses %1 on")
                 .arg(sn.isEmpty() ? QStringLiteral("a skill") : humanize(sn)),
             kCyan, actorId, str(report, "playerIdOtherPlayer"));
    } else if (id == QLatin1String("skillWasted")) {
        push(QStringLiteral("wastes %1")
                 .arg(humanize(enumName(report.value(QStringLiteral("skill"))))),
             kOrange, actorId);
    } else if (id == QLatin1String("reRoll")) {
        // ReportReRoll: playerId + reRollSource + successful + roll (NO
        // minimumRoll on the wire).
        DiceLogLine l;
        const QString src = enumName(report.value(QStringLiteral("reRollSource")));
        l.text = QStringLiteral("Re-Roll%1").arg(src.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(humanize(src)));
        l.color = kBlue;
        l.actorId = actorId;
        fillD6(l, report);
        l.d6Target = 0; // no printed threshold
        out.append(l);
    } else if (id == QLatin1String("leader")) {
        // ReportLeader: TEAM_ID + leaderState.
        const QString ls = enumName(report.value(QStringLiteral("leaderState")));
        push(QStringLiteral("uses Leader%1")
                 .arg(ls.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(humanize(ls))),
             kCyan, teamId);
    } else if (id == QLatin1String("throwIn")) {
        // ReportThrowIn: direction + directionRoll + distanceRoll[].
        DiceLogLine l;
        l.text = QStringLiteral("Throw-In (%1)")
                     .arg(humanize(enumName(report.value(QStringLiteral("direction")))));
        l.color = kLightGray;
        l.d6Roll = num(report, "directionRoll");
        fillD6Rolls(l, arr(report, "distanceRoll"));
        // No win/lose result on a throw-in: dice only, no cross.
        l.d6ShowResult = false;
        out.append(l);
    } else if (id == QLatin1String("kickoffResult")) {
        // ReportKickoffResult: kickoffResult enum + kickoffRoll.
        DiceLogLine l;
        l.text = QStringLiteral("Kickoff: %1")
                     .arg(humanize(enumName(report.value(QStringLiteral("kickoffResult")))));
        l.color = kLightGray;
        l.d6Roll = num(report, "kickoffRoll");
        l.d6ShowResult = report.contains(QStringLiteral("successful"));
        out.append(l);
    } else if (id == QLatin1String("kickoffScatter")) {
        // ReportKickoffScatter: ballCoordinateEnd + rolls.
        DiceLogLine l;
        l.text = QStringLiteral("Kickoff scatter: %1")
                     .arg(coordText(report, "ballCoordinateEnd").isEmpty()
                              ? QStringLiteral("?")
                              : coordText(report, "ballCoordinateEnd"));
        l.color = kLightGray;
        fillD6Rolls(l, arr(report, "rollScatterDistance"));
        out.append(l);
    } else if (id == QLatin1String("dedicatedFans")) {
        // ReportDedicatedFans: rollHome/rollAway + modifiers + teamId.
        DiceLogLine l;
        l.text = QStringLiteral("Dedicated Fans");
        l.color = kPurple;
        l.actorId = teamId;
        l.d6Rolls << QString::number(num(report, "rollHome"));
        l.d6Rolls << QString::number(num(report, "rollAway"));
        l.d6Roll = num(report, "rollHome");
        out.append(l);
    } else if (id == QLatin1String("timeoutEnforced")) {
        // ReportTimeoutEnforced: COACH (a plain NAME string, not an id).
        push(QStringLiteral("Timeout enforced for %1!").arg(str(report, "coach")), kRed);
    } else if (id == QLatin1String("teamCaptainRoll")) {
        // ReportTeamCaptainRoll: TEAM_ID + roll + minimumRoll + successful.
        DiceLogLine l;
        l.text = QStringLiteral("Team Captain");
        l.color = kCyan;
        l.actorId = teamId;
        fillD6(l, report);
        out.append(l);
    } else if (id == QLatin1String("prayerRoll")) {
        // bb2025: roll + teamName + homeTeam (no minimumRoll); the older
        // payload also carried choosingTeamId + prayer + minimumRoll —
        // handle both.
        DiceLogLine l;
        const QString pn = enumName(report.value(QStringLiteral("prayer")));
        l.text = QStringLiteral("Prayer%1")
                     .arg(pn.isEmpty() ? QString() : QStringLiteral(": %1").arg(humanize(pn)));
        l.color = flag(report, "successful") ? kGreen : kGray;
        const QString tn = str(report, "teamName");
        l.actorId = !tn.isEmpty() ? tn : teamId; // name (UI passes it through) or id
        l.d6Roll = num(report, "roll");
        l.d6Target = num(report, "minimumRoll");
        l.d6Ok = flag(report, "successful");
        l.d6ReRolled = flag(report, "reRolled");
        out.append(l);
    } else if (id == QLatin1String("prayerEnd")) {
        push(QStringLiteral("Prayer ends: %1")
                 .arg(humanize(enumName(report.value(QStringLiteral("prayer"))))),
             kGray);
    } else if (id == QLatin1String("prayerWasted")) {
        push(QStringLiteral("Prayer wasted"), kOrange, actorId);
    } else if (id == QLatin1String("prayerAmount")) {
        push(QStringLiteral("Prayer: %1").arg(num(report, "teamValue")), kGray);
    } else if (id == QLatin1String("injury")) {
        // ReportInjury (mixed): defenderId = the player INJURED,
        // attackerId = the blocker, armorRoll/injuryRoll/casualtyRoll int[].
        const QString iType = enumName(report.value(QStringLiteral("injuryType")));
        const QJsonArray ar = arr(report, "armorRoll");
        const QJsonArray ir = arr(report, "injuryRoll");
        const QJsonArray cr = arr(report, "casualtyRoll");
        const bool armorBroken = flag(report, "armorBroken");
        const QString ist = enumName(report.value(QStringLiteral("injury")));
        const QString si = enumName(report.value(QStringLiteral("seriousInjury")));
        if (!ar.isEmpty()) {
            DiceLogLine l;
            // ROUND 13 TURN 8 (user, rules — he is always right): an armor
            // roll that BREAKS the armor is a SUCCESS of the roll ("se
            // l'armatura è BROKEN allora è un SUCCESSO, non un FAILURE") —
            // and ONLY on that success does the injury roll happen (1-7
            // stunned, 8+ KO/injured, 15-16 dead). A failed armor roll
            // leaves the player prone on the ground with NO injury roll.
            // So: BROKEN -> Success label, holds -> Failure label.
            l.text = armorBroken ? QStringLiteral("Armor BROKEN!")
                                 : QStringLiteral("Armor holds");
            l.color = armorBroken ? kOrange : kGray;
            l.defenderId = defenderId; // victim
            fillD6Rolls(l, ar);
            l.d6Ok = armorBroken;
            l.d6ShowResult = true;
            out.append(l);
        }
        if (!ir.isEmpty()) {
            DiceLogLine l;
            l.text = iType.isEmpty() ? QStringLiteral("Injury")
                                     : QStringLiteral("Injury (%1)").arg(humanize(iType));
            l.color = kYellow;
            l.defenderId = defenderId;
            fillD6Rolls(l, ir);
            out.append(l);
        }
        if (!cr.isEmpty()) {
            DiceLogLine l;
            l.text = QStringLiteral("is %1!%2")
                         .arg(ist.isEmpty() ? QStringLiteral("out") : humanize(ist))
                         .arg(si.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(humanize(si)));
            l.color = kRed;
            l.defenderId = defenderId;
            fillD6Rolls(l, cr);
            out.append(l);
        }
        if (ar.isEmpty() && ir.isEmpty() && cr.isEmpty()) {
            push(QStringLiteral("is %1!%2")
                     .arg(ist.isEmpty() ? QStringLiteral("out") : humanize(ist))
                     .arg(si.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(humanize(si))),
                 kRed, QString(), defenderId);
        }
    } else if (id == QLatin1String("turnEnd")) {
        // ReportTurnEnd (mixed): playerIdTouchdown + heatRoll + arrays.
        DiceLogLine l;
        l.text = QStringLiteral("Turn ended");
        if (!str(report, "playerIdTouchdown").isEmpty())
            l.text = QStringLiteral("Turn ended (touchdown)");
        l.color = kGray;
        l.actorId = str(report, "playerIdTouchdown");
        // Round 13 turn 4 (BB3 log): turn separators are CENTERED lines.
        l.separator = true;
        l.actorIsPlayer = true; // a player id (touchdown scorer) or empty
        l.d6Roll = num(report, "heatRoll");
        if (l.d6Roll > 0)
            l.text += QStringLiteral("  heat");
        out.append(l);
    } else if (id == QLatin1String("startHalf")) {
        DiceLogLine l;
        l.text = QStringLiteral("Half %1 starts").arg(num(report, "half"));
        l.color = kGray;
        l.separator = true; // round 13 turn 4: centered separator
        out.append(l);
    } else if (id == QLatin1String("coinThrow")) {
        // ReportCoinThrow: coach + coinThrowHeads + coinChoiceHeads.
        DiceLogLine l;
        l.text = QStringLiteral("Coin Toss: %1")
                     .arg(flag(report, "coinThrowHeads") ? QStringLiteral("Heads") : QStringLiteral("Tails"));
        l.color = kGray;
        l.separator = true; // round 13 turn 4: centered separator
        out.append(l);
    } else if (id == QLatin1String("receiveChoice")) {
        push(QStringLiteral("Receive choice: %1")
                 .arg(humanize(enumName(report.value(QStringLiteral("receiveChoice"))))),
             kLightGray, teamId);
    } else if (id == QLatin1String("passBlock")) {
        push(QStringLiteral("Pass Block%1")
                 .arg(flag(report, "passBlockAvailable") ? QStringLiteral(" available") : QStringLiteral(" blocked")),
             kLightGray, teamId);
    } else if (id == QLatin1String("passDeviate")) {
        DiceLogLine l;
        l.text = QStringLiteral("Pass deviates to %1")
                     .arg(coordText(report, "ballCoordinateEnd").isEmpty()
                              ? QStringLiteral("?")
                              : coordText(report, "ballCoordinateEnd"));
        l.color = kLightGray;
        fillD6Rolls(l, arr(report, "rollScatterDistance"));
        out.append(l);
    } else if (id == QLatin1String("selectBlitzTarget") || id == QLatin1String("selectGazeTarget")) {
        // attackerId + defenderId.
        push(id == QLatin1String("selectBlitzTarget") ? QStringLiteral("Blitz target")
                                                      : QStringLiteral("Gaze target"),
             kLightGray, str(report, "attackerId"), defenderId);
    } else if (id == QLatin1String("trapDoor")) {
        DiceLogLine l;
        l.text = flag(report, "escaped") ? QStringLiteral("escapes through the Trap Door!")
                                         : QStringLiteral("falls into the Trap Door!");
        l.color = flag(report, "escaped") ? kGreen : kRed;
        l.actorId = actorId;
        l.d6Roll = num(report, "roll");
        out.append(l);
    } else if (id == QLatin1String("throwAtPlayer") || id == QLatin1String("throwAtStallingPlayer")) {
        DiceLogLine l;
        l.text = id == QLatin1String("throwAtStallingPlayer")
                     ? QStringLiteral("throws at the stalling player")
                     : QStringLiteral("throws at");
        l.color = kLightGray;
        l.actorId = actorId;
        l.d6Roll = num(report, "roll");
        l.d6Ok = flag(report, "successful");
        out.append(l);
    } else if (id == QLatin1String("stallerDetected")) {
        push(QStringLiteral("is detected stalling!"), kOrange, actorId);
    } else if (id == QLatin1String("indomitable")) {
        push(QStringLiteral("Indomitable"), kCyan, str(report, "attackerId").isEmpty() ? actorId : str(report, "attackerId"), defenderId);
    } else if (id == QLatin1String("oldPro")) {
        DiceLogLine l;
        l.text = QStringLiteral("Old Pro (re-roll)");
        l.color = kBlue;
        l.actorId = actorId;
        l.d6Roll = num(report, "roll");
        out.append(l);
    } else if (id == QLatin1String("pickMeUp")) {
        DiceLogLine l;
        l.text = QStringLiteral("Pick Me Up");
        l.color = flag(report, "successful") ? kGreen : kGray;
        l.actorId = actorId;
        l.d6Roll = num(report, "roll");
        l.d6Ok = flag(report, "successful");
        out.append(l);
    } else if (id == QLatin1String("twoForOne")) {
        // ReportTwoForOne: playerId + used + partnerId.
        push(QStringLiteral("Two For One"), kCyan, actorId, str(report, "partnerId"));
    } else if (id == QLatin1String("hitAndRun")) {
        push(QStringLiteral("Hit and Run (%1)")
                 .arg(humanize(enumName(report.value(QStringLiteral("direction"))))),
             kLightGray, actorId);
    } else if (id == QLatin1String("cardDeactivated")) {
        push(QStringLiteral("Card deactivated: %1")
                 .arg(humanize(enumName(report.value(QStringLiteral("card"))))),
             kGray);
    } else if (id == QLatin1String("playCard")) {
        push(QStringLiteral("plays card: %1")
                 .arg(humanize(enumName(report.value(QStringLiteral("card"))))),
             kCyan, teamId, str(report, "playerId"));
    } else if (id == QLatin1String("inducement")) {
        push(QStringLiteral("Inducement: %1 (%2)")
                 .arg(humanize(enumName(report.value(QStringLiteral("inducementType")))))
                 .arg(str(report, "value")),
             kGray, teamId);
    } else if (id == QLatin1String("masterChefRoll")) {
        DiceLogLine l;
        l.text = QStringLiteral("Master Chef%1")
                     .arg(num(report, "reRollsStolen") > 0
                              ? QStringLiteral(" (steals %1 re-roll)").arg(num(report, "reRollsStolen"))
                              : QString());
        l.color = kYellow;
        l.actorId = teamId;
        fillBlockFaces(l, arr(report, "masterChefRoll"));
        out.append(l);
    } else if (id == QLatin1String("cardEffectRoll")) {
        DiceLogLine l;
        l.text = QStringLiteral("%1: %2")
                     .arg(humanize(enumName(report.value(QStringLiteral("card")))))
                     .arg(humanize(enumName(report.value(QStringLiteral("cardEffect")))));
        l.color = kCyan;
        l.d6Roll = num(report, "roll");
        out.append(l);
    } else if (id == QLatin1String("officiousRefRoll")) {
        DiceLogLine l;
        l.text = QStringLiteral("Officious Ref");
        l.color = kOrange;
        l.actorId = actorId;
        l.d6Roll = num(report, "roll");
        out.append(l);
    } else if (id == QLatin1String("tentaclesShadowingRoll")) {
        // defenderId + tentacleRoll + minimumRoll + successful + reRolled.
        DiceLogLine l;
        l.text = QStringLiteral("Tentacles Shadowing");
        l.color = flag(report, "successful") ? kGreen : kRed;
        l.defenderId = defenderId;
        l.d6Roll = num(report, "tentacleRoll");
        l.d6Target = num(report, "minimumRoll");
        l.d6Ok = flag(report, "successful");
        l.d6ReRolled = flag(report, "reRolled");
        out.append(l);
    } else if (id == QLatin1String("kickTeamMateRoll")) {
        DiceLogLine l;
        l.text = QStringLiteral("Kick Team-mate%1")
                     .arg(num(report, "distance") > 0
                              ? QStringLiteral(" (%1 away)").arg(num(report, "distance"))
                              : QString());
        l.color = flag(report, "successful") ? kGreen : kGray;
        l.actorId = actorId;
        l.defenderId = defenderId;
        fillD6Rolls(l, arr(report, "rolls"));
        l.d6Ok = flag(report, "successful");
        out.append(l);
    } else if (id == QLatin1String("weatherMageResult")) {
        push(QStringLiteral("Weather Mage: %1 (was %2)")
                 .arg(humanize(enumName(report.value(QStringLiteral("weather")))))
                 .arg(humanize(enumName(report.value(QStringLiteral("oldWeather"))))),
             kCyan);
    } else if (id == QLatin1String("nervesOfSteel")) {
        push(QStringLiteral("Nerves of Steel"), kCyan, actorId);
    } else if (id == QLatin1String("cloudBurster")) {
        push(QStringLiteral("Cloud Burster"), kCyan, str(report, "throwerId"), str(report, "interceptorId"));
    } else if (id == QLatin1String("animalSavagery")) {
        push(QStringLiteral("Animal Savagery"), kOrange, str(report, "attackerId"), defenderId);
    } else if (id == QLatin1String("fumblerooskie")) {
        push(QStringLiteral("Fumblerooskie"), kOrange, actorId);
    } else if (id == QLatin1String("projectileVomit")) {
        push(QStringLiteral("Projectile Vomit"), kOrange, QString(), defenderId);
    } else if (id == QLatin1String("raiseDead")) {
        push(QStringLiteral("Raise Dead (Nurgle's Rot)"), kPurple, actorId);
    } else if (id == QLatin1String("biteSpectator")) {
        push(QStringLiteral("bites a spectator!"), kOrange, actorId);
    } else if (id == QLatin1String("secretWeaponBan")) {
        DiceLogLine l;
        l.text = QStringLiteral("Secret Weapon Ban");
        l.color = kOrange;
        fillD6Rolls(l, arr(report, "rolls"));
        out.append(l);
    } else if (id == QLatin1String("riotousRookies")) {
        DiceLogLine l;
        l.text = QStringLiteral("Riotous Rookies");
        l.color = kOrange;
        l.actorId = teamId;
        l.d6Roll = num(report, "riotousRoll");
        out.append(l);
    } else if (id == QLatin1String("cheeringFans")) {
        DiceLogLine l;
        l.text = QStringLiteral("Cheering Fans");
        l.color = kPurple;
        l.actorId = teamId;
        l.d6Rolls << QString::number(num(report, "rollHome"));
        l.d6Rolls << QString::number(num(report, "rollAway"));
        l.d6Roll = num(report, "rollHome");
        out.append(l);
    } else if (id == QLatin1String("mascotUsed")) {
        DiceLogLine l;
        l.text = QStringLiteral("Mascot");
        l.color = kPurple;
        l.actorId = teamId;
        fillD6(l, report);
        out.append(l);
    } else if (id == QLatin1String("breatheFire")) {
        push(QStringLiteral("Breathe Fire"), kOrange, QString(), defenderId);
    } else if (id == QLatin1String("thenIStartedBlastin") || id == QLatin1String("thrownKeg")) {
        DiceLogLine l;
        l.text = id == QLatin1String("thrownKeg") ? QStringLiteral("throws a Keg")
                                                  : QStringLiteral("Then I Started Blasin'");
        l.color = flag(report, "successful") ? kGreen : kRed;
        l.actorId = actorId;
        l.defenderId = str(report, "targetPlayerId");
        l.d6Roll = num(report, "roll");
        l.d6Ok = flag(report, "successful");
        out.append(l);
    } else if (id == QLatin1String("pumpUpTheCrowdReRoll")) {
        push(QStringLiteral("Pump Up The Crowd (re-roll)"), kBlue, actorId);
    } else if (id == QLatin1String("showStarReRoll")) {
        push(QStringLiteral("Show Star (re-roll)"), kBlue, actorId);
    } else if (id == QLatin1String("biasedRef")) {
        DiceLogLine l;
        l.text = QStringLiteral("Biased Ref%1")
                     .arg(flag(report, "foulingPlayerBanned") ? QStringLiteral(" (fouling player BANNED!)") : QString());
        l.color = kOrange;
        l.d6Roll = num(report, "roll");
        out.append(l);
    } else if (id == QLatin1String("doubleHiredStarPlayer")) {
        push(QStringLiteral("Double-Hired Star Player: %1").arg(str(report, "starPlayerName")), kGray);
    } else if (id == QLatin1String("wizardUse")) {
        push(QStringLiteral("Wizard: %1")
                 .arg(humanize(enumName(report.value(QStringLiteral("wizardSpell"))))),
             kCyan, teamId);
    } else if (id == QLatin1String("noPlayersToField")) {
        push(QStringLiteral("No players to field!"), kOrange, teamId);
    } else if (id == QLatin1String("briberyAndCorruptionReRoll")) {
        push(QStringLiteral("Bribery & Corruption (re-roll)"), kBlue, teamId);
    } else if (id == QLatin1String("fanFactor")) {
        DiceLogLine l;
        l.text = QStringLiteral("Fan Factor");
        l.color = kPurple;
        l.actorId = teamId;
        l.d6Roll = num(report, "dedicatedFansRoll");
        out.append(l);
    } else if (id == QLatin1String("winningsRoll")) {
        DiceLogLine l;
        l.text = QStringLiteral("Winnings Roll");
        l.color = kGray;
        l.d6Rolls << QString::number(num(report, "winningsRollHome"));
        l.d6Rolls << QString::number(num(report, "winningsRollAway"));
        l.d6Roll = num(report, "winningsRollHome");
        out.append(l);
    } else if (id == QLatin1String("penaltyShootout")) {
        DiceLogLine l;
        l.text = QStringLiteral("Penalty Shootout");
        l.color = kYellow;
        l.d6Rolls << QString::number(num(report, "rollHome"));
        l.d6Rolls << QString::number(num(report, "rollAway"));
        l.d6Roll = num(report, "rollHome");
        out.append(l);
    } else if (id == QLatin1String("playerEvent")) {
        push(str(report, "message").isEmpty() ? QStringLiteral("event") : str(report, "message"),
             kGray, actorId);
    } else if (id == QLatin1String("teamEvent")) {
        push(str(report, "message").isEmpty() ? QStringLiteral("event") : str(report, "message"),
             kGray, teamId);
    } else if (id == QLatin1String("event")) {
        push(str(report, "message").isEmpty() ? QStringLiteral("event") : str(report, "message"),
             kGray);
    } else if (id == QLatin1String("winnings") || id == QLatin1String("gameOver")) {
        push(QStringLiteral("=== Game Over ==="), kGray);
    } else if (id == QLatin1String("weather")) {
        const QString w = enumName(report.value(QStringLiteral("weather")));
        push(QStringLiteral("Weather: %1")
                 .arg(w.isEmpty() ? QStringLiteral("Unknown") : humanize(w)),
             kCyan);
    } else if (id == QLatin1String("spectators")) {
        push(QStringLiteral("Spectators"), kPurple);
    } else if (id == QLatin1String("argueTheCall")) {
        push(flag(report, "argueTheCallSuccessful") ? QStringLiteral("Argues the call — successful!")
                                                    : QStringLiteral("Argues the call — not successful"),
             kOrange, actorId);
    } else if (id == QLatin1String("kickoffRiot")) {
        push(QStringLiteral("Kickoff: RIOT!"), kOrange);
    } else if (id == QLatin1String("kickoffTimeout")) {
        push(QStringLiteral("Kickoff: Timeout!"), kOrange);
    }
    // Every report not matched above (no-dice / bookkeeping / exotic
    // inducements: balefulHex, allYouCanEat, brilliantCoachingReRoll, ...)
    // falls through to the clean named line below — NEVER the raw JSON
    // payload (user rule: no numeric ids / raw strings in the log).

    // Unknown report: a clean named line, never the raw JSON (round 9: the
    // old fallback dumped the payload, leaking ids into the log).
    if (out.isEmpty())
        push(humanize(id), kGray, actorId, defenderId);

    return out;
}
