// =============================================================================
// MainWindow — FUMBBL Reborn main window
//
// HUD layout (Mockup F):
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
//     strips.
// =============================================================================
#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QRect>

#include "core/matchlist.h"
#include "core/reportparser.h"
#include "ui/fieldview.h"

class ActionLogWidget;
class ChatWidget;
class FloatingPanel;
class LoginDialog;
class MatchListClient;
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
    // gateLogin: the client login gate (LoginDialog modal over the
    // stadium at startLogin()/after Log out; the Spectate dialog is
    // unreachable before a verified login). Production (main.cpp) passes
    // true; tools and tests stay un-gated.
    explicit MainWindow(QWidget *parent = nullptr, bool gateLogin = false);

    // --- UI state updaters (driven by the FUMBBL core) ----------------------
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
    // Public so it can be invoked from --spectate-now / tests. Gated: if
    // the client login has not been verified yet, the LoginDialog comes
    // first (and the spectate dialog only opens after a verified login).
    void openSpectateDialog();
    // Client login gate (product decision 0.2.2): the modal LoginDialog
    // over the stadium. While it runs, nothing in the client is clickable
    // (the dialog is modal — menus included). The credentials are verified
    // against the FUMBBL server (a join attempt — the server checks the
    // password there); on success the dialog closes and the full UI is
    // live (the user is already spectating the verification game).
    // Returns true when the login was verified.
    bool startLogin();
    // True once the login was verified by the server (until Log out).
    bool isAuthenticated() const { return m_authenticated; }
    // View-menu equivalent for one floating panel (tests/screenshots).
    void setPanelVisible(const QString &name, bool visible);

    // Panel layout persistence (JSON via QSettings).
    QByteArray savedLayout() const;
    void restoreLayout(const QByteArray &state);
    // Restore the default panel arrangement (positions AND sizes) — the
    // "Reset Layout" action. Public so tests can verify the size-restore fix.
    void resetLayout();

    // --- Core (spectator) -----------------------------------------------------
    // The game model (exposed for tests / tooling; the UI reads from it).
    GameModel *model() const { return m_model; }
    // Show the info card for a player (no selection: the spectator does not
    // select players — the field rim glow is the REMOTE coach's selection,
    // driven by the model's actingPlayer).
    void showPlayerCardById(const QString &playerId);

    // Match-list team logos (e.g. "/i/486335") used as the logo source when
    // the game state's logoUrl is null. Public so tests can drive the same
    // path the Spectate dialog takes.
    void setFallbackTeamLogos(const QString &home, const QString &away);
    // Test seams (no live network): put the gate in the "verification
    // attempt in flight" state — the state half of onLoginRequested, whose
    // match-list fetch the test skips; markAttemptSocketStartedForTest
    // stands in for connectAsSpectator.
    void beginLoginAttemptForTest(const QString &coach, const QByteArray &passwordMd5,
                                  LoginDialog *dialog);
    void markAttemptSocketStartedForTest();
    // TEMPORARY (throwaway live harness): the FULL production login
    // attempt (real match-list fetch, real game selection, real session
    // join) with an explicit dialog so the gate branch is active.
    void startLoginAttemptForLiveTest(const QString &coach, const QByteArray &passwordMd5,
                                      LoginDialog *dialog);
    // The connection status line exactly as the user reads it.
    QString connectionStatusForTest() const;

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
    void onWsClosed(int code, const QString &reason);

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

    // Set the active coach (empty = none) and the Log out action's enabled
    // state. Production: the connect flow sets it, Log out clears it.
    // Public so the shared-PC contract can be exercised by tests.
    void setActiveCoach(const QString &coach);

