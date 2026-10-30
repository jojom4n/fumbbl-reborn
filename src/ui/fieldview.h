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

#include <QElapsedTimer>
#include <QEvent>
#include <QPoint>
#include <QSet>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include "core/assets.h"
#include "ui/rainsim.h"

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
    bool isRaining() const { return m_raining; } // round 12: animated rain on
    // Round 13 (user): the STADIUM (an interchangeable top-down backdrop, its
    // own asset + manifest). The field is fitted into the manifest's pit and
    // the dugout faces into the manifest's dugout rects — data-driven, so any
    // stadium aligns. `name` defaults to "medieval_arena".
    void setStadium(const QString &name = QStringLiteral("medieval_arena"));
    // The widget-space center of field cell (x,y) under the CURRENT
    // zoom/pan (zoom 1 / pan 0 = the fitted scene). Used by the tests so the
    // click points never hard-code the scene geometry.
    QPoint cellCenterPx(int x, int y) const;
    // Geometry (public for the preview tool + tests): the PITCH (field fitted
    // into the manifest pit) and a team's dugout (top band) in scene (widget
    // at zoom 1) space.
    QRectF fieldRect() const;
    QRectF dugoutRect(bool home) const;
    // The stadium image rect in scene (widget, zoom 1) space — public for
    // the preview tool / tests (the navigation clamp target).
    QRectF sceneRect() const;
    // Visual render position (cell coords) of a token: the animated position
    // while a move is in flight, else its cell center (public for tests).
    QPointF tokenVisualPos(const PlayerToken &t) const;

    // Selected player. Rendered (round 9, user decision) as a BLUE GLOWING
    // RADIAL circle: a soft radial halo (QRadialGradient, alpha fading out)
    // plus a crisp blue ring, drawn in field space so it follows zoom/pan.
    // The halo PULSES (a sinusoidal ~1.2 Hz breathing on alpha/radius, via a
    // QTimer that repaints only the token's rect — never the whole field).
    // Selection does NOT open the action wheel (that was round 9 bug: the
    // wheel must not follow the click selection).
    void setSelectedPlayer(const QString &playerId);
    QString selectedPlayer() const { return m_selectedPlayer; }

    // Round 13 turn 4 (user): the player being BLOCKED — a RED glowing rim
    // (like the selection's blue glow, reserved red = injury/failure/block).
    // Empty id = no block glow. Drawn in field space, follows zoom/pan and
    // the token's animated glide.
    void setBlockTarget(const QString &playerId);
    QString blockTarget() const { return m_blockTarget; }

    // Action wheel (Mockup F play mode — kept for the future game client,
    // round 9): the wheel + target brackets are drawn ONLY for an explicit
    // target (setActionWheelTarget), never for the selection or for the
    // FFB acting-player state. In spectator mode the target stays empty and
    // nothing is drawn; in play mode a right-click ON A PLAYER sets the
    // target (right-drag on empty space still pans, right-click empty
    // still resets the view).
    void setActionWheelTarget(const QString &playerId);
    QString actionWheelTarget() const { return m_wheelTarget; }
    void setPlayMode(bool play); // spectator (default) vs game mode
    bool playMode() const { return m_playMode; }

    // Telemetry (used by tests / debug): rendered token count and ball spot.
    int playerCount() const { return m_homePlayers.size() + m_awayPlayers.size(); }
    QPoint ballPosition() const { return m_ballInPlay ? QPoint(m_ballX, m_ballY) : QPoint(-1, -1); }
    // The topmost token at a widget point (nullptr if none / outside field).
    // Honors the current zoom/pan (Mockup F round 8 field navigation).
    const PlayerToken *tokenAt(const QPoint &pos) const;
    // The box-strip entry at a widget point (nullptr if none): hovering a
    // bench face shows the player card too.
    const BoxEntry *boxEntryAt(const QPoint &pos, bool &sideHome) const;

    // Field navigation (round 13, user spec — replaces the round-8 drag pan
    // and the retracted left-drag idea):
    //   * mouse WHEEL = zoom (1.0..3.0, anchored on the cursor)
    //   * EDGE PAN: the mouse pressed against the window's edge scrolls the
    //     view in that direction (no button needed);
    //   * WASD: W up, A left, S down, D right;
    //   * MULTI-MONITOR: while the cursor is in the edge zone it is CLAMPED
    //     to the window (QCursor::setPos) so it can never spill onto the
    //     neighboring monitor;
    //   * LEFT CLICK = select/show card (round-12 behavior, kept);
    //   * RIGHT CLICK = reset the view.
    // NO button-drag pan anymore (left or right).
    qreal zoom() const { return m_zoom; }
    QPointF pan() const { return m_pan; } // tests
    void resetView();

    // Pure edge-pan computation (testable without a real cursor): the pan
    // delta in WIDGET px for the cursor at `cursor` (global) given the
    // top-level `windowRect` (global). Zero outside the edge zone / at
    // zoom 1. Looking left = content moves right = positive x.
    static QPointF edgePanDelta(const QRect &windowRect, const QPoint &cursor,
                                qreal zoom);

    // The actor's CUMULATIVE move trail (round 13): one polyline per move
    // action, through the field cell centers, with an arrow at the end of
    // each path. Empty = nothing drawn. See GameModel::moveTrace() for the
    // persistence semantics (no timeout; appends per move; cleared when the
    // actor changes or a new turn starts).
    void setMoveTrace(const QVector<QVector<QPoint>> &paths);
    QVector<QVector<QPoint>> moveTrace() const { return m_moveTrace; }

    QSize sizeHint() const override;

