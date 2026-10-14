// =============================================================================
// PlayerInfoWidget — implementation
// =============================================================================
#include "ui/playerinfowidget.h"

#include "core/assets.h"

#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

namespace {
// Team accent colors (match the roster panels / field token colors).
const QColor kHomeAccent(0x7c, 0xa8, 0x6f);
const QColor kAwayAccent(0xd8, 0xc0, 0x7a);

QString escapeHtml(const QString &s)
{
    QString out = s;
    out.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    out.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    out.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    return out;
}

// One stat cell: small uppercase label on top, bold value below.
QLabel *makeStatCell(const QString &label, int value, QGridLayout *grid, int row, int col)
{
    auto *cell = new QLabel;
    cell->setAlignment(Qt::AlignCenter);
    QFont vFont;
    vFont.setBold(true);
    vFont.setPointSize(12);
    cell->setFont(vFont);
    cell->setText(QStringLiteral("%1<br><span style=\"font-size:9px;color:#999;\">%2</span>")
                      .arg(value)
                      .arg(label));
    cell->setTextFormat(Qt::RichText);
    grid->addWidget(cell, row, col);
    return cell;
}
} // namespace

PlayerInfoWidget::PlayerInfoWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- Placeholder ---------------------------------------------------------
    m_placeholder = new QLabel(this);
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setTextFormat(Qt::RichText);
    m_placeholder->setText(QStringLiteral(
        "<div style=\"color:#888;\">"
        "<div style=\"font-size:28px;\">&#128100;</div>"
        "<div>Select a player on the field<br>or in a roster</div>"
        "</div>"));
    m_placeholder->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    root->addWidget(m_placeholder);

    // --- Content -------------------------------------------------------------
    m_content = new QWidget(this);
    auto *h = new QHBoxLayout(m_content);
    h->setContentsMargins(0, 0, 8, 8);
    h->setSpacing(8);

    // Accent strip (side color)
    m_accentBar = new QLabel(m_content);
    m_accentBar->setFixedWidth(4);
    m_accentBar->setStyleSheet(QStringLiteral("background:#7ca86f;"));

    auto *v = new QVBoxLayout;
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);

    // Header: bust portrait + name/subtitle
    m_portrait = new QLabel(m_content);
    m_portrait->setFixedSize(52, 52);
    m_portrait->setAlignment(Qt::AlignCenter);
    m_portrait->setStyleSheet(QStringLiteral(
        "border: 1px solid #3a3a46; border-radius: 4px; background: #22222c;"));

    m_nameLabel = new QLabel(m_content);
    QFont nFont;
    nFont.setBold(true);
    nFont.setPointSize(13);
    m_nameLabel->setFont(nFont);

    m_subLabel = new QLabel(m_content);
    m_subLabel->setStyleSheet(QStringLiteral("color:#aaa;"));

    auto *header = new QHBoxLayout;
    header->setSpacing(8);
    header->addWidget(m_portrait, 0, Qt::AlignTop);
    auto *nameCol = new QVBoxLayout;
    nameCol->setSpacing(2);
    nameCol->addWidget(m_nameLabel);
    nameCol->addWidget(m_subLabel);
    header->addLayout(nameCol, 1);
    v->addLayout(header);

    m_stateLabel = new QLabel(m_content);
    m_stateLabel->setWordWrap(true);
    m_stateLabel->setStyleSheet(QStringLiteral("color:#ddd;"));
    v->addWidget(m_stateLabel);

    // Stats grid
    auto *stats = new QGridLayout;
    stats->setSpacing(4);
    m_statMa = makeStatCell(QStringLiteral("MA"), 0, stats, 0, 0);
    m_statSt = makeStatCell(QStringLiteral("ST"), 0, stats, 0, 1);
    m_statAg = makeStatCell(QStringLiteral("AG"), 0, stats, 0, 2);
    m_statPa = makeStatCell(QStringLiteral("PA"), 0, stats, 0, 3);
    m_statAv = makeStatCell(QStringLiteral("AV"), 0, stats, 0, 4);
    v->addLayout(stats);

    // Skills
    m_skillList = new QListWidget(m_content);
    m_skillList->setFrameShape(QFrame::NoFrame);
    m_skillList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_skillList->setIconSize(QSize(20, 20));
    v->addWidget(m_skillList, 1);

    // Injuries
    m_injuryLabel = new QLabel(m_content);
    m_injuryLabel->setWordWrap(true);
    m_injuryLabel->setStyleSheet(QStringLiteral("color:#e07070;"));
    m_injuryLabel->setVisible(false);
    v->addWidget(m_injuryLabel);

    h->addWidget(m_accentBar);
    h->addLayout(v, 1);

    root->addWidget(m_content);

    showPlaceholder();
}