private:
    void createActions();
    void createMenus();
    void createToolbar();
    void createCentralWidget();
    void createPanels();
    void createStatusBar();

    // Panel layout.
    FloatingPanel *makePanel(const QString &name, const QString &title,
                             QWidget *content, const QColor &accent);
    void applyDefaultLayout();
    void applySavedLayout(const QByteArray &state);
    void clampPanelsToStage();
    void onStageFirstSized();
    QRect defaultGeometryFor(const QString &name) const;

    void onSessionStatus(const QString &status, const QString &message);
    void onConnectRequested();
    // Report dedup: the server re-sends reports (step retries / re-syncs)
    // and the official client does NOT dedupe — we do: a report whose
    // canonical fingerprint arrived within the last kDedupWindowMs is
    // dropped; a batch large enough to be a FULL re-sync (>= kResyncBatch
    // reports while the log is already populated) rebuilds the log from the
    // batch.
    //
    // Window sizing: the same player can legitimately repeat an identical
    // report within a couple of minutes (move -> down -> stand up -> move;
    // the same defender blocked twice; turnEnd is identical every turn).
    // Capture ground truth (game_1957772): step-retry re-sends arrive
    // within ~1-3 lines (a couple of seconds), while real repeats are tens
    // of seconds apart. 2.5s catches the retries and lets the real
    // repeats through.
    static constexpr int kDedupWindowMs = 2500;
    static constexpr int kResyncBatch = 10;
    QHash<QString, qint64> m_reportFpTimes; // fingerprint -> last-seen ms
    int m_lastKickoffSeq = 0; // kickoff flights already started
    QElapsedTimer m_clock;
    int m_loggedReportCount = 0;
    // Field click: empty id = deselect.
    void onFieldPlayerClicked(const QString &playerId);
    // Hover card: show/hide the player info on the opposite side
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
    // File > Log out (shared PC): end the session, clear this coach's
    // remembered credential and re-open the login gate — the user picks:
    // new credentials or quit (the dialog's Quit button).
    void onLogout();


    bool m_layoutApplied = false; // default/saved geometry applied once

    // Core
    SpectatorSession *m_session = nullptr;
    GameModel *m_model = nullptr;
    QNetworkAccessManager *m_logoNam = nullptr; // team logo downloads
    QString m_homeLogoUrl; // currently loaded/loaded-in-flight logo URLs
    QString m_awayLogoUrl;
    // Match-list logo paths (set from the Spectate dialog); used when the
    // game state's logoUrl is null — the common case.
    QString m_fallbackHomeLogo;
    QString m_fallbackAwayLogo;
    QPixmap m_homeLogoPm;
    QPixmap m_awayLogoPm;
    // The coach the session was requested for (empty = no active session).
    // Drives the Log out action's enabled state; cleared by Log out only.
    QString m_activeCoach;

    // --- Client login gate (0.2.2) -----------------------------------------
    // The LoginDialog is modal over the stadium; while it runs nothing in
    // the client is clickable (menus included). The credentials it yields
    // are VERIFIED against the server before the dialog can close: the
    // join attempt is the verification (the server checks the password
    // there). m_loginCoach/m_loginMd5 then feed every spectate.
    bool m_gateLogin = false;
    bool m_authenticated = false;
    bool m_authenticating = false; // a login attempt is in flight
    // True once the in-flight attempt's socket is up (set right before
    // connectAsSpectator). A ws close arriving BEFORE that belongs to a
    // PREVIOUS attempt (the server's late 1000 for a rejected login, or
    // the previous session's close after a Log out) and must not cancel
    // this verification.
    bool m_attemptSocketStarted = false;
    // The verified-login teardown: after the gate's join confirmed the
    // credentials, the verification session is closed on purpose (a
    // successful login enables the UI, it does NOT spectate the random
    // match the join landed on). Its close (1000) is silent.
    bool m_loginTeardown = false;
    // While a LOGIN VERIFICATION join is in flight (from the join start
    // until the post-verification wipe): the server streams the
    // verification game's state as soon as the session opens — that
    // match must NEVER reach the UI (no field, no rosters, no chat).
    // A successful login only enables the interface; the user chooses
    // a match from Spectate Game.
    bool m_loginSuppressMatchUi = false;
    QString m_loginCoach;
    QByteArray m_loginMd5;
    LoginDialog *m_loginDialog = nullptr;
    MatchListClient *m_loginList = nullptr; // picks the verification game
    // Login gate handlers.
    void onLoginRequested(const QString &coach, const QByteArray &passwordMd5);
    void onLoginMatchesReceived(QVector<FFBLiveMatch> matches);

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
