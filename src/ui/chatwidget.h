// =============================================================================
// ChatWidget — match chat (serverTalk / clientTalk), Mockup F restyle (r8)
// =============================================================================
#pragma once

#include <QColor>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QPaintEvent;

class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(QWidget *parent = nullptr);

    // `senderColor` colors the bold sender name (team accent when known).
    void addMessage(const QString &sender, const QString &text,
                    const QColor &senderColor = QColor());
    void clearChat();

    // Telemetry (tests/debug): all messages, one per line.
    QString toPlainText() const;

signals:
    void messageSent(const QString &message);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QScrollArea *m_scroll = nullptr;
    QWidget *m_stack = nullptr;
    QVBoxLayout *m_stackLayout = nullptr;
    QLineEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
    QStringList m_plain; // toPlainText() backing store
};
