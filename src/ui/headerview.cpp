// =============================================================================
// HeaderView — implementation (Mockup F header design)
// =============================================================================
#include "ui/headerview.h"

#include <QFontMetricsF>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPainter>

#include <cmath>
#include <QPushButton>

namespace {

const QColor INK(34, 40, 50);       // primary text
const QColor INK2(104, 114, 128);   // secondary text
const QColor GREEN(56, 142, 60);      // team on turn (round 12: green, was gold)
const QColor GREEN_DARK(27, 94, 32); // emboss shadow

// Team name of the team on turn: green, raised (embossed dark shadow below)
// and GLOWING — a soft wide green halo is stroked around the glyph outline
// before the text itself (round 12: the gold emboss was replaced with a
// glowing green one, user request). yTop is the text TOP (Mockup F
// coordinates); drawText needs the baseline.
void greenEmboss(QPainter &p, int x, int yTop, const QString &s, const QFont &f)
{
    p.setFont(f);
    const int base = yTop + int(QFontMetricsF(f).ascent());

    // Glow: two passes of the glyph outline stroked with a soft green.
    QPainterPath path;
    path.addText(x, base, f, s);
    p.save();
    p.setBrush(Qt::NoBrush);
    QPen halo(QColor(129, 223, 129, 55));
    halo.setWidthF(9.0);
    halo.setJoinStyle(Qt::RoundJoin);
    p.setPen(halo);
    p.drawPath(path);
    QPen halo2(QColor(102, 210, 102, 80));
    halo2.setWidthF(3.5);
    halo2.setJoinStyle(Qt::RoundJoin);
    p.setPen(halo2);
    p.drawPath(path);
    p.restore();

    // Emboss: dark green shadow 2px below, bright green text on top.
    p.setPen(GREEN_DARK);
    p.drawText(x, base + 2, s);
    p.setPen(GREEN);
    p.drawText(x, base, s);
}

// Reroll dot (3 slots per team, filled = remaining).
void coin(QPainter &p, qreal cx, qreal cy, qreal r, bool active)
{
    p.setPen(QPen(active ? QColor(150, 118, 30) : QColor(160, 162, 170), 1));
    p.setBrush(active ? QColor(212, 175, 55) : QColor(196, 198, 204));
    p.drawEllipse(QPointF(cx, cy), r, r);
    if (active) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(240, 216, 130, 200));
        p.drawEllipse(QPointF(cx - r * 0.12, cy - r * 0.22), r * 0.5, r * 0.35);
    }
}

void sunIcon(QPainter &p, qreal x, qreal y, qreal s)
{
    const QPointF c(x + s * 0.5, y + s * 0.5);
    p.setPen(QPen(QColor(190, 140, 40), 1));
    p.setBrush(QColor(240, 196, 80));
    p.drawEllipse(c, s * 0.22, s * 0.22);
    p.setPen(QPen(QColor(240, 196, 80), 2));
    for (int k = 0; k < 8; ++k) {
        const qreal ang = k * 3.14159265 / 4.0;
        p.drawLine(c + QPointF(s * 0.36 * std::cos(ang), s * 0.36 * std::sin(ang)),
                   c + QPointF(s * 0.48 * std::cos(ang), s * 0.48 * std::sin(ang)));
    }
}

void rainIcon(QPainter &p, qreal x, qreal y, qreal s)
{
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(168, 180, 196));
    p.drawEllipse(QRectF(x, y + s * 0.25, s * 0.75, s * 0.6));
    p.drawEllipse(QRectF(x + s * 0.25, y, s * 0.7, s * 0.6));
    p.drawEllipse(QRectF(x + s * 0.55, y + s * 0.25, s * 0.45, s * 0.55));
    p.setPen(QPen(QColor(90, 140, 200), 2));
    for (const qreal dx : { 0.2, 0.5, 0.8 }) {
        const qreal x0 = x + s * dx;
        p.drawLine(QPointF(x0, y + s * 0.9), QPointF(x0 - s * 0.08, y + s * 1.3));
    }
}

} // namespace

