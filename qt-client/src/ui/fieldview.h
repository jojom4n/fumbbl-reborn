// =============================================================================
// FieldView — 26x15 Blood Bowl field widget
//
// Coordinate convention (matches server FieldCoordinate.java and the TS app):
//   X in [0, 25]  -> horizontal, X=0 is the home end zone (left side)
//   Y in [0, 14]  -> vertical,   Y=0 is the top row
//   Column 0 = home TD, column 25 = away TD, center line between 12 and 13.
// =============================================================================
#pragma once

#include <QVector>
#include <QWidget>

class FieldView : public QWidget
{
    Q_OBJECT

public:
    struct PlayerToken {
        int x = 0;          // field X (0..25)
        int y = 0;          // field Y (0..14)
        int number = 0;     // player number (0 = unknown)
        bool hasBall = false;
        int state = 1;      // ffb PlayerState (1=standing; 3=prone, 4=stunned, ...)
        QString name;       // display name (for tooltips)
    };

    explicit FieldView(QWidget *parent = nullptr);

    void setHomeTeamName(const QString &name);
    void setAwayTeamName(const QString &name);
    void setPlayers(const QVector<PlayerToken> &home, const QVector<PlayerToken> &away);
    void setBall(int x, int y, bool inPlay);
    void setTurn(int turn);

    // Telemetry (used by tests / debug): rendered token count and ball spot.
    int playerCount() const { return m_homePlayers.size() + m_awayPlayers.size(); }
    QPoint ballPosition() const { return m_ballInPlay ? QPoint(m_ballX, m_ballY) : QPoint(-1, -1); }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static constexpr int kFieldWidth = 26;   // X: 0..25
    static constexpr int kFieldHeight = 15;  // Y: 0..14

    QRectF fieldRect() const;
    QRectF cellRect(int x, int y) const;

    QString m_homeName = QStringLiteral("Team 1");
    QString m_awayName = QStringLiteral("Team 2");
    QVector<PlayerToken> m_homePlayers;
    QVector<PlayerToken> m_awayPlayers;
    int m_ballX = 13;
    int m_ballY = 7;
    bool m_ballInPlay = false;
    int m_turn = 1;
};
