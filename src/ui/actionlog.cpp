// =============================================================================
// ActionLogWidget — implementation (BB3-like readability)
// =============================================================================
#include "ui/actionlog.h"

#include <QEnterEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
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

// BB3-like readability: the dice must be extremely readable and coherent
// with the text — 24px dice against a 10.5pt line (20px/10pt reads as
// "tiny, not coherent"). Rows get real height and the dice sit at ~1.5x
// the text cap height, like Blood Bowl 3's log.
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
// filter (a std::function captured by the row widget, run on a freed name
// label while the log was cleared under the mouse -> crash). The label
// only carries its id in a dynamic property.
const char kNameIdProp[] = "logNameId";
const char kNameIsPlayerProp[] = "logNameIsPlayer";

// The middle of a log row is a FLOW, not a single-line HBox: a long
// action text ("Catch (Inaccurate Pass or Scattered)") would be CLIPPED
// mid-word because the scroll area has no horizontal scrollbar.
// Item 0 is the action text (stretchy, word-wraps); the rest are atomic
// (dice, "vs N Result"). The flow: the text wraps WORD BY WORD across the
// available width; the atomics then sit right after the END OF THE LAST
// LINE (when it fits there) or drop to their own line below (when it
// doesn't). No item is ever clipped, no matter how narrow the log.
// The row grows to fit (minimum height, not fixed).
class LogMidLayout : public QLayout
{
public:
    explicit LogMidLayout(int hSpacing = 3, int vSpacing = 2)
        : m_h(hSpacing), m_v(vSpacing) {}
    ~LogMidLayout() override { qDeleteAll(itemList); }
    void addItem(QLayoutItem *item) override { itemList.append(item); }
    int count() const override { return itemList.size(); }
    QLayoutItem *itemAt(int i) const override
    { return (i >= 0 && i < itemList.size()) ? itemList[i] : nullptr; }
    QLayoutItem *takeAt(int i) override { return itemList.takeAt(i); }
    void setGeometry(const QRect &rect) override { doLayout(rect); }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return measure(width).h; }
    QSize sizeHint() const override
    {
        // The width is dictated by the row (stretch 1): size against the
        // current width so the wrapped height is right from the start.
        // (The log's stack height is NOT derived from this — see
        // ActionLogWidget::contentHeight, which measures the laid-out
        // rows instead, immune to mid-layout cache staleness.)
        const int w = parentWidget() ? qMax(40, parentWidget()->width()) : 200;
        return QSize(0, measure(w).h);
    }
    QSize minimumSize() const override { return QSize(0, 0); }

