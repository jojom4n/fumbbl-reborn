// =============================================================================
// MainWindow — implementation
//
// HUD layout: the field fills the whole window; the panels are translucent
// FloatingPanels that float over it (draggable by their title strip,
// resizable by their edges, hideable with ×). Layout persistence is a small
// JSON (positions AND sizes) in QSettings; "Reset Layout" restores the
// default arrangement with the default sizes.
// =============================================================================
#include "ui/mainwindow.h"

#include "core/assets.h"
#include "core/credentialstore.h"
#include "core/fumbblws.h"
#include "core/gamemodel.h"
#include "core/reportparser.h"
#include "core/spectatorsession.h"
#include "ui/chatwidget.h"
#include "ui/logindialog.h"
#include "ui/spectatedialog.h"
#include "ui/actionlog.h"
#include "ui/fieldview.h"

#include <algorithm>
#include "ui/headerview.h"
#include "ui/panel.h"
#include "ui/playerinfowidget.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCryptographicHash>
#include <QEvent>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QWebSocketProtocol>
#include <QToolBar>

#include <iterator>
#include <QSettings>
#include <QStatusBar>


namespace {
const QString kLayoutSettingsKey = QStringLiteral("mainwindow/panelLayout");
} // namespace

MainWindow::MainWindow(QWidget *parent, bool gateLogin)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("FUMBBL Reborn"));
    resize(1400, 860);
    m_gateLogin = gateLogin;

    createActions();
    createToolbar();
    createCentralWidget();
    createPanels();
    createStatusBar();
    createMenus(); // after the panels: the View menu toggles them

    // The window starts maximized so the field is as large as possible;
    // the default/saved panel geometry is applied on the first stage resize
    // (caught with an event filter), and clamped on every subsequent resize.
    m_stage->installEventFilter(this);
    showMaximized();

    // --- Core (spectator) wiring --------------------------------------------
    m_model = new GameModel(this);
    m_session = new SpectatorSession(this);

    // Login gate: the public match list picks the VERIFICATION game (the
    // join attempt is where the server checks the password).
    m_loginList = new MatchListClient(this);
    connect(m_loginList, &MatchListClient::matchesReceived, this,
            &MainWindow::onLoginMatchesReceived);
    connect(m_loginList, &MatchListClient::fetchFailed, this,
            [this](const QString &message) {
                if (!m_authenticating || !m_loginDialog)
                    return;
                m_authenticating = false;
                m_loginDialog->showServerMessage(
                    tr("Could not reach the FUMBBL server: %1").arg(message.left(160)));
            });

    // QElapsedTimer::elapsed() on a timer that was NEVER started returns a
    // huge NEGATIVE value in Qt 6 — the report-dedup window test (now -
    // last < window) was then true for every report and the action log
    // stayed empty. Start the clock up front.
    m_clock.start();

    // Team logo downloads (field watermarks + header crests).
    m_logoNam = new QNetworkAccessManager(this);

    connect(m_session, &SpectatorSession::gameStateReceived, this, &MainWindow::onGameState);
    connect(m_model, &GameModel::stateUpdated, this, &MainWindow::applyModelToUi);
    connect(m_session, &SpectatorSession::modelSyncReceived, this, &MainWindow::onModelSync);
    connect(m_session, &SpectatorSession::gameTimeReceived, this, &MainWindow::onGameTime);
    connect(m_session, &SpectatorSession::talkReceived, this, &MainWindow::onTalk);
    connect(m_session, &SpectatorSession::joined, this, &MainWindow::onJoined);
    connect(m_session, &SpectatorSession::progress, this, &MainWindow::onSessionProgress);
    connect(m_session, &SpectatorSession::fatal, this, &MainWindow::onSessionFatal);
    connect(m_session, &SpectatorSession::wsClosed, this, &MainWindow::onWsClosed);
    connect(m_session, &SpectatorSession::statusReceived, this, &MainWindow::onSessionStatus);
    // A socket drop schedules a reconnect; once it re-opens the login
    // exchange (version + challenge + join) re-runs automatically.
    connect(m_session->ws(), &FumbblWs::reconnectScheduled, this,
            [this](int attempt) {
                setConnectionStatus(tr("Connection lost — reconnecting (attempt %1)…")
                                       .arg(attempt),
                                    false);
            });
    connect(m_session, &SpectatorSession::protocolSending, this,
            [this](const QString &id, const QJsonObject &)
            { this->appendProtocol(QStringLiteral(">> send "), id); });
    connect(m_session, &SpectatorSession::protocolReceived, this,
            [this](const QString &id, const QJsonObject &)
            { this->appendProtocol(QStringLiteral("<< recv "), id); });

    // Field click-to-select (empty id = clicked an empty square = deselect).
    connect(m_field, &FieldView::playerClicked, this, &MainWindow::onFieldPlayerClicked);
    // Hover card: the player info follows the mouse over the field / bench
    // and sits on the opposite side of the field.
    connect(m_field, &FieldView::playerHovered, this, &MainWindow::onPlayerHovered);
    // Hover a player NAME in the action log -> show that player's card
    // (like the field hover/click). Empty id = the mouse left the name.
    connect(m_actionLog, &ActionLogWidget::playerHoveredId, this,
            [this](const QString &playerId) {
                if (playerId.isEmpty()) {
                    hidePlayerCard();
                    return;
                }
                showPlayerCardById(playerId);
            });

    // The chat input is relayed to the session (clientTalk); no-op while
    // not joined.
    connect(this, &MainWindow::chatMessageSent, this,
            [this](const QString &message) {
                if (m_session && m_session->isJoined())
                    m_session->ws()->sendChat(message);
            });
    connect(m_connectAction, &QAction::triggered, this, &MainWindow::connectionRequested);
    // The menu action relays into the connectionRequested signal; this is
    // what turns it into the actual dialog.
    connect(this, &MainWindow::connectionRequested, this, &MainWindow::onConnectRequested);
}

// -----------------------------------------------------------------------------
// Actions / Menus
// -----------------------------------------------------------------------------
void MainWindow::createActions()
{
    m_connectAction = new QAction(tr("&Spectate Game…"), this);
    m_connectAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+J")));
    m_connectAction->setStatusTip(tr("Watch a live FUMBBL match as spectator"));

    // Shared PC: end THIS coach's session — the disconnect plus the
    // remembered credential are both gone, so the next player's dialog
    // opens empty. The keychain entry survives (returning coach retypes
    // only the name, never the password).
    m_disconnectAction = new QAction(tr("Log &out"), this);
    m_disconnectAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+K")));
    m_disconnectAction->setStatusTip(
        tr("End the session and forget the coach for the next player"));
    m_disconnectAction->setEnabled(false);
    connect(m_disconnectAction, &QAction::triggered, this, &MainWindow::onLogout);

    m_quitAction = new QAction(tr("&Quit"), this);
    m_quitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
    connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_resetLayoutAction = new QAction(tr("Reset &Layout"), this);
    m_resetLayoutAction->setStatusTip(tr("Restore the default panel arrangement "
                                         "(positions and sizes)"));
    connect(m_resetLayoutAction, &QAction::triggered, this, &MainWindow::resetLayout);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_connectAction);
    fileMenu->addAction(m_disconnectAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    // Rosters are not in the menu (the field box strips cover box/KO/
    // reserve); action log and chat are pinned overlays (always visible,
    // not toggleable). "Player Info" is NOT in the menu either: the card
    // works (and must work) on HOVER over a player, and only the hover
    // drives it.
    viewMenu->addAction(m_panelViewActions.value(QStringLiteral("protocol")));
    viewMenu->addSeparator();
    viewMenu->addAction(m_resetLayoutAction);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    auto *aboutAction = new QAction(tr("&About FUMBBL Reborn"), this);
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("About FUMBBL Reborn"),
                           tr("<h3>FUMBBL Reborn %1</h3>"
                              "<p>Native Qt6 client for the FUMBBL online "
                              "platform (spectator mode).</p>"
                              "<p>Protocol compatible with the official FUMBBL client "
                              "(LZString-compressed WebSocket, 2s ping).</p>")
                               .arg(QApplication::applicationVersion()));
    });
    helpMenu->addAction(aboutAction);
    auto *aboutQtAction = new QAction(tr("About &Qt"), this);
    connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
    helpMenu->addAction(aboutQtAction);
}

