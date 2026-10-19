// =============================================================================
// MainWindow — FUMBBL Reborn main window
//
// HUD layout (phase 10):
//   - The field fills the whole window (fullscreen, no side stripes).
//   - Panels (rosters, player info, dice log, chat, protocol) are translucent
//     FloatingPanels that overlay the field; they stay draggable (title strip)
//     and resizable (edges) and can be hidden with × / re-shown from the View
//     menu. Layout (positions AND sizes) is persisted and "Reset Layout"
//     restores the default arrangement with default sizes.
//   - Native menu bar (File / View / Help) + thin header toolbar on top.
// =============================================================================
#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QRect>

class ChatWidget;
class DiceLogWidget;
class FieldView;
class FloatingPanel;
class GameModel;
class QCloseEvent;
class QPlainTextEdit;
class HeaderBar;
class PlayerInfoWidget;
class RosterWidget;
class SpectatorSession;
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

    // Open the "Spectate Game" dialog (also triggered by File > Spectate Game…).
    // Public so it can be invoked from --spectate-now / tests.
    void openSpectateDialog();

    // Panel layout persistence (JSON via QSettings, phase 10).
    QByteArray savedLayout() const;
    void restoreLayout(const QByteArray &state);
    // Restore the default panel arrangement (positions AND sizes) — the
    // "Reset Layout" action. Public so tests can verify the size-restore fix.
    void resetLayout();

    // --- Core (spectator) -----------------------------------------------------
    // The game model (exposed for tests / tooling; the UI reads from it).
    GameModel *model() const { return m_model; }
    // The currently selected player (playerId, "" = none).
    QString selectedPlayerId() const { return m_selectedPlayerId; }
    // Select a model player (field highlight + Player Info panel). Public so
    // tests can drive the same path the field/roster clicks take.
    void selectPlayer(const QString &playerId);

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
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void createActions();
    void createMenus();
    void createToolbar();
    void createCentralWidget();
    void createPanels();
    void createStatusBar();

    // Panel layout (phase 10).
    FloatingPanel *makePanel(const QString &name, const QString &title,
                             QWidget *content, const QColor &accent);
    void applyDefaultLayout();
    void applySavedLayout(const QByteArray &state);
    void clampPanelsToStage();
    void onStageFirstSized();
    QRect defaultGeometryFor(const QString &name) const;

    void onWsClosed(int code, const QString &reason);
    void onConnectRequested();
    void showPlayerInfo(int number, bool homeSide);
    // Field click: empty id = deselect.
    void onFieldPlayerClicked(const QString &playerId);

    QString m_selectedPlayerId; // the player shown in the Player Info panel
    bool m_layoutApplied = false; // default/saved geometry applied once

    // Core
    SpectatorSession *m_session = nullptr;
    GameModel *m_model = nullptr;

    // Widgets
    QWidget *m_stage = nullptr; // central container: field + overlay panels
    HeaderBar *m_header = nullptr;
    FieldView *m_field = nullptr;
    RosterWidget *m_homeRoster = nullptr;
    RosterWidget *m_awayRoster = nullptr;
    PlayerInfoWidget *m_playerInfo = nullptr;
    DiceLogWidget *m_diceLog = nullptr;
    ChatWidget *m_chat = nullptr;

    // Overlay panels (child of m_stage, floating over the field)
    QHash<QString, FloatingPanel *> m_panels; // name -> panel
    QPlainTextEdit *m_protocolLog = nullptr;

    // Actions
    QAction *m_connectAction = nullptr;
    QAction *m_disconnectAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_resetLayoutAction = nullptr;
    QHash<QString, QAction *> m_panelViewActions; // name -> View-menu action

    QLabel *m_statusLabel = nullptr;
    QLabel *m_connectionLabel = nullptr;
};