private:
    struct M {
        int h = 0;          // total height
        int textW = 0;      // the text label's width (the full avail)
        int textH = 0;      // the wrapped text's height
        int lines = 1;      // the wrapped text's line count
        int lastLineWidth = 0; // actual width of the text's last line
        bool atomicsBelow = false; // atomics on their own line under the text
    };
    QLabel *stretchyLabel(int i) const
    {
        auto *l = dynamic_cast<QLabel *>(itemList[i]->widget());
        return (l && l->hasHeightForWidth()) ? l : nullptr;
    }
    bool hasText() const { return !itemList.isEmpty() && stretchyLabel(0); }
    int atomicWidth(int from) const
    {
        int w = 0;
        for (int i = from; i < itemList.size(); ++i)
            w += itemList[i]->sizeHint().width() + m_h;
        return w;
    }
    int atomicHeight(int from) const
    {
        int h = 0;
        for (int i = from; i < itemList.size(); ++i)
            h = qMax(h, itemList[i]->sizeHint().height());
        return h;
    }
    M measure(int avail) const
    {
        M m;
        if (itemList.isEmpty())
            return m;
        avail = qMax(20, avail);
        if (!hasText()) { // atomics only (dice/tail): never overflow a log
            m.textW = avail;
            m.h = qMax(atomicHeight(0), kDice);
            return m;
        }
        // Simulate the QLabel word wrap of the text at width `avail`
        // (same font, same space width -> same breaks as the label).
        const QFontMetrics fm(stretchyLabel(0)->font());
        const int lineH = itemList[0]->sizeHint().height();
        int x = 0;
        m.lines = 1;
        const QStringList words =
            stretchyLabel(0)->text().split(' ', Qt::SkipEmptyParts);
        for (const QString &wd : words) {
            const int ww = fm.horizontalAdvance(wd);
            const int sp = (x > 0) ? fm.horizontalAdvance(QLatin1String(" ")) : 0;
            if (x + sp + ww > avail && x > 0) {
                m.lines++;
                x = 0;
            } else {
                x += sp;
            }
            x += qMin(ww, qMax(0, avail - x)); // a word longer than the
        }                                       // line: clamped (QLabel
                                                 // char-breaks; log texts
                                                 // never reach this)
        m.lastLineWidth = x;
        m.textW = avail;
        m.textH = m.lines * lineH + (m.lines - 1) * m_v;
        const int atW = atomicWidth(1);
        const int atH = atomicHeight(1);
        if (m.lastLineWidth + m_h + atW <= avail)
            m.h = qMax(m.textH, atH); // atomics after the last line's end
        else {
            m.atomicsBelow = true;
            m.h = m.textH + m_v + atH; // atomics on their own line
        }
        return m;
    }
    static void place(QLayoutItem *it, int x, int y, int w, int h)
    {
        if (auto *wg = it->widget())
            wg->setGeometry(x, y, w, h);
    }
    void doLayout(const QRect &r)
    {
        if (itemList.isEmpty())
            return;
        const M m = measure(r.width());
        const int y = r.y();
        if (!hasText()) {
            int x = r.x();
            for (int i = 0; i < itemList.size(); ++i) {
                const QSize sz = itemList[i]->sizeHint();
                x += m_h;
                place(itemList[i], x, y + (m.h - sz.height()) / 2,
                      sz.width(), sz.height());
                x += sz.width();
            }
            return;
        }
        const int lineH = itemList[0]->sizeHint().height();
        // The text block; vertically centered when a die makes the row
        // taller than the text itself (the single-line dice case).
        const int textY = y + qMax(0, (m.h - m.textH) / 2);
        place(itemList[0], r.x(), textY, m.textW, m.textH);
        if (m.atomicsBelow) {
            int x = r.x();
            const int y2 = textY + m.textH + m_v;
            for (int i = 1; i < itemList.size(); ++i) {
                const QSize sz = itemList[i]->sizeHint();
                x += m_h;
                place(itemList[i], x, y2, sz.width(), sz.height());
                x += sz.width();
            }
        } else {
            // After the END OF THE LAST LINE, centered on that line.
            const int lastLineTop = textY + (m.lines - 1) * (lineH + m_v);
            int x = r.x() + m.lastLineWidth + m_h;
            for (int i = 1; i < itemList.size(); ++i) {
                const QSize sz = itemList[i]->sizeHint();
                place(itemList[i], x, lastLineTop + (lineH - sz.height()) / 2,
                      sz.width(), sz.height());
                x += sz.width() + m_h;
            }
        }
    }
    QVector<QLayoutItem *> itemList;
    int m_h, m_v;
};

} // namespace

// ---------------------------------------------------------------------------
// AutoScrollToggle — white square + check: reads as a real checkbox at a
// glance, so the follow behavior is easy to toggle.
// ---------------------------------------------------------------------------
AutoScrollToggle::AutoScrollToggle(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(18);
    setFixedWidth(88);
}

void AutoScrollToggle::setChecked(bool on)
{
    if (m_on == on)
        return;
    m_on = on;
    update();
    emit toggled(on);
}

void AutoScrollToggle::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        setChecked(!m_on);
    QWidget::mousePressEvent(event);
}

