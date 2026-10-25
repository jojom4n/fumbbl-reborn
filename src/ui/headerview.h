// =============================================================================
// HeaderView — Mockup F match header (custom-painted strip, 62px tall)
//
//   [crest]  HOME NAME (gold, embossed when on turn)  o o o
//            coach · race        [END TURN]
//                    H : A
//            [TURN n] [HALF]   m:ss   (sun|rain) Weather
//   END TURN  coach · race    o o o   AWAY NAME  [crest]
//
// Design rules (Mockup F, 23/ott):
//   * the team ON TURN has its name in gold, raised (embossed) — no dots
//     and no borders; the other name stays plain ink
//   * rerolls are dots: 3 slots, filled = remaining
//   * the turn timer COUNTS UP (FUMBBL semantics) — no decreasing bar
//   * END TURN sits next to the ACTIVE human coach (either side), clearly
//     below the reroll dots
//   * red is reserved for injury/failure: accents come from the team colors
// =============================================================================
#pragma once

#include <QPixmap>
#include <QWidget>

class QPushButton;
class QResizeEvent;

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
    void setRerolls(int home, int away);
    void setLogos(const QPixmap &home, const QPixmap &away);
    void setWeather(const QString &weather); // display name ("Nice Weather")

    // State accessors (used by the UI wiring tests).
    QString teamHomeName() const { return m_home; }
    QString teamAwayName() const { return m_away; }
    int scoreHome() const { return m_scoreHome; }
    int scoreAway() const { return m_scoreAway; }
    int turnNumber() const { return m_turn; }
    int halfNumber() const { return m_half; }
    int elapsedSeconds() const { return m_elapsedSeconds; }
    bool homePlaying() const { return m_homePlaying; }
    int rerollsHome() const { return m_rerollsHome; }
    int rerollsAway() const { return m_rerollsAway; }

    QSize sizeHint() const override { return QSize(800, 62); }
    int fixedHeight() const { return 62; }

signals:
    // The spectator asked to end the turn (wired to the session later).
    void endTurnRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void layoutEndTurn();
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
    int m_rerollsHome = 0;
    int m_rerollsAway = 0;
    QPixmap m_homeLogo;
    QPixmap m_awayLogo;
    QString m_weather;

    QPushButton *m_endTurn = nullptr;
};
