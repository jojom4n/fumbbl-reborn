// =============================================================================
// MainWindow — implementation
//
// HUD layout (phase 10): the field fills the whole window; the panels are
// translucent FloatingPanels that float over it (draggable by their title
// strip, resizable by their edges, hideable with ×). Layout persistence is a
// small JSON (positions AND sizes) in QSettings; "Reset Layout" restores the
// default arrangement with the default sizes.
// =============================================================================
#include "ui/mainwindow.h"

#include "core/gamemodel.h"
#include "core/reportparser.h"
#include "core/spectatorsession.h"
#include "ui/chatwidget.h"
#include "ui/connectdialog.h"
#include "ui/dicelogwidget.h"
#include "ui/fieldview.h"
#include "ui/headerbar.h"
#include "ui/panel.h"
#include "ui/playerinfowidget.h"
#include "ui/rosterwidget.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QSettings>
#include <QStatusBar>


namespace {
const QString kLayoutSettingsKey = QStringLiteral("mainwindow/panelLayout");
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("FUMBBL Reborn"));
    resize(1400, 860);

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
    const QStringList panelOrder = { QStringLiteral("home_roster"),
                                     QStringLiteral("away_roster"),
                                     QStringLiteral("player_info"),
                                     QStringLiteral("dice_log"),
                                     QStringLiteral("chat"),
                                     QStringLiteral("protocol") };
    for (const QString &name : panelOrder)
        viewMenu->addAction(m_panelViewActions.value(name));
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

void MainWindow::createCentralWidget()
{
    // The stage is the central widget: it hosts the field (full rect) plus
    // the floating overlay panels, so everything shares one top-level window.
    m_stage = new QWidget(this);
    m_stage->setObjectName(QStringLiteral("stage"));
    m_stage->setStyleSheet(QStringLiteral("#stage { background: #0c1014; }"));

    m_field = new FieldView(m_stage);
    m_field->setObjectName(QStringLiteral("field"));
    setCentralWidget(m_stage);
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

    m_homeRoster = new RosterWidget(m_stage);
    m_homeRoster->setObjectName(QStringLiteral("roster_home"));
    m_homeRoster->setTeamColor(homeAccent);
    makePanel(QStringLiteral("home_roster"), tr("Home Roster"), m_homeRoster, homeAccent);

    m_awayRoster = new RosterWidget(m_stage);
    m_awayRoster->setObjectName(QStringLiteral("roster_away"));
    m_awayRoster->setTeamColor(awayAccent);
    makePanel(QStringLiteral("away_roster"), tr("Away Roster"), m_awayRoster, awayAccent);

    m_playerInfo = new PlayerInfoWidget(m_stage);
    makePanel(QStringLiteral("player_info"), tr("Player Info"), m_playerInfo, neutral);

    m_diceLog = new DiceLogWidget(m_stage);
    makePanel(QStringLiteral("dice_log"), tr("Dice Log"), m_diceLog, neutral);

    m_chat = new ChatWidget(m_stage);
    makePanel(QStringLiteral("chat"), tr("Chat"), m_chat, neutral);

    m_protocolLog = new QPlainTextEdit(m_stage);
    m_protocolLog->setReadOnly(true);
    m_protocolLog->setMaximumBlockCount(4000);
    m_protocolLog->setStyleSheet(QStringLiteral("background: rgba(10, 13, 18, 200); "
                                                "color: #9fb0c4;"));
    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(8);
    m_protocolLog->setFont(mono);
    makePanel(QStringLiteral("protocol"), tr("Protocol Log"), m_protocolLog, neutral);
}

QRect MainWindow::defaultGeometryFor(const QString &name) const
{
    // Relative to the stage size; clamped by applyDefaultLayout().
    const int w = m_stage ? m_stage->width() : 1400;
    const int h = m_stage ? m_stage->height() : 860;
    if (name == QLatin1String("home_roster"))
        return QRect(10, 10, 280, 540);
    if (name == QLatin1String("away_roster"))
        return QRect(w - 290, 10, 280, 540);
    if (name == QLatin1String("player_info"))
        return QRect(10, h - 330, 440, 320);
    if (name == QLatin1String("dice_log"))
        return QRect(460, h - 210, 380, 200);
    if (name == QLatin1String("chat"))
        return QRect(w - 460, h - 210, 450, 200);
    if (name == QLatin1String("protocol"))
        return QRect(w - 290, 560, 280, qMax(160, h - 570));
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
        it.value()->show();
        it.value()->raise();
    }
    for (auto it = m_panelViewActions.cbegin(); it != m_panelViewActions.cend(); ++it)
        it.value()->setChecked(true);
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
        it.value()->setGeometry(g);
        it.value()->setVisible(p.contains(QStringLiteral("v")) ? p.value(QStringLiteral("v")).toBool() : true);
    }
    for (auto it = m_panelViewActions.cbegin(); it != m_panelViewActions.cend(); ++it)
        it.value()->setChecked(m_panels.value(it.key())->isVisible());
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
}

// -----------------------------------------------------------------------------
// UI state updaters
// -----------------------------------------------------------------------------
void MainWindow::setTeams(const QString &homeName, const QString &awayName)
{
    m_header->setTeams(homeName, awayName);
    m_field->setHomeTeamName(homeName);
    m_field->setAwayTeamName(awayName);
    m_field->setHomeColor(PlayerInfoWidget::teamColor(homeName, true));
    m_field->setAwayColor(PlayerInfoWidget::teamColor(awayName, false));
    if (m_model) {
        m_field->setHomeRace(m_model->teamHomeRace());
        m_field->setAwayRace(m_model->teamAwayRace());
    }
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
        pt.positionName = t.positionName;
        pt.isActing = t.isActing;
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
    d.cards = p->cards;

    m_playerInfo->showPlayer(d);
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
