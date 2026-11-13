// =============================================================================
// HeaderView — Mockup F match header (custom-painted strip, 62px tall)
//
//   [crest]  HOME NAME (gold, embossed when on turn)  o o o
//            coach · race
//                    H : A
//            [TURN n] [HALF]   m:ss   (sun|rain) Weather
//   o o o   coach · race   AWAY NAME  [crest]
//
// Layout: strict ZONES — left = team 1, center = score/turn/half/timer/
// weather, right = team 2. The widget EXPANDS to the full toolbar width so
// the center zone is centered on the window.
//
// Design rules (Mockup F):
//   * the team ON TURN has its name in gold, raised (embossed) — no dots
//     and no borders; the other name stays plain ink
//   * rerolls are dots: 3 slots, filled = remaining
//   * the turn timer COUNTS UP (FUMBBL semantics) — no decreasing bar
//   * red is reserved for injury/failure: accents come from the team colors
//   * no END TURN button — this is a spectator client; ending the turn is
//     not part of spectating.
// =============================================================================
#pragma once

#include <QPixmap>
#include <QWidget>


QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

class HeaderView : public QWidget
{
    Q_OBJECT

public:
    explicit HeaderView(QWidget *parent = nullptr);

    void setTeams(const QString &home, const QString &away);
    void setRaces(const QString &home, const QString &away);
    void setCoaches(const QString &home, const QString &away);
    void setAccents(const QColor &home, const QColor &away); // team colors
    void setScores(int home, int away);
    void setTurn(int turn, int half);
    void setTurnElapsedSeconds(int seconds); // count-up display
    void setHomePlaying(bool homePlaying);   // which team is on turn
    // As many dots as the TEAM has rerolls (could be 0 or 6); a dot is gold
    // while it is still available THIS TURN and turns white as it is used
    // (turnData.reRolls).
    void setRerolls(int totalHome, int totalAway, int leftHome, int leftAway);
    void setLogos(const QPixmap &home, const QPixmap &away);
    void setWeather(const QString &weather); // display name ("Nice Weather")

    // Team inducements: one badge per type, drawn right of the home reroll
    // dots (left of the away ones, mirrored). Hover shows a tooltip with
    // the FFB display name and the effect.
    struct InducementBadge {
        QString typeId; // protocol id ("bribes", "bloodweiserBabes", ...)
        int count = 0;  // UNITS STILL AVAILABLE (value − uses, server live
                        // data). <= 0 = fully consumed → badge drawn gray.
                        // A count > 1 shows a small disc with the number.
    };
    void setInducements(const QVector<InducementBadge> &home,
                        const QVector<InducementBadge> &away);

    // State accessors (used by the UI wiring tests).
    QString teamHomeName() const { return m_home; }
    QString teamAwayName() const { return m_away; }
    int scoreHome() const { return m_scoreHome; }
    int scoreAway() const { return m_scoreAway; }
    int turnNumber() const { return m_turn; }
    int halfNumber() const { return m_half; }
    int elapsedSeconds() const { return m_elapsedSeconds; }
    bool homePlaying() const { return m_homePlaying; }
    int rerollsHome() const { return m_rerollsHome; }        // left this turn
    int rerollsAway() const { return m_rerollsAway; }
    int rerollsTotalHome() const { return m_rerollsTotalHome; } // number of dots
    int rerollsTotalAway() const { return m_rerollsTotalAway; }
    int inducementCountHome() const { return m_inducementHome.size(); }
    int inducementCountAway() const { return m_inducementAway.size(); }
    // Tooltip text for the badge under `pos` (empty = none) — test seam for
    // the hover tooltips.
    QString inducementTipAt(const QPoint &pos) const;

    QSize sizeHint() const override { return QSize(800, 62); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    // "m:ss" for a count-up elapsed time.
    static QString formatElapsed(int totalSeconds);

    QString m_home = QStringLiteral("Home");
    QString m_away = QStringLiteral("Away");
    QString m_homeRace;
    QString m_awayRace;
    QString m_homeCoach;
    QString m_awayCoach;
    QColor m_homeAccent{0x34, 0x6c, 0xb4};
    QColor m_awayAccent{0xc1, 0x98, 0x37};
    int m_scoreHome = 0;
    int m_scoreAway = 0;
    int m_turn = 1;
    int m_half = 1;
    int m_elapsedSeconds = 0;
    bool m_homePlaying = true;
    int m_rerollsHome = 0;      // gold dots (remaining this turn)
    int m_rerollsAway = 0;
    int m_rerollsTotalHome = 0; // total dots drawn
    int m_rerollsTotalAway = 0;
    QPixmap m_homeLogo;
    QPixmap m_awayLogo;
    QString m_weather;
    QVector<InducementBadge> m_inducementHome;
    QVector<InducementBadge> m_inducementAway;
    // Badge hit rects, rebuilt on every paint (positional truth).
    QVector<QRectF> m_indRectsHome;
    QVector<QRectF> m_indRectsAway;

    // The ACTIVE team's name GLOWS with a breathing pulse (the turn holder
    // must pop out of the strip). A timer advances the phase; greenEmboss
    // reads m_glowPhase. Repaints only while a match is live (a team name
    // is set) — otherwise the timer stays stopped and the idle header costs
    // nothing.
    QTimer *m_glowTimer = nullptr;
    qreal m_glowPhase = 0.0;
    void ensureGlowTimer();
    void glowTick();
};
