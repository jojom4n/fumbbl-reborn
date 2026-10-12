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

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>

class ChatWidget;
class DiceLogWidget;
class FieldView;
class GameModel;
class QCloseEvent;
class QPlainTextEdit;
class HeaderBar;
class PlayerInfoWidget;
class RosterWidget;
class SpectatorSession;
class QLabel;
class QAction;

namespace ads {
class CDockManager;
class CDockWidget;
}

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
    // Backed by QDS saveState()/restoreState() (XML layout serialization).
    QByteArray savedLayout() const;
    void restoreLayout(const QByteArray &state);

    // --- Core (spectator) -----------------------------------------------------
    // The game model (exposed for tests / tooling; the UI reads from it).
    GameModel *model() const { return m_model; }

    // Controller entry points for incoming match data. In production these are
    // driven by the SpectatorSession signals; they are public so the core->UI
    // path can be exercised directly (e.g. by tests) without a live network.
    void applyModelToUi();
    void onGameState(const QJsonObject &game);
    void onModelSync(const QJsonArray &changes, const QJsonArray &reports);
    void onGameTime(qint64 gameTimeMs, qint64 turnTimeMs);
    void onTalk(const QJsonObject &payload);
    void onJoined(const QJsonObject &payload);
    void onSessionProgress(const QString &message);
    void onSessionFatal(const QString &message);

signals:
    void chatMessageSent(const QString &message);
    void connectionRequested();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createActions();
    void createMenus();
    void createToolbar();
    void createDockManager();
    void createCentralWidget();
    void createDockWidgets();
    void createStatusBar();

    // Dock layout persistence (QSettings-backed via QDS state).
    void loadLayout();
    void onWsClosed(int code, const QString &reason);
    void onConnectRequested();
    void showPlayerInfo(int number, bool homeSide);

    // Core
    SpectatorSession *m_session = nullptr;
    GameModel *m_model = nullptr;

    // Widgets
    HeaderBar *m_header = nullptr;
    FieldView *m_field = nullptr;
    RosterWidget *m_homeRoster = nullptr;
    RosterWidget *m_awayRoster = nullptr;
    PlayerInfoWidget *m_playerInfo = nullptr;
    DiceLogWidget *m_diceLog = nullptr;
    ChatWidget *m_chat = nullptr;

    // Docking (QT Advanced Docking System)
    ads::CDockManager *m_dockManager = nullptr; // central widget of the window

    // Dock panels
    ads::CDockWidget *m_fieldDock = nullptr; // central anchor (locked)
    ads::CDockWidget *m_homeRosterDock = nullptr;
    ads::CDockWidget *m_awayRosterDock = nullptr;
    ads::CDockWidget *m_playerInfoDock = nullptr;
    ads::CDockWidget *m_diceLogDock = nullptr;
    ads::CDockWidget *m_chatDock = nullptr;
    ads::CDockWidget *m_protocolDock = nullptr;
    QPlainTextEdit *m_protocolLog = nullptr;

    // Default dock arrangement, remembered for the "Reset Layout" action.
    QByteArray m_defaultLayout;

    // Actions
    QAction *m_connectAction = nullptr;
    QAction *m_disconnectAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_resetLayoutAction = nullptr;

    QLabel *m_statusLabel = nullptr;
    QLabel *m_connectionLabel = nullptr;
};
