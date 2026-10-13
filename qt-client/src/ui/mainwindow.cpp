// =============================================================================
// MainWindow — implementation
//
// Docking is provided by the QT Advanced Docking System (QDS, vendored in
// third_party/QDS — used as-is, never modified):
//   - ads::CDockManager is the central widget of the QMainWindow (it sets
//     itself up automatically when its parent is a QMainWindow).
//   - The Field is the central, LOCKED anchor (not closable/movable/floatable);
//     its (empty) title bar is hidden via HideSingleCentralWidgetTitleBar.
//   - Roster / Player Info / Dice Log / Chat / Protocol Log are regular
//     CDockWidgets: dockable, floatable, tabbable, draggable.
//   - Layout persistence uses QDS saveState()/restoreState() (XML).
// =============================================================================
#include "ui/mainwindow.h"

#include "DockAreaWidget.h"
#include "DockManager.h"
#include "DockWidget.h"

#include "core/gamemodel.h"
#include "core/reportparser.h"
#include "core/spectatorsession.h"
#include "ui/chatwidget.h"
#include "ui/connectdialog.h"
#include "ui/dicelogwidget.h"
#include "ui/fieldview.h"
#include "ui/headerbar.h"
#include "ui/playerinfowidget.h"
#include "ui/rosterwidget.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSettings>
#include <QStatusBar>

// Version of the serialized layout. Bump when the dock set changes
// incompatibly; QDS refuses to restore a state saved with a different version.
static const int kLayoutStateVersion = 1;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("FUMBBL Reborn"));
    resize(1400, 860);

    createActions();
    createToolbar();
    createDockManager();
    createCentralWidget();
    createDockWidgets();
    createStatusBar();
    createMenus(); // after the docks: the View menu uses their toggleViewAction()

    // Remember the default arrangement for the "Reset Layout" action, then
    // restore the user's saved layout (if any) on top of it.
    m_defaultLayout = m_dockManager->saveState(kLayoutStateVersion);
    loadLayout();

    // --- Core (spectator) wiring --------------------------------------------
    m_model = new GameModel(this);
    m_session = new SpectatorSession(this);

    connect(m_session, &SpectatorSession::gameStateReceived, this, &MainWindow::onGameState);
    connect(m_model, &GameModel::stateUpdated, this, &MainWindow::applyModelToUi);
    connect(m_session, &SpectatorSession::modelSyncReceived, this, &MainWindow::onModelSync);
    connect(m_session, &SpectatorSession::gameTimeReceived, this, &MainWindow::onGameTime);
    connect(m_session, &SpectatorSession::talkReceived, this, &MainWindow::onTalk);
    connect(m_session, &SpectatorSession::joined, this, &MainWindow::onJoined);
    connect(m_session, &SpectatorSession::progress, this, &MainWindow::onSessionProgress);
    connect(m_session, &SpectatorSession::fatal, this, &MainWindow::onSessionFatal);
    connect(m_session, &SpectatorSession::wsClosed, this, &MainWindow::onWsClosed);
    connect(m_session, &SpectatorSession::protocolSending, this,
            [this](const QString &id, const QJsonObject &)
            { this->appendProtocol(QStringLiteral(">> send "), id); });
    connect(m_session, &SpectatorSession::protocolReceived, this,
            [this](const QString &id, const QJsonObject &)
            { this->appendProtocol(QStringLiteral("<< recv "), id); });

    connect(m_homeRoster, &RosterWidget::playerSelected, this,
            [this](int number) { this->showPlayerInfo(number, true); });
    connect(m_awayRoster, &RosterWidget::playerSelected, this,
            [this](int number) { this->showPlayerInfo(number, false); });

    // Field click-to-select (empty id = clicked an empty square = deselect).
    connect(m_field, &FieldView::playerClicked, this, &MainWindow::onFieldPlayerClicked);

    connect(m_chat, &ChatWidget::messageSent, this, &MainWindow::chatMessageSent);
    connect(m_connectAction, &QAction::triggered, this, &MainWindow::connectionRequested);
}