void AutoScrollToggle::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int bs = 12; // the white square
    const QRectF box(1, (height() - bs) / 2.0, bs, bs);
    p.setPen(QPen(QColor(0x55, 0x5c, 0x68), 1));
    p.setBrush(QColor(255, 255, 255));
    p.drawRoundedRect(box, 2, 2);
    if (m_on) {
        QPen ck(QColor(0x1f, 0x6b, 0x2a), 2.0);
        ck.setCapStyle(Qt::RoundCap);
        ck.setJoinStyle(Qt::RoundJoin);
        p.setPen(ck);
        QPolygonF tick;
        tick << box.topLeft() + QPointF(2.4, 6.2)
             << box.topLeft() + QPointF(4.8, 8.6)
             << box.topRight() + QPointF(-2.6, -3.2);
        p.drawPolyline(tick);
    }
    QFont f = font();
    f.setPointSizeF(8);
    p.setFont(f);
    p.setPen(QColor(0x22, 0x28, 0x32));
    p.drawText(QRectF(box.right() + 5, 0, width() - box.right() - 3, height()),
               Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("auto-scroll"));
}

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
    // The chevron points where the click TAKES the log: compact shows ▴
    // ("click to expand"), expanded shows ▾ ("click to collapse").
    m_chevron = new QLabel(QString::fromUtf8("\u25b4"), header); // ▴
    m_chevron->setCursor(Qt::PointingHandCursor);
    m_chevron->setStyleSheet(QStringLiteral("color: #222832; font-size: 13px; font-weight: bold;"));
    m_chevron->setFixedWidth(16);
    // The autoscroll toggle in the title bar. It reflects the live
    // stick-to-bottom state: unchecks when the user scrolls up, re-checks
    // when the user brings the log back to the bottom; the user can also
    // tick it to force following. A visible WHITE SQUARE checkbox so it
    // reads as a toggle at a glance.
    m_autoCheck = new AutoScrollToggle(header);
    m_autoCheck->setChecked(true);
    connect(m_autoCheck, &AutoScrollToggle::toggled, this, [this](bool on) {
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
    // The inner stack is sized MANUALLY (not widgetResizable): a
    // widgetResizable QScrollArea re-lays out the inner widget only from
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
    // USER scroll detection. A value change that is NOT ours = the user
    // moved the view: at the bottom (or a hair above) -> autoscroll
    // re-arms; up -> it pauses and the checkbox unchecks.
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
        //
        // A flow row's height DEPENDS ON ITS WIDTH, and the layout
        // sizeHint caches go stale mid-layout (measured while a row was
        // still a stub width) — using m_stackLayout->sizeHint() over-grew
        // the stack until the bottom stretch swallowed the newest lines.
        // Instead: (1) lay the rows out at the REAL width, (2) measure the
        // content from the ACTUAL row heights.
        const int w = m_scroll->viewport()->width();
        m_stack->setGeometry(0, 0, w, qMax(1, m_stack->height()));
        m_stackLayout->activate(); // synchronous pass at the real width
        const int need = qMax(m_scroll->viewport()->height(), contentHeight());
        m_stack->setGeometry(0, 0, w, need);
        m_progScroll = false;
        if (m_stick)
            scrollToEnd();
    });
}

int ActionLogWidget::contentHeight() const
{
    if (!m_stackLayout || !m_stack)
        return 0;
    const QMargins mg = m_stackLayout->contentsMargins();
    int h = mg.top() + mg.bottom();
    const auto rows = m_stack->findChildren<QWidget *>(QString(),
                                                       Qt::FindDirectChildrenOnly);
    for (int i = 0; i < rows.size(); ++i) {
        h += rows[i]->height();
        if (i + 1 < rows.size())
            h += m_stackLayout->spacing();
    }
    return h;
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
    // The NAME hover. `watched` is a name label (the id is in a dynamic
    // property); entering emits the id, leaving emits empty.
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
    // Stick-to-bottom: true when the user is already at
    // (or within a hair of) the newest line. New entries only auto-scroll
    // in that case; a user reading the history is never yanked down.
    const QScrollBar *sb = m_scroll ? m_scroll->verticalScrollBar() : nullptr;
    if (!sb)
        return true;
    return sb->maximum() - sb->value() <= 8;
}

// HOVER ON A SINGLE BLOCK DIE shows its official meaning — so each die is
// its OWN widget (a rich-text <img> cannot carry a per-image tooltip). The
// row's middle is rebuilt as a small layout:
// [action text] [block dice... with tooltips] [d6 dice...] [result tail].
//
// Official BB2025 block die face names (match the GW die).
const char *const kBlockDieTip[7] = {
    "",                // 0: unused
    "Player Down",     // 1: skull
    "Both Down",       // 2: skull + burst
    "Pushback",        // 3: arrow
    "Pushback",        // 4: arrow
    "Stumble",         // 5: '!' in a burst
    "POW"              // 6: burst
};

// One middle row split into renderable parts (dice become widgets, the
// text/tail stay rich text). NOTE: a participant the UI could not resolve
// is DROPPED, never shown as a raw id (no numeric ids in the log). Dice
// assets: the PRE-SCALED *_s.png (96px), downscaled to kDice when the
// widget paints them (4K-ready).
struct MidParts {
    QString text;
    QList<int> blockFaces;
    QList<int> d6Faces;
    QString tailHtml;
};

static MidParts splitMiddle(const DiceLogLine &line)
{
    MidParts parts;
    parts.text = line.text;
    for (const QString &face : line.blockFaces)
        if (const int v = face.toInt(); v >= 1 && v <= 6)
            parts.blockFaces << v;
    // d6 rolls: red die faces + target + result. d6Rolls covers multi-die
    // reports (throw-in, scatter, injury rolls, ...); d6Roll alone the
    // single-die skill rolls (every roll shows the DIE, never a number).
    if (!line.d6Rolls.isEmpty()) {
        for (const QString &face : line.d6Rolls)
            if (const int v = face.toInt(); v >= 1 && v <= 6)
                parts.d6Faces << v;
    } else if (line.d6Roll > 0) {
        parts.d6Faces << line.d6Roll;
    }
    if (!parts.d6Faces.isEmpty()) {
        if (line.d6Target > 0)
            parts.tailHtml += QStringLiteral(" <span style=\"color:%1\">vs %2</span>")
                                  .arg(kMuted.name())
                                  .arg(line.d6Target);
        // BB3: the RESULT is emphasized — green SUCCESS / red Failure,
        // bold, like Blood Bowl 3's log.
        if (line.d6ShowResult)
            parts.tailHtml += line.d6Ok
                                  ? QStringLiteral(" <b style=\"color:%1\">SUCCESS</b>").arg(kOkGreen.name())
                                  : QStringLiteral(" <b style=\"color:%1\">Failure</b>").arg(kFailRed.name());
        if (line.d6ReRolled)
            parts.tailHtml += QStringLiteral(" <span style=\"color:%1\">[RR]</span>").arg(kMuted.name());
    }
    return parts;
}

