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
    // already-resolved display names (empty = no participant on that role).
    void appendLine(const DiceLogLine &line, const QString &actorName = QString(),
                    const QString &defenderName = QString());
    void clearLog();

    // Plain-text dump of all entries (tests / debugging).
    QString toPlainText() const;

    // Compact (3 rows) vs expanded (10 rows) heights.
    static int compactHeight();
    static int expandedHeight();

signals:
    void expandedChanged(bool expanded);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QString renderLine(const DiceLogLine &line, const QString &actorName,
                       const QString &defenderName) const;
    void addEntry(const QString &html, const QString &plain);

    QScrollArea *m_scroll = nullptr;
    QWidget *m_stack = nullptr;
    QVBoxLayout *m_stackLayout = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_chevron = nullptr;
    bool m_expanded = false;
    int m_entryCount = 0;
    QStringList m_plain; // toPlainText() backing store
};
