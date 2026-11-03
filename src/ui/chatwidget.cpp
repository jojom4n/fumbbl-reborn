// =============================================================================
// ChatWidget — implementation (see chatwidget.h for the design)
// =============================================================================
#include "ui/chatwidget.h"

#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace {
const int kRowH = 20;
const int kMaxMessages = 300;

// Mockup F palette (compose_f.py).
const QColor INK(34, 40, 50);
const QColor INK2(104, 114, 128);
const QColor PAPER(250, 250, 253);
const QColor RED(196, 60, 52);

const int kHeaderH = 22; // expanded: "CHAT" + collapse chevron strip
} // namespace

QSize ChatWidget::pillSize()
{
    // Mockup pill 128x44; the widget is 8px taller so the unread badge
    // (which sits 8px above the pill top in the mockup) is not clipped.
    return QSize(128, 52);
}

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("chat"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    // Header strip (expanded only): "CHAT" + collapse chevron (the
    // chevron collapses the chat to the pill).
    auto *headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);
    m_header = new QLabel(QStringLiteral("CHAT"), this);
    {
        QFont hf = m_header->font();
        hf.setPointSizeF(9);
        hf.setBold(true);
        m_header->setFont(hf);
        m_header->setStyleSheet(QStringLiteral("color: #687280; letter-spacing: 1px;"));
    }
    m_chevron = new QLabel(QString::fromUtf8("\u25be"), this); // ▾ collapse
    m_chevron->setCursor(Qt::PointingHandCursor);
    m_chevron->setFixedWidth(18);
    m_chevron->setAlignment(Qt::AlignCenter);
    m_chevron->setStyleSheet(QStringLiteral("color: #222832; font-size: 13px; font-weight: bold;"));
    headerRow->addWidget(m_header);
    headerRow->addStretch(1);
    headerRow->addWidget(m_chevron);
    layout->addLayout(headerRow);

    // Messages (scrollable, translucent).
    m_scroll = new QScrollArea(this);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setWidgetResizable(true);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    m_stack = new QWidget(m_scroll);
    m_stack->setStyleSheet(QStringLiteral("background: transparent;"));
    m_stackLayout = new QVBoxLayout(m_stack);
    m_stackLayout->setContentsMargins(0, 0, 2, 0);
    m_stackLayout->setSpacing(2);
    m_stackLayout->addStretch(1);
    m_scroll->setWidget(m_stack);
    layout->addWidget(m_scroll, 1);

    // Input row.
    auto *inputRow = new QHBoxLayout();
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(QStringLiteral("Write a message… (Enter to send)"));
    m_input->setClearButtonEnabled(true);
    m_input->setStyleSheet(QStringLiteral(
        "QLineEdit { background: rgba(252,252,254,200); color: #222832;"
        " border: 1px solid rgba(0,0,0,90); border-radius: 9px; padding: 4px 8px; }"));
    m_sendButton = new QPushButton(QStringLiteral("Send"), this);
    m_sendButton->setCursor(Qt::PointingHandCursor);
    m_sendButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #222832; color: white; font-weight: bold;"
        " border: none; border-radius: 9px; padding: 4px 12px; }"
        "QPushButton:hover { background: #3a4150; }"));
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_sendButton);
    layout->addLayout(inputRow);

    const auto send = [this]() {
        const QString text = m_input->text().trimmed();
        if (text.isEmpty())
            return;
        m_input->clear();
        emit messageSent(text);
    };
    connect(m_sendButton, &QPushButton::clicked, this, send);
    connect(m_input, &QLineEdit::returnPressed, this, send);
}

void ChatWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    if (m_collapsed) {
        // --- Mockup F pill (128x44, r=22), drawn 8px down: the unread
        //     badge pokes 8px above the pill top, like the mockup. --------
        const int y0 = 8;
        const QRectF pill(0, y0, 128, 44);
        // Soft shadow.
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(20, 26, 34, 60));
        p.drawRoundedRect(pill.translated(2, 4), 22, 22);
        // Paper pill.
        p.setBrush(QColor(PAPER.red(), PAPER.green(), PAPER.blue(), 235));
        p.drawRoundedRect(pill, 22, 22);

        // Speech-bubble outline icon (mockup: circle + arc tail, 18px).
        const QRectF icon(12, y0 + 10, 18, 18);
        QPen iconPen(QColor(INK.red(), INK.green(), INK.blue(), 255), 2.0);
        p.setPen(iconPen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(icon);
        p.drawArc(QRectF(icon), 30 * 16, 120 * 16);

        // "CHAT" 13px bold ink.
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(13);
        p.setFont(f);
        p.setPen(INK);
        p.drawText(QRectF(38, y0 + 10, 80, 18), Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("CHAT"));

        // Unread badge: red circle 22px, top-right corner of the pill.
        if (m_unread > 0) {
            const QRectF badge(128 - 26, y0 - 8, 22, 22);
            p.setPen(QPen(QColor(255, 255, 255, 255), 1.5));
            p.setBrush(RED);
            p.drawEllipse(badge);
            QFont bf = p.font();
            bf.setBold(true);
            bf.setPixelSize(11);
            p.setFont(bf);
            p.setPen(Qt::white);
            p.drawText(badge, Qt::AlignCenter,
                       m_unread > 99 ? QStringLiteral("99+") : QString::number(m_unread));
        }
        return;
    }

    // --- Expanded: very translucent paper card ---------------------------
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(252, 252, 254, 165));
    p.drawRoundedRect(r, 10, 10);
    QPen border(QColor(0, 0, 0, 90));
    border.setWidthF(1.2);
    p.setPen(border);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 10, 10);
}

void ChatWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_collapsed) {
        // Click anywhere on the pill: re-open the chat (and clear the badge).
        setCollapsed(false);
        return;
    }
    // Expanded: the chevron collapses the chat to the pill.
    if (m_chevron && m_chevron->rect().contains(m_chevron->mapFrom(this, event->pos()))) {
        setCollapsed(true);
        return;
    }
    QWidget::mousePressEvent(event);
}

void ChatWidget::setCollapsed(bool collapsed)
{
    if (m_collapsed == collapsed)
        return;
    m_collapsed = collapsed;
    applyCollapsed(collapsed);
    update();
    emit collapsedChanged(m_collapsed);
}

void ChatWidget::applyCollapsed(bool collapsed)
{
    m_header->setVisible(!collapsed);
    m_chevron->setVisible(!collapsed);
    m_scroll->setVisible(!collapsed);
    m_input->setVisible(!collapsed);
    m_sendButton->setVisible(!collapsed);

    if (collapsed)
        setFixedSize(pillSize());
    else {
        setFixedSize(560, 250);
        m_unread = 0; // opening the chat clears the unread badge
    }
}

void ChatWidget::addMessage(const QString &sender, const QString &text,
                            const QColor &senderColor)
{
    if (m_collapsed) {
        // Message while collapsed: stored + counted for the badge.
        ++m_unread;
        update();
    }

    const QString color = senderColor.isValid() ? senderColor.name()
                                                : QStringLiteral("#222832");
    const QString html =
        QStringLiteral("<b><span style=\"color:%1\">%2</span></b><span>: %3</span>")
            .arg(color, sender.toHtmlEscaped(), text.toHtmlEscaped());

    auto *label = new QLabel(m_stack);
    label->setTextFormat(Qt::RichText);
    label->setText(html);
    label->setStyleSheet(QStringLiteral("background: transparent; color: #222832;"));
    QFont f = label->font();
    f.setPointSizeF(10);
    label->setFont(f);
    label->setFixedHeight(kRowH);
    m_stackLayout->insertWidget(m_stackLayout->count() - 1, label);

    m_plain << QStringLiteral("%1: %2").arg(sender, text);
    while (m_plain.size() > kMaxMessages)
        m_plain.removeFirst();

    m_scroll->ensureWidgetVisible(label);
    const auto sb = m_scroll->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void ChatWidget::clearChat()
{
    for (auto *w : m_stack->findChildren<QLabel *>(QString(), Qt::FindDirectChildrenOnly))
        w->deleteLater();
    m_plain.clear();
    m_unread = 0;
    update();
}

QString ChatWidget::toPlainText() const
{
    return m_plain.join(QStringLiteral("\n"));
}