void ActionLogWidget::addEntry(const DiceLogLine &line, const QString &actorName,
                               const QString &defenderName, const QString &actorId,
                               const QString &defenderId, const QColor &actorColor,
                               const QColor &defenderColor)
{
    // BB3-like row:
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

    // The middle is a LAYOUT, not one rich-text label: every die is its
    // own QLabel so a hover on ONE block die shows THAT die's tooltip (1
    // Player Down ... 6 POW) — rich text <img> cannot carry per-image
    // tooltips. It is a FLOW layout (LogMidLayout): long lines WRAP
    // instead of being clipped mid-word.
    const MidParts parts = splitMiddle(line);
    auto *mid = new QWidget(row);
    auto *ml = new LogMidLayout(3, 2);
    ml->setContentsMargins(0, 0, 0, 0);
    mid->setLayout(ml);
    if (!parts.text.isEmpty()) {
        auto *tl = new QLabel(parts.text.toHtmlEscaped(), mid);
        tl->setStyleSheet(style);
        tl->setFont(f);
        tl->setWordWrap(true); // the stretchy item: wraps when it must
        ml->addWidget(tl);
    }
    const auto dieLabel = [mid, ml, style](const QString &resource, const QString &tip) {
        auto *dl = new QLabel(mid);
        const QPixmap big(resource); // 96px pre-scaled asset
        dl->setPixmap(big.scaled(kDice, kDice, Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation));
        dl->setFixedSize(kDice, kDice);
        dl->setStyleSheet(QStringLiteral("background: transparent;"));
        if (!tip.isEmpty())
            dl->setToolTip(tip);
        ml->addWidget(dl);
        return dl;
    };
    for (const int v : parts.blockFaces)
        dieLabel(QStringLiteral(":/dice/block%1_s.png").arg(v),
                 QString::fromUtf8(kBlockDieTip[v]));
    for (const int v : parts.d6Faces)
        dieLabel(QStringLiteral(":/dice/d6_%1_s.png").arg(v), QString());
    if (!parts.tailHtml.isEmpty()) {
        auto *tl2 = new QLabel(parts.tailHtml, mid);
        tl2->setTextFormat(Qt::RichText);
        tl2->setStyleSheet(style);
        tl2->setFont(f);
        ml->addWidget(tl2);
    }
    // (no stretch: LogMidLayout is a flow — the leftover space on the
    // right stays empty, as the stretch did before)
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

    // MINIMUM, not fixed — a wrapped row is taller than kRowH, a short one
    // keeps the classic 30px rhythm.
    row->setMinimumHeight(kRowH);
    m_stackLayout->insertWidget(m_stackLayout->count() - 1, row); // before the stretch
    settleScroll(); // only when the user is (back) at the bottom

    // Plain dump: actor + text + "→ defender" (the defender is part of the
    // visible line, so it belongs in the plain form too).
    QString plain = QStringLiteral("%1 %2").arg(actorName, line.text).trimmed();
    if (!defenderName.isEmpty())
        plain += QStringLiteral(" \u2192 %1").arg(defenderName);
    m_plain << plain;
    ++m_entryCount;
    while (m_plain.size() > kMaxEntries)
        m_plain.removeFirst();

    // (The scroll happens right after the row is inserted — m_stick is
    // the live user-driven stick-to-bottom state.)
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
    // Manual inner geometry: keep the stack as wide as the viewport and at
    // least as tall as the content.
    if (m_scroll && m_stack && m_stackLayout) {
        // Same as settleScroll — lay the rows out at the REAL width first,
        // then measure the content from the actual row heights (a flow
        // row's height depends on its width; the sizeHint cache is not
        // trustworthy mid-resize).
        const int w = m_scroll->viewport()->width();
        m_stack->setGeometry(0, 0, w, qMax(1, m_stack->height()));
        m_stackLayout->activate();
        const int need = qMax(m_scroll->viewport()->height(), contentHeight());
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
