// =============================================================================
// RosterWidget — team roster panel (all roster players + status)
// =============================================================================
#pragma once

#include <QListWidget>

// Row layout: "  #  Name        ST AG MA PA  status  [skills]"
class RosterWidget : public QListWidget
{
    Q_OBJECT

public:
    explicit RosterWidget(QWidget *parent = nullptr);

    void setTeamName(const QString &name);
    void setTeamColor(const QColor &color);
    void clearRoster();
    void addPlayer(int number, const QString &name, const QString &position,
                   int strength, int agility, int movement, int armour,
                   const QString &skills, const QString &status);

signals:
    void playerSelected(int number);

private slots:
    void onItemClicked(QListWidgetItem *item);
};
