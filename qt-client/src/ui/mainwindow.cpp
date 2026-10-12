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

#include "ui/chatwidget.h"
#include "ui/dicelogwidget.h"
#include "ui/fieldview.h"
#include "ui/headerbar.h"
#include "ui/playerinfowidget.h"
#include "ui/rosterwidget.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
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
    m_connectAction->setEnabled(false); // enabled once the core network layer lands

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
