// =============================================================================
// SpectateDialog — File > Spectate Game… (spectator)
//
// Shows the live FUMBBL match list (GET /api/match/current, via OAuth) in a
// filterable table (text over team/coach names, race, division) plus the
// OAuth credentials (collapsible, remembered in the same JSON file the TS
// client uses) and a manual game-id field. Selecting a row and clicking
// "Spectate" (or double-clicking it) starts the spectator session for that
// game.
// =============================================================================
#pragma once

#include "core/matchlist.h"

#include <QDialog>

class FumbblAuth;
class MatchListClient;
class QCheckBox;
class QLabel;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

class SpectateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpectateDialog(QWidget *parent = nullptr);

    // Drive the dialog programmatically (tools/tests): select a row and
    // fire the same path a double-click / the "Spectate" button takes.
    bool selectGame(int gameId);
    bool selectFirstGame();
    void spectate();
    // Read-outs for tools/tests.
    int selectedGameId() const;
    int currentTableRow() const;
    int tableRowCount() const;
    // Test seams: inject a match list without a network round-trip and reach
    // the manual game-id field.
    void loadMatchesForTest(const QVector<FFBLiveMatch> &matches);
    QSpinBox *manualGameIdForTest() const { return m_manualGameId; }

signals:
    // The match list table has been populated (diagnostic/tests).
    void listLoaded();
    // Same contract ConnectDialog used: emit right before accept().
    void connectRequested(const QString &clientId, const QString &clientSecret,
                          const QString &username, int gameId);

private slots:
    void onMatchesReceived(QVector<FFBLiveMatch> matches);
    void onFetchFailed(const QString &message);
    void onAccessTokenReady(const QString &accessToken);
    void onAuthFailed(const QString &step, const QString &error);
    void refreshList();
    void applyFilters();

private:
    struct Credentials {
        QString clientId;
        QString clientSecret;
        QString username;
        bool valid() const
        {
            return !clientId.isEmpty() && !clientSecret.isEmpty() && !username.isEmpty();
        }
    };

    void loadCredentials();
    void saveCredentials() const;
    Credentials currentCredentials() const;
    void rebuildFilterCombos();
    void spectateBtnState();    // enable/disable the Spectate button

    FumbblAuth *m_auth = nullptr;
    MatchListClient *m_matchList = nullptr;

    QVector<FFBLiveMatch> m_matches;

    // Filters
    QLineEdit *m_textFilter = nullptr;
    QComboBox *m_raceFilter = nullptr;
    QComboBox *m_divisionFilter = nullptr;
    QPushButton *m_refreshBtn = nullptr;

    // Table
    QTableWidget *m_table = nullptr;

    // Credentials (collapsible)
    QPushButton *m_credToggle = nullptr;
    QGroupBox *m_credGroup = nullptr;
    QLineEdit *m_clientId = nullptr;
    QLineEdit *m_clientSecret = nullptr;
    QLineEdit *m_username = nullptr;
    QCheckBox *m_remember = nullptr;
    QPushButton *m_loadBtn = nullptr;

    QSpinBox *m_manualGameId = nullptr;
    QPushButton *m_spectateBtn = nullptr;
    QLabel *m_status = nullptr;
};
