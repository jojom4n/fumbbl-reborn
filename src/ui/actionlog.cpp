// =============================================================================
// ActionLogWidget — implementation (BB3-like readability, round 13 turn 4)
// =============================================================================
#include "ui/actionlog.h"

#include <QCheckBox>
#include <QEnterEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <QTimer>
#include <QApplication>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace {

// Round 13 turn 4 (user): BB3-like readability. The dice must be
// "estremamente leggibili e coerenti col testo" — 24px dice against a
// 10.5pt line (the 20px/10pt of round 13 turn 3 read as "tiny, not
// coherent"). Rows get real height and the dice sit at ~1.5x the text cap
// height, like Blood Bowl 3's log.
const int kRowH = 30;
const int kDice = 24;
const int kHeaderH = 26;   // title + chevron strip
const int kPad = 8;
const int kMaxEntries = 300;

const QColor kInk(34, 40, 50);
const QColor kOkGreen(46, 125, 50);
const QColor kFailRed(198, 40, 40);
const QColor kMuted(110, 116, 128);

// A player/team NAME in the log — the hover is handled by the LOG's event
// filter (round 13 turn 4: the old std::function captured the row widget;
// clearing the log with the mouse on a name ran enter/leave on freed
// memory -> crash). The label only carries its id in a dynamic property.
const char kNameIdProp[] = "logNameId";
const char kNameIsPlayerProp[] = "logNameIsPlayer";

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
    // Round 13 turn 9 (user): the tiny "auto" autoscroll checkbox in the
    // title bar (small font). It reflects the live stick-to-bottom state:
    // unchecks when the user scrolls up, re-checks when the user brings the
    // log back to the bottom; the user can also tick it to force following.
    // Turn 15 (user): the label was "auto" — now "auto-scroll".
    m_autoCheck = new QCheckBox(QStringLiteral("auto-scroll"), header);
    m_autoCheck->setStyleSheet(QStringLiteral("color: #222832; background: transparent;"));
    QFont cf = m_autoCheck->font();
    cf.setPointSizeF(8);
    m_autoCheck->setFont(cf);
    m_autoCheck->setChecked(true);
    connect(m_autoCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_stick = on;
        if (on)
            scrollToEnd();
    });
    hl->addWidget(m_title);
    hl->addStretch(1);
    hl->addWidget(m_autoCheck);
    hl->addWidget(m_chevron);
    outer->addWidget(header);

    // Entries live in a scroll area (translucent, no frame).
    m_scroll = new QScrollArea(this);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // Round 13 turn 9: the inner stack is sized MANUALLY (not widgetResizable):
    // a widgetResizable QScrollArea re-lays out the inner widget only from
    // its own resizeEvent, so a burst of appends (live game) left the
    // scrollbar range stale (maximum frozen at the old content height).
    // Manual geometry = the classic pattern, fully deterministic.
    m_scroll->setWidgetResizable(false);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    m_stack = new QWidget(m_scroll);
    m_stack->setStyleSheet(QStringLiteral("background: transparent;"));
    m_stackLayout = new QVBoxLayout(m_stack);
    m_stackLayout->setContentsMargins(kPad, 2, kPad, 2);
    m_stackLayout->setSpacing(3);
    m_stackLayout->addStretch(1);
    m_scroll->setWidget(m_stack);
    outer->addWidget(m_scroll, 1);

    // The scrollbar's value is watched to know when the USER scrolls:
    // at the bottom -> stick (auto) re-arms; up -> it pauses. Programmatic
    // scrolls (m_progScroll) are ignored so the state is user-driven only.
    connect(m_scroll->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int) { onScrollbarMoved(); });
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
    p.setBrush(QColor(252, 252, 254, 165));
    p.drawRoundedRect(r, 10, 10);
    QPen border(QColor(0, 0, 0, 90));
    border.setWidthF(1.2);
    p.setPen(border);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 10, 10);
}

void ActionLogWidget::onScrollbarMoved()
{
    // Round 13 turn 9: USER scroll detection. A value change that is NOT
    // ours = the user moved the view: at the bottom (or a hair above) ->
    // autoscroll re-arms ("dovrebbe scrollare automaticamente se l'utente
    // lo riporta in fondo"); up -> it pauses and the checkbox unchecks.
    if (m_progScroll)
        return;
    const bool at = atBottom();
    if (at != m_stick) {
        m_stick = at;
        if (m_autoCheck) {
            m_autoCheck->blockSignals(true);
            m_autoCheck->setChecked(m_stick);
            m_autoCheck->blockSignals(false);
        }
    }
}

