// =============================================================================
// ChatWidget — match chat (serverTalk / clientTalk), Mockup F.
//
//   * EXPANDED: the very translucent light card with BOLD SENDER (team
//     accent when known) + ink text, input row at the bottom, and a small
//     "CHAT" header with a collapse chevron (▾) — clicking it collapses the
//     chat DELTA to the mockup pill.
//   * COLLAPSED: the mockup-F pill (128x44, radius 22, paper, shadow):
//     speech-bubble outline icon, "CHAT" label 13px bold ink, and a RED
//     unread badge (circle 22px, white bold count) in the top-right corner.
//     Clicking the pill re-opens the chat.
//   * UNREAD COUNT: messages arriving while collapsed increment the badge;
//     it resets to 0 when the chat is opened.
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
class QMouseEvent;

class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(QWidget *parent = nullptr);

    // `senderColor` colors the bold sender name (team accent when known).
    // While collapsed the message is still stored (and auto-scrolled); the
    // unread badge increments.
    void addMessage(const QString &sender, const QString &text,
                    const QColor &senderColor = QColor());
    void clearChat();

    // Telemetry (tests/debug): all messages, one per line.
    QString toPlainText() const;

    // Collapse to the mockup pill / expand back to the card.
    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return m_collapsed; }
    int unreadCount() const { return m_unread; }

    // Pill geometry (Mockup F: 128x44).
    static QSize pillSize();

signals:
    void messageSent(const QString &message);
    // Emitted after a collapse/expand: the pinned position must be
    // re-applied by the parent (the widget size changed).
    void collapsedChanged(bool collapsed);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void applyCollapsed(bool collapsed);

    QScrollArea *m_scroll = nullptr;
    QWidget *m_stack = nullptr;
    QVBoxLayout *m_stackLayout = nullptr;
    QLabel *m_header = nullptr;   // expanded: "CHAT" + chevron strip
    QLabel *m_chevron = nullptr;  // expanded: collapse chevron (▾)
    QLineEdit *m_input = nullptr;
    QPushButton *m_sendButton = nullptr;
    QStringList m_plain; // toPlainText() backing store
    bool m_collapsed = false;
    int m_unread = 0;
};
