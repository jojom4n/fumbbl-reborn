// =============================================================================
// HeaderBar — implementation
// =============================================================================
#include "ui/headerbar.h"

#include <QLabel>

namespace {
QString headerStyle()
{
    return QStringLiteral("QLabel { font-size: 14px; font-weight: 600; padding: 2px 6px; }");
}
} // namespace

HeaderBar::HeaderBar(QWidget *parent)
    : QToolBar(parent)
{
    setMovable(true);
    setFloatable(true);
    setAllowedAreas(Qt::TopToolBarArea);

    auto makeLabel = [this](QLabel **out) {
        auto *label = new QLabel(this);
        label->setStyleSheet(headerStyle());
        addWidget(label);
        *out = label;
        return label;
    };

    m_teamsLabel = makeLabel(&m_teamsLabel);
    m_teamsLabel->setText(QStringLiteral("Team 1  vs  Team 2"));

    m_scoreLabel = makeLabel(&m_scoreLabel);
    m_scoreLabel->setText(QStringLiteral("0 : 0"));

    m_turnLabel = makeLabel(&m_turnLabel);
    m_turnLabel->setText(QStringLiteral("Turn 1"));

    m_timerLabel = makeLabel(&m_timerLabel);
    m_timerLabel->setText(QStringLiteral("30s"));

    m_spectatorsLabel = makeLabel(&m_spectatorsLabel);
    m_spectatorsLabel->setText(QStringLiteral("Spectators: 0"));
}

void HeaderBar::setTeams(const QString &home, const QString &away)
{
    m_teamsLabel->setText(QStringLiteral("%1  vs  %2").arg(home, away));
}

void HeaderBar::setScore(int home, int away)
{
    m_scoreLabel->setText(QStringLiteral("%1 : %2").arg(home).arg(away));
}

void HeaderBar::setTurn(int turn)
{
    m_turnLabel->setText(QStringLiteral("Turn %1").arg(turn));
}

void HeaderBar::setTimerSeconds(int remaining)
{
    m_timerLabel->setText(QStringLiteral("%1s").arg(remaining));
}

void HeaderBar::setSpectators(int count)
{
    m_spectatorsLabel->setText(QStringLiteral("Spectators: %1").arg(count));
}