// -----------------------------------------------------------------------------
// Toolbar / Dock manager / Central / Docks
// -----------------------------------------------------------------------------
void MainWindow::createToolbar()
{
    // The Mockup F header is a custom-painted strip (gold embossed team on
    // turn, reroll dots, END TURN, count-up timer, weather). It lives in a
    // plain toolbar so it keeps the toolbar's movable behavior.
    m_headerToolbar = new QToolBar(this);
    m_headerToolbar->setObjectName(QStringLiteral("header_toolbar"));
    m_headerToolbar->setMovable(true);
    addToolBar(Qt::TopToolBarArea, m_headerToolbar);
    m_headerView = new HeaderView(m_headerToolbar);
    m_headerToolbar->addWidget(m_headerView);
    // No header content before a game state arrives — the whole strip is
    // hidden until the first sync.
    m_headerToolbar->setVisible(false);
    // No END TURN — this is a spectator client; ending the turn is not part
    // of spectating (the button is gone from HeaderView itself).
}

void MainWindow::createCentralWidget()
{
    // The stage is the central widget: it hosts the field (full rect) plus
    // the floating overlay panels, so everything shares one top-level window.
    m_stage = new QWidget(this);
    m_stage->setObjectName(QStringLiteral("stage"));
    // Light stage (Mockup F): the field covers it; the paper color only shows
    // if a sliver is ever exposed during resize.
    m_stage->setStyleSheet(QStringLiteral("#stage { background: #e9e9ee; }"));

    m_field = new FieldView(m_stage);
    m_field->setObjectName(QStringLiteral("field"));
    setCentralWidget(m_stage);
}

void MainWindow::setPanelVisible(const QString &name, bool visible)
{
    if (auto *a = m_panelViewActions.value(name, nullptr))
        a->setChecked(visible);
}

FloatingPanel *MainWindow::makePanel(const QString &name, const QString &title,
                                     QWidget *content, const QColor &accent)
{
    auto *panel = new FloatingPanel(title, content, m_stage);
    panel->setAccent(accent);
    panel->hide(); // shown once the default/saved geometry is applied
    connect(panel, &FloatingPanel::hideRequested, panel, &QWidget::hide);

    auto *viewAction = new QAction(title, this);
    viewAction->setCheckable(true);
    viewAction->setChecked(true);
    connect(viewAction, &QAction::toggled, panel, &QWidget::setVisible);
    m_panelViewActions.insert(name, viewAction);
    m_panels.insert(name, panel);
    return panel;
}

void MainWindow::createPanels()
{
    const QColor homeAccent(0x7c, 0xa8, 0x6f);
    const QColor awayAccent(0xd8, 0xc0, 0x7a);
    const QColor neutral(0x8f, 0xa6, 0xc0);
    Q_UNUSED(homeAccent)
    Q_UNUSED(awayAccent)

    // Player info: a FloatingPanel driven from HOVER, pinned to the side of
    // the field OPPOSITE the hovered player. The hover card is CHROMELESS —
    // no "Player Info" title text, no close button; the card content fills
    // the whole panel.
    m_playerInfo = new PlayerInfoWidget(m_stage);
    FloatingPanel *playerInfoPanel =
        makePanel(QStringLiteral("player_info"), tr("Player Info"), m_playerInfo, neutral);
    playerInfoPanel->setChromeless(true);

    // Action log (bottom-left) and chat (bottom-right): Mockup F pinned
    // overlays with their own translucent chrome — NOT draggable panels.
    m_actionLog = new ActionLogWidget(m_stage);
    m_actionLog->setFixedWidth(560);
    // The action log EXPANDS UPWARD — it stays pinned to the bottom, so
    // when the chevron grows it taller, re-pin it: the top edge lifts
    // (growing downwards would run off the visible bottom of the screen).
    connect(m_actionLog, &ActionLogWidget::expandedChanged, this,
            [this](bool) { pinOverlays(); });

    m_chat = new ChatWidget(m_stage);
    m_chat->setFixedSize(560, 250);
    // Relay the chat input to the session (the send is a no-op while
    // not joined — see the chatMessageSent relay in the connect block).
    connect(m_chat, &ChatWidget::messageSent, this, &MainWindow::chatMessageSent);
    // The chat collapses to the pill (128x52) — re-pin the corner overlay
    // whenever the size changes.
    connect(m_chat, &ChatWidget::collapsedChanged, this, [this](bool) { pinOverlays(); });
    // The chat starts COLLAPSED (the mockup shows the pill).
    m_chat->setCollapsed(true);

    m_protocolLog = new QPlainTextEdit(m_stage);
    m_protocolLog->setObjectName(QStringLiteral("protocol"));
    m_protocolLog->setReadOnly(true);
    m_protocolLog->setMaximumBlockCount(4000);
    m_protocolLog->setStyleSheet(QStringLiteral("background: rgba(10, 13, 18, 200); "
                                                "color: #9fb0c4;"));
    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(8);
    m_protocolLog->setFont(mono);
    makePanel(QStringLiteral("protocol"), tr("Protocol Log"), m_protocolLog, neutral);

    pinOverlays();
}

QRect MainWindow::defaultGeometryFor(const QString &name) const
{
    // Relative to the stage size; clamped by applyDefaultLayout().
    const int w = m_stage ? m_stage->width() : 1400;
    const int h = m_stage ? m_stage->height() : 860;
    // Grid-aligned (FloatingPanel::kGridStep = 20) — the panels snap to this
    // invisible grid while dragged, so the defaults live on it too.
    const int G = 20;
    if (name == QLatin1String("player_info"))
        // Hover-driven: the default left slot; onPlayerHovered re-pins it
        // to the side opposite the player.
        return QRect(G, ((h - 340) / G) * G, 440, 320);
    if (name == QLatin1String("protocol"))
        // Right edge stays 70px clear of the window edge: the away box strip
        // lives there.
        return QRect((w / G) * G - 290, G, 220, qMax(160, ((h - 580) / G) * G));
    return QRect(40, 40, 320, 240);
}

void MainWindow::resetLayout()
{
    applyDefaultLayout();
}

void MainWindow::applyDefaultLayout()
{
    for (auto it = m_panels.cbegin(); it != m_panels.cend(); ++it) {
        QRect g = defaultGeometryFor(it.key());
        // Clamp to the stage and minimum panel size.
        if (m_stage) {
            g = g.intersected(QRect(QPoint(0, 0), m_stage->size()));
            if (g.width() < FloatingPanel::kMinWidth)
                g.setWidth(FloatingPanel::kMinWidth);
            if (g.height() < FloatingPanel::kMinHeight)
                g.setHeight(FloatingPanel::kMinHeight);
        }
        it.value()->setGeometry(g);
        // Player Info is hover-driven: a saved/last "visible" state is the
        // residue of the last hover, not a user choice — never start it up.
        if (it.key() == QLatin1String("player_info")) {
            it.value()->hide();
        } else {
            it.value()->show();
            it.value()->raise();
        }
    }
    for (auto it = m_panelViewActions.cbegin(); it != m_panelViewActions.cend(); ++it) {
        it.value()->blockSignals(true);
        it.value()->setChecked(it.key() != QLatin1String("player_info"));
        it.value()->blockSignals(false);
    }
    m_layoutApplied = true;
}

