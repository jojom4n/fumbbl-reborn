// =============================================================================
// ConnectDialog — File > Connect… (spectator)
//
// Collects clientId / clientSecret / username / gameId and starts the
// SpectatorSession flow. Credentials can be remembered in the same JSON file
// the TS client and spectator_smoke use:
//   ~/.local/share/com.saruman.fumbbl-reborn/fumbbl-credentials.json
//   { "fumbbl_credentials": { "clientId", "clientSecret", "username" } }
// (fallback read key: "credentials"). The last used gameId is kept in
// QSettings instead.
// =============================================================================
#pragma once

#include <QDialog>

class QCheckBox;
class QFormLayout;
class QLineEdit;
class QSpinBox;

class ConnectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectDialog(QWidget *parent = nullptr);

signals:
    void connectRequested(const QString &clientId, const QString &clientSecret,
                          const QString &username, int gameId);

private:
    void loadSavedCredentials();
    void saveCredentials() const;

    QLineEdit *m_clientId = nullptr;
    QLineEdit *m_clientSecret = nullptr;
    QLineEdit *m_username = nullptr;
    QSpinBox *m_gameId = nullptr;
    QCheckBox *m_remember = nullptr;
};
