// =============================================================================
// MainWindow — FUMBBL Reborn main window
//
// HUD layout (Mockup F round 8):
//   - The field fills the whole window (fullscreen, no side stripes).
//   - The header is the zoned Mockup F strip (left team / center info /
//     right team), full width, NO end-turn button (spectator client).
//   - Player info is a HOVER card: it appears when the mouse is over a
//     player (field or bench) and sits on the OPPOSITE side of the field
//     from the player, far away from him.
//   - Action log (bottom-left) and chat (bottom-right) are very translucent
//     pinned overlays, like the mockup; the protocol log stays a draggable
//     FloatingPanel (dev tool). Layout persistence still applies to the
//     remaining panels.
//   - No roster panels: box/bench/KO/reserve are visible in the field's box
//     strips (round 8).
//   - Field navigation: mouse wheel = zoom, right-mouse drag = pan.
// =============================================================================
#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QRect>

#include "core/reportparser.h"
#include "ui/fieldview.h"

class ActionLogWidget;
class ChatWidget;
class FloatingPanel;
class GameModel;
class HeaderView;
class QCloseEvent;
class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QToolBar;
class PlayerInfoWidget;
class SpectatorSession;
class QLabel;
class QAction;
struct FFBPlayer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // --- UI state updaters (driven by the FUMBBL core in later phases) -----
    void setTeams(const QString &homeName, const QString &awayName);
    void setScore(int home, int away);
    void setTurn(int turn);
    void setSpectators(int count);
    void setConnectionStatus(const QString &status, bool connected);

    // Resolve a raw server id (player OR team) to a display name against
    // the current model. Public so tests can drive the action-log naming.
    QString resolveId(const QString &id) const;
    void appendDiceLog(const DiceLogLine &line);
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
    // Show the info card for a player (no selection: the spectator does not
    // select players — the field rim glow is the REMOTE coach's selection,
    // driven by the model's actingPlayer).
    void showPlayerCardById(const QString &playerId);
    // Select a model player (field highlight + Player Info panel). Public so
    // tests can drive the same path the field/roster clicks take.

    // Round 10: match-list team logos (e.g. "/i/486335") used as the logo
    // source when the game state's logoUrl is null. Public so tests can
    // drive the same path the Spectate dialog takes.
    void setFallbackTeamLogos(const QString &home, const QString &away);

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
    // Wipe all match state (model/field/rosters/logs/selection) when the
    // user starts a different game from "Spectate Game". Public (it is a
    // controller entry point — the Spectate Game lambda calls it — so tests
    // and tooling can drive the exact same path).
public:
    void clearMatchUi();

    // The spectator session (for tooling/tests).
    SpectatorSession *session() const { return m_session; }

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
    void onSessionStatus(const QString &status, const QString &message);
    void onConnectRequested();
    // Field click: empty id = deselect.
    void onFieldPlayerClicked(const QString &playerId);
    // Hover card (round 8): show/hide the player info on the opposite side
    // of the field from the hovered player. Empty token = hide.
    void onPlayerHovered(const FieldView::PlayerToken &token);
    void showPlayerCard(const FFBPlayer *p, int fieldX);
    void hidePlayerCard();
    // Pin the corner overlays (action log / chat) to the stage.
    void pinOverlays();
    // The FloatingPanel wrapping a content widget (nullptr if none).
    FloatingPanel *panelFor(QWidget *content);
    // Per-match team logos (field watermarks + header crests): fetched from
    // the game state's logoUrl once per match.
    void loadTeamLogos();
    void onLogoReplyFinished(QNetworkReply *reply);


    bool m_layoutApplied = false; // default/saved geometry applied once

    // Core
    SpectatorSession *m_session = nullptr;
    GameModel *m_model = nullptr;
    QNetworkAccessManager *m_logoNam = nullptr; // team logo downloads
    QString m_homeLogoUrl; // currently loaded/loaded-in-flight logo URLs
    QString m_awayLogoUrl;
    // Round 10: match-list logo paths (set from the Spectate dialog); used
    // when the game state's logoUrl is null — the common case.
    QString m_fallbackHomeLogo;
    QString m_fallbackAwayLogo;
    QPixmap m_homeLogoPm;
    QPixmap m_awayLogoPm;

    // Widgets
    QWidget *m_stage = nullptr; // central container: field + overlay panels
    QToolBar *m_headerToolbar = nullptr;
    HeaderView *m_headerView = nullptr; // Mockup F header (custom-painted)
    FieldView *m_field = nullptr;
    PlayerInfoWidget *m_playerInfo = nullptr;
    ActionLogWidget *m_actionLog = nullptr; // bottom-left overlay
    ChatWidget *m_chat = nullptr;           // bottom-right overlay

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
    QLabel *m_spectatorsLabel = nullptr;
};
