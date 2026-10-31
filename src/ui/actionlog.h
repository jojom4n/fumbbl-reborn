// =============================================================================
// ActionLogWidget — Mockup F action log (bottom-left overlay)
//
// Round 8 redesign (user: "l'action log deve essere come nel mockup",
// "non ci mettere i numeri: metti i dadi", "ci metti i nomi giusti"):
//   * a very translucent light card pinned to the stage corner (no title
//     bar chrome — just "ACTION LOG" + an expand chevron, like the mockup)
//   * bold ink text on the near-transparent card
//   * rolls are rendered as DIE FACE IMAGES (the official GW-style block
//     dice and the red d6 from assets/dice/), never as numbers: the
//     structured DiceLogLine carries the faces, this widget draws them
//   * player/team NAMES (resolved against the model by the caller), not
//     raw numeric ids
//
// Round 13 TURN 4 (user): the log must be ORDERED AND READABLE LIKE
// BLOODBOWL 3 (ref faces-work bb3 log): player names in TEAM COLOR +
// ":", action text, DICE that are extremely readable and COHERENT WITH
// THE TEXT (sized to the text, crisp), SUCCESS/FAILURE emphasized in
// green/red, comfortable row spacing, and turn separators.
//
// Hover (round 13): hovering a player NAME shows that player's info card.
// Round 13 turn 4: the hover handlers are an EVENT FILTER on the log
// (never a std::function captured by the row widget): deleting rows while
// the mouse is over them (log re-sync / clear) used to fire enter/leave on
// freed NameLabels -> crash. The filter is owned by the (long-lived) log
// and auto-removed with the row.
//
// Scroll (round 13 turn 4): STICK-TO-BOTTOM — a new entry scrolls the log
// to the newest line ONLY if the user is already at the bottom; if the
// user scrolled up to read older entries, new entries do NOT yank the view
// down.
// =============================================================================
#pragma once

#include <QColor>
#include <QWidget>

#include "core/reportparser.h"

class QMouseEvent;
class QEvent;

class QScrollArea;
class QVBoxLayout;
class QLabel;
class QCheckBox;
class QScrollBar;

class ActionLogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ActionLogWidget(QWidget *parent = nullptr);

    // Append one parsed report line. `actorName` / `defenderName` are the
    // already-resolved display names (empty = no participant on that role);
    // `actorId` / `defenderId` are the raw ids (hover on a name shows that
    // player's card — never shown as text, only used to resolve the card);
    // `actorColor` / `defenderColor` tint the names (TEAM color, BB3 style;
    // empty color = neutral ink).
    void appendLine(const DiceLogLine &line, const QString &actorName = QString(),
                    const QString &defenderName = QString(),
                    const QString &actorId = QString(),
                    const QString &defenderId = QString(),
                    const QColor &actorColor = QColor(),
                    const QColor &defenderColor = QColor());
    void clearLog();

    // Plain-text dump of all entries (tests / debugging).
    QString toPlainText() const;

    // Compact (3 rows) vs expanded (10 rows) heights.
    static int compactHeight();
    static int expandedHeight();

signals:
    void expandedChanged(bool expanded);
    // Round 13: hover on a player NAME in the log — the raw id (empty =
    // the mouse left the name). The caller shows/hides the info card.
    void playerHoveredId(const QString &playerId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    // Round 13 turn 4: the name-label hover lives HERE (owned by the log,
    // safe against row deletion under the mouse).
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // The middle part of a row: action text + dice + vs/result (rich text).
    QString renderMiddle(const DiceLogLine &line) const;
    void addEntry(const DiceLogLine &line, const QString &actorName,
                  const QString &defenderName, const QString &actorId,
                  const QString &defenderId, const QColor &actorColor,
                  const QColor &defenderColor);
    // True if the user is at (or near) the BOTTOM of the scroll.
    bool atBottom() const;
    // Scroll the newest line into view (user + programmatic-safe).
    void scrollToEnd();
    // Live stick state from a USER scrollbar move (turn 9).
    void onScrollbarMoved();
    // Settle the scroll (0ms, coalesced): re-lay out the inner widget and
    // scroll to the end when stick-to-bottom is on.
    void settleScroll();

    QScrollArea *m_scroll = nullptr;
    QWidget *m_stack = nullptr;
    QVBoxLayout *m_stackLayout = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_chevron = nullptr;
    // Round 13 turn 9 (user): autoscroll is LIVE — it re-arms the moment
    // the user brings the log back to the bottom (stick-to-bottom), and a
    // tiny "auto" checkbox in the title bar shows/controls it.
    QCheckBox *m_autoCheck = nullptr;
    bool m_stick = true;      // follow the newest line
    bool m_progScroll = false; // value changes we caused (not user scrolls)
    bool m_settlePending = false; // one settle pass per event-loop iteration
    bool m_expanded = false;
    int m_entryCount = 0;
    QStringList m_plain; // toPlainText() backing store
};