signals:
    // Left click on the field. Carries the clicked player's id, or an empty
    // string when the click landed on an empty square (deselect).
    void playerClicked(const QString &playerId);
    // Hover over a player (field token or bench face). An EMPTY token
    // (playerId empty) means the mouse is over nothing.
    void playerHovered(const PlayerToken &token);
    // Play mode only: right-click landed on a player — the action wheel
    // target was set and the game logic should show the wheel menu.
    void actionWheelRequested(const QString &playerId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override; // round 12: reseed the rain
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override; // round 13: WASD pan
    void keyReleaseEvent(QKeyEvent *event) override; // round 13: stop smooth pan

private:
    static constexpr int kFieldWidth = 26;   // X: 0..25
    static constexpr int kFieldHeight = 15;  // Y: 0..14

    // Round 13 (user): the SCENE is the STADIUM (an interchangeable top-down
    // backdrop + manifest). The 26x15 field is fitted (square cells) into the
    // manifest's FIELD PIT; the dugout faces go into the manifest's DUGOUT
    // rects (top band: home LEFT, away RIGHT). The mapping is data-driven so
    // any stadium aligns (no hard-coded scene grid anymore).
    QRectF mapNormRect(const QRectF &norm) const; // image-space 0..1 -> scene px
    QRectF cellRect(int x, int y) const;
    QPointF cellCenter(int x, int y) const;
    // Paint one dugout's bench slots (round 12: on the dugout panel image).
    void paintBoxStrip(QPainter &p, const QColor &accent,
                       const QVector<BoxEntry> &entries, bool leftSide) const;
    // The slot rectangles of one dugout (shared by painter + hover, field
    // space). Two columns x four rows per section (RESERVE/KO/INJURED).
    QVector<QPair<QRectF, const BoxEntry*>> boxSlots(
        const QVector<BoxEntry> &entries, bool leftSide) const;
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
    QString m_blockTarget; // round 13 turn 4: red glow (player being blocked)
    QPixmap m_homeLogo;  // watermark for the home half
    QPixmap m_awayLogo;  // watermark for the away half
    QString m_weather;   // display name; "rain" substring => ANIMATED rain

    // Round 13 (user): the STADIUM (interchangeable backdrop + manifest).
    // m_stadium carries the normalized fieldPit / dugoutHome / dugoutAway
    // rects that drive the alignment. The art is loaded on demand (assets).
    QString m_stadiumName = QStringLiteral("medieval_arena");
    assets::StadiumManifest m_stadium;

    // Round 12: animated rain (user request) — falling droplet streaks +
    // splash ripples where they land; ripples inside the puddles are bigger
    // and last longer. The sim is stepped by a ~30fps timer and painted in
    // the zoomed field space (the rain belongs to the field scene).
    RainSim m_rain;
    QTimer *m_rainTimer = nullptr;
    QElapsedTimer m_rainClock;
    bool m_raining = false;
    // Round 13 (user): puddles are DYNAMIC — they accumulate over match time
    // (stepPuddles), smaller and organic, fading in. Stored as FRACTIONS of
    // the field so they stay put across resizes.
    struct Puddle {
        qreal fx = 0.5, fy = 0.5; // center, fraction of the field (0..1)
        qreal fr = 0.03;          // radius, fraction of the field height
        qreal squash = 0.7;       // ry/rx (0.55..0.8)
        qreal rotDeg = 0.0;       // rotation (organic, not a perfect ellipse)
        double birthMs = 0.0;     // when it appeared (m_puddleClockMs)
    };
    QVector<Puddle> m_puddles;
    double m_puddleClockMs = 0.0;
    double m_nextPuddleAt = 0.0;
    void rainConfigure();
    void stepPuddles(float dtMs);
    QRectF puddleRect(const Puddle &pd, const QRectF &f) const;
    QVector<QRectF> puddleRects() const;
    void drawPuddles(QPainter &p, const QRectF &f) const;
    void drawRain(QPainter &p);
    void drawMowStripes(QPainter &p, const QRectF &f) const;
    void drawTouchdownZones(QPainter &p, const QRectF &f) const;
    void drawMoveTrail(QPainter &p);

    // Field navigation state (round 8, pan rewritten in round 13).
    qreal m_zoom = 1.0;      // 1.0 = fit, up to 3.0
    QPointF m_pan;           // translation in widget px (0,0 at zoom 1)
    QString m_hoverId; // hover key: token playerId, "box:<name>", or ""

    // Round 13: EDGE PAN — a fast timer polls the global cursor position:
    // inside the window's edge zone the view scrolls in that direction and
    // the cursor is clamped to the window (multi-monitor: it must not spill
    // onto the neighboring screen). Active only while zoomed (at zoom 1 the
    // letterboxed scene cannot pan, and trapping the mouse would be a
    // nuisance).
    QTimer *m_edgePanTimer = nullptr;
    void edgePanTick();

    // Round 13 (user): SMOOTH keyboard pan — the held WASD keys are tracked
    // and a fast timer pans continuously (no more jumpy auto-repeat steps).
    QSet<int> m_panKeys;
    QTimer *m_panTimer = nullptr;
    void smoothPanTick();
    // Round 13 (user): right button + DRAG = pan (brought back). A right
    // CLICK (no drag) in play mode on a player still sets the action wheel.
    // The view RESET moved to the MIDDLE click (right click no longer resets).
    QPoint m_rightPressPos;
    QPoint m_rightDragLast;
    bool m_rightDragging = false;

    // Round 13: the actor's cumulative move trail (drawn under the tokens).
    QVector<QVector<QPoint>> m_moveTrace;

    // Round 13 (user): per-square MOVEMENT ANIMATION. Tokens do not jump to
    // the new cell — they glide through the grid, square by square, like the
    // official client (fast + pleasant). The MODEL stays authoritative (the
    // visual position is a pure render detail): on each setPlayers a player
    // that moved gets a waypoint path (old cell -> new cell, grid steps) and
    // a fast timer interpolates the VISUAL position through it. m_prevModel
    // is the last model layout (to detect movers); m_visualPos is the current
    // render position in cell coordinates (x+0.5 = cell center).
    struct TokenAnim {
        QVector<QPointF> waypoints; // cell coords, walked in order
        int leg = 0;                // current leg: waypoints[leg] -> [leg+1]
        qreal t = 0.0;              // 0..1 progress on the current leg
        bool active = false;
    };
    QHash<QString, TokenAnim> m_tokenAnim;
    QHash<QString, QPointF> m_visualPos;
    QHash<QString, QPoint> m_prevModel;
    QPoint m_prevBall;
    bool m_havePrevBall = false;
    QTimer *m_animTimer = nullptr;
    void startTokenAnim(const QString &id, const QPoint &to);
    void animTick();
    void ensureAnimTimer();
    bool anyTokenAnimActive() const;

    // Round 9: selection glow pulse + explicit action-wheel target.
    QTimer *m_pulseTimer = nullptr; // breathes the selection halo
    qreal m_pulsePhase = 0.0;       // 0..2π, ~1.2 Hz
    QString m_wheelTarget;          // explicit action-wheel target (play mode)
    bool m_playMode = false;        // spectator client: false (default)
    QString m_rightPressPlayerId;   // player under the right button at press
};
