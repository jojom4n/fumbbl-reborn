// =============================================================================
// ChatWidget — match chat (serverTalk / clientTalk)
// =============================================================================
#pragma once

#include <QWidget>

class QLineEdit;
class QListWidget;
class QPushButton;

class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(QWidget *parent = nullptr);

    void addMessage(const QString &sender, const QString &text);
    void clearChat();

signals:
    void messageSent(const QString &message);

private:
    QListWidget *m_messages = nullptr;
    QLineEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
};
