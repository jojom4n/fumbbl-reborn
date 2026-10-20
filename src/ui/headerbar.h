// =============================================================================
// HeaderBar — top toolbar with match summary (teams, score, turn, timer…)
// A movable QToolBar: can be repositioned/detached like any Office toolbar.
// =============================================================================
#pragma once

#include <QToolBar>

class QLabel;

class HeaderBar : public QToolBar
{
    Q_OBJECT

public:
    explicit HeaderBar(QWidget *parent = nullptr);

    void setTeams(const QString &home, const QString &away);
    void setScore(int home, int away);
    void setTurn(int turn);
    void setTimerSeconds(int remaining);
    void setSpectators(int count);

private:
    QLabel *m_teamsLabel = nullptr;
    QLabel *m_scoreLabel = nullptr;
    QLabel *m_turnLabel = nullptr;
    QLabel *m_timerLabel = nullptr;
    QLabel *m_spectatorsLabel = nullptr;
};
