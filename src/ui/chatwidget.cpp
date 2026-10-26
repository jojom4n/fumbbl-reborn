// =============================================================================
// ChatWidget — Mockup F chat (bottom-right overlay)
//
// Round 8 restyle (user: "il chat deve essere come nel mockup"):
//   * very translucent light card (the widget paints its own chrome; it is
//     pinned to the stage corner by MainWindow, like the action log)
//   * messages: BOLD SENDER (team accent color when known) + ink text,
//     one row each, newest at the bottom (auto-scroll)
//   * input line at the bottom: rounded, paper, ink text
// =============================================================================
#include "ui/chatwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace {
const int kRowH = 20;
const int kMaxMessages = 300;
}

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("chat"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

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
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    // Very translucent paper, like the action log (Mockup F light theme).
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(252, 252, 254, 165));
    p.drawRoundedRect(r, 10, 10);
    QPen border(QColor(0, 0, 0, 90));
    border.setWidthF(1.2);
    p.setPen(border);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 10, 10);
}

void ChatWidget::addMessage(const QString &sender, const QString &text,
                            const QColor &senderColor)
{
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
}

QString ChatWidget::toPlainText() const
{
    return m_plain.join(QStringLiteral("\n"));
}
