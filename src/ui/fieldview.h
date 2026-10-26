// =============================================================================
// FieldView — 26x15 field widget
//
// Coordinate convention (matches server FieldCoordinate.java and the TS app):
//   X in [0, 25]  -> horizontal, X=0 is the home end zone (left side)
//   Y in [0, 14]  -> vertical,   Y=0 is the top row
//   Column 0 = home TD, column 25 = away TD, center line between 12 and 13.
//
// Visual design (Mockup F, 23/ott): spectacular baked field texture
// (assets/field/field.png: crimson 1-column TD zones with vertical
// TOUCHDOWN, grid-aligned mowing stripes), exact 26x15 grid over the whole
// field (the TD columns show their 15 cells), emphasized side-zone
// boundaries (row 0 / row 14), a SINGLE center line, per-match team-logo
// watermarks (~19% alpha), white circular action wheel + target brackets
// on the acting player, the bench box strips on the left (home) and right
// (away) edges — RESERVE / KO / INJURED sections with circular faces — and
// a MicroProse-style top-down rain overlay when the weather is rain.
// =============================================================================
#pragma once

#include <QEvent>
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
        bool sideHome = true; // which team this token belongs to (hover cards)
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
    // Bench box strips (left = home, right = away): one entry per player in
    // the box; grouped by base state into RESERVE (9) / KO (5) /
    // INJURED (6,7,8,10) sections, like the Mockup F strips.
    struct BoxEntry {
        QString name;
        QString positionName; // face lookup
        QString race;         // team race (face fallback)
        int state = 0;        // ffb PlayerState
    };
    void setBoxes(const QVector<BoxEntry> &home, const QVector<BoxEntry> &away);
    void setBall(int x, int y, bool inPlay);
    void setTurn(int turn);
    // Per-match team logos (FUMBBL /i/<id> PNGs) drawn as watermarks centered
    // on each half (~18% alpha). Null pixmaps = no watermark.
    void setTeamLogos(const QPixmap &home, const QPixmap &away);
    // Weather display name ("Nice Weather", "Light Rain", "Pouring Rain",
    // ...). When it contains "rain" a top-down droplet overlay is drawn.
    void setWeather(const QString &weather);

    // Selected player (highlighted with a ring). Empty id = nothing selected.
    void setSelectedPlayer(const QString &playerId);
    QString selectedPlayer() const { return m_selectedPlayer; }

    // Telemetry (used by tests / debug): rendered token count and ball spot.
    int playerCount() const { return m_homePlayers.size() + m_awayPlayers.size(); }
    QPoint ballPosition() const { return m_ballInPlay ? QPoint(m_ballX, m_ballY) : QPoint(-1, -1); }
    // The topmost token at a widget point (nullptr if none / outside field).
    // Honors the current zoom/pan (Mockup F round 8 field navigation).
    const PlayerToken *tokenAt(const QPoint &pos) const;
    // The box-strip entry at a widget point (nullptr if none): hovering a
    // bench face shows the player card too.
    const BoxEntry *boxEntryAt(const QPoint &pos, bool &sideHome) const;

    // Field navigation (round 8): mouse wheel = zoom (1.0..3.0, anchored on
    // the cursor), right-mouse drag while zoomed = pan, right click = reset.
    qreal zoom() const { return m_zoom; }
    void resetView();

    QSize sizeHint() const override;

signals:
    // Left click on the field. Carries the clicked player's id, or an empty
    // string when the click landed on an empty square (deselect).
    void playerClicked(const QString &playerId);
    // Hover over a player (field token or bench face). An EMPTY token
    // (playerId empty) means the mouse is over nothing.
    void playerHovered(const PlayerToken &token);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    static constexpr int kFieldWidth = 26;   // X: 0..25
    static constexpr int kFieldHeight = 15;  // Y: 0..14

    QRectF fieldRect() const;
    QRectF cellRect(int x, int y) const;
    // Paint one box strip (the Mockup F left/right bench strip).
    void paintBoxStrip(QPainter &p, const QRectF &f, const QColor &accent,
                       const QVector<BoxEntry> &entries, bool leftSide) const;
    // The filled-slot rectangles of one strip (shared by painter + hover).
    QVector<QPair<QRectF, const BoxEntry*>> boxSlots(
        const QRectF &f, const QVector<BoxEntry> &entries, bool leftSide) const;
    // Map a widget point through the zoom/pan transform into field space.
    QPointF toFieldSpace(const QPointF &pos) const;
    void clampPan();

    QString m_homeName = QStringLiteral("Team 1");
    QString m_awayName = QStringLiteral("Team 2");
    QColor m_homeColor{0xc8, 0x96, 0x3c}; // gold
    QColor m_awayColor{0x4a, 0x6a, 0xb8}; // blue
    QString m_homeRace;
    QString m_awayRace;
    QVector<PlayerToken> m_homePlayers;
    QVector<PlayerToken> m_awayPlayers;
    QVector<BoxEntry> m_homeBox;
    QVector<BoxEntry> m_awayBox;
    int m_ballX = 13;
    int m_ballY = 7;
    bool m_ballInPlay = false;
    int m_turn = 1;
    QString m_selectedPlayer; // playerId highlighted with a ring
    QPixmap m_homeLogo;  // watermark for the home half
    QPixmap m_awayLogo;  // watermark for the away half
    QString m_weather;   // display name; "rain" substring => droplet overlay

    // Field navigation state (round 8).
    qreal m_zoom = 1.0;      // 1.0 = fit, up to 3.0
    QPointF m_pan;           // translation in widget px (0,0 at zoom 1)
    bool m_panning = false;  // right button down
    QPoint m_panStart;       // cursor at press
    QPointF m_panStartOffset; // pan at press
    QString m_hoverId; // hover key: token playerId, "box:<name>", or ""
};
