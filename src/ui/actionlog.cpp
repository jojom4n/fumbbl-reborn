// =============================================================================
// ActionLogWidget — implementation (see actionlog.h for the design rules)
// =============================================================================
#include "ui/actionlog.h"

#include <QEnterEvent>
#include <QEvent>
#include <QFunctionPointer>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <QVBoxLayout>

#include <functional>

namespace {

// Round 13: 40px rows and 32px dice (the 48/44 were "eccessivamente
// grandi" — the log looked big and confusing, user). Spacing between
// consecutive dice is done with explicit &nbsp; (Qt's rich-text <img> does
// NOT honor `hspace`, which is why the block dice used to touch).
// Round 13 (user): dice must be PROPORTIONAL to the text ("sempre troppo
// grandi... proporzionati al testo"). The log font is 10pt (~16px line), so
// the dice sit at 20px — clearly a die, but inline with the text, not a big
// red block. Row height matches.
const int kRowH = 28;
const int kDice = 20;
const int kHeaderH = 26;   // title + chevron strip
const int kPad = 8;
const int kMaxEntries = 300;

// The parser's palette was tuned for the dark theme; on the light Mockup-F
// card every accent is darkened so it stays readable.
QColor inkColor(const QColor &c)
{
    if (c.alpha() < 10)
        return QColor(34, 40, 50);
    QColor d = c;
    d.setAlpha(255);
    return d.darker(185);
}

// Round 13: a player NAME in the log — hover shows that player's info card
// (user: "hover sui nomi dei giocatori nell'action log → mostra la relativa
// scheda, come il click sul campo"). Plain QLabel + enter/leave events (no
// Q_OBJECT needed: we override virtuals on an existing QObject base).
class NameLabel : public QLabel
{
public:
    NameLabel(const QString &playerId, const QString &html,
              const std::function<void(const QString &)> &hoverCb,
              QWidget *parent)
        : QLabel(html, parent), m_id(playerId), m_cb(hoverCb)
    {
        setCursor(Qt::PointingHandCursor);
    }
    QString playerId() const { return m_id; }

protected:
    void enterEvent(QEnterEvent *) override { m_cb(m_id); }
    void leaveEvent(QEvent *) override { m_cb(QString()); }

private:
    QString m_id;
    std::function<void(const QString &)> m_cb;
};

} // namespace

ActionLogWidget::ActionLogWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("action_log"));
    setFixedHeight(compactHeight());

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Header: "ACTION LOG" + expand chevron (mockup style).
    auto *header = new QWidget(this);
    header->setFixedHeight(kHeaderH);
    auto *hl = new QHBoxLayout(header);
    hl->setContentsMargins(kPad, 0, kPad, 0);
    m_title = new QLabel(QStringLiteral("ACTION LOG"), header);
    m_title->setStyleSheet(QStringLiteral("color: #222832; font-weight: bold;"));
    QFont tf = m_title->font();
    tf.setPointSizeF(9);
    m_title->setFont(tf);
    // Round 9 (user): the chevron points where the click TAKES the log —
    // compact shows ▴ ("click to expand"), expanded shows ▾ ("click to
    // collapse"). The old mapping was inverted.
    m_chevron = new QLabel(QString::fromUtf8("\u25b4"), header); // ▴
    m_chevron->setCursor(Qt::PointingHandCursor);
    m_chevron->setStyleSheet(QStringLiteral("color: #222832; font-size: 13px; font-weight: bold;"));
    m_chevron->setFixedWidth(16);
    hl->addWidget(m_title);
    hl->addStretch(1);
    hl->addWidget(m_chevron);
    outer->addWidget(header);

    // Entries live in a scroll area (translucent, no frame).
    m_scroll = new QScrollArea(this);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setWidgetResizable(true);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    m_stack = new QWidget(m_scroll);
    m_stack->setStyleSheet(QStringLiteral("background: transparent;"));
    m_stackLayout = new QVBoxLayout(m_stack);
    m_stackLayout->setContentsMargins(kPad, 2, kPad, 2);
    m_stackLayout->setSpacing(2);
    m_stackLayout->addStretch(1);
    m_scroll->setWidget(m_stack);
    outer->addWidget(m_scroll, 1);
}

int ActionLogWidget::compactHeight()
{
    return kHeaderH + 3 * kRowH + kPad + 2;
}

int ActionLogWidget::expandedHeight()
{
    return kHeaderH + 10 * kRowH + kPad + 2;
}

void ActionLogWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    // Very translucent paper (mockup: the field shows right through).
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(252, 252, 254, 150));
    p.drawRoundedRect(r, 10, 10);
    QPen border(QColor(0, 0, 0, 90));
    border.setWidthF(1.2);
    p.setPen(border);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 10, 10);
}

