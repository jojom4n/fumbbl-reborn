// =============================================================================
// PlayerInfoWidget — implementation (Mockup F light card)
// =============================================================================
#include "ui/playerinfowidget.h"

#include "core/assets.h"

#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#include <QResizeEvent>

namespace {
const QColor INK(0x22, 0x28, 0x32);
const QColor INK2(0x68, 0x72, 0x80);
const QColor CHIP_GRAY(0x9a, 0xa0, 0xa8);
const int CHIP_H = 22;   // chip height
const int CHIP_PAD = 9;  // horizontal padding inside a chip
const int CHIP_GAP = 5;  // gap between chips
} // namespace

// =============================================================================
// SkillChipsWidget — wrapping row of outlined chips
// =============================================================================
SkillChipsWidget::SkillChipsWidget(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

// Chip widths for the current text.
static QVector<int> chipWidths(const QVector<SkillChipsWidget::Chip> &chips,
                               const QFontMetrics &fm)
{
    QVector<int> ws;
    for (const SkillChipsWidget::Chip &c : chips)
        ws.append(fm.horizontalAdvance(c.text) + 2 * CHIP_PAD);
    return ws;
}

// Lay the chips out in centered rows; returns one rect per chip.
static QVector<QRectF> chipLayout(const QVector<int> &ws, int width)
{
    QVector<QRectF> out;
    const int n = ws.size();
    int i = 0;
    int y = 0;
    while (i < n) {
        // Find how many chips fit on this row.
        int j = i, x = 0;
        while (j < n) {
            const int next = x + ws[j] + (x > 0 ? CHIP_GAP : 0);
            if (x > 0 && next > width)
                break;
            x = next;
            ++j;
        }
        const int rowW = x - CHIP_GAP; // drop the trailing gap
        int cx = (width - rowW) / 2;
        for (int k = i; k < j; ++k) {
            out.append(QRectF(cx, y, ws[k], CHIP_H));
            cx += ws[k] + CHIP_GAP;
        }
        i = j;
        y += CHIP_H + CHIP_GAP;
    }
    return out;
}


void SkillChipsWidget::setChips(const QVector<Chip> &chips, const QColor &accent)
{
    m_chips = chips;
    m_accent = accent;
    updateGeometry();
    update();
}

// The wrapped height depends on the width — tell the layout so it re-sizes
// the widget whenever the card is resized (otherwise extra rows clip).
int SkillChipsWidget::heightForWidth(int w) const
{
    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(10);
    const QFontMetrics fm(f);
    int bottom = CHIP_H + CHIP_GAP;
    for (const QRectF &r : chipLayout(chipWidths(m_chips, fm), qMax(w, 100)))
        bottom = qMax(bottom, int(r.bottom()));
    return bottom;
}

void SkillChipsWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateGeometry();
}

QSize SkillChipsWidget::sizeHint() const
{
    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(10);
    const QFontMetrics fm(f);
    const int w = qMax(width(), 100);
    int bottom = CHIP_H + CHIP_GAP; // one row (or the "No skills" line)
    for (const QRectF &r : chipLayout(chipWidths(m_chips, fm), w))
        bottom = qMax(bottom, int(r.bottom()));
    return QSize(w, bottom);
}

void SkillChipsWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_chips.isEmpty()) {
        p.setPen(CHIP_GRAY);
        p.setFont(font());
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("No skills"));
        return;
    }

    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(10);
    p.setFont(f);
    const QFontMetrics fm(f);
    const auto rects = chipLayout(chipWidths(m_chips, fm), width());

    for (int i = 0; i < m_chips.size(); ++i) {
        const QRectF r = rects[i];
        const bool used = m_chips[i].used;
        const QColor border = used ? QColor(0xc8, 0xcc, 0xd4) : m_accent;
        const QColor text = used ? CHIP_GRAY : INK;
        p.setPen(QPen(border, 1.2));
        p.setBrush(QColor(255, 255, 255));
        p.drawRoundedRect(r, CHIP_H / 2.0, CHIP_H / 2.0);
        p.setPen(text);
        p.drawText(r.adjusted(0, 1, 0, -1), Qt::AlignCenter, m_chips[i].text);
    }
}