void PlayerInfoWidget::setAccentColor(const QColor &color)
{
    m_accentBar->setStyleSheet(QStringLiteral("background:%1;").arg(color.name()));
}

void PlayerInfoWidget::showPlaceholder()
{
    m_shownPlayerId.clear();
    m_portrait->clear();
    m_placeholder->setVisible(true);
    m_content->setVisible(false);
}

void PlayerInfoWidget::showPlayer(const PlayerInfoData &info)
{
    if (!info.isValid()) {
        showPlaceholder();
        return;
    }
    m_shownPlayerId = info.playerId;
    m_placeholder->setVisible(false);
    m_content->setVisible(true);

    setAccentColor(info.sideHome ? kHomeAccent : kAwayAccent);

    // Header
    const QPixmap bust = assets::playerBust(info.archetype, info.sideHome);
    if (!bust.isNull())
        m_portrait->setPixmap(bust.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_nameLabel->setText(info.name);
    QString sub;
    if (info.number > 0)
        sub += QStringLiteral("#%1").arg(info.number);
    if (!info.position.isEmpty())
        sub += (sub.isEmpty() ? QString() : QStringLiteral(" · ")) + info.position;
    if (!info.positionKeywords.isEmpty())
        sub += QStringLiteral(" (%1)").arg(info.positionKeywords.join(QStringLiteral(", ")));
    sub += QStringLiteral(" · %1").arg(info.sideHome ? QStringLiteral("Home")
                                                     : QStringLiteral("Away"));
    if (!info.race.isEmpty())
        sub += QStringLiteral(" · %1").arg(info.race);
    m_subLabel->setText(sub);

    // State line
    QStringList parts;
    if (!info.status.isEmpty())
        parts << info.status;
    if (!info.stateText.isEmpty())
        parts << info.stateText;
    if (!info.stateFlags.isEmpty())
        parts << info.stateFlags.join(QStringLiteral(", "));
    m_stateLabel->setText(parts.join(QStringLiteral(" — ")));
    if (info.isActing) {
        m_stateLabel->setText(QStringLiteral("&#9889; ") + m_stateLabel->text());
        m_stateLabel->setTextFormat(Qt::RichText);
    } else {
        m_stateLabel->setTextFormat(Qt::PlainText);
    }

    // Stats
    m_statMa->setText(QStringLiteral("%1<br><span style=\"font-size:9px;color:#999;\">MA</span>")
                          .arg(info.ma));
    m_statSt->setText(QStringLiteral("%1<br><span style=\"font-size:9px;color:#999;\">ST</span>")
                          .arg(info.st));
    m_statAg->setText(QStringLiteral("%1<br><span style=\"font-size:9px;color:#999;\">AG</span>")
                          .arg(info.ag));
    m_statPa->setText(QStringLiteral("%1<br><span style=\"font-size:9px;color:#999;\">PA</span>")
                          .arg(info.pa));
    m_statAv->setText(QStringLiteral("%1<br><span style=\"font-size:9px;color:#999;\">AV</span>")
                          .arg(info.av));

    // Skills (with icons)
    m_skillList->clear();
    for (const PlayerInfoData::Skill &s : info.skills) {
        auto *item = new QListWidgetItem(m_skillList);
        QString text = s.name;
        if (!s.value.isEmpty())
            text += QStringLiteral(" (%1)").arg(s.value);
        item->setText(text);
        item->setIcon(QIcon(assets::skillPixmap(s.name)));
        item->setToolTip(s.used ? QStringLiteral("Used this turn")
                                : QStringLiteral("Available"));
        if (s.used) {
            item->setForeground(QColor(0x77, 0x77, 0x77));
        }
    }
    if (info.skills.isEmpty()) {
        auto *item = new QListWidgetItem(m_skillList);
        item->setText(QStringLiteral("No skills"));
        item->setForeground(QColor(0x77, 0x77, 0x77));
    }

    // Active inducement cards (with icons)
    if (!info.cards.isEmpty()) {
        auto *header = new QListWidgetItem(QStringLiteral("In-play cards"), m_skillList);
        QFont hf;
        hf.setBold(true);
        header->setFont(hf);
        header->setFlags(Qt::NoItemFlags);
        for (const QString &cardName : info.cards) {
            auto *item = new QListWidgetItem(m_skillList);
            item->setText(cardName);
            item->setIcon(QIcon(assets::cardPixmap(cardName)));
            item->setToolTip(QStringLiteral("Active inducement card"));
        }
    }

    // Injuries
    if (!info.injuries.isEmpty()) {
        m_injuryLabel->setText(QStringLiteral("Injuries: %1").arg(info.injuries.join(QStringLiteral(", "))));
        m_injuryLabel->setVisible(true);
    } else {
        m_injuryLabel->setVisible(false);
    }
}