// A widgetResizable QScrollArea re-lays out its inner widget only on its
// OWN resizeEvent; a burst of appends (live game: dozens of reports in a
// row) can outgrow the viewport without one arriving, leaving the scrollbar
// range stale (maximum stuck at the old content height). Nudge the size by
// a pixel and back: forces the inner layout + scrollbar range to settle on
// the new content height. Coalesced to one pass per event-loop iteration
// (m_settlePending) so a batch of appends settles exactly once, after the
// whole batch has laid out.
void ActionLogWidget::settleScroll()
{
    if (m_settlePending)
        return;
    m_settlePending = true;
    QTimer::singleShot(0, this, [this]() {
        m_settlePending = false;
        if (!m_scroll)
            return;
        // The range churn during the nudge is OURS: ignore it in the
        // user-scroll detection so it can never flip the stick state.
        m_progScroll = true;
        // Manual inner geometry: as wide as the viewport, as tall as the
        // content needs (at least the viewport, so an empty log fills it).
        m_stackLayout->invalidate(); // force the size cache to recompute
        const int w = m_scroll->viewport()->width();
        const int need = qMax(m_scroll->viewport()->height(),
                              m_stackLayout->sizeHint().height());
        m_stack->setGeometry(0, 0, w, need);
        m_progScroll = false;
        if (m_stick)
            scrollToEnd();
    });
}

void ActionLogWidget::scrollToEnd()
{
    if (!m_scroll)
        return;
    auto *sb = m_scroll->verticalScrollBar();
    m_progScroll = true;
    sb->setValue(sb->maximum());
    // The flag must survive this event-loop pass (Qt may re-emit
    // valueChanged when the range settles after the layout) but not the
    // next user scroll: a 0ms timer clears it right after the batch.
    QTimer::singleShot(0, this, [this] { m_progScroll = false; });
}