HeaderView::HeaderView(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(62);
    setAutoFillBackground(true);
    // Round 8: expand to the full toolbar width so the ZONES land where the
    // mockup puts them (left team / center info / right team) instead of
    // clustering at the left edge.
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void HeaderView::setTeams(const QString &home, const QString &away)
{
    m_home = home;
    m_away = away;
    update();
}

void HeaderView::setRaces(const QString &home, const QString &away)
{
    m_homeRace = home;
    m_awayRace = away;
    update();
}

void HeaderView::setCoaches(const QString &home, const QString &away)
{
    m_homeCoach = home;
    m_awayCoach = away;
    update();
}

void HeaderView::setAccents(const QColor &home, const QColor &away)
{
    m_homeAccent = home;
    m_awayAccent = away;
    update();
}

void HeaderView::setScores(int home, int away)
{
    m_scoreHome = home;
    m_scoreAway = away;
    update();
}

void HeaderView::setTurn(int turn, int half)
{
    m_turn = turn;
    m_half = half;
    update();
}

void HeaderView::setTurnElapsedSeconds(int seconds)
{
    if (m_elapsedSeconds == seconds)
        return;
    m_elapsedSeconds = qMax(0, seconds);
    update();
}

void HeaderView::setHomePlaying(bool homePlaying)
{
    if (m_homePlaying == homePlaying)
        return;
    m_homePlaying = homePlaying;
    update();
}

void HeaderView::setRerolls(int home, int away)
{
    m_rerollsHome = home;
    m_rerollsAway = away;
    update();
}

void HeaderView::setLogos(const QPixmap &home, const QPixmap &away)
{
    m_homeLogo = home;
    m_awayLogo = away;
    update();
}

void HeaderView::setWeather(const QString &weather)
{
    if (m_weather == weather)
        return;
    m_weather = weather;
    update();
}

QString HeaderView::formatElapsed(int totalSeconds)
{
    return QStringLiteral("%1:%2")
        .arg(totalSeconds / 60)
        .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
}

void HeaderView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    // Round 11: smooth pixmap scaling — the 240px crests were drawn at 40px
    // with fast scaling and came out grainy (user report).
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const int W = width();
    const int H = height();

    // base strip + bottom hairline
    p.fillRect(0, 0, W, H, QColor(250, 250, 253));
    p.fillRect(0, H - 1, W, 1, QColor(0, 0, 0, 40));

    // --- crests -------------------------------------------------------------
    if (!m_homeLogo.isNull())
        p.drawPixmap(QRectF(14, 11, 40, 40), m_homeLogo,
                     QRectF(0, 0, m_homeLogo.width(), m_homeLogo.height()));
    if (!m_awayLogo.isNull())
        p.drawPixmap(QRectF(W - 54, 11, 40, 40), m_awayLogo,
                     QRectF(0, 0, m_awayLogo.width(), m_awayLogo.height()));

    // --- names + sublines -----------------------------------------------------
    // NOTE: QPainter::drawText(x, y, ...) takes y as the BASELINE, while the
    // Mockup F coordinates are text TOPS — so every draw adds the ascent.
    // Round 12: team names are BIGGER and bold (user request).
    QFont nameF = font();
    nameF.setPointSizeF(17);
    nameF.setBold(true);
    QFont subF = font();
    subF.setPointSizeF(10);
    const QFontMetricsF fmName(nameF), fmSub(subF);

    const QString subH = m_homeCoach + QStringLiteral("  ·  ") + m_homeRace;
    const QString subA = m_awayCoach + QStringLiteral("  ·  ") + m_awayRace;

    const QString homeName = m_home.toUpper();
    const QString awayName = m_away.toUpper();
    const int nameY = 6 + int(fmName.ascent());
    const int subY = 31 + int(fmSub.ascent());
    if (m_homePlaying) {
        greenEmboss(p, 64, 6, homeName, nameF);
        p.setPen(INK);
        p.drawText(W - 64 - fmName.horizontalAdvance(awayName), nameY, awayName);
    } else {
        p.setPen(INK);
        p.drawText(64, nameY, homeName);
        greenEmboss(p, W - 64 - fmName.horizontalAdvance(awayName), 6, awayName, nameF);
    }
    p.setPen(INK2);
    p.setFont(subF);
    p.drawText(64, subY, subH);
    p.drawText(W - 64 - fmSub.horizontalAdvance(subA), subY, subA);

    // --- reroll dots (3 slots, filled = remaining) ----------------------------
    // Home: 16px right of the home name; away: mirrored, 16px left of the
    // away name (mockup layout).
    const qreal xh = 64 + fmName.horizontalAdvance(homeName) + 16;
    for (int i = 0; i < 3; ++i)
        coin(p, xh + i * 19, 17, 7, i < m_rerollsHome);
    const qreal xa = W - 64 - 3 * 19 - 16 - fmName.horizontalAdvance(awayName);
    for (int i = 0; i < 3; ++i)
        coin(p, xa + i * 19, 17, 7, i < m_rerollsAway);

    // --- center: score ---------------------------------------------------------
    // Round 9: the score/center-row gap was too small — score moved up
    // (font 20, top 3) and the center row down (y 38) in the 62px strip.
    QFont scoreF = font();
    scoreF.setPointSizeF(20);
    scoreF.setWeight(QFont::Black);
    const QString score =
        QString::number(m_scoreHome) + QStringLiteral(" : ") + QString::number(m_scoreAway);
    const QFontMetricsF fmScore(scoreF);
    p.setPen(INK);
    p.setFont(scoreF);
    p.drawText(W / 2 - fmScore.horizontalAdvance(score) / 2, 3 + int(fmScore.ascent()), score);

    // --- center row: [TURN n][HALF]  time  weather ------------------------------
    // The turn timer counts UP (FUMBBL semantics) — no decreasing bar.
    QFont chipF = font();
    chipF.setPointSizeF(10);
    chipF.setBold(true);
    QFont timeF = font();
    timeF.setPointSizeF(12);
    timeF.setWeight(QFont::Black);
    QFont labF = font();
    labF.setPointSizeF(10);

    const QString turnT = QStringLiteral("TURN %1").arg(m_turn);
    const QString halfT = m_half == 1 ? QStringLiteral("1ST HALF")
                          : m_half == 2 ? QStringLiteral("2ND HALF")
                          : QStringLiteral("%1TH HALF").arg(m_half);
    const QString timeT = formatElapsed(m_elapsedSeconds);
    const QFontMetricsF fmChip(chipF), fmTime(timeF), fmLab(labF);
    const qreal w1 = fmChip.horizontalAdvance(turnT) + 14;
    const qreal w2 = fmChip.horizontalAdvance(halfT) + 14;
    const qreal w3 = fmTime.horizontalAdvance(timeT);
    const qreal w4 = 26 + fmLab.horizontalAdvance(m_weather);
    const qreal total = w1 + 8 + w2 + 16 + w3 + 16 + w4;
    const qreal y = 38; // round 9: below the score with a real gap (was 34)
    // Round 10: the TURN / HALF pills were 16px tall and the glyph's descent
    // touched the bottom border. Taller pills (20px) with the text vertically
    // centered leave a real gap below the font.
    const qreal chipH = 20.0;
    const auto chipBase = [y, chipH](const QFontMetricsF &fm) {
        return y + (chipH - (fm.ascent() + fm.descent())) / 2.0 + fm.ascent();
    };
    qreal x = W / 2 - total / 2;

    p.setPen(QPen(m_homeAccent, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(x, y, w1, chipH), 10, 10);
    p.setPen(m_homeAccent);
    p.setFont(chipF);
    p.drawText(x + 7, chipBase(fmChip), turnT);
    x += w1 + 8;
    p.setPen(QPen(INK2, 1));
    p.drawRoundedRect(QRectF(x, y, w2, chipH), 10, 10);
    p.setPen(INK2);
    p.drawText(x + 7, chipBase(fmChip), halfT);
    x += w2 + 16;
    p.setPen(INK);
    p.setFont(timeF);
    p.drawText(x, chipBase(fmTime), timeT);
    x += w3 + 16;
    const qreal iconY = y + (chipH - 18) / 2.0;
    if (m_weather.toLower().contains(QLatin1String("rain")))
        rainIcon(p, x, iconY, 18);
    else
        sunIcon(p, x, iconY, 18);
    p.setPen(INK2);
    p.setFont(labF);
    p.drawText(x + 26, chipBase(fmLab), m_weather);
}
