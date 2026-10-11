// =============================================================================
// MainWindow — FUMBBL Reborn main window
//
// Office/VS-style layout:
//   - Native menu bar (File / View / Help) with accelerators
//   - Movable header toolbar (teams, score, turn, timer, spectators)
//   - Central field widget (the "anchor")
//   - Dockable panels: home roster (left), away roster (right),
//     player info / dice log / chat (bottom). All dockable, floatable,
//     tabbable, resizable — layout persistable via saveState()/restoreState().
// =============================================================================
#pragma once

#include <QMainWindow>

class ChatWidget;
class DiceLogWidget;
class FieldView;
class QPlainTextEdit;
class HeaderBar;
class PlayerInfoWidget;
class RosterWidget;
class QLabel;
class QAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // --- UI state updaters (driven by the FUMBBL core in later phases) -----
    void setTeams(const QString &homeName, const QString &awayName);
    void setScore(int home, int away);
    void setTurn(int turn);
    void setTimerSeconds(int remaining);
    void setSpectators(int count);
    void setConnectionStatus(const QString &status, bool connected);

    void appendDiceLog(const QString &text, const QColor &color = QColor());
    void appendChat(const QString &sender, const QString &text);
    void appendProtocol(const QString &direction, const QString &text);

    // Restore/dock layout persistence (QSettings-backed, phase 3).
    QByteArray savedLayout() const;
    void restoreLayout(const QByteArray &state);

signals:
    void chatMessageSent(const QString &message);
    void connectionRequested();

private:
    void createActions();
    void createMenus();
    void createToolbar();
    void createCentralWidget();
    void createDockWidgets();
    void createStatusBar();

    // Widgets
    HeaderBar *m_header = nullptr;
    FieldView *m_field = nullptr;
    RosterWidget *m_homeRoster = nullptr;
    RosterWidget *m_awayRoster = nullptr;
    PlayerInfoWidget *m_playerInfo = nullptr;
    DiceLogWidget *m_diceLog = nullptr;
    ChatWidget *m_chat = nullptr;

    // Docks
    QDockWidget *m_homeRosterDock = nullptr;
    QDockWidget *m_awayRosterDock = nullptr;
    QDockWidget *m_playerInfoDock = nullptr;
    QDockWidget *m_diceLogDock = nullptr;
    QDockWidget *m_chatDock = nullptr;
    QDockWidget *m_protocolDock = nullptr;
    QPlainTextEdit *m_protocolLog = nullptr;

    // Actions
    QAction *m_connectAction = nullptr;
    QAction *m_disconnectAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_resetLayoutAction = nullptr;

    QLabel *m_statusLabel = nullptr;
    QLabel *m_connectionLabel = nullptr;
};
