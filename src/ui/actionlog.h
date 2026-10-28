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
// Round 13 (user): the dice were "piccoli, senza spacing, simboli non
// leggibili" — rows are now 48px, dice 44px with real spacing (hspace 8)
// between consecutive dice, and HOVERING A PLAYER'S NAME shows that
// player's info card (like the field hover/click).
// =============================================================================
#pragma once

#include <QWidget>

#include "core/reportparser.h"

class QMouseEvent;

class QScrollArea;
class QVBoxLayout;
class QLabel;

class ActionLogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ActionLogWidget(QWidget *parent = nullptr);

    // Append one parsed report line. `actorName` / `defenderName` are the
    // already-resolved display names (empty = no participant on that role);
    // `actorId` / `defenderId` are the raw ids (round 13: hover on a name
    // shows that player's card — never shown as text, only used to resolve
    // the card).
    void appendLine(const DiceLogLine &line, const QString &actorName = QString(),
                    const QString &defenderName = QString(),
                    const QString &actorId = QString(),
                    const QString &defenderId = QString());
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

private:
    // The middle part of a row: action text + dice + vs/result (rich text).
    QString renderMiddle(const DiceLogLine &line) const;
    void addEntry(const DiceLogLine &line, const QString &actorName,
                  const QString &defenderName, const QString &actorId,
                  const QString &defenderId);

    QScrollArea *m_scroll = nullptr;
    QWidget *m_stack = nullptr;
    QVBoxLayout *m_stackLayout = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_chevron = nullptr;
    bool m_expanded = false;
    int m_entryCount = 0;
    QStringList m_plain; // toPlainText() backing store
};
