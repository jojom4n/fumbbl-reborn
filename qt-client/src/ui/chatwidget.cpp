// =============================================================================
// ChatWidget — implementation
// =============================================================================
#include "ui/chatwidget.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_messages = new QListWidget(this);
    layout->addWidget(m_messages, 1);

    auto *inputRow = new QHBoxLayout();
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(QStringLiteral("Write a message… (Enter to send)"));
    m_sendButton = new QPushButton(QStringLiteral("Send"), this);
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

void ChatWidget::addMessage(const QString &sender, const QString &text)
{
    auto *item = new QListWidgetItem(this->m_messages);
    item->setText(QStringLiteral("%1: %2").arg(sender, text));
    m_messages->addItem(item);
    m_messages->scrollToItem(item, QAbstractItemView::EnsureVisible);
}

void ChatWidget::clearChat()
{
    m_messages->clear();
}