// =============================================================================
// PlayerInfoWidget
// =============================================================================
QColor PlayerInfoWidget::teamColor(const QString &teamName, bool home)
{
    const uint h = qHash(teamName.toUtf8());
    const int hue = int((h % 200) + (home ? 0 : 100)) % 360;
    return QColor::fromHsv(hue, 150, 130);
}

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

    // --- Content (the card) --------------------------------------------------
    m_content = new QWidget(this);
    m_content->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *h = new QHBoxLayout(m_content);
    h->setContentsMargins(0, 0, 8, 8);
    h->setSpacing(8);

    // Accent strip (side color)
    m_accentBar = new QLabel(m_content);
    m_accentBar->setFixedWidth(4);
    m_accentBar->setStyleSheet(QStringLiteral("background:#4a7cba;"));

    auto *v = new QVBoxLayout;
    v->setContentsMargins(4, 2, 0, 0);
    v->setSpacing(6);

    // Portrait: circular face inside a team ring, centered.
    m_portrait = new QLabel(m_content);
    m_portrait->setFixedSize(72, 72);
    m_portrait->setAlignment(Qt::AlignCenter);
    v->addWidget(m_portrait, 0, Qt::AlignHCenter);

    m_nameLabel = new QLabel(m_content);
    m_nameLabel->setAlignment(Qt::AlignHCenter);
    QFont nFont;
    nFont.setBold(true);
    nFont.setPointSize(13);
    m_nameLabel->setFont(nFont);
    m_nameLabel->setStyleSheet(QStringLiteral("color:#222832;"));
    v->addWidget(m_nameLabel);

    m_subLabel = new QLabel(m_content);
    m_subLabel->setAlignment(Qt::AlignHCenter);
    m_subLabel->setStyleSheet(QStringLiteral("color:#687280;"));
    v->addWidget(m_subLabel);

    m_teamLabel = new QLabel(m_content);
    m_teamLabel->setAlignment(Qt::AlignHCenter);
    m_teamLabel->setStyleSheet(QStringLiteral("color:#687280;"));
    v->addWidget(m_teamLabel);

    m_divider = new QFrame(m_content);
    m_divider->setFrameShape(QFrame::HLine);
    m_divider->setStyleSheet(QStringLiteral("color:#d5d5dd; background:#d5d5dd;"));
    v->addWidget(m_divider);

    // Stats row
    auto *stats = new QGridLayout;
    stats->setSpacing(4);
    auto makeStat = [this](const QString &label, int value, QGridLayout *grid,
                           int col) -> QLabel * {
        auto *cell = new QLabel;
        cell->setAlignment(Qt::AlignCenter);
        cell->setText(QStringLiteral(
            "<span style=\"font-size:9px;color:#687280;letter-spacing:1px;\">%1</span>"
            "<br><span style=\"font-size:17px;font-weight:800;color:#222832;\">%2</span>")
            .arg(label)
            .arg(value));
        cell->setTextFormat(Qt::RichText);
        grid->addWidget(cell, 0, col);
        return cell;
    };
    m_statMa = makeStat(QStringLiteral("MA"), 0, stats, 0);
    m_statSt = makeStat(QStringLiteral("ST"), 0, stats, 1);
    m_statAg = makeStat(QStringLiteral("AG"), 0, stats, 2);
    m_statPa = makeStat(QStringLiteral("PA"), 0, stats, 3);
    m_statAv = makeStat(QStringLiteral("AV"), 0, stats, 4);
    v->addLayout(stats);

    // Skills header + chips
    m_skillsHeader = new QLabel(QStringLiteral("SKILLS"), m_content);
    m_skillsHeader->setAlignment(Qt::AlignHCenter);
    m_skillsHeader->setStyleSheet(
        QStringLiteral("color:#687280; font-size:9px; letter-spacing:1px;"));
    v->addWidget(m_skillsHeader);

    m_skillChips = new SkillChipsWidget(m_content);
    v->addWidget(m_skillChips); // stretched: chips wrap on the card width

    // Active inducement cards (rare) — a compact centered line.
    m_cardsLabel = new QLabel(m_content);
    m_cardsLabel->setAlignment(Qt::AlignHCenter);
    m_cardsLabel->setWordWrap(true);
    m_cardsLabel->setStyleSheet(QStringLiteral("color:#8a6d00; font-size:9px;"));
    m_cardsLabel->setVisible(false);
    v->addWidget(m_cardsLabel);

    // Player state (STANDING / MOVING / PRONE / STUNNED), colored, centered.
    m_stateLabel = new QLabel(m_content);
    m_stateLabel->setAlignment(Qt::AlignHCenter);
    m_stateLabel->setWordWrap(true);
    QFont sFont;
    sFont.setBold(true);
    sFont.setPointSize(12);
    m_stateLabel->setFont(sFont);
    m_stateLabel->setStyleSheet(QStringLiteral("color:#3d8f5f;"));
    v->addWidget(m_stateLabel);

    // Condition flags (Confused / Rooted / ...) — small, centered.
    m_flagsLabel = new QLabel(m_content);
    m_flagsLabel->setAlignment(Qt::AlignHCenter);
    m_flagsLabel->setWordWrap(true);
    m_flagsLabel->setStyleSheet(QStringLiteral("color:#687280; font-size:9px;"));
    m_flagsLabel->setVisible(false);
    v->addWidget(m_flagsLabel);

    // Injuries — red, bottom.
    m_injuryLabel = new QLabel(m_content);
    m_injuryLabel->setWordWrap(true);
    m_injuryLabel->setAlignment(Qt::AlignHCenter);
    m_injuryLabel->setStyleSheet(QStringLiteral("color:#c0392b;"));
    m_injuryLabel->setVisible(false);
    v->addWidget(m_injuryLabel);

    v->addStretch(1);

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

    const QColor accent = teamColor(info.teamName, info.sideHome);
    setAccentColor(accent);

    // Portrait: the piece's face clipped to a circle inside a team-colored
    // ring (team color is applied at runtime, never baked into the asset).
    const QPixmap face = assets::playerFace(info.position, info.race);
    {
        QPixmap canvas(72, 72);
        canvas.fill(Qt::transparent);
        {
            QPainter pr(&canvas);
            pr.setRenderHint(QPainter::Antialiasing);
            // Clip the face to the circle.
            QPainterPath clip;
            clip.addEllipse(QRectF(5, 5, 62, 62));
            pr.setClipPath(clip);
            if (!face.isNull())
                pr.drawPixmap(QRectF(5, 5, 62, 62), face,
                              QRectF(0, 0, face.width(), face.height()));
            else
                pr.fillRect(QRectF(5, 5, 62, 62), QColor(0x2a, 0x2a, 0x34));
        }
        {
            QPainter pr(&canvas);
            pr.setRenderHint(QPainter::Antialiasing);
            pr.setPen(QPen(accent, 3));
            pr.setBrush(Qt::NoBrush);
            pr.drawEllipse(QRectF(3.5, 3.5, 65, 65));
        }
        m_portrait->setPixmap(canvas);
    }

    m_nameLabel->setText(info.name.toUpper());

    // "#4 · Wood Elf Catcher" (number · race position). The position name
    // from the model may already carry the race prefix — don't double it.
    QString sub;
    if (info.number > 0)
        sub = QStringLiteral("#%1").arg(info.number);
    QString pos = info.position;
    if (!info.race.isEmpty() && !pos.isEmpty() &&
        !pos.startsWith(info.race, Qt::CaseInsensitive))
        pos = info.race + QLatin1Char(' ') + pos;
    if (!pos.isEmpty())
        sub += (sub.isEmpty() ? QString() : QStringLiteral(" · ")) + pos;
    if (sub.isEmpty())
        sub = info.positionKeywords.join(QStringLiteral(", "));
    m_subLabel->setText(sub);
    m_teamLabel->setText(info.teamName);

    // Stats (label on top, big bold value below).
    auto setStat = [](QLabel *cell, const QString &label, int value) {
        cell->setText(QStringLiteral(
            "<span style=\"font-size:9px;color:#687280;letter-spacing:1px;\">%1</span>"
            "<br><span style=\"font-size:17px;font-weight:800;color:#222832;\">%2</span>")
            .arg(label)
            .arg(value));
    };
    setStat(m_statMa, QStringLiteral("MA"), info.ma);
    setStat(m_statSt, QStringLiteral("ST"), info.st);
    setStat(m_statAg, QStringLiteral("AG"), info.ag);
    setStat(m_statPa, QStringLiteral("PA"), info.pa);
    setStat(m_statAv, QStringLiteral("AV"), info.av);

    // Skills — outlined chips (used-this-turn chips dimmed).
    QVector<SkillChipsWidget::Chip> chips;
    for (const PlayerInfoData::Skill &s : info.skills) {
        SkillChipsWidget::Chip c;
        c.text = s.name.toUpper(); // mockup shows uppercase chips
        if (!s.value.isEmpty())
            c.text += QStringLiteral(" %1").arg(s.value);
        c.used = s.used;
        chips.append(c);
    }
    m_skillChips->setChips(chips, accent);

    // Active inducement cards (rare) — a compact line under the skills.
    if (!info.cards.isEmpty()) {
        m_cardsLabel->setText(QStringLiteral("CARDS: %1")
                                  .arg(info.cards.join(QStringLiteral(", "))));
        m_cardsLabel->setVisible(true);
    } else {
        m_cardsLabel->setVisible(false);
    }

    // Player state — state only (no Active/ACTING chip, Mockup F round 6).
    // Colored: green for standing/moving, red for prone, amber for stunned,
    // gray for players in the box.
    QString state = info.stateText;
    if (state.isEmpty() && !info.status.isEmpty())
        state = info.status;
    QString stateColor = QStringLiteral("#3d8f5f"); // green (standing/moving)
    if (info.stateText.contains(QLatin1String("Prone"), Qt::CaseInsensitive))
        stateColor = QStringLiteral("#c0392b"); // red
    else if (info.stateText.contains(QLatin1String("Stun"), Qt::CaseInsensitive))
        stateColor = QStringLiteral("#c07f1f"); // amber
    else if (info.stateText.contains(QLatin1String("box"), Qt::CaseInsensitive))
        stateColor = QStringLiteral("#687280"); // gray (in the box)
    m_stateLabel->setText(state.toUpper());
    m_stateLabel->setStyleSheet(
        QStringLiteral("color:%1; font-weight:700;").arg(stateColor));

    // Condition flags on their own line when present.
    if (!info.stateFlags.isEmpty()) {
        m_flagsLabel->setText(info.stateFlags.join(QStringLiteral(" · ")));
        m_flagsLabel->setVisible(true);
    } else {
        m_flagsLabel->setVisible(false);
    }

    // Injuries
    if (!info.injuries.isEmpty()) {
        m_injuryLabel->setText(QStringLiteral("Injuries: %1")
                                   .arg(info.injuries.join(QStringLiteral(", "))));
        m_injuryLabel->setVisible(true);
    } else {
        m_injuryLabel->setVisible(false);
    }
}
