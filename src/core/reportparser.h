// =============================================================================
// ReportParser — FUMBBL server reports -> dice log lines
//
// C++ port of the TS client's reportParser.ts (itself based on the ffb-common
// Report*.java classes). Spectator subset: the common reports are rendered;
// unknown reports produce a neutral line so nothing is silently lost.
//
// Report JSON shape (server): { reportId: "<name>", ...type-specific fields }
// Roll fields shared by most rolls: roll, minimumRoll, successful, reRolled,
// rollModifiers[], statBasedRollModifier {name, value}.
// =============================================================================
#pragma once

#include <QColor>
#include <QJsonObject>
#include <QVector>

// One action-log entry. The dice are carried as STRUCTURED data, not as
// numbers in the text: the UI renders them as official-style die face
// images (Mockup F round 8: "non ci mettere i numeri: metti i dadi").
struct DiceLogLine {
    // Action description ("Dodge", "Pass (Incomplete)", "is OUT!", ...).
    // May still contain an unresolved id token for actorId/defenderId — the
    // UI substitutes display names before showing it.
    QString text;
    QColor color;
    // Primary actor's server id (player OR team). The UI resolves it to a
    // display name against the GameModel, so the log shows
    // "Adrian Custennin (1) Dodge…" instead of a raw numeric id.
    QString actorId;
    // Secondary participant (defender in a block, victim of an injury).
    QString defenderId;
    // d6 roll: 0 = no d6 in this line; 1..6 -> rendered as a d6 face image.
    int d6Roll = 0;
    int d6Target = 0;        // the "vs N" threshold (0 = hide)
    bool d6Ok = false;       // success -> green check, failure -> red cross
    bool d6ReRolled = false; // append [RR]
    // Block roll: the six faces ("1".."6"), rendered as block die images.
    QStringList blockFaces;
};

// Parses one report object into zero or more dice log lines.
QVector<DiceLogLine> parseReport(const QJsonObject &report);