void MainWindow::applySavedLayout(const QByteArray &state)
{
    if (state.isEmpty()) {
        applyDefaultLayout();
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(state);
    const QJsonObject panels = doc.object().value(QStringLiteral("panels")).toObject();
    bool any = false;
    for (auto it = m_panels.cbegin(); it != m_panels.cend(); ++it) {
        const QJsonObject p = panels.value(it.key()).toObject();
        if (p.isEmpty())
            continue;
        any = true;
        QRect g = defaultGeometryFor(it.key()); // sane base, then override
        if (p.contains(QStringLiteral("x"))) g.moveLeft(p.value(QStringLiteral("x")).toInt());
        if (p.contains(QStringLiteral("y"))) g.moveTop(p.value(QStringLiteral("y")).toInt());
        if (p.contains(QStringLiteral("w"))) g.setWidth(p.value(QStringLiteral("w")).toInt());
        if (p.contains(QStringLiteral("h"))) g.setHeight(p.value(QStringLiteral("h")).toInt());
        // Player Info is hover-driven: never restore it visible (see
        // applyDefaultLayout) — its saved v:true is the last hover's
        // residue, not a choice.
        bool vis;
        if (it.key() == QLatin1String("player_info"))
            vis = false;
        else
            vis = p.contains(QStringLiteral("v")) ? p.value(QStringLiteral("v")).toBool() : true;
        it.value()->setGeometry(g);
        it.value()->setVisible(vis);
        // Sync the View-menu action from the SAVED value, not from
        // isVisible() (false at this point: the top-level is not shown yet,
        // and the action's toggled signal would re-hide the panel).
        if (QAction *action = m_panelViewActions.value(it.key())) {
            action->blockSignals(true);
            action->setChecked(vis);
            action->blockSignals(false);
        }
    }
    if (any) {
        clampPanelsToStage();
        m_layoutApplied = true;
    } else {
        applyDefaultLayout();
    }
}

void MainWindow::clampPanelsToStage()
{
    if (!m_stage)
        return;
    for (auto it = m_panels.cbegin(); it != m_panels.cend(); ++it) {
        FloatingPanel *panel = it.value();
        if (!panel->isVisible())
            continue;
        QRect g = panel->geometry();
        // Keep a 60px slice of the title reachable; never leave the stage.
        g.moveLeft(qBound(-(g.width() - 60), g.x(), m_stage->width() - 60));
        g.moveTop(qBound(0, g.y(), m_stage->height() - FloatingPanel::kTitleHeight));
        g.setWidth(qMin(g.width(), m_stage->width() - g.x()));
        g.setHeight(qMin(g.height(), m_stage->height() - g.y()));
        if (g != panel->geometry())
            panel->setGeometry(g);
    }
}

// One-shot: apply the saved (or default) layout the first time the stage has
// a real size (the window starts maximized, so this fires right after show).
void MainWindow::onStageFirstSized()
{
    if (m_layoutApplied)
        return;
    QSettings settings;
    applySavedLayout(settings.value(kLayoutSettingsKey).toByteArray());
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_stage && event->type() == QEvent::Resize) {
        m_field->setGeometry(m_stage->rect());
        onStageFirstSized();
        clampPanelsToStage();
        pinOverlays();
    }
    return QMainWindow::eventFilter(obj, event);
}


void MainWindow::createStatusBar()
{
    m_statusLabel = new QLabel(tr("Ready"), this);
    statusBar()->addWidget(m_statusLabel);

    m_connectionLabel = new QLabel(tr("Not connected"), this);
    m_connectionLabel->setStyleSheet(QStringLiteral("color: #888;"));
    statusBar()->addPermanentWidget(m_connectionLabel);

    // The Mockup F header has no spectators slot — it lives in the status bar.
    m_spectatorsLabel = new QLabel(tr("Spectators: 0"), this);
    statusBar()->addPermanentWidget(m_spectatorsLabel);
}

// -----------------------------------------------------------------------------
// UI state updaters
// -----------------------------------------------------------------------------
void MainWindow::setTeams(const QString &homeName, const QString &awayName)
{
    m_headerView->setTeams(homeName, awayName);
    if (m_model) {
        m_headerView->setRaces(m_model->teamHomeRace(), m_model->teamAwayRace());
        m_headerView->setCoaches(m_model->teamHomeCoach(), m_model->teamAwayCoach());
    }
    m_field->setHomeColor(PlayerInfoWidget::teamColor(homeName, true));
    m_field->setAwayColor(PlayerInfoWidget::teamColor(awayName, false));
    if (m_model) {
        m_field->setHomeRace(m_model->teamHomeRace());
        m_field->setAwayRace(m_model->teamAwayRace());
    }
}

void MainWindow::setScore(int home, int away)
{
    m_headerView->setScores(home, away);
}

void MainWindow::setTurn(int turn)
{
    m_headerView->setTurn(turn, m_model ? m_model->half() : 1);
}

void MainWindow::setSpectators(int count)
{
    m_spectatorsLabel->setText(tr("Spectators: %1").arg(count));
}

QString MainWindow::connectionStatusForTest() const
{
    return m_statusLabel->text();
}

void MainWindow::setConnectionStatus(const QString &status, bool connected)
{
    m_statusLabel->setText(status);
    m_connectionLabel->setText(connected ? tr("Connected") : tr("Not connected"));
    m_connectionLabel->setStyleSheet(connected ? QStringLiteral("color: #6fbf73;")
                                               : QStringLiteral("color: #888;"));
}

void MainWindow::appendDiceLog(const DiceLogLine &line)
{
    // The 4th/5th args are the RAW ids — the log's name labels are
    // HOVERABLE and need the id to open the player's card.
    // Names are tinted with the TEAM color (like Blood Bowl 3's log), and
    // the is-player flags tell the log which names are hoverable (a TEAM
    // id has no card — its name stays plain).
    DiceLogLine l = line; // the model is authoritative: recompute the flags
    l.actorIsPlayer = l.actorId.isEmpty() || m_model->findPlayerById(l.actorId);
    l.defenderIsPlayer = l.defenderId.isEmpty() || m_model->findPlayerById(l.defenderId);

    const auto teamColorOf = [this](const QString &id) -> QColor {
        if (const FFBPlayer *p = m_model ? m_model->findPlayerById(id) : nullptr)
            return PlayerInfoWidget::teamColor(
                p->sideHome ? m_model->teamHomeName() : m_model->teamAwayName(),
                p->sideHome);
        if (id == m_model->teamHomeId())
            return PlayerInfoWidget::teamColor(m_model->teamHomeName(), true);
        if (id == m_model->teamAwayId())
            return PlayerInfoWidget::teamColor(m_model->teamAwayName(), false);
        return QColor(); // not a player nor a team: neutral ink
    };
    m_actionLog->appendLine(l, resolveId(line.actorId),
                            resolveId(line.defenderId),
                            line.actorId, line.defenderId,
                            teamColorOf(line.actorId),
                            teamColorOf(line.defenderId));
}

void MainWindow::appendChat(const QString &sender, const QString &text)
{
    m_chat->addMessage(sender, text);
}

void MainWindow::appendProtocol(const QString &direction, const QString &text)
{
    m_protocolLog->appendPlainText(QStringLiteral("%1 %2").arg(direction, text));
}

// -----------------------------------------------------------------------------
// Core -> UI wiring (spectator)
// -----------------------------------------------------------------------------
void MainWindow::onGameState(const QJsonObject &game)
{
    if (m_loginSuppressMatchUi)
        return; // verification join: its game state never reaches the UI
    if (m_model->initialize(game))
        setConnectionStatus(tr("Game state received"), true);
}

