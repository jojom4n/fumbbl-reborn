// =============================================================================
// PlayerInfoWidget — details of the selected player (Mockup F card)
//
// Light card layout (approved Mockup F):
//   [accent bar]  (team color, full height, left edge)
//         (circular portrait with team ring)
//              PLAYER NAME  (bold, uppercase, centered)
//            #N · Race Position  (centered, secondary)
//                 Team name  (centered, secondary)
//   ───────────────────────────────────────
//     MA    ST    AG    PA    AV   (small labels, big bold values)
//                 SKILLS  (small caps)
//     [+AV]  [BLOCK]  [SIDESTEP]   (outlined chips, accent border;
//                                   used-this-turn chips dimmed)
//
//                     MOVING  (player state, colored, centered)
//
// State only — no "Active"/"ACTING" chip (Mockup F round 6). Red is reserved
// for injury/failure. Filled by MainWindow from the GameModel.
// =============================================================================
#pragma once

#include <QColor>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QFrame;
class QLabel;

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
    bool isActing = false; // holds the current turn (field wheel, not card)

    bool isValid() const { return !playerId.isEmpty() && !name.isEmpty(); }
};

// A wrapping row of outlined chips (Mockup F skill chips). Qt rich text does
// not support display:inline-block, so the chips are painted by hand.
class SkillChipsWidget : public QWidget
{
    Q_OBJECT

public:
    struct Chip {
        QString text;
        bool used = false;
    };
    explicit SkillChipsWidget(QWidget *parent = nullptr);
    void setChips(const QVector<Chip> &chips, const QColor &accent);
    QSize sizeHint() const override;
    int heightForWidth(int w) const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QVector<Chip> m_chips;
    QColor m_accent{0x4a, 0x7c, 0xba};
};

class PlayerInfoWidget : public QWidget
{
    Q_OBJECT

public:
    // Deterministic team color (hashed from the team name, shifted for
    // home/away so both sides stay readable). Faces carry NO team color —
    // the UI draws the colored ring around them at runtime.
    static QColor teamColor(const QString &teamName, bool home);

    explicit PlayerInfoWidget(QWidget *parent = nullptr);

    void showPlayer(const PlayerInfoData &info);
    void showPlaceholder();

    // Id of the player currently shown ("" when showing the placeholder).
    QString shownPlayerId() const { return m_shownPlayerId; }
    // Displayed name (for tests; upper-case as shown on the card).
    QString shownName() const;

private:
    void setAccentColor(const QColor &color);

    QLabel *m_placeholder = nullptr; // shown when no player is selected
    QWidget *m_content = nullptr;    // the structured card

    QLabel *m_accentBar = nullptr;   // team-color strip (left edge)
    QLabel *m_portrait = nullptr;    // circular face inside a team ring
    QLabel *m_nameLabel = nullptr;
    QLabel *m_subLabel = nullptr;    // #nr · Race Position
    QLabel *m_teamLabel = nullptr;   // team name
    QFrame *m_divider = nullptr;

    QLabel *m_statMa = nullptr, *m_statSt = nullptr, *m_statAg = nullptr,
        *m_statPa = nullptr, *m_statAv = nullptr;
    QLabel *m_skillsHeader = nullptr; // "SKILLS" small caps
    SkillChipsWidget *m_skillChips = nullptr; // wrapping outlined chips
    QLabel *m_cardsLabel = nullptr;    // active inducement cards (rare)
    QLabel *m_stateLabel = nullptr;    // STANDING / MOVING / PRONE / STUNNED
    QLabel *m_flagsLabel = nullptr;    // Confused / Rooted / ... (small)
    QLabel *m_injuryLabel = nullptr;   // red, bottom

    QString m_shownPlayerId;
};