// -----------------------------------------------------------------------------
// Actions / Menus
// -----------------------------------------------------------------------------
void MainWindow::createActions()
{
    m_connectAction = new QAction(tr("&Connect…"), this);
    m_connectAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+J")));
    m_connectAction->setStatusTip(tr("Connect to a FUMBBL match as spectator"));

    m_disconnectAction = new QAction(tr("&Disconnect"), this);
    m_disconnectAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+K")));
    m_disconnectAction->setEnabled(false);

    m_quitAction = new QAction(tr("&Quit"), this);
    m_quitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
    connect(m_quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_resetLayoutAction = new QAction(tr("Reset &Layout"), this);
    m_resetLayoutAction->setStatusTip(tr("Restore the default panel arrangement"));
    connect(m_resetLayoutAction, &QAction::triggered, this, [this]() {
        if (!m_defaultLayout.isEmpty())
            m_dockManager->restoreState(m_defaultLayout, kLayoutStateVersion);
    });
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_connectAction);
    fileMenu->addAction(m_disconnectAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_homeRosterDock->toggleViewAction());
    viewMenu->addAction(m_awayRosterDock->toggleViewAction());
    viewMenu->addAction(m_playerInfoDock->toggleViewAction());
    viewMenu->addAction(m_diceLogDock->toggleViewAction());
    viewMenu->addAction(m_chatDock->toggleViewAction());
    viewMenu->addAction(m_protocolDock->toggleViewAction());
    viewMenu->addSeparator();
    viewMenu->addAction(m_resetLayoutAction);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    auto *aboutAction = new QAction(tr("&About FUMBBL Reborn"), this);
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("About FUMBBL Reborn"),
                           tr("<h3>FUMBBL Reborn %1</h3>"
                              "<p>Native Qt6 client for the FUMBBL online Blood Bowl "
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
    m_header = new HeaderBar(this);
    addToolBar(Qt::TopToolBarArea, m_header);
}

void MainWindow::createDockManager()
{
    // CDockManager becomes the central widget of this QMainWindow itself.
    m_dockManager = new ads::CDockManager(this);

    // Follow the application's dark palette instead of ADS's built-in schemes.
    m_dockManager->setColorSchemeMode(ads::CDockManager::ColorSchemeMode::FollowPalette);

    // The field is the single central dock widget: hide its title bar
    // (it has no title bar actions, so the bar would just be empty chrome).
    ads::CDockManager::setConfigFlag(ads::CDockManager::HideSingleCentralWidgetTitleBar,
                                     true);
}

void MainWindow::createCentralWidget()
{
    m_field = new FieldView(this);
    m_fieldDock = new ads::CDockWidget(m_dockManager, tr("Field"), this);
    m_fieldDock->setObjectName(QStringLiteral("dock_field"));
    m_fieldDock->setWidget(m_field, ads::CDockWidget::ForceNoScrollArea);
    // The field is the fixed anchor of the layout: not closable, not movable,
    // not floatable (it only takes focus when clicked).
    m_fieldDock->setFeatures(ads::CDockWidget::DockWidgetFocusable);
    m_dockManager->setCentralWidget(m_fieldDock);
}

// Creates a CDockWidget wrapping `content` and registers it with the manager.
static ads::CDockWidget *makeDock(ads::CDockManager *manager, const QString &title,
                                  const QString &objectName, QWidget *content,
                                  QWidget *parent)
{
    auto *dock = new ads::CDockWidget(manager, title, parent);
    dock->setObjectName(objectName);
    dock->setWidget(content);
    return dock;
}

void MainWindow::createDockWidgets()
{
    // QDS split semantics: addDockWidget(area, w, targetArea) places w relative
    // to targetArea; "Left/Right" areas split HORIZONTALLY (side by side),
    // "Top/Bottom" areas split VERTICALLY (stacked).

    // --- Left column: protocol log above the home roster --------------------
    m_homeRoster = new RosterWidget(this);
    m_homeRoster->setObjectName(QStringLiteral("roster_home"));
    m_homeRoster->setTeamColor(QColor(0x7c, 0xa8, 0x6f));
    m_homeRosterDock =
        makeDock(m_dockManager, tr("Home Roster"), QStringLiteral("dock_home_roster"),
                 m_homeRoster, this);
    auto homeArea = m_dockManager->addDockWidget(ads::LeftDockWidgetArea, m_homeRosterDock);

    m_protocolLog = new QPlainTextEdit(this);
    m_protocolLog->setReadOnly(true);
    m_protocolLog->setMaximumBlockCount(4000);
    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(8);
    m_protocolLog->setFont(mono);
    m_protocolDock =
        makeDock(m_dockManager, tr("Protocol Log"), QStringLiteral("dock_protocol"),
                 m_protocolLog, this);
    m_dockManager->addDockWidget(ads::TopDockWidgetArea, m_protocolDock, homeArea);

    // --- Right: away roster ---------------------------------------------------
    m_awayRoster = new RosterWidget(this);
    m_awayRoster->setObjectName(QStringLiteral("roster_away"));
    m_awayRoster->setTeamColor(QColor(0xd8, 0xc0, 0x7a));
    m_awayRosterDock =
        makeDock(m_dockManager, tr("Away Roster"), QStringLiteral("dock_away_roster"),
                 m_awayRoster, this);
    m_dockManager->addDockWidget(ads::RightDockWidgetArea, m_awayRosterDock);

    // --- Bottom band: player info | dice log | chat (side by side) ------------
    m_playerInfo = new PlayerInfoWidget(this);
    m_playerInfoDock =
        makeDock(m_dockManager, tr("Player Info"), QStringLiteral("dock_player_info"),
                 m_playerInfo, this);
    auto playerInfoArea =
        m_dockManager->addDockWidget(ads::BottomDockWidgetArea, m_playerInfoDock);

    m_diceLog = new DiceLogWidget(this);
    m_diceLogDock =
        makeDock(m_dockManager, tr("Dice Log"), QStringLiteral("dock_dice_log"),
                 m_diceLog, this);
    auto diceLogArea =
        m_dockManager->addDockWidget(ads::RightDockWidgetArea, m_diceLogDock, playerInfoArea);

    m_chat = new ChatWidget(this);
    m_chatDock = makeDock(m_dockManager, tr("Chat"), QStringLiteral("dock_chat"), m_chat, this);
    m_dockManager->addDockWidget(ads::RightDockWidgetArea, m_chatDock, diceLogArea);
}

void MainWindow::createStatusBar()
{
    m_statusLabel = new QLabel(tr("Ready"), this);
    statusBar()->addWidget(m_statusLabel);

    m_connectionLabel = new QLabel(tr("Not connected"), this);
    m_connectionLabel->setStyleSheet(QStringLiteral("color: #888;"));
    statusBar()->addPermanentWidget(m_connectionLabel);
}

// -----------------------------------------------------------------------------
// UI state updaters
// -----------------------------------------------------------------------------
void MainWindow::setTeams(const QString &homeName, const QString &awayName)
{
    m_header->setTeams(homeName, awayName);
    m_field->setHomeTeamName(homeName);
    m_field->setAwayTeamName(awayName);
    m_homeRoster->setTeamName(tr("%1 (Home)").arg(homeName));
    m_awayRoster->setTeamName(tr("%1 (Away)").arg(awayName));
}

void MainWindow::setScore(int home, int away)
{
    m_header->setScore(home, away);
}

void MainWindow::setTurn(int turn)
{
    m_header->setTurn(turn);
    m_field->setTurn(turn);
}

void MainWindow::setTimerSeconds(int remaining)
{
    m_header->setTimerSeconds(remaining);
}

void MainWindow::setSpectators(int count)
{
    m_header->setSpectators(count);
}

void MainWindow::setConnectionStatus(const QString &status, bool connected)
{
    m_statusLabel->setText(status);
    m_connectionLabel->setText(connected ? tr("Connected") : tr("Not connected"));
    m_connectionLabel->setStyleSheet(connected ? QStringLiteral("color: #6fbf73;")
                                               : QStringLiteral("color: #888;"));
}

void MainWindow::appendDiceLog(const QString &text, const QColor &color)
{
    m_diceLog->appendEntry(text, color);
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
    if (m_model->initialize(game))
        setConnectionStatus(tr("Game state received"), true);
}

void MainWindow::applyModelToUi()
{
    if (!m_model || !m_model->isInitialized())
        return;

    setTeams(m_model->teamHomeName(), m_model->teamAwayName());
    m_header->setScore(m_model->scoreHome(), m_model->scoreAway());
    setTurn(m_model->turn());

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
        (t.sideHome ? homeTokens : awayTokens).append(pt);
    }
    m_field->setPlayers(homeTokens, awayTokens);

    // Keep the selection valid: if the selected player still exists refresh
    // the panel (state/skills may have changed); if it was removed, deselect.
    if (!m_selectedPlayerId.isEmpty()) {
        if (m_model->findPlayerById(m_selectedPlayerId))
            selectPlayer(m_selectedPlayerId);
        else
            onFieldPlayerClicked(QString()); // deselect
    }

    int bx = -1, by = -1;
    if (m_model->ballAt(bx, by))
        m_field->setBall(bx, by, true);
    else
        m_field->setBall(-1, -1, false); // held / no coordinate: hidden

    // Rosters: ALL roster players (the widget keeps its own order), with a
    // short status label for off-field states (KO/BH/SI/RIP/RES/BAN/...).
    m_homeRoster->clearRoster();
    for (const FFBPlayer &p : m_model->rosterHome()) {
        m_homeRoster->addPlayer(p.number, p.name, p.position, p.st, p.ag, p.ma, p.av,
                                p.skills.join(QStringLiteral(" ")), GameModel::stateLabel(p.state));
    }
    m_awayRoster->clearRoster();
    for (const FFBPlayer &p : m_model->rosterAway()) {
        m_awayRoster->addPlayer(p.number, p.name, p.position, p.st, p.ag, p.ma, p.av,
                                p.skills.join(QStringLiteral(" ")), GameModel::stateLabel(p.state));
    }
}

