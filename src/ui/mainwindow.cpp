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
#include "ui/spectatedialog.h"
#include "ui/actionlog.h"
#include "ui/fieldview.h"
#include "ui/headerview.h"
#include "ui/panel.h"
#include "ui/playerinfowidget.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QImage>
#include <QJsonArray>
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
#include <QToolBar>
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
    connect(m_session, &SpectatorSession::reopenScheduled, this,
            [this]() { setConnectionStatus(tr("Connection restored — re-joining…"), false); });
    connect(m_session, &SpectatorSession::protocolSending, this,
            [this](const QString &id, const QJsonObject &)
            { this->appendProtocol(QStringLiteral(">> send "), id); });
    connect(m_session, &SpectatorSession::protocolReceived, this,
            [this](const QString &id, const QJsonObject &)
            { this->appendProtocol(QStringLiteral("<< recv "), id); });

    // Field click-to-select (empty id = clicked an empty square = deselect).
    connect(m_field, &FieldView::playerClicked, this, &MainWindow::onFieldPlayerClicked);
    // Hover card (round 8): the player info follows the mouse over the
    // field / bench and sits on the opposite side of the field.
    connect(m_field, &FieldView::playerHovered, this, &MainWindow::onPlayerHovered);

    connect(m_chat, &ChatWidget::messageSent, this, &MainWindow::chatMessageSent);
    connect(m_connectAction, &QAction::triggered, this, &MainWindow::connectionRequested);
    // The menu action relays into the connectionRequested signal; this is
    // what turns it into the actual dialog (missing since phase 6 — the
    // menu entry was silently dead).
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
    // Round 8: rosters gone (field box strips cover box/KO/reserve), action
    // log and chat are pinned overlays (always visible, not toggleable).
    const QStringList panelOrder = { QStringLiteral("player_info"),
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
    // Round 8: no END TURN — this is a spectator client; ending the turn is
    // not part of spectating (the button is gone from HeaderView itself).
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

    // Player info: a FloatingPanel, but round 8 drives it from HOVER and
    // pins it to the side of the field OPPOSITE the hovered player.
    m_playerInfo = new PlayerInfoWidget(m_stage);
    makePanel(QStringLiteral("player_info"), tr("Player Info"), m_playerInfo, neutral);

    // Action log (bottom-left) and chat (bottom-right): Mockup F pinned
    // overlays with their own translucent chrome — NOT draggable panels.
    m_actionLog = new ActionLogWidget(m_stage);
    m_actionLog->setFixedWidth(560);

    m_chat = new ChatWidget(m_stage);
    m_chat->setFixedSize(560, 250);

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
        const bool vis = p.contains(QStringLiteral("v")) ? p.value(QStringLiteral("v")).toBool() : true;
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
    m_field->setHomeTeamName(homeName);
    m_field->setAwayTeamName(awayName);
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
    m_field->setTurn(turn);
}

void MainWindow::setSpectators(int count)
{
    m_spectatorsLabel->setText(tr("Spectators: %1").arg(count));
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
    m_actionLog->appendLine(line, resolveId(line.actorId), resolveId(line.defenderId));
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
    setScore(m_model->scoreHome(), m_model->scoreAway());
    setTurn(m_model->turn());

    // Header (Mockup F): reroll dots, on-turn gold emphasis, weather, team
    // accents — all straight from the model.
    m_headerView->setRerolls(m_model->reRollsHome(), m_model->reRollsAway());
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
            out.append(e);
        }
        return out;
    };
    m_field->setBoxes(boxEntries(true), boxEntries(false));

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

    // Weather: drives the field's rain overlay ("Nice Weather" = none).
    m_field->setWeather(m_model->weather());

    // Round 8: no roster panels — box/bench/KO/reserve are visible in the
    // field's box strips (setBoxes above).
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
        // Structured dice: the action log renders the die faces (round 8:
        // "non ci mettere i numeri: metti i dadi") and resolved names.
        for (const DiceLogLine &line : parseReport(report))
            appendDiceLog(line);
    }
}

// Resolves a raw server id to a display name (round 8: "ci metti i nomi
// giusti" — no numeric ids in the action log). Player ids become
// "Name (number)"; TEAM ids (prayers, bribes, throw-ins, team captain)
// become the team name. Unresolved ids stay visible rather than vanishing.
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
    return id;
}

