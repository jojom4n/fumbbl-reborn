// =============================================================================
// PlayerInfoWidget — details of the selected player
//
// Structured panel (not a plain text box): header with name/number/position,
// current state, stats grid (MA/ST/AG/PA/AV), skills list (used-this-turn
// skills dimmed), injuries. Filled by MainWindow from the GameModel.
// =============================================================================
#pragma once

#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QListWidget;

// Everything the panel needs about one player (assembled by the caller from
// GameModel data, so the widget stays decoupled from the model).
struct PlayerInfoData {
    QString playerId;
    QString name;
    int number = 0;
    QString position;        // full position name ("Werewolf")
    QString positionShorthand; // "W"
    QStringList positionKeywords; // "Blitzer, Undead, ..."
    QString race;            // team race
    QString teamName;
    bool sideHome = true;

    // Current situation
    QString status;          // "active" / ... (playerStatus)
    QString stateText;       // "Standing", "Prone", "in the box (KO)", ...
    QStringList stateFlags;  // "Confused", "Rooted", "Chomped", ...
    int ma = 0, st = 0, ag = 0, pa = 0, av = 0;

    struct Skill {
        QString name;
        QString value; // e.g. Loner "4" (empty = none)
        bool used = false; // usedSkills (this turn)
    };
    QVector<Skill> skills;
    QStringList injuries;  // lasting injuries + recovering injury
    QStringList cards;     // active inducement card names
    int archetype = 0;     // bust icon (0 blocker .. 4 thrower)
    bool isActing = false; // holds the current turn

    bool isValid() const { return !playerId.isEmpty() && !name.isEmpty(); }
};

class PlayerInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlayerInfoWidget(QWidget *parent = nullptr);

    void showPlayer(const PlayerInfoData &info);
    void showPlaceholder();

    // Id of the player currently shown ("" when showing the placeholder).
    QString shownPlayerId() const { return m_shownPlayerId; }

private:
    void setAccentColor(const QColor &color);

    QLabel *m_placeholder = nullptr; // shown when no player is selected
    QWidget *m_content = nullptr;    // the structured panel

    QLabel *m_accentBar = nullptr;   // thin side-color strip
    QLabel *m_portrait = nullptr;    // bust icon
    QLabel *m_nameLabel = nullptr;
    QLabel *m_subLabel = nullptr;    // #nr · position (keywords)
    QLabel *m_stateLabel = nullptr;  // status + field state + flags + acting
    QLabel *m_statMa = nullptr, *m_statSt = nullptr, *m_statAg = nullptr,
        *m_statPa = nullptr, *m_statAv = nullptr;
    QListWidget *m_skillList = nullptr;
    QLabel *m_injuryLabel = nullptr;

    QString m_shownPlayerId;
};