void MainWindow::applyModelToUi()
{
    if (!m_model || !m_model->isInitialized())
        return;

    setTeams(m_model->teamHomeName(), m_model->teamAwayName());
    setScore(m_model->scoreHome(), m_model->scoreAway());
    setTurn(m_model->turn());

    // Header (Mockup F): reroll dots, on-turn gold emphasis, weather, team
    // accents — all straight from the model.
    // N dots = the team's rerolls (0..6); gold while available this turn,
    // white once used (turnData.reRolls).
    m_headerView->setRerolls(m_model->reRollsHome(), m_model->reRollsAway(),
                             m_model->turnReRollsHome(), m_model->turnReRollsAway());
    // The header strip exists only once a game state has loaded — this
    // function is only reached after initialize(), so make it visible now.
    m_headerToolbar->setVisible(true);
    m_headerView->setHomePlaying(m_model->homePlaying());
    m_headerView->setWeather(m_model->weather());
    m_headerView->setAccents(
        PlayerInfoWidget::teamColor(m_model->teamHomeName(), true),
        PlayerInfoWidget::teamColor(m_model->teamAwayName(), false));
    m_headerView->setRaces(m_model->teamHomeRace(), m_model->teamAwayRace());
    m_headerView->setCoaches(m_model->teamHomeCoach(), m_model->teamAwayCoach());

    // Per-match team logos (fetched async; applied when the reply lands).
    loadTeamLogos();

    // Field: split the model tokens by side (box players are already filtered
    // out by the model).
    QVector<FieldView::PlayerToken> homeTokens;
    QVector<FieldView::PlayerToken> awayTokens;
    for (const FieldToken &t : m_model->fieldTokens()) {
        FieldView::PlayerToken pt;
        pt.x = t.x;
        pt.y = t.y;
        pt.number = t.number;
        pt.hasBall = t.hasBall;
        pt.state = t.state;
        pt.name = t.name;
        pt.playerId = t.playerId;
        pt.positionName = t.positionName;
        pt.isActing = t.isActing;
        (t.sideHome ? homeTokens : awayTokens).append(pt);
    }
    m_field->setPlayers(homeTokens, awayTokens);

    // The actor's CUMULATIVE move trail (one polyline per move action,
    // drawn progressively behind the player as the server steps him square
    // by square; cleared when the actor changes or a new turn starts —
    // see GameModel::moveSegments()).
    m_field->setMoveTrace(m_model->moveSegments(),
                          m_model->blockStepMarks());

    // Bench box strips (left = home, right = away): players with a box
    // coordinate (x < 0 or x > 25), grouped by state inside the field view.
    auto boxEntries = [this](bool sideHome) {
        const auto roster = sideHome ? m_model->rosterHome() : m_model->rosterAway();
        const QString race = sideHome ? m_model->teamHomeRace() : m_model->teamAwayRace();
        QVector<FieldView::BoxEntry> out;
        for (const FFBPlayer &pl : roster) {
            if (!pl.hasCoords || !GameModel::isBoxX(pl.fieldX))
                continue;
            FieldView::BoxEntry e;
            e.name = pl.name;
            e.positionName = pl.position;
            e.race = race;
            e.state = pl.state;
            e.number = pl.number; // number badge on the bench face
            out.append(e);
        }
        // The dugout is SORTED: reserves first, then KO (downed, state
        // base 5), then injured (badly hurt 6 / serious injury 7); the rest
        // (dead/banned) last. Roster order is kept inside each group
        // (stable sort).
        // PlayerState bases (ffb-common PlayerState.java):
        //   5 KNOCKED_OUT, 6 BADLY_HURT, 7 SERIOUS_INJURY, 8 RIP, 9 RESERVE
        const auto rank = [](int state) {
            const int base = state & 0xFF;
            if (base == 9)
                return 0; // reserve (never on the field)
            if (base == 5)
                return 1; // KO
            if (base == 6 || base == 7)
                return 2; // injured
            return 3;     // dead / banned / other
        };
        std::stable_sort(out.begin(), out.end(),
                         [&rank](const FieldView::BoxEntry &a,
                                 const FieldView::BoxEntry &b) {
                             return rank(a.state) < rank(b.state);
                         });
        return out;
    };
    m_field->setBoxes(boxEntries(true), boxEntries(false));

    // The field rim GLOW is the REMOTE human's selection — it follows the
    // game state's actingPlayer (set live by the actingPlayerSetPlayerId
    // model change when the coach picks a player to act, cleared when the
    // action ends). The spectator does NOT select players: clicks only
    // drive the info card.
    const QString acting = m_model->activePlayerId();
    if (!acting.isEmpty() && m_model->findPlayerById(acting))
        m_field->setSelectedPlayer(acting);
    else
        m_field->setSelectedPlayer(QString());

    // The RED glow on the player being blocked (armed by the blockRoll/
    // block report, cleared by the model when the block resolves). A FOUL
    // target is red-glow-only (no helmet/die marker between the pair —
    // there is no die in a foul).
    if (m_model->blockGlowOnly())
        m_field->setBlockMarker(QString(), m_model->blockTargetId(), 0);
    else
        m_field->setBlockMarker(m_model->blockAttackerId(), m_model->blockTargetId(),
                                m_model->blockDieFace());

    // The per-turn token marks ("come su FUMBBL"), straight from the
    // model. (The move numbers are derived by the field view from the move
    // trace itself: number k sits BETWEEN step k's two cells.)
    m_field->setActedPlayers(m_model->actedThisTurn());
    m_field->setBlitzingPlayer(m_model->blitzingPlayerId());
    // The pass receiver's WHITE glow (fieldModelSetTargetSelectionState
    // SELECTED -> model->passTargetId()).
    m_field->setPassTarget(m_model->passTargetId());
    // The KICKOFF flight — the model bumps its seq when the kickoff
    // landing cell arrives (the wire sends only the landing; the
    // trajectory + bounces + kicker glow are synthesized in the field
    // view).
    if (m_model->kickoffSeq() != m_lastKickoffSeq) {
        m_lastKickoffSeq = m_model->kickoffSeq();
        m_field->beginKickoffFlight(m_model->kickoffOrigin(),
                                    m_model->kickoffKickerId());
    }

    // The info card tracks the player it currently shows: a model sync
    // refreshes its content (state changes to box, injuries, ...) and a
    // player removed from the game hides the card (the card is opened by
    // hover/click, but its DATA stays live).
    if (m_playerInfo && !m_playerInfo->shownPlayerId().isEmpty()) {
        const FFBPlayer *cp = m_model->findPlayerById(m_playerInfo->shownPlayerId());
        if (cp)
            showPlayerCard(cp, cp->hasCoords ? cp->fieldX : (cp->sideHome ? 6 : 19));
        else
            m_playerInfo->showPlaceholder();
    }

    int bx = -1, by = -1;
    if (m_model->ballAt(bx, by))
        m_field->setBall(bx, by, true);
    else
        m_field->setBall(-1, -1, false); // held / no coordinate: hidden

    // Weather: drives the field's rain overlay ("Nice Weather" = none).
    m_field->setWeather(m_model->weather());

    // No roster panels: box/bench/KO/reserve live in the field's box
    // strips (setBoxes above).
}

static QString reportFingerprint(const QJsonObject &report)
{
    // Canonical compact JSON -> short hash. Two identical reports (the same
    // event re-sent) share a fingerprint; two genuinely different events
    // almost never do (different dice / players / ids).
    //
    // The server sends the SAME injury report TWICE — first with
    // skipInjuryParts=CAS, then skipInjuryParts=EVERYTHING_BUT_CAS
    // (ground truth: tests/live_reports_capture.jsonl, def 18416181, both
    // sends armorRoll [1,2]). skipInjuryParts is a DISPLAY-CONTROL flag,
    // not event data — hash the report WITHOUT it, so the re-send is
    // deduped like any other.
    QJsonObject r = report;
    r.remove(QStringLiteral("skipInjuryParts"));
    const QByteArray canonical = QJsonDocument(r).toJson(QJsonDocument::Compact);
    const QByteArray md5 = QCryptographicHash::hash(canonical, QCryptographicHash::Md5);
    return QString::fromLatin1(md5.constData(), 8);
}

