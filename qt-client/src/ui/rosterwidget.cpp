// =============================================================================
// RosterWidget — implementation
// =============================================================================
#include "ui/rosterwidget.h"

#include <QFont>

RosterWidget::RosterWidget(QWidget *parent)
    : QListWidget(parent)
{
    QFont mono(QStringLiteral("Monospace"));
    mono.setStyleHint(QFont::TypeWriter);
    setFont(mono);
    setSelectionMode(QAbstractItemView::SingleSelection);

    addItem(QStringLiteral("(no team data — connect to a game)"));
    connect(this, &QListWidget::itemClicked, this, &RosterWidget::onItemClicked);
}

void RosterWidget::setTeamName(const QString &name)
{
    setWindowTitle(name);
}

void RosterWidget::setTeamColor(const QColor &color)
{
    setStyleSheet(QString("QListWidget { color: %1; }").arg(color.name()));
}

void RosterWidget::clearRoster()
{
    clear();
}

void RosterWidget::addPlayer(int number, const QString &name, const QString &position,
                             int strength, int agility, int movement, int armour,
                             const QString &skills, const QString &status)
{
    const QString numberStr = number > 0 ? QStringLiteral("%1  ").arg(number) : QStringLiteral("  ");
    const QString statusStr = status.isEmpty() ? QString()
                                               : QStringLiteral("(%1) ").arg(status);
    const QString line = QStringLiteral("%1%2  %3-22s  S:%4 A:%5 M:%6 P:%7  [%8]")
                             .arg(numberStr)
                             .arg(statusStr)
                             .arg(position)
                             .arg(strength)
                             .arg(agility)
                             .arg(movement)
                             .arg(armour)
                             .arg(skills);
    auto *item = new QListWidgetItem(line, this);
    item->setData(Qt::UserRole, number);
    if (!status.isEmpty() && status != QStringLiteral("active")) {
        item->setForeground(QColor(0xc4, 0x6a, 0x5a));
    }
}

void RosterWidget::onItemClicked(QListWidgetItem *item)
{
    const int number = item->data(Qt::UserRole).toInt();
    emit playerSelected(number);
}
