// =============================================================================
// MainWindow — implementation
// =============================================================================
#include "ui/mainwindow.h"

#include "ui/chatwidget.h"
#include "ui/dicelogwidget.h"
#include "ui/fieldview.h"
#include "ui/headerbar.h"
#include "ui/playerinfowidget.h"
#include "ui/rosterwidget.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("FUMBBL Reborn"));
    resize(1400, 860);

    createActions();
    createToolbar();
    createCentralWidget();
    createDockWidgets();
    createStatusBar();
    createMenus(); // after the docks: the View menu uses their toggleViewAction()

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
        this->setDockOptions({});
        m_homeRosterDock->setDockLocation(Qt::LeftDockWidgetArea);
        m_awayRosterDock->setDockLocation(Qt::RightDockWidgetArea);
        m_playerInfoDock->setDockLocation(Qt::BottomDockWidgetArea);
        m_diceLogDock->setDockLocation(Qt::BottomDockWidgetArea);
        m_chatDock->setDockLocation(Qt::BottomDockWidgetArea);
        m_protocolDock->setDockLocation(Qt::LeftDockWidgetArea);
        this->splitDockWidget(m_playerInfoDock, m_diceLogDock, Qt::Horizontal);
        this->splitDockWidget(m_diceLogDock, m_chatDock, Qt::Horizontal);
        this->splitDockWidget(m_protocolDock, m_homeRosterDock, Qt::Horizontal);
        this->setDockOptions(QMainWindow::AllowTabbedDocks | QMainWindow::AnimatedDocks |
                             QMainWindow::GroupedDragging);
        m_homeRosterDock->show();
        m_awayRosterDock->show();
        m_playerInfoDock->show();
        m_diceLogDock->show();
        m_chatDock->show();
        m_protocolDock->show();
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
// Toolbar / Central / Docks
// -----------------------------------------------------------------------------
void MainWindow::createToolbar()
{
    m_header = new HeaderBar(this);
    addToolBar(Qt::TopToolBarArea, m_header);
}

void MainWindow::createCentralWidget()
{
    m_field = new FieldView(this);
    setCentralWidget(m_field);
}

void MainWindow::createDockWidgets()
{
    m_homeRoster = new RosterWidget(this);
    m_homeRoster->setTeamColor(QColor(0x7c, 0xa8, 0x6f));
    m_homeRosterDock = new QDockWidget(tr("Home Roster"), this);
    m_homeRosterDock->setObjectName(QStringLiteral("dock_home_roster"));
    m_homeRosterDock->setWidget(m_homeRoster);
    m_homeRosterDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_awayRoster = new RosterWidget(this);
    m_awayRoster->setTeamColor(QColor(0xd8, 0xc0, 0x7a));
    m_awayRosterDock = new QDockWidget(tr("Away Roster"), this);
    m_awayRosterDock->setObjectName(QStringLiteral("dock_away_roster"));
    m_awayRosterDock->setWidget(m_awayRoster);
    m_awayRosterDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_playerInfo = new PlayerInfoWidget(this);
    m_playerInfoDock = new QDockWidget(tr("Player Info"), this);
    m_playerInfoDock->setObjectName(QStringLiteral("dock_player_info"));
    m_playerInfoDock->setWidget(m_playerInfo);
    m_playerInfoDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_diceLog = new DiceLogWidget(this);
    m_diceLogDock = new QDockWidget(tr("Dice Log"), this);
    m_diceLogDock->setObjectName(QStringLiteral("dock_dice_log"));
    m_diceLogDock->setWidget(m_diceLog);
    m_diceLogDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_chat = new ChatWidget(this);
    m_chatDock = new QDockWidget(tr("Chat"), this);
    m_chatDock->setObjectName(QStringLiteral("dock_chat"));
    m_chatDock->setWidget(m_chat);
    m_chatDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_protocolLog = new QPlainTextEdit(this);
    m_protocolLog->setReadOnly(true);
    m_protocolLog->setMaximumBlockCount(4000);
    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(8);
    m_protocolLog->setFont(mono);
    m_protocolDock = new QDockWidget(tr("Protocol Log"), this);
    m_protocolDock->setObjectName(QStringLiteral("dock_protocol"));
    m_protocolDock->setWidget(m_protocolLog);
    m_protocolDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::BottomDockWidgetArea);

    addDockWidget(Qt::LeftDockWidgetArea, m_homeRosterDock);
    addDockWidget(Qt::RightDockWidgetArea, m_awayRosterDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_playerInfoDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_diceLogDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_chatDock);
    addDockWidget(Qt::LeftDockWidgetArea, m_protocolDock);

    // Bottom band: player info | dice log | chat (side by side)
    splitDockWidget(m_playerInfoDock, m_diceLogDock, Qt::Horizontal);
    splitDockWidget(m_diceLogDock, m_chatDock, Qt::Horizontal);
    // Left column: protocol log above home roster
    splitDockWidget(m_protocolDock, m_homeRosterDock, Qt::Horizontal);

    setDockOptions(DockOption::AllowTabbedDocks | DockOption::AnimatedDocks |
                   DockOption::GroupedDragging);
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
    return saveState();
}

void MainWindow::restoreLayout(const QByteArray &state)
{
    if (!state.isEmpty())
        restoreState(state);
}