void MainWindow::onModelSync(const QJsonArray &changes, const QJsonArray &reports)
{
    if (m_loginSuppressMatchUi)
        return; // verification join: its model sync never reaches the UI
    if (changes.isEmpty() && reports.isEmpty())
        return;
    if (!changes.isEmpty())
        m_model->applyModelChanges(changes); // emits stateUpdated -> applyModelToUi

    // The FUMBBL server re-sends reports (a failed step is retried and its
    // reports flow again; re-joins re-sync). The official client does not
    // dedupe; we do, three ways:
    //  1. A batch of >= kResyncBatch reports while the log is already
    //     populated is a FULL re-sync: rebuild the log from it (no dupes).
    //  2. A single report whose fingerprint arrived within the last
    //     kDedupWindowMs is a re-send: drop it (a real repeat of an
    //     identical event is minutes apart — different turns — never within
    //     the window).
    //  3. (UI-level) a line identical to the previous one is skipped by the
    //     widget (last resort, catches whatever the above let through).
    if (reports.size() >= kResyncBatch && m_loggedReportCount > 0) {
        m_actionLog->clearLog();
        m_reportFpTimes.clear();
        m_loggedReportCount = 0;
    }
    const qint64 now = m_clock.elapsed();
    for (const QJsonValue &rv : reports) {
        const QJsonObject report = rv.toObject();
        if (report.isEmpty())
            continue;
        const QString fp = reportFingerprint(report);
        const qint64 last = m_reportFpTimes.value(fp, -kDedupWindowMs - 1);
        if (now - last < kDedupWindowMs)
            continue; // the server re-sent this exact report: drop it
        m_reportFpTimes.insert(fp, now);
        ++m_loggedReportCount;

        // The RED GLOW on the DEFENDER (the player being blocked) must be
        // on for the whole block action and die when the ATTACKER's blue
        // actor glow disappears. Ground truth (game_1957772 capture):
        // `blockRoll` carries defenderId = NULL — the reports that DO carry
        // it are `block` and `blockChoice` (sequence: selectBlitzTarget ->
        // block -> blockRoll -> blockChoice -> injury). Arm on those; the
        // attacker is the model's current actor (the block reports arrive
        // while the attacker is the actor). NOT on `injury`: it lands with
        // the resolution changes (actor already gone) and would re-arm a
        // dead glow. Re-arming on the same defender is a no-op; a new
        // defender replaces the glow — one red glow at a time, no stacking
        // (multiple blocks per turn must not stack glows).
        const QString rid = report.value(QStringLiteral("reportId")).toString();
        // The foul's VICTIM (the report's defenderId — see ReportFoul
        // ground truth) gets the block-defender's red glow, glow-only (no
        // marker).
        if (rid == QLatin1String("foul")) {
            const QString victim =
                report.value(QStringLiteral("defenderId")).toString();
            if (!victim.isEmpty()) {
                m_model->setFoulTargetId(victim);
                if (m_field)
                    m_field->setBlockMarker(QString(), victim, 0);
            }
        }
        if (rid == QLatin1String("block") || rid == QLatin1String("blockRoll")
            || rid == QLatin1String("blockChoice")) {
            const QString victim = report.value(QStringLiteral("defenderId")).toString();
            // FRENZY's second block: the server re-runs the whole block
            // sequence for the same actor (StepEnd Blocking pushes a fresh
            // Block sequence, same defender). On a NEW `block` report the
            // previous choice is STALE — reset the die face so the marker
            // starts as the pending-die helmet again, then the new
            // blockRoll/blockChoice re-fill it.
            if (rid == QLatin1String("block")) {
                m_model->setBlockDieFace(0);
                // The block COSTS A MOVEMENT — the `block` report starts a
                // block sequence (also a FRENZY's second one): reserve its
                // number and where it is painted.
                if (!victim.isEmpty())
                    m_model->noteBlockStep(victim);
            }
            QString target = victim;
            if (target.isEmpty() && !m_model->blockTargetId().isEmpty())
                // The `blockRoll` report carries defenderId NULL
                // (capture-verified) — for a frenzy second block this is
                // the only arming chance between the `block` report and
                // the `blockChoice`: reuse the current target so the
                // helmet keeps showing.
                target = m_model->blockTargetId();
            if (!target.isEmpty()) {
                m_model->setBlockTargetId(target);
                // The blockChoice carries the CHOSEN die: blockRoll[] +
                // diceIndex (capture-verified). The marker between
                // attacker/defender switches helmet -> die symbol.
                if (rid == QLatin1String("blockChoice")) {
                    const QJsonArray roll =
                        report.value(QStringLiteral("blockRoll")).toArray();
                    const int idx = report.value(QStringLiteral("diceIndex")).toInt(-1);
                    if (idx >= 0 && idx < roll.size())
                        m_model->setBlockDieFace(roll.at(idx).toInt());
                }
                // Reports-only syncs do NOT emit stateUpdated (no model
                // changes): push the marker straight to the field.
                if (m_field)
                    m_field->setBlockMarker(m_model->blockAttackerId(), target,
                                            m_model->blockDieFace());
            }
        }

        // A SUCCESSFUL go-for-it roll: the player's next new route is the
        // GFI extra move, drawn red on the trail.
        if (rid == QLatin1String("goForItRoll")
            && report.value(QStringLiteral("successful")).toBool())
            m_model->setGoForItPending(
                report.value(QStringLiteral("playerId")).toString());

        // Structured dice: the action log renders the die faces (never bare
        // numbers) and resolved names. Official BlockMessage: "X blocks Y:" —
        // the `block` report carries only the defender; the attacker is the
        // current actor (the block reports arrive while the attacker is the
        // actor).
        QVector<DiceLogLine> lines = parseReport(report);
        // The WHOLE block sequence is titled with the acting player — the
        // blitzer for a blitz block, the attacker for a regular block. The
        // `block`/`blockRoll`/`blockChoice` reports carry the choosing TEAM
        // (or nothing); the `foul` report carries only the VICTIM. In all
        // cases the acting player is filled from the model (the reports
        // arrive while the actor is the one performing the action).
        if (rid == QLatin1String("block") || rid == QLatin1String("blockRoll")
            || rid == QLatin1String("blockChoice") || rid == QLatin1String("foul")) {
            const QString attacker = m_model->activePlayerId();
            for (DiceLogLine &l : lines) {
                if (!l.actorId.isEmpty())
                    continue;
                l.actorId = attacker;
                if (rid == QLatin1String("block"))
                    l.text = (attacker == m_model->blitzingPlayerId())
                                 ? QStringLiteral("blitzes")
                                 : QStringLiteral("blocks");
            }
        }
        for (const DiceLogLine &line : lines)
            appendDiceLog(line);
    }
    // Keep the fingerprint table bounded (a full game has a few hundred).
    if (m_reportFpTimes.size() > 2000) {
        const qint64 cutoff = now - kDedupWindowMs;
        for (auto it = m_reportFpTimes.begin(); it != m_reportFpTimes.end(); )
            it = (it.value() < cutoff) ? m_reportFpTimes.erase(it) : std::next(it);
    }
}

// Resolves a raw server id to a display name (no numeric ids in the
// action log). Player ids become "Name (number)"; TEAM ids (prayers,
// bribes, throw-ins, team captain) become the team name. A PURELY NUMERIC
// id the model cannot resolve is DROPPED (empty string) instead of being
// printed raw. Plain name strings (e.g. the coach in timeoutEnforced, or
// the teamName of the bb2025 prayerRoll) pass through untouched.
QString MainWindow::resolveId(const QString &id) const
{
    if (id.isEmpty() || !m_model)
        return id;
    if (const FFBPlayer *p = m_model->findPlayerById(id))
        return p->number > 0
                   ? QStringLiteral("%1 (%2)").arg(p->name).arg(p->number)
                   : p->name;
    if (id == m_model->teamHomeId())
        return m_model->teamHomeName();
    if (id == m_model->teamAwayId())
        return m_model->teamAwayName();
    bool numeric = !id.isEmpty();
    for (const QChar &c : id)
        numeric = numeric && c.isDigit();
    return numeric ? QString() : id; // never print an unresolved numeric id
}

// The match list carries logo paths like "/i/486335" — resolve them
// against the FUMBBL CDN base into an absolute URL.
static QString resolveLogoUrl(const QString &rel)
{
    if (rel.isEmpty())
        return QString();
    if (rel.startsWith(QLatin1String("http")))
        return rel;
    QString r = rel;
    while (r.startsWith(QLatin1Char('/')))
        r = r.mid(1);
    return QStringLiteral("https://cdn.fumbbl.com/") + r;
}

void MainWindow::setFallbackTeamLogos(const QString &home, const QString &away)
{
    m_fallbackHomeLogo = home;
    m_fallbackAwayLogo = away;
}