bool ActionLogWidget::eventFilter(QObject *watched, QEvent *event)
{
    // Round 13 turn 4: the NAME hover. `watched` is a name label (the id is
    // in a dynamic property); entering emits the id, leaving emits empty.
    // This filter is owned by the LOG (which outlives its rows) and Qt
    // removes it automatically when the row is deleted — no freed-memory
    // callbacks, ever.
    if (watched->property(kNameIdProp).isValid()) {
        if (event->type() == QEvent::Enter) {
            emit playerHoveredId(watched->property(kNameIdProp).toString());
            return false;
        }
        if (event->type() == QEvent::Leave) {
            emit playerHoveredId(QString());
            return false;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool ActionLogWidget::atBottom() const
{
    // Stick-to-bottom (round 13 turn 4): true when the user is already at
    // (or within a hair of) the newest line. New entries only auto-scroll
    // in that case; a user reading the history is never yanked down.
    const QScrollBar *sb = m_scroll ? m_scroll->verticalScrollBar() : nullptr;
    if (!sb)
        return true;
    return sb->maximum() - sb->value() <= 8;
}

QString ActionLogWidget::renderMiddle(const DiceLogLine &line) const
{
    // NOTE (round 9, user rule): a participant the UI could not resolve is
    // DROPPED, never shown as raw ids — "MAI id numerici a schermo".
    QString html = line.text.toHtmlEscaped();

    // Dice: the PRE-SCALED assets (*_s.png, 96px) rendered at kDice —
    // downscaling stays crisp at any DPI (4K-ready). Spacing between
    // consecutive dice is an explicit thin gap element (Qt rich text does
    // NOT honor `hspace` on <img>).
    // Round 13 turn 8: NO QString::arg here. The triple .arg() chain on a
    // template with a REPEATED %2 placeholder made Qt 6.12 emit
    // "QString::arg: Argument missing" warnings (visible in the user's crash
    // log — harmless to the render, but noise where the real crash lived).
    // Plain concatenation: zero placeholders, zero arg calls, zero warnings.
    // Turn 12 BUG (user: "TUTTI i dadi non appaiono"): the opening quotes
    // of the attribute VALUES were missing (`src=:/dice/x.png"` instead of
    // `src=":/dice/x.png"`) — the HTML parser swallowed the whole
    // `:/dice/x.png"` as the unquoted value, the resource never resolved
    // and every die rendered as an empty broken-image box.
    const auto dieImg = [](const QString &src) {
        const QString sz = QString::number(kDice);
        return QStringLiteral(" <img src=\"") + src
             + QStringLiteral("\" width=\"") + sz
             + QStringLiteral("\" height=\"") + sz
             + QStringLiteral("\"><span style=\"color:#ffffff\">&nbsp;</span>");
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
            html += QStringLiteral(" <span style=\"color:%1\">vs %2</span>")
                        .arg(kMuted.name())
                        .arg(line.d6Target);
        // Round 13 turn 4 (BB3): the RESULT is emphasized — green SUCCESS /
        // red Failure, bold, like Blood Bowl 3's log.
        if (line.d6ShowResult)
            html += line.d6Ok
                        ? QStringLiteral(" <b style=\"color:%1\">SUCCESS</b>").arg(kOkGreen.name())
                        : QStringLiteral(" <b style=\"color:%1\">Failure</b>").arg(kFailRed.name());
        if (line.d6ReRolled)
            html += QStringLiteral(" <span style=\"color:%1\">[RR]</span>").arg(kMuted.name());
    }

    return html;
}

void ActionLogWidget::addEntry(const DiceLogLine &line, const QString &actorName,
                               const QString &defenderName, const QString &actorId,
                               const QString &defenderId, const QColor &actorColor,
                               const QColor &defenderColor)
{
    // BB3-like row (round 13 turn 4):
    //   ActorName (TEAM color, bold, hoverable): action + dice + result  →
    //   DefenderName (TEAM color, bold, hoverable)
    // Separator lines (turn/half/coin): centered, muted, hairline above.
    const bool separator = line.separator;

    auto *row = new QWidget(m_stack);
    auto *rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(4);

    const QString style = QStringLiteral("background: transparent; color: #222832;");
    QFont f = font();
    f.setPointSizeF(10.5);

    if (separator) {
        // A quiet centered separator ("--- Turn ended ---" etc.): hairline
        // on top, muted small-caps text, hairline below.
        auto *sep = new QLabel(QStringLiteral("\u2014  %1  \u2014")
                                   .arg(line.text.toHtmlEscaped()), row);
        QFont sf = font();
        sf.setPointSizeF(8.5);
        sf.setBold(true);
        sep->setFont(sf);
        sep->setStyleSheet(QStringLiteral("background: transparent; color: #6a707c;"
                                          "border-top: 1px solid rgba(0,0,0,60);"));
        sep->setAlignment(Qt::AlignCenter);
        rl->addWidget(sep, 1);
        row->setFixedHeight(kRowH - 4);
        m_stackLayout->insertWidget(m_stackLayout->count() - 1, row); // before stretch
        m_plain << line.text;
        ++m_entryCount;
        while (m_plain.size() > kMaxEntries)
            m_plain.removeFirst();
        settleScroll(); // keep the scrollbar range honest + follow
        return;
    }

    const auto makeName = [this, row, f, style](const QString &id, const QString &name,
                                                const QColor &color, bool isPlayer) -> QLabel * {
        if (name.isEmpty() || id.isEmpty() || !isPlayer)
            return nullptr; // only PLAYER names are hoverable (team names
                            // have no card — keep them plain, not clickable)
        auto *nl = new QLabel(row);
        nl->setText(QStringLiteral("<b style=\"color:%1\">%2</b>")
                        .arg(color.isValid() ? color.name() : kInk.name())
                        .arg(name.toHtmlEscaped()));
        nl->setTextFormat(Qt::RichText);
        nl->setStyleSheet(style);
        nl->setFont(f);
        nl->setCursor(Qt::PointingHandCursor);
        nl->setProperty(kNameIdProp, id);
        nl->setProperty(kNameIsPlayerProp, true);
        nl->installEventFilter(this); // hover via the LOG (deletion-safe)
        return nl;
    };

    const bool actorPlayer = !actorName.isEmpty() && !actorId.isEmpty()
                             && actorColor.isValid() && line.actorIsPlayer;
    const bool defPlayer = !defenderName.isEmpty() && !defenderId.isEmpty()
                           && defenderColor.isValid() && line.defenderIsPlayer;

    if (!actorName.isEmpty()) {
        auto *nl = makeName(actorId, actorName, actorColor, line.actorIsPlayer);
        if (nl) {
            rl->addWidget(nl, 0, Qt::AlignVCenter);
        } else {
            auto *pl = new QLabel(QStringLiteral("<b style=\"color:%1\">%2</b>")
                                       .arg(actorColor.isValid() ? actorColor.name() : kInk.name())
                                       .arg(actorName.toHtmlEscaped()), row);
            pl->setTextFormat(Qt::RichText);
            pl->setStyleSheet(style);
            QFont bf = f;
            bf.setBold(true);
            pl->setFont(bf);
            rl->addWidget(pl, 0, Qt::AlignVCenter);
        }
        // BB3: "Name:" — the colon sticks to the name.
        auto *colon = new QLabel(QStringLiteral(":"), row);
        colon->setStyleSheet(style);
        QFont cf = f;
        cf.setBold(true);
        colon->setFont(cf);
        rl->addWidget(colon, 0, Qt::AlignVCenter);
    }

    auto *mid = new QLabel(renderMiddle(line), row);
    mid->setTextFormat(Qt::RichText);
    mid->setStyleSheet(style);
    mid->setFont(f);
    rl->addWidget(mid, 1, Qt::AlignVCenter);

    if (!defenderName.isEmpty()) {
        auto *nl = makeName(defenderId, defenderName, defenderColor, line.defenderIsPlayer);
        if (nl) {
            auto *arrow = new QLabel(QString::fromUtf8("\u2192"), row);
            arrow->setStyleSheet(QStringLiteral("background: transparent; color: #6a707c;"));
            rl->addWidget(arrow, 0, Qt::AlignVCenter);
            rl->addWidget(nl, 0, Qt::AlignVCenter);
        } else {
            auto *pl = new QLabel(
                QStringLiteral(" \u2192 <b style=\"color:%1\">%2</b>")
                    .arg(defenderColor.isValid() ? defenderColor.name() : kInk.name())
                    .arg(defenderName.toHtmlEscaped()), row);
            pl->setTextFormat(Qt::RichText);
            pl->setStyleSheet(style);
            QFont bf = f;
            bf.setBold(true);
            pl->setFont(bf);
            rl->addWidget(pl, 0, Qt::AlignVCenter);
        }
    }

    row->setFixedHeight(kRowH);
    m_stackLayout->insertWidget(m_stackLayout->count() - 1, row); // before the stretch
    settleScroll(); // turn 9: only when the user is (back) at the bottom

    // Plain dump: actor + text + "→ defender" (the defender is part of the
    // visible line, so it belongs in the plain form too).
    QString plain = QStringLiteral("%1 %2").arg(actorName, line.text).trimmed();
    if (!defenderName.isEmpty())
        plain += QStringLiteral(" \u2192 %1").arg(defenderName);
    m_plain << plain;
    ++m_entryCount;
    while (m_plain.size() > kMaxEntries)
        m_plain.removeFirst();

    // (Turn 9: the scroll happens right after the row is inserted —
    // m_stick is the live user-driven stick-to-bottom state.)
}

void ActionLogWidget::appendLine(const DiceLogLine &line, const QString &actorName,
                                 const QString &defenderName, const QString &actorId,
                                 const QString &defenderId, const QColor &actorColor,
                                 const QColor &defenderColor)
{
    addEntry(line, actorName, defenderName, actorId, defenderId, actorColor, defenderColor);
}

void ActionLogWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Manual inner geometry (turn 9): keep the stack as wide as the
    // viewport and at least as tall as the content.
    if (m_scroll && m_stack && m_stackLayout) {
        const int w = m_scroll->viewport()->width();
        const int need = qMax(m_scroll->viewport()->height(),
                              m_stackLayout->sizeHint().height());
        m_stack->setGeometry(0, 0, w, need);
        // Re-layout can move the scrollbar range: if we are following,
        // stay pinned to the newest line (the app re-lays out the log
        // constantly — the follow must survive it).
        if (m_stick) {
            auto *sb = m_scroll->verticalScrollBar();
            m_progScroll = true;
            sb->setValue(sb->maximum());
            m_progScroll = false;
        }
    }
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
    // Entries are ROW WIDGETS (name labels inside) — delete the direct
    // children (the rows), not just top-level labels. (Safe to do under the
    // mouse: the hover filter is removed with each row.)
    for (auto *w : m_stack->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
        w->deleteLater();
    m_plain.clear();
    m_entryCount = 0;
}

QString ActionLogWidget::toPlainText() const
{
    return m_plain.join(QStringLiteral("\n"));
}