void MainWindow::loadTeamLogos()
{
    if (!m_model || !m_logoNam)
        return;
    const QString homeUrl = m_model->teamHomeLogoUrl();
    const QString awayUrl = m_model->teamAwayLogoUrl();

    const auto fetch = [this](const QString &url, int side) {
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
                       QStringLiteral("logo fetch failed: %1")
                           .arg(reply->errorString()));
        reply->deleteLater();
        return;
    }
    const QByteArray data = reply->readAll();
    reply->deleteLater();
    QImage img;
    if (!img.loadFromData(data)) // QPixmap(QByteArray) is unreliable here
        return;
    const QPixmap pm = QPixmap::fromImage(img);
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
    // serverGameTime.turnTime = ELAPSED ms of the turn. The Mockup F header
    // counts UP (FUMBBL semantics) — no decreasing progress bar.
    m_headerView->setTurnElapsedSeconds(static_cast<int>(turnTimeMs / 1000));
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
    setConnectionStatus(message, false);
    appendProtocol(QStringLiteral("FATAL "), message);
}

void MainWindow::onWsClosed(int code, const QString &reason)
{
    setConnectionStatus(tr("Connection closed (%1: %2)").arg(code).arg(reason), false);
    appendProtocol(QStringLiteral("ws closed "), QStringLiteral("%1 %2").arg(code).arg(reason));
}

// serverStatus from the server (e.g. ERROR_WRONG_PASSWORD when a join's
// one-time token was rejected). The session layer retries automatically;
// the banner shows the real server error instead of dead silence.
void MainWindow::onSessionStatus(const QString &status, const QString &message)
{
    const bool isError = status.startsWith(QLatin1String("ERROR"), Qt::CaseInsensitive);
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
    SpectateDialog dialog(this);
    // The dialog emits connectRequested() right before accept(); hook it up
    // to the session so the flow starts as the dialog closes.
    connect(&dialog, &SpectateDialog::connectRequested, this,
            [this](const QString &clientId, const QString &clientSecret,
                   const QString &username, int gameId)
            {
                // Starting (or SWITCHING) a match: clear the previous state
                // first, so the UI doesn't stay on the old match while the
                // new session is being established.
                clearMatchUi();
                setConnectionStatus(tr("Connecting to game %1…").arg(gameId), false);
                m_session->connectAsSpectator(clientId, clientSecret, username, gameId);
            });
    dialog.exec();
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
    m_field->setHomeTeamName(QString());
    m_field->setAwayTeamName(QString());
    m_actionLog->clearLog();
    m_chat->clearChat();
    hidePlayerCard();
    setTeams(QString(), QString());
    setScore(0, 0);
    setTurn(1);
    m_headerView->setTurnElapsedSeconds(0);
    m_headerView->setRerolls(0, 0);
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

    // Round 8: fill + positioning live in showPlayerCard (shared with the
    // hover path).
    showPlayerCard(p, p->hasCoords ? p->fieldX : (p->sideHome ? 6 : 19));
}

// ---------------------------------------------------------------------------
// Hover player card (round 8): appears over the mouse, far from the player,
// on the opposite side of the field.
// ---------------------------------------------------------------------------
void MainWindow::onPlayerHovered(const FieldView::PlayerToken &token)
{
    if (token.playerId.isEmpty() && token.name.isEmpty()) {
        hidePlayerCard();
        return;
    }
    const FFBPlayer *p = nullptr;
    if (!token.playerId.isEmpty()) {
        p = m_model ? m_model->findPlayerById(token.playerId) : nullptr;
    } else if (m_model && !token.name.isEmpty()) {
        // Bench face: resolve by name on the right side.
        const auto roster = token.sideHome ? m_model->rosterHome()
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
    // Hovering doesn't steal the click selection; it just shows the card.
    m_selectedPlayerId = p->playerId;
    m_field->setSelectedPlayer(p->playerId);
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
    const int ph = qMax(panel->height(), 300);
    const int sw = m_stage->width();
    const int sh = m_stage->height();
    const bool playerLeft = fieldX < 13;
    // 80px edge margin: the bench box strips live on the field edges
    // (~62px wide) and must stay visible.
    const int x = playerLeft ? sw - pw - 80 : 80;
    const int y = qBound(20, (sh - ph) / 2, sh - ph - 20);
    panel->setGeometry(x, y, pw, ph);
    panel->show();
    panel->raise();
}

void MainWindow::hidePlayerCard()
{
    if (m_playerInfo) {
        m_selectedPlayerId.clear();
        m_playerInfo->showPlaceholder();
        m_field->setSelectedPlayer(QString());
    }
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