void MainWindow::loadTeamLogos()
{
    if (!m_model || !m_logoNam)
        return;
    // The GAME STATE often carries logoUrl = null, but the match list
    // (fetched by the Spectate dialog) always has the team logos — fall
    // back to it when the state has none.
    QString homeUrl = m_model->teamHomeLogoUrl();
    QString awayUrl = m_model->teamAwayLogoUrl();
    if (homeUrl.isEmpty())
        homeUrl = resolveLogoUrl(m_fallbackHomeLogo);
    if (awayUrl.isEmpty())
        awayUrl = resolveLogoUrl(m_fallbackAwayLogo);
    if (homeUrl.isEmpty() && awayUrl.isEmpty())
        appendProtocol(QStringLiteral("< "),
                       QStringLiteral("no logo source (game state logoUrl null "
                                      "and no match-list logo)"));

    const auto fetch = [this](const QString &url, int side) {
        // Log every step so a live run shows exactly where a fetch breaks
        // (URL composition / fetch error / image decode).
        appendProtocol(QStringLiteral("< "),
                       QStringLiteral("logo request (%1): %2")
                           .arg(side == 0 ? QStringLiteral("home") : QStringLiteral("away"))
                           .arg(url));
        QNetworkRequest req;
        req.setUrl(QUrl(url));
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("fumbbl-reborn-qt"));
        QNetworkReply *reply = m_logoNam->get(req);
        reply->setProperty("side", QVariant(side));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            onLogoReplyFinished(reply);
        });
    };
    if (!homeUrl.isEmpty() && homeUrl != m_homeLogoUrl) {
        m_homeLogoUrl = homeUrl;
        fetch(homeUrl, 0);
    }
    if (!awayUrl.isEmpty() && awayUrl != m_awayLogoUrl) {
        m_awayLogoUrl = awayUrl;
        fetch(awayUrl, 1);
    }
}

void MainWindow::onLogoReplyFinished(QNetworkReply *reply)
{
    const int side = reply->property("side").toInt(0);
    if (reply->error() != QNetworkReply::NoError) {
        appendProtocol(QStringLiteral("< "),
                       QStringLiteral("logo fetch failed: %1 (HTTP %2)")
                           .arg(reply->errorString())
                           .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()));
        reply->deleteLater();
        return;
    }
    // Capture the content-type BEFORE deleting the reply (the failure path
    // must be visible: a 404 page or an unexpected format).
    const QString contentType =
        reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const QByteArray data = reply->readAll();
    reply->deleteLater();
    QImage img;
    if (!img.loadFromData(data)) { // QPixmap(QByteArray) is unreliable here
        appendProtocol(QStringLiteral("< "),
                       QStringLiteral("logo decode failed (%1 bytes, content-type '%2')")
                           .arg(data.size())
                           .arg(contentType));
        return;
    }
    appendProtocol(QStringLiteral("< "),
                   QStringLiteral("logo ok: %1x%2 (%3 bytes)")
                       .arg(img.width()).arg(img.height()).arg(data.size()));
    // The fetched logo often comes with a FLAT backdrop (white is the
    // common case, but any color — the user may also upload their own
    // image), which paints an ugly opaque box behind the field watermark.
    // Key the backdrop out: the color is estimated from the image border
    // (not assumed white) and removed by a border flood fill; images that
    // already have real alpha, or whose border is not a flat backdrop, are
    // left untouched.
    const QImage keyed = assets::keyBackground(img);
    if (!img.hasAlphaChannel() && keyed.hasAlphaChannel()) {
        // Log only when a REAL amount of backdrop was removed (keyBackground
        // converts to ARGB32 even when its guard skips the key, so the alpha
        // channel alone is not proof).
        int transparent = 0, total = 0;
        for (int y = 0; y < keyed.height(); y += 4)
            for (int x = 0; x < keyed.width(); x += 4) {
                ++total;
                if (qAlpha(keyed.pixel(x, y)) == 0)
                    ++transparent;
            }
        if (total > 0 && transparent * 100 > total * 5)
            appendProtocol(QStringLiteral("< "),
                           QStringLiteral("logo flat backdrop keyed out (%1% transparent)")
                               .arg(transparent * 100 / total));
    }
    const QPixmap pm = QPixmap::fromImage(keyed);
    if (side == 0) {
        m_homeLogoPm = pm;
        m_field->setTeamLogos(pm, m_awayLogoPm);
        m_headerView->setLogos(pm, m_awayLogoPm);
    } else {
        m_awayLogoPm = pm;
        m_field->setTeamLogos(m_homeLogoPm, pm);
        m_headerView->setLogos(m_homeLogoPm, pm);
    }
}

void MainWindow::onGameTime(qint64, qint64 turnTimeMs)
{
    if (m_loginSuppressMatchUi)
        return; // verification join: its clock never reaches the UI
    // serverGameTime.turnTime = ELAPSED ms of the turn. The Mockup F header
    // counts UP (FUMBBL semantics) — no decreasing progress bar.
    m_headerView->setTurnElapsedSeconds(static_cast<int>(turnTimeMs / 1000));
}

void MainWindow::onTalk(const QJsonObject &payload)
{
    if (m_loginSuppressMatchUi)
        return; // verification join: its chat never reaches the UI
    const QString coach = payload.value(QStringLiteral("coach")).toString();
    const QString mode = payload.value(QStringLiteral("talkMode")).toString();
    QString sender = coach.isEmpty() ? QStringLiteral("?") : coach;
    if (mode == QLatin1String("STAFF") || mode == QLatin1String("DEV"))
        sender = QStringLiteral("[%1] %2").arg(mode).arg(coach);
    for (const QJsonValue &t : payload.value(QStringLiteral("talks")).toArray())
        m_chat->addMessage(sender, t.toString());
}

void MainWindow::onJoined(const QJsonObject &payload)
{
    // Login gate: a successful join IS the credential verification —
    // the server checked the password to accept it. Close the dialog.
    if (m_authenticating && m_loginDialog) {
        m_authenticated = true;
        m_authenticating = false;
        // Log out goes live for the verified coach: the gate flow never
        // passes through the Spectate dialog's connect hook.
        const QString coach = payload.value(QStringLiteral("coach")).toString();
        setActiveCoach(coach);
        m_loginDialog->acceptLogin();
        // The login join was a VERIFICATION, not a spectate request: a
        // successful login only ENABLES the interface — it must not sit
        // on the (random) match the join landed on. End the verification
        // session now (its close is a silent teardown; manual closes
        // never auto-reconnect).
        m_loginTeardown = true;
        m_session->disconnect();
        // The server sends the game state (gameState/modelSync) in the
        // SAME burst as serverJoin — those frames are already queued and
        // land AFTER this handler, re-rendering the match. Wipe the match
        // state on the next event-loop pass, after the queued frames
        // (the socket is closing: nothing newer can follow).
        QTimer::singleShot(0, this, [this, coach]() {
            clearMatchUi();
            m_loginSuppressMatchUi = false; // real matches may reach the UI now
            setConnectionStatus(tr("Logged in as %1 — choose a match (Spectate Game…).")
                                    .arg(coach),
                                true);
        });
        return;
    }
    setSpectators(payload.value(QStringLiteral("spectators")).toInt());
    setConnectionStatus(tr("Spectating as %1")
                            .arg(payload.value(QStringLiteral("coach")).toString()),
                        true);
}

void MainWindow::onSessionProgress(const QString &message)
{
    setConnectionStatus(message, true);
    appendProtocol(QStringLiteral("progress "), message);
}

void MainWindow::onSessionFatal(const QString &message)
{
    // Login gate: the verification attempt died (e.g. the join watchdog
    // gave up on a silent server) — report it in the dialog and keep it
    // open for a retry.
    if (m_authenticating && m_loginDialog) {
        m_authenticating = false;
        m_loginSuppressMatchUi = false;
        m_loginDialog->showServerMessage(message);
        return;
    }
    setConnectionStatus(message, false);
    appendProtocol(QStringLiteral("FATAL "), message);
}

void MainWindow::onWsClosed(int code, const QString &reason)
{
    // The verified-login teardown (onJoined closed the verification
    // session on purpose): a silent 1000, not a failure.
    if (m_loginTeardown) {
        m_loginTeardown = false;
        return;
    }
    // Login gate: the socket went away mid-verification with no status in
    // flight — surface it in the dialog (a clean manual close after Log
    // out is the silent case: nothing to report).
    if (m_authenticating && m_loginDialog) {
        // A close BEFORE this attempt's socket started belongs to a
        // PREVIOUS attempt (the server's late 1000 for a rejected login —
        // wrong password, then the right one — or the previous session's
        // close after a Log out): it must not cancel this verification.
        if (!m_attemptSocketStarted)
            return;
        m_authenticating = false;
        m_loginSuppressMatchUi = false;
        m_loginDialog->showServerMessage(
            tr("Connection closed (%1: %2)").arg(code).arg(reason));
        return;
    }
    if (code == static_cast<int>(QWebSocketProtocol::CloseCodeNormal) && !m_authenticated
        && m_activeCoach.isEmpty())
        return; // manual close after logout: the banner already says so
    setConnectionStatus(tr("Connection closed (%1: %2)").arg(code).arg(reason), false);
    appendProtocol(QStringLiteral("ws closed "),
                   QStringLiteral("%1 %2").arg(code).arg(reason));
}