QString ActionLogWidget::renderMiddle(const DiceLogLine &line) const
{
    // NOTE (round 9, user rule): a participant the UI could not resolve is
    // DROPPED, never shown as a raw id — "MAI id numerici a schermo".
    QString html = line.text.toHtmlEscaped();

    // Round 11: the dice use the PRE-SCALED assets (*_s.png) rendered 1:1 —
    // the 256px originals were re-scaled by the rich-text layout with fast
    // scaling (grainy d6, square pips, unreadable skulls).
    // Round 13 (user): 32px dice with RELIABLE spacing. The block dice used
    // to touch because `hspace` is NOT honored by Qt's rich-text <img>; use
    // an explicit non-breaking space before every die so consecutive dice are
    // always separated (block AND d6, the same way).
    // NOTE: `src` already carries the leading ':' (":/dice/..."), so the
    // format string must NOT add another one (that produced "::/dice/...",
    // an unresolvable resource path -> broken-image icons in the log).
    const auto dieImg = [](const QString &src) {
        return QStringLiteral(" &nbsp;<img src=\"%1\" width=\"%2\" height=\"%2\">")
            .arg(src)
            .arg(kDice)
            .arg(kDice);
    };
    if (!line.blockFaces.isEmpty()) {
        for (const QString &face : line.blockFaces) {
            const int v = face.toInt();
            if (v < 1 || v > 6)
                continue;
            html += dieImg(QStringLiteral(":/dice/block%1_s.png").arg(v));
        }
    }

    // d6 rolls: red die faces + target + result. d6Rolls covers multi-die
    // reports (throw-in, scatter, injury rolls, ...); d6Roll alone the
    // single-die skill rolls (round 9: every roll shows the DIE, never a
    // number).
    if (!line.d6Rolls.isEmpty()) {
        for (const QString &face : line.d6Rolls) {
            const int v = face.toInt();
            if (v < 1 || v > 6)
                continue;
            html += dieImg(QStringLiteral(":/dice/d6_%1_s.png").arg(v));
        }
    } else if (line.d6Roll > 0) {
        html += dieImg(QStringLiteral(":/dice/d6_%1_s.png").arg(line.d6Roll));
    }
    if (line.d6Roll > 0 || !line.d6Rolls.isEmpty()) {
        if (line.d6Target > 0)
            html += QStringLiteral(" <span>vs %1</span>").arg(line.d6Target);
        // The check/cross only when the report carries a real result
        // (`successful` field) — throw-ins & co. roll dice without one.
        if (line.d6ShowResult)
            html += line.d6Ok
                        ? QStringLiteral(" <b><span style=\"color:#2e7d32\">\u2713</span></b>")
                        : QStringLiteral(" <b><span style=\"color:#c62828\">\u2717</span></b>");
        if (line.d6ReRolled)
            html += QStringLiteral(" <span style=\"color:#5f6368\">[RR]</span>");
    }

    return html;
}

void ActionLogWidget::addEntry(const DiceLogLine &line, const QString &actorName,
                               const QString &defenderName, const QString &actorId,
                               const QString &defenderId)
{
    // Round 13: the row is [actor name | action text + dice | defender
    // name] — the two names are HOVERABLE (they show the player's card);
    // the middle is plain rich text.
    auto *row = new QWidget(m_stack);
    auto *rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(3);

    const auto style = QStringLiteral("background: transparent; color: #222832;");
    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(10);

    const auto hover = [this](const QString &id) { emit playerHoveredId(id); };

    if (!actorName.isEmpty()) {
        auto *nl = new NameLabel(
            actorId, QStringLiteral(
                         "<b><span style=\"color:#8a6d1f\">%1</span></b>")
                         .arg(actorName.toHtmlEscaped()),
            hover, row);
        nl->setStyleSheet(style);
        nl->setFont(f);
        rl->addWidget(nl, 0, Qt::AlignVCenter);
    }

    auto *mid = new QLabel(renderMiddle(line), row);
    mid->setTextFormat(Qt::RichText);
    mid->setStyleSheet(style);
    mid->setFont(f);
    rl->addWidget(mid, 1, Qt::AlignVCenter);

    if (!defenderName.isEmpty()) {
        auto *dl = new NameLabel(
            defenderId, QStringLiteral(
                            "<b><span style=\"color:%1\">%2</span></b>")
                            .arg(inkColor(line.color).name())
                            .arg(defenderName.toHtmlEscaped()),
            hover, row);
        dl->setStyleSheet(style);
        dl->setFont(f);
        rl->addWidget(dl, 0, Qt::AlignVCenter);
    }

    row->setFixedHeight(kRowH);
    m_stackLayout->insertWidget(m_stackLayout->count() - 1, row); // before the stretch

    // Plain dump: actor + text + "→ defender" (the defender is part of the
    // visible line, so it belongs in the plain form too).
    QString plain = QStringLiteral("%1 %2").arg(actorName, line.text).trimmed();
    if (!defenderName.isEmpty())
        plain += QStringLiteral(" \u2192 %1").arg(defenderName);
    m_plain << plain;
    ++m_entryCount;
    while (m_plain.size() > kMaxEntries)
        m_plain.removeFirst();

    // Keep the newest entry visible.
    m_scroll->ensureWidgetVisible(row);
    const auto sb = m_scroll->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void ActionLogWidget::appendLine(const DiceLogLine &line, const QString &actorName,
                                 const QString &defenderName, const QString &actorId,
                                 const QString &defenderId)
{
    addEntry(line, actorName, defenderName, actorId, defenderId);
}

void ActionLogWidget::mousePressEvent(QMouseEvent *event)
{
    // Chevron click: expand/collapse the log (mockup behavior).
    const QPoint inChevron = m_chevron ? m_chevron->mapFrom(this, event->pos()) : QPoint(-1, -1);
    if (m_chevron && m_chevron->rect().contains(inChevron)) {
        m_expanded = !m_expanded;
        setFixedHeight(m_expanded ? expandedHeight() : compactHeight());
        m_chevron->setText(m_expanded ? QString::fromUtf8("\u25be") : QString::fromUtf8("\u25b4"));
        emit expandedChanged(m_expanded);
    }
    QWidget::mousePressEvent(event);
}

void ActionLogWidget::clearLog()
{
    // Round 13: entries are ROW WIDGETS now (name labels inside) — delete
    // the direct children (the rows), not just top-level labels.
    for (auto *w : m_stack->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
        w->deleteLater();
    m_plain.clear();
    m_entryCount = 0;
}

QString ActionLogWidget::toPlainText() const
{
    return m_plain.join(QStringLiteral("\n"));
}
