// =============================================================================
// ActionLogWidget — Mockup F action log (bottom-left overlay)
//
// Design (Mockup F):
//   * a very translucent light card pinned to the stage corner (no title
//     bar chrome — just "ACTION LOG" + an expand chevron, like the mockup)
//   * bold ink text on the near-transparent card
//   * rolls are rendered as DIE FACE IMAGES (the official GW-style block
//     dice and the red d6 from assets/dice/), never as numbers: the
//     structured DiceLogLine carries the faces, this widget draws them
//   * player/team NAMES (resolved against the model by the caller), not
//     raw numeric ids
//
// Readability (Blood Bowl 3 style): player names in TEAM COLOR + ":",
// action text, DICE that are extremely readable and COHERENT WITH THE
// TEXT (sized to the text, crisp), SUCCESS/FAILURE emphasized in green/red,
// comfortable row spacing, and turn separators.
//
// Hover: hovering a player NAME shows that player's info card. The hover
// handlers are an EVENT FILTER on the log (never a std::function captured
// by the row widget): deleting rows while the mouse is over them (log
// re-sync / clear) fires enter/leave on freed NameLabels -> crash. The
// filter is owned by the (long-lived) log and auto-removed with the row.
//
// Scroll: STICK-TO-BOTTOM — a new entry scrolls the log to the newest line
// ONLY if the user is already at the bottom; if the user scrolled up to
// read older entries, new entries do NOT yank the view down.
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
class QScrollBar;

// The auto-scroll toggle: a visible WHITE SQUARE with a check inside, so
// it reads as a clickable checkbox at a glance.
class AutoScrollToggle : public QWidget
{
    Q_OBJECT

public:
    explicit AutoScrollToggle(QWidget *parent = nullptr);
    bool checked() const { return m_on; }
    void setChecked(bool on);

signals:
    void toggled(bool on);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    bool m_on = true;
};

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
    // Hover on a player NAME in the log — the raw id (empty = the mouse
    // left the name). The caller shows/hides the info card.
    void playerHoveredId(const QString &playerId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    // The name-label hover lives HERE (owned by the log, safe against row
    // deletion under the mouse).
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void addEntry(const DiceLogLine &line, const QString &actorName,
                  const QString &defenderName, const QString &actorId,
                  const QString &defenderId, const QColor &actorColor,
                  const QColor &defenderColor);
    // True if the user is at (or near) the BOTTOM of the scroll.
    bool atBottom() const;
    // Scroll the newest line into view (safe for user and programmatic scrolls).
    void scrollToEnd();
    // Live stick state from a USER scrollbar move.
    void onScrollbarMoved();
    // Settle the scroll (0ms, coalesced): re-lay out the inner widget and
    // scroll to the end when stick-to-bottom is on.
    void settleScroll();
    // The content height from the ACTUAL laid-out rows (never
    // the layout sizeHint: a flow row's height depends on its width and
    // the cached hints go stale mid-layout, inflating the stack until
    // the bottom stretch swallows the newest lines).
    int contentHeight() const;

    QScrollArea *m_scroll = nullptr;
    QWidget *m_stack = nullptr;
    QVBoxLayout *m_stackLayout = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_chevron = nullptr;
    // Autoscroll is LIVE — it re-arms the moment the user brings the log
    // back to the bottom (stick-to-bottom), and the "auto-scroll" checkbox
    // (white square + check) shows/controls it.
    AutoScrollToggle *m_autoCheck = nullptr;
    bool m_stick = true;      // follow the newest line
    bool m_progScroll = false; // value changes we caused (not user scrolls)
    bool m_settlePending = false; // one settle pass per event-loop iteration
    bool m_expanded = false;
    int m_entryCount = 0;
    QStringList m_plain; // toPlainText() backing store
};