// serverStatus from the server (e.g. a join rejected for the wrong
// password): shown as-is in the banner. There is NO auto-retry on a
// rejection (the server rate-limits repeated failures) — the user fixes
// the credential and reconnects.
void MainWindow::onSessionStatus(const QString &status, const QString &message)
{
    const bool isError = status.startsWith(QLatin1String("ERROR"), Qt::CaseInsensitive);
    // Login gate: a rejection (e.g. ERROR_WRONG_PASSWORD) is the
    // verification failing — show it in the dialog and stay open for a
    // retry. There is NO auto-retry on a rejection (the server rate-limits
    // repeated failures).
    if (isError && m_authenticating && m_loginDialog) {
        m_authenticating = false;
        m_loginSuppressMatchUi = false;
        m_loginDialog->showServerMessage(tr("Server: %1 %2").arg(status, message));
        return;
    }
    if (isError)
        setConnectionStatus(tr("Server: %1 %2").arg(status, message), false);
    appendProtocol(QStringLiteral("status "), QStringLiteral("%1 %2").arg(status, message));
}

void MainWindow::onConnectRequested()
{
    openSpectateDialog();
}

void MainWindow::openSpectateDialog()
{
    // Login gate first: the spectate dialog is only reachable once the
    // client login was verified by the server (the session then uses
    // m_loginCoach/m_loginMd5 — no account fields in the spectate dialog).
    if (m_gateLogin && !m_authenticated) {
        startLogin();
        if (!m_authenticated)
            return;
    }
    SpectateDialog dialog(this);
    // The dialog emits connectRequested() right before accept(); hook it up
    // to the session so the flow starts as the dialog closes.
    connect(&dialog, &SpectateDialog::connectRequested, this,
            [this, &dialog](int gameId)
            {
                // This match's session is starting: Log out becomes live
                // (the active coach is the logged-in one).
                setActiveCoach(m_loginCoach);
                // Remember the match list's team logos NOW (the dialog is
                // destroyed when exec() returns) — they are the fallback
                // when the game state's logoUrl is null.
                const QPair<QString, QString> logos = dialog.selectedTeamLogos();
                m_fallbackHomeLogo = logos.first;
                m_fallbackAwayLogo = logos.second;
                // Starting (or SWITCHING) a match: clear the previous state
                // first, so the UI doesn't stay on the old match while the
                // new session is being established.
                clearMatchUi();
                setConnectionStatus(tr("Connecting to game %1…").arg(gameId), false);
                m_session->connectAsSpectator(m_loginCoach, m_loginMd5, gameId);
            });
    dialog.exec();
}

// -----------------------------------------------------------------------------
// Client login gate
// -----------------------------------------------------------------------------
bool MainWindow::startLogin()
{
    if (m_loginDialog)
        return m_authenticated; // a gate is already in flight
    m_loginDialog = new LoginDialog(this);
    m_loginDialog->setModal(true);
    connect(m_loginDialog, &LoginDialog::loginRequested, this, &MainWindow::onLoginRequested);
    // Quit button: close the client (at startup or after a Log out).
    connect(m_loginDialog, &LoginDialog::quitRequested, qApp, &QApplication::quit);
    m_loginDialog->exec();
    const bool verified = m_authenticated;
    delete m_loginDialog;
    m_loginDialog = nullptr;
    return verified;
}

void MainWindow::onLoginRequested(const QString &coach, const QByteArray &passwordMd5)
{
    m_authenticating = true;
    // The attempt's socket is not up yet (the match-list fetch is still in
    // flight): a ws close in this window belongs to the PREVIOUS attempt.
    m_attemptSocketStarted = false;
    m_loginCoach = coach;
    m_loginMd5 = passwordMd5;
    setConnectionStatus(tr("Verifying credentials…"), false);
    // The JOIN ATTEMPT is the verification: the server checks the password
    // there (FumbblRequestCheckAuthorization — before any game lookup) and
    // answers serverJoin (ok) or a serverStatus rejection (wrong password).
    // Game for the attempt: the last spectated one if it is still live,
    // otherwise the first live match.
    m_loginList->fetch();
}

void MainWindow::onLoginMatchesReceived(QVector<FFBLiveMatch> matches)
{
    if (!m_authenticating)
        return;
    const int lastGame = QSettings().value(QStringLiteral("connect/lastGameId")).toInt();
    int gameId = 0;
    for (const FFBLiveMatch &m : matches)
        if (m.half > 0 && m.id == lastGame) {
            gameId = lastGame;
            break;
        }
    if (gameId == 0)
        for (const FFBLiveMatch &m : matches)
            if (m.half > 0) {
                gameId = m.id;
                break;
            }
    if (gameId == 0) {
        m_authenticating = false;
        if (m_loginDialog)
            m_loginDialog->showServerMessage(
                tr("There are no live matches to verify against right now — "
                   "try again in a moment."));
        return;
    }
    m_attemptSocketStarted = true; // from here a close is THIS attempt's
    // From here until the post-verification wipe, the verification game's
    // data must not reach the UI (the server starts streaming it as soon
    // as the session opens — see the guards in the match-data handlers).
    m_loginSuppressMatchUi = true;
    m_session->connectAsSpectator(m_loginCoach, m_loginMd5, gameId);
}

void MainWindow::beginLoginAttemptForTest(const QString &coach, const QByteArray &passwordMd5,
                                          LoginDialog *dialog)
{
    m_loginDialog = dialog;
    m_loginCoach = coach;
    m_loginMd5 = passwordMd5;
    m_authenticating = true;
    m_loginSuppressMatchUi = true; // a verification join is in flight
    m_attemptSocketStarted = false;
}

void MainWindow::markAttemptSocketStartedForTest()
{
    m_attemptSocketStarted = true;
}

void MainWindow::setActiveCoach(const QString &coach)
{
    m_activeCoach = coach;
    m_disconnectAction->setEnabled(!coach.isEmpty());
}

// File > Log out (shared PC): end this coach's session and drop the
// remembered credential, so the NEXT player's dialog opens empty. The
// keychain entry survives: a returning coach retypes only the name (the
// MD5 comes back from the store); nobody retypes a password they already
// remembered.
void MainWindow::onLogout()
{
    m_session->disconnect(); // manual close: the watchdog and reconnect stay off
    setActiveCoach(QString());
    m_authenticated = false;
    clearMatchUi();
    setConnectionStatus(tr("Logged out."), false);
    // The remembered credential goes (the keychain entry survives: a
    // returning coach retypes only the name).
    QFile::remove(CredentialStore::credentialsPath());
    // Shared PC: the next player logs in from the same gate (the dialog
    // also offers Quit).
    if (m_gateLogin)
        startLogin();
}

// Wipes all match state (model, field, rosters, logs, selection) — used
// when the user picks a different game from "Spectate Game".
void MainWindow::clearMatchUi()
{
    m_model->reset();
    onFieldPlayerClicked(QString()); // deselect (no-op when nothing selected)
    m_field->setPlayers({}, {});
    m_field->setBoxes({}, {});
    m_field->setBall(-1, -1, false);
    m_actionLog->clearLog();
    m_chat->clearChat();
    // A new match = a fresh dedup universe (report fingerprints from the
    // old game must not suppress this game's reports).
    m_reportFpTimes.clear();
    m_loggedReportCount = 0;
    hidePlayerCard();
    setTeams(QString(), QString());
    setScore(0, 0);
    setTurn(1);
    m_headerView->setTurnElapsedSeconds(0);
    m_headerView->setRerolls(0, 0, 0, 0);
    // The header goes back to hidden until a new game loads.
    m_headerToolbar->setVisible(false);
    m_headerView->setWeather(QString());
    m_headerView->setLogos(QPixmap(), QPixmap());
    m_field->setTeamLogos(QPixmap(), QPixmap());
    m_homeLogoUrl.clear();
    m_awayLogoUrl.clear();
    m_homeLogoPm = QPixmap();
    m_awayLogoPm = QPixmap();
    setSpectators(0);
}

