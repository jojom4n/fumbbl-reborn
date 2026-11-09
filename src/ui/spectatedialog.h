// =============================================================================
// SpectateDialog — File > Spectate Game… (spectator)
//
// Shows the live FUMBBL match list (GET /api/match/current, public) in a
// filterable table (division tabs, text over team/coach names, race) plus a
// manual game-id field. The list auto-refreshes every 20 s (polling) while
// the dialog is open. Selecting a row and clicking "Spectate" (or
// double-clicking it) starts the spectator session for that game with the
// client's LOGIN credentials (LoginDialog — the user is already
// authenticated with the server; no account fields here).
// =============================================================================
#pragma once

#include "core/matchlist.h"

#include <QDialog>
#include <QPair>
#include <QTabBar>
#include <QTimer>

class MatchListClient;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

class SpectateDialog : public QDialog
{
    Q_OBJECT

public:
    // autoLoadList: fetch the public match list on construction. Tests pass
    // false to stay network-free.
    explicit SpectateDialog(QWidget *parent = nullptr, bool autoLoadList = true);

    // Drive the dialog programmatically (tools/tests): select a row and
    // fire the same path a double-click / the "Spectate" button takes.
    bool selectGame(int gameId);
    bool selectFirstGame();
    void spectate();
    // Read-outs for tools/tests.
    int selectedGameId() const;
    int currentTableRow() const;
    int tableRowCount() const;
    // Team logos of the currently selected row ({home, away}) — fallback
    // source for the game state's often-null logoUrl. May be empty.
    QPair<QString, QString> selectedTeamLogos() const;
    // Test seams: inject a match list without a network round-trip and reach
    // the manual game-id field.
    void loadMatchesForTest(const QVector<FFBLiveMatch> &matches);
    QSpinBox *manualGameIdForTest() const { return m_manualGameId; }
    // Polling seams (tests): the R0 fixed interval + start/stop the timer
    // without a network round-trip.
    int pollIntervalMsForTest() const { return m_pollTimer ? m_pollTimer->interval() : -1; }
    void startPollingForTest() { if (m_pollTimer) m_pollTimer->start(); }
    // Division tabs (tests): count + select by tab text.
    int divisionTabCountForTest() const { return m_divisionTabs->count(); }
    bool selectDivisionTabForTest(const QString &division)
    {
        for (int i = 0; i < m_divisionTabs->count(); ++i)
            if (m_divisionTabs->tabText(i) == division) {
                m_divisionTabs->setCurrentIndex(i);
                return true;
            }
        return false;
    }

signals:
    // The match list table has been populated (diagnostic/tests).
    void listLoaded();
    // Emitted right before accept(). The session is started with the
    // client's login credentials (MainWindow holds them).
    void connectRequested(int gameId);

private slots:
    void onMatchesReceived(QVector<FFBLiveMatch> matches);
    void onFetchFailed(const QString &message);
    void refreshList();
    void applyFilters();

private:
    void rebuildFilterCombos();
    void spectateBtnState();    // enable/disable the Spectate button

    MatchListClient *m_matchList = nullptr;
    QVector<FFBLiveMatch> m_matches;

    // Filters
    QLineEdit *m_textFilter = nullptr;
    QComboBox *m_raceFilter = nullptr;
    QTabBar *m_divisionTabs = nullptr; // "All" + one tab per division
    QPushButton *m_refreshBtn = nullptr;
    QTimer *m_pollTimer = nullptr;     // 20 s fixed polling (R0)

    // Table
    QTableWidget *m_table = nullptr;

    QSpinBox *m_manualGameId = nullptr;
    QPushButton *m_spectateBtn = nullptr;
    QLabel *m_status = nullptr;
};
