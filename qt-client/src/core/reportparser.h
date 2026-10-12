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

struct DiceLogLine {
    QString text;
    QColor color;
};

// Parses one report object into zero or more dice log lines.
QVector<DiceLogLine> parseReport(const QJsonObject &report);