void MainWindow::onFieldPlayerClicked(const QString &playerId)
{
    // The spectator does not select players (the rim glow is the remote
    // coach's selection, model-driven). Clicking a player only shows its
    // info card — exactly like hovering it. Clicking empty field hides the
    // card; the glow is never touched by local input.
    if (playerId.isEmpty()) {
        if (m_playerInfo)
            m_playerInfo->showPlaceholder();
        return;
    }
    showPlayerCardById(playerId);
}

void MainWindow::showPlayerCardById(const QString &playerId)
{
    const FFBPlayer *p = m_model ? m_model->findPlayerById(playerId) : nullptr;
    if (!p)
        return; // unknown id (stale click)
    // Fill + positioning live in showPlayerCard (shared with the hover
    // path).
    showPlayerCard(p, p->hasCoords ? p->fieldX : (p->sideHome ? 6 : 19));
}

// ---------------------------------------------------------------------------
// Hover player card: appears over the mouse, far from the player, on the
// opposite side of the field.
// ---------------------------------------------------------------------------
void MainWindow::onPlayerHovered(const FieldView::PlayerToken &token)
{
    if (token.playerId.isEmpty() && token.name.isEmpty()) {
        hidePlayerCard();
        return;
    }
    const FFBPlayer *p = nullptr;
    // Crash fix: the bench path used to hold `p = &c` where c is an
    // element of a LOCAL roster COPY (rosterHome()/rosterAway() return by
    // value) — that copy died at the end of the else-if block and the later
    // p->hasCoords / showPlayerCard(p) reads were USE-AFTER-FREE (valgrind:
    // read in a block freed by QArrayDataPointer<FFBPlayer>::~QArrayDataPointer).
    // The corrupted data leaked into the card (shownPlayerId) and later
    // aborted in GameModel::findPlayerById (ASSERT "p" qarraydatapointer.h).
    // The copy must outlive every use of p: function scope.
    QVector<FFBPlayer> roster;
    if (!token.playerId.isEmpty()) {
        p = m_model ? m_model->findPlayerById(token.playerId) : nullptr;
    } else if (m_model && !token.name.isEmpty()) {
        // Bench face: resolve by name on the right side.
        roster = token.sideHome ? m_model->rosterHome()
                                : m_model->rosterAway();
        for (const FFBPlayer &c : roster)
            if (c.name == token.name) {
                p = &c;
                break;
            }
    }
    if (!p) {
        hidePlayerCard();
        return;
    }
    // Hovering ONLY shows the card. The rim glow is the REMOTE coach's
    // selection (model-driven) — no local mouse input moves it.
    const int fieldX = p->hasCoords ? p->fieldX : (p->sideHome ? 6 : 19);
    showPlayerCard(p, fieldX);
}

void MainWindow::showPlayerCard(const FFBPlayer *p, int fieldX)
{
    if (!p || !m_playerInfo || !m_stage)
        return;

    // Fill (shared with the click path: assemble PlayerInfoData).
    PlayerInfoData d;
    d.playerId = p->playerId;
    d.name = p->name;
    d.number = p->number;
    d.position = p->position;
    d.positionShorthand = p->positionShorthand;
    d.positionKeywords = p->positionKeywords;
    d.race = p->sideHome ? m_model->teamHomeRace() : m_model->teamAwayRace();
    d.teamName = p->sideHome ? m_model->teamHomeName() : m_model->teamAwayName();
    d.sideHome = p->sideHome;
    d.status = p->playerStatus;
    d.ma = p->ma;
    d.st = p->st;
    d.ag = p->ag;
    d.pa = p->pa;
    d.av = p->av;
    d.isActing = !m_model->activePlayerId().isEmpty() && p->playerId == m_model->activePlayerId();

    if (p->hasCoords) {
        if (GameModel::isBoxX(p->fieldX))
            d.stateText = tr("in the box (%1)").arg(GameModel::stateLabel(p->state));
        else if (GameModel::isOnField(p->fieldX, p->fieldY))
            d.stateText = GameModel::stateText(p->state);
    }
    d.stateFlags = GameModel::stateFlags(p->state);

    const QHash<QString, QString> &values = p->skillValues;
    for (const QString &s2 : p->skills) {
        PlayerInfoData::Skill sk;
        sk.name = s2;
        sk.value = values.value(s2);
        sk.used = p->usedSkills.contains(s2);
        d.skills.append(sk);
    }
    d.injuries = p->lastingInjuries;
    if (!p->recoveringInjury.isEmpty())
        d.injuries << tr("Recovering: %1").arg(p->recoveringInjury);
    d.cards = p->cards;

    m_playerInfo->showPlayer(d);

    // POSITION: opposite side of the field, vertically centered, far from
    // the player (mockup: the card is pinned to a corner-ish spot).
    FloatingPanel *panel = panelFor(m_playerInfo);
    if (!panel)
        return;
    const int pw = qMax(panel->width(), 420);
    const int sw = m_stage->width();
    const int sh = m_stage->height();
    const bool playerLeft = fieldX < 13;
    // 80px edge margin: the bench box strips live on the field edges
    // (~62px wide) and must stay visible.
    const int x = playerLeft ? sw - pw - 80 : 80;

    // Size the card to its CONTENT. A fixed height clipped the bottom of
    // cards with many skills (the state/injury text ran off the card
    // edge). Measure at the FINAL WIDTH so the skill-chip wrap height is
    // accounted for, then clamp to the stage.
    panel->setGeometry(x, 20, pw, 100);
    panel->show();
    panel->raise();
    const int need = m_playerInfo->sizeHint().height() + 16;
    const int ph = qBound(300, need, sh - 40);
    const int y = qBound(20, (sh - ph) / 2, sh - ph - 20);
    panel->setGeometry(x, y, pw, ph);
}

void MainWindow::hidePlayerCard()
{
    // Hiding the card never touches the rim glow — the glow is the remote
    // coach's selection, model-driven.
    if (m_playerInfo)
        m_playerInfo->showPlaceholder();
    if (FloatingPanel *panel = panelFor(m_playerInfo))
        panel->hide();
}

// The FloatingPanel that wraps a content widget (player info card).
FloatingPanel *MainWindow::panelFor(QWidget *content)
{
    for (auto it = m_panels.cbegin(); it != m_panels.cend(); ++it)
        if (it.value()->contentWidget() == content)
            return it.value();
    return nullptr;
}

// Pin the Mockup F corner overlays to the stage (bottom-left action log,
// bottom-right chat).
void MainWindow::pinOverlays()
{
    if (!m_stage)
        return;
    const int sw = m_stage->width();
    const int sh = m_stage->height();
    if (sw < 400 || sh < 300)
        return;
    if (m_actionLog)
        m_actionLog->move(20, sh - m_actionLog->height() - 20);
    if (m_chat)
        m_chat->move(sw - m_chat->width() - 20, sh - m_chat->height() - 20);
}

QByteArray MainWindow::savedLayout() const
{
    QJsonObject panels;
    for (auto it = m_panels.cbegin(); it != m_panels.cend(); ++it) {
        const QRect g = it.value()->geometry();
        QJsonObject p;
        p.insert(QStringLiteral("x"), g.x());
        p.insert(QStringLiteral("y"), g.y());
        p.insert(QStringLiteral("w"), g.width());
        p.insert(QStringLiteral("h"), g.height());
        p.insert(QStringLiteral("v"), it.value()->isVisible());
        panels.insert(it.key(), p);
    }
    QJsonObject root;
    root.insert(QStringLiteral("panels"), panels);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void MainWindow::restoreLayout(const QByteArray &state)
{
    applySavedLayout(state);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    // Never persist a state where every panel is hidden (e.g. the window was
    // closed before the first sizing): that would trap the user in an
    // empty HUD. "Reset Layout" always recovers, but avoid the footgun.
    bool anyVisible = false;
    for (auto it = m_panels.cbegin(); it != m_panels.cend(); ++it)
        anyVisible |= it.value()->isVisible();
    if (anyVisible)
        settings.setValue(kLayoutSettingsKey, savedLayout());
    QMainWindow::closeEvent(event);
}