void MainWindow::onModelSync(const QJsonArray &changes, const QJsonArray &reports)
{
    if (changes.isEmpty() && reports.isEmpty())
        return;
    if (!changes.isEmpty())
        m_model->applyModelChanges(changes); // emits stateUpdated -> applyModelToUi
    for (const QJsonValue &rv : reports) {
        const QJsonObject report = rv.toObject();
        if (report.isEmpty())
            continue;
        for (const DiceLogLine &line : parseReport(report))
            m_diceLog->appendEntry(line.text, line.color);
    }
}

void MainWindow::onGameTime(qint64, qint64 turnTimeMs)
{
    // serverGameTime.turnTime = ELAPSED ms of the 30s turn; UI shows the
    // remaining seconds (TS reference).
    const int remaining = qMax(0, 30 - static_cast<int>(turnTimeMs / 1000));
    m_header->setTimerSeconds(remaining);
}

void MainWindow::onTalk(const QJsonObject &payload)
{
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
    m_header->setSpectators(payload.value(QStringLiteral("spectators")).toInt());
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
    setConnectionStatus(message, false);
    appendProtocol(QStringLiteral("FATAL "), message);
}

void MainWindow::onWsClosed(int code, const QString &reason)
{
    setConnectionStatus(tr("Connection closed (%1: %2)").arg(code).arg(reason), false);
    appendProtocol(QStringLiteral("ws closed "), QStringLiteral("%1 %2").arg(code).arg(reason));
}

