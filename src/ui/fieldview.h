// =============================================================================
// FieldView — 26x15 field widget
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
        QString playerId;   // server id (for selection)
        QString positionName; // "Ogre", "Zombie Lineman", ... (face lookup)
        bool isActing = false;
    };

    explicit FieldView(QWidget *parent = nullptr);

    void setHomeTeamName(const QString &name);
    void setAwayTeamName(const QString &name);
    // Team colors: the colored circle behind each face is drawn at runtime
    // (home/away), never baked into the face assets.
    void setHomeColor(const QColor &color);
    void setAwayColor(const QColor &color);
    // Team races: used for the face fallback of custom-named star pieces.
    void setHomeRace(const QString &race);
    void setAwayRace(const QString &race);
    void setPlayers(const QVector<PlayerToken> &home, const QVector<PlayerToken> &away);
    void setBall(int x, int y, bool inPlay);
    void setTurn(int turn);

    // Selected player (highlighted with a ring). Empty id = nothing selected.
    void setSelectedPlayer(const QString &playerId);
    QString selectedPlayer() const { return m_selectedPlayer; }

    // Telemetry (used by tests / debug): rendered token count and ball spot.
    int playerCount() const { return m_homePlayers.size() + m_awayPlayers.size(); }
    QPoint ballPosition() const { return m_ballInPlay ? QPoint(m_ballX, m_ballY) : QPoint(-1, -1); }
    // The topmost token at a widget point (nullptr if none / outside field).
    const PlayerToken *tokenAt(const QPoint &pos) const;

    QSize sizeHint() const override;

signals:
    // Left click on the field. Carries the clicked player's id, or an empty
    // string when the click landed on an empty square (deselect).
    void playerClicked(const QString &playerId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    static constexpr int kFieldWidth = 26;   // X: 0..25
    static constexpr int kFieldHeight = 15;  // Y: 0..14

    QRectF fieldRect() const;
    QRectF cellRect(int x, int y) const;

    QString m_homeName = QStringLiteral("Team 1");
    QString m_awayName = QStringLiteral("Team 2");
    QColor m_homeColor{0xc8, 0x96, 0x3c}; // gold
    QColor m_awayColor{0x4a, 0x6a, 0xb8}; // blue
    QString m_homeRace;
    QString m_awayRace;
    QVector<PlayerToken> m_homePlayers;
    QVector<PlayerToken> m_awayPlayers;
    int m_ballX = 13;
    int m_ballY = 7;
    bool m_ballInPlay = false;
    int m_turn = 1;
    QString m_selectedPlayer; // playerId highlighted with a ring
};
