// =============================================================================
// ReportParser — FUMBBL server reports -> action log lines
//
// FIELD-BY-FIELD ALIGNED with the authoritative ffb-common Java sources:
//   /home/saruman/Projects/ffb/ffb-common/src/main/java/com/fumbbl/ffb/
//     - report/Report*.java (+ report/mixed, report/bb2025 variants)
//     - json/IJsonOption.java   (the actual on-wire JSON key names)
//   and the ffb-server step classes (who emits what, with which id).
//
// Field names verified against the Java sources (early guessing produced
// real bugs):
//   * ReportPlayerAction carries the subject in `actingPlayerId`, NOT
//     `playerId`  -> "move"/"stand up" lost their subject
//   * ReportBlock carries ONLY `defenderId` (the player being blocked;
//     there is no attackerId) and NO dice
//   * the six block dice arrive in the `blockRoll` report
//     (choosingTeamId + blockRoll[6] + defenderId), not in `block`
//   * the dice choice + outcome arrive in `blockChoice`
//     (nrOfDice, blockRoll[6], diceIndex, blockResult, defenderId)
//   * ReportLeader / TeamCaptain / PlayCard / ... reference a TEAM id
//     (`teamId`/`choosingTeamId`), resolved to a team name by the UI
//   * ReportTimeoutEnforced carries the coach NAME (a plain string)
//
// The LIVE ground truth is captured with tools/reports_capturer (one raw
// report JSON per line): after a live capture, re-align against that file.
//
// Report JSON shape (server): { reportId: "<name>", ...type-specific fields }
// Roll fields shared by most skill rolls (ReportSkillRoll.java):
//   playerId, roll, minimumRoll, successful, reRolled, rollModifiers[]
// Enums serialize as {"name": "..."}; coordinates as {"x": n, "y": n}.
// =============================================================================
#pragma once

#include <QColor>
#include <QJsonObject>
#include <QVector>

// One action-log entry. The dice are carried as STRUCTURED data, not as
// numbers in the text: the UI renders them as official-style die face
// images (dice are never shown as bare numbers).
struct DiceLogLine {
    // Action description ("Dodge", "Pass (Incomplete)", "is OUT!", ...).
    // May still contain an unresolved id token for actorId/defenderId — the
    // UI substitutes display names before showing it.
    QString text;
    QColor color;
    // Primary actor's server id (player OR team — or a plain name string,
    // e.g. the coach in timeoutEnforced; the UI passes names through).
    QString actorId;
    // Secondary participant (defender in a block, victim of an injury).
    QString defenderId;
    // d6 roll: 0 = no d6 in this line; 1..6 -> rendered as a d6 face image.
    // With d6Rolls (multi-die reports like throw-in/scatter), d6Roll is the
    // FIRST die (it drives the "vs N" / ok-cross rendering).
    int d6Roll = 0;
    int d6Target = 0;        // the "vs N" threshold (0 = hide)
    bool d6Ok = false;       // success -> green check, failure -> red cross
    bool d6ShowResult = false; // true only when the report carries a
                               // `successful` field (throw-ins & co. have
                               // dice WITHOUT a win/lose result — no cross)
    bool d6ReRolled = false; // append [RR]
    // Multiple d6 faces (throw-in distance, scatter, dedicated fans...):
    // each 1..6, rendered as d6 face images. Empty = single d6Roll only.
    QStringList d6Rolls;
    // Block roll: the six faces ("1".."6"), rendered as block die images.
    QStringList blockFaces;
    // Structural hints for the UI.
    bool separator = false;     // a turn/half/coin separator line (centered)
    bool actorIsPlayer = true;   // actorId is a PLAYER id (hover -> card)
    bool defenderIsPlayer = true; // defenderId is a PLAYER id
};

// Parses one report object into zero or more dice log lines.
QVector<DiceLogLine> parseReport(const QJsonObject &report);

// A structured toast event for the live overlay (R0), extracted from the
// SAME report JSON as the log lines (the log stays human text; the overlay
// needs structure). kind == "" (empty vector) = the report carries no toast.
struct ToastEvent {
    QString kind; // "completion" | "regeneration" | "raiseDead" | "timeout" | "injury"
    QString playerId; // subject (catcher / rolled player / injured defender)
    QString text; // free text (timeout: the coach's name)
    QString result; // injury: the outcome enum (knockedOut/badlyHurt/seriousInjury/rip;
                    // empty = stunned or no injury)
    bool armorBroken = false; // injury: the armor roll broke the armor
    bool success = false;     // skill rolls: the roll succeeded
};

// Extracts the toast events from one report (empty = no toast for it).
QVector<ToastEvent> parseToastEvents(const QJsonObject &report);