void MainWindow::onConnectRequested()
{
    ConnectDialog dialog(this);
    // The dialog emits connectRequested() right before accept(); hook it up
    // to the session so the flow starts as the dialog closes.
    connect(&dialog, &ConnectDialog::connectRequested, this,
            [this](const QString &clientId, const QString &clientSecret,
                   const QString &username, int gameId)
            { m_session->connectAsSpectator(clientId, clientSecret, username, gameId); });
    dialog.exec();
}

void MainWindow::showPlayerInfo(int number, bool homeSide)
{
    const FFBPlayer *p = m_model->findPlayerByNumber(number, homeSide);
    if (!p) {
        m_selectedPlayerId.clear();
        m_playerInfo->showPlaceholder();
        m_field->setSelectedPlayer(QString());
        return;
    }
    selectPlayer(p->playerId);
}

void MainWindow::onFieldPlayerClicked(const QString &playerId)
{
    if (playerId.isEmpty()) {
        m_selectedPlayerId.clear();
        m_playerInfo->showPlaceholder();
        m_field->setSelectedPlayer(QString());
        return;
    }
    if (!m_model->findPlayerById(playerId))
        return; // unknown id (stale click)
    selectPlayer(playerId);
}

void MainWindow::selectPlayer(const QString &playerId)
{
    const FFBPlayer *p = m_model->findPlayerById(playerId);
    if (!p) {
        m_selectedPlayerId.clear();
        m_playerInfo->showPlaceholder();
        m_field->setSelectedPlayer(QString());
        return;
    }
    m_selectedPlayerId = playerId;
    m_field->setSelectedPlayer(playerId);

    // Assemble the panel data (widget stays decoupled from the model).
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
    for (const QString &s : p->skills) {
        PlayerInfoData::Skill sk;
        sk.name = s;
        sk.value = values.value(s);
        sk.used = p->usedSkills.contains(s);
        d.skills.append(sk);
    }
    d.injuries = p->lastingInjuries;
    if (!p->recoveringInjury.isEmpty())
        d.injuries << tr("Recovering: %1").arg(p->recoveringInjury);

    m_playerInfo->showPlayer(d);
}

QByteArray MainWindow::savedLayout() const
{
    return m_dockManager->saveState(kLayoutStateVersion);
}

void MainWindow::restoreLayout(const QByteArray &state)
{
    if (!state.isEmpty())
        m_dockManager->restoreState(state, kLayoutStateVersion);
}

// -----------------------------------------------------------------------------
// Dock layout persistence (QSettings)
// -----------------------------------------------------------------------------
namespace {
const QString kLayoutSettingsKey = QStringLiteral("mainwindow/dockLayout");
} // namespace

void MainWindow::loadLayout()
{
    QSettings settings;
    const QByteArray state = settings.value(kLayoutSettingsKey).toByteArray();
    restoreLayout(state);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue(kLayoutSettingsKey, savedLayout());
    QMainWindow::closeEvent(event);
}
