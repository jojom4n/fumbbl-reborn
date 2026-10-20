// =============================================================================
// gen_assets — procedural icon generator for FUMBBL Reborn (Qt6)
//
// Draws the icon assets with QPainter (no external art, no downloads):
//   assets/skills/<slug>.png               64x64  skill icons
//   assets/cards/<slug>.png                64x64  inducement card icons
//
// Player FACES (assets/faces/<slug>.png + assets/faces.txt, one cartoon face
// per named roster piece, all 31 BB2025 races) are generated separately by
// the one-off tool draw_faces.cpp — they are committed and NOT regenerated
// here.
//
// Usage:
//   ./gen_assets [output_dir]     (default: <repo>/qt-client/assets)
// =============================================================================
#include <QCoreApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QtMath>

#include <cstdio>
#include <functional>

// ---------------------------------------------------------------------------
// Shared palette
// ---------------------------------------------------------------------------
namespace art {
const QColor kOutline(0x24, 0x16, 0x08);      // miniature outline (very dark brown)
const QColor kSkin(0xcf, 0x98, 0x58);         // fantasy tan skin
const QColor kSkinShade(0xb3, 0x7d, 0x43);
const QColor kHome(0x4a, 0x7c, 0x3f);         // home armor green
const QColor kHomeLight(0x6a, 0xa3, 0x5c);
const QColor kAway(0xc4, 0xa3, 0x5a);         // away armor gold
const QColor kAwayLight(0xe0, 0xc4, 0x82);
const QColor kGold(0xd8, 0xb4, 0x4a);         // trims / icon symbols
const QColor kGoldBright(0xf0, 0xd8, 0x8a);
const QColor kIron(0x5a, 0x5a, 0x66);         // helmet metal
const QColor kIronLight(0x8a, 0x8a, 0x9a);
const QColor kHair(0x3a, 0x2a, 0x18);
const QColor kEye(0x22, 0x1a, 0x12);
const QColor kIconBg(0x23, 0x23, 0x30);       // skill/card icon background
const QColor kIconBgCard(0x2a, 0x24, 0x38);
const QColor kWhite(0xf0, 0xec, 0xe0);
} // namespace art

static void outlinePen(QPainter &p, qreal w = 2.0, const QColor &c = art::kOutline)
{
    QPen pen(c, w);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
}

static void filled(QPainter &p, const QPainterPath &path, const QColor &fill, qreal outlineW = 2.0)
{
    outlinePen(p, outlineW);
    p.setBrush(fill);
    p.drawPath(path);
}

// ---------------------------------------------------------------------------
// Player busts — 96x96, transparent background
//
// Layout (96 units):
//   shoulders: y 58..96   (armor, team color)
//   neck:      y 46..60
//   head:      ellipse c(48,36) rx20 ry21
//   face:      eyes y~34, brows above, mouth y~46
//   headgear:  per archetype, covers head top / sides
// ---------------------------------------------------------------------------
namespace bust {

// Proportions (96x96), tabletop-miniature style:
//   plinth:    y 86..96  (dark disc under the bust)
//   shoulders: y 56..96  (broad, armored, team color)
//   neck:      y 46..60
//   head:      smaller than the torso, c(48,33), jaw slightly square
//   face:      eyes y~30, brows above, mouth y~42.5
//
// The face is shaded (right-side crescent) to give a rounded, sculpted
// mini look instead of a flat cartoon.

static void plinth(QPainter &p)
{
    p.setBrush(QColor(0x30, 0x30, 0x3c));
    outlinePen(p, 2.0);
    p.drawEllipse(QRectF(16, 86, 64, 10));
    p.setBrush(QColor(0x3c, 0x3c, 0x4a));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(20, 86, 56, 6));
}

static void shoulders(QPainter &p, bool home, bool spiked)
{
    const QColor armor = home ? art::kHome : art::kAway;
    const QColor armorLight = home ? art::kHomeLight : art::kAwayLight;

    // --- torso / shoulders ----------------------------------------------------
    QPainterPath torso;
    torso.moveTo(8, 96);
    torso.lineTo(8, 78);
    torso.cubicTo(8, 64, 20, 57, 32, 56);
    torso.lineTo(64, 56);
    torso.cubicTo(76, 57, 88, 64, 88, 78);
    torso.lineTo(88, 96);
    torso.closeSubpath();
    filled(p, torso, armor, 2.5);

    // Central chest plate (lighter)
    QPainterPath plate;
    plate.addRoundedRect(QRectF(39, 58, 18, 38), 4, 4);
    p.setBrush(armorLight);
    outlinePen(p, 1.6);
    p.drawPath(plate);
    p.setBrush(art::kGold);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(45, 62, 6, 6)); // gem

    // Gold shoulder trim
    outlinePen(p, 2.0, art::kGold);
    p.setBrush(Qt::NoBrush);
    QPainterPath trim;
    trim.moveTo(11, 80);
    trim.cubicTo(13, 66, 22, 60, 34, 58.5);
    p.drawPath(trim);
    QPainterPath trimR;
    trimR.moveTo(85, 80);
    trimR.cubicTo(83, 66, 74, 60, 62, 58.5);
    p.drawPath(trimR);

    // Pauldrons (shoulder caps)
    for (int side = -1; side <= 1; side += 2) {
        const qreal cx = 48 + side * 29;
        p.setBrush(armorLight);
        outlinePen(p, 1.8);
        p.drawEllipse(QRectF(cx - 11, 58, 22, 16));
        p.setBrush(art::kGold);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(cx - 2.5, 64, 5, 5)); // rivet
        if (spiked) {
            p.setBrush(armorLight);
            outlinePen(p, 1.6);
            QPolygonF spike;
            spike << QPointF(cx - 6, 60) << QPointF(cx - 2, 46) << QPointF(cx + 2, 60);
            p.drawPolygon(spike);
            QPolygonF spike2;
            spike2 << QPointF(cx + 1, 59) << QPointF(cx + 6, 48) << QPointF(cx + 9, 61);
            p.drawPolygon(spike2);
        }
    }

    // --- neck ----------------------------------------------------------------
    QPainterPath neck;
    neck.addRect(41, 46, 14, 14);
    p.setBrush(art::kSkinShade);
    outlinePen(p, 1.6);
    p.drawPath(neck);
}

static void head(QPainter &p)
{
    // Slightly square jaw
    QPainterPath skull;
    skull.moveTo(32, 34);
    skull.cubicTo(32, 15, 64, 15, 64, 34);
    skull.cubicTo(64, 43, 59, 49, 52, 51.5);
    skull.lineTo(44, 51.5);
    skull.cubicTo(37, 49, 32, 43, 32, 34);
    skull.closeSubpath();
    filled(p, skull, art::kSkin, 2.2);

    // Ears
    p.setBrush(art::kSkin);
    outlinePen(p, 1.6);
    p.drawEllipse(QRectF(28.5, 33, 6, 9));
    p.drawEllipse(QRectF(61.5, 33, 6, 9));

    // Right-side shading crescent (sculpted mini look)
    QPainterPath shade;
    shade.moveTo(55, 17.5);
    shade.cubicTo(61, 21, 64, 28, 64, 34);
    shade.cubicTo(64, 43, 59, 49, 52, 51.5);
    shade.lineTo(49, 50.5);
    shade.cubicTo(55, 47, 58.5, 42, 58.5, 34);
    shade.cubicTo(58.5, 26, 56, 20, 52, 17.8);
    shade.closeSubpath();
    p.setBrush(art::kSkinShade);
    p.setPen(Qt::NoPen);
    p.drawPath(shade);

    // Jaw shadow
    p.setBrush(art::kSkinShade);
    QPainterPath jawShadow;
    jawShadow.moveTo(44, 51.5);
    jawShadow.lineTo(52, 51.5);
    jawShadow.cubicTo(50, 54, 46, 54, 44, 51.5);
    p.drawPath(jawShadow);
}

// Face parts.  browTilt: >0 = angry (inner ends lower).
// mouth: 0 neutral, 1 scowl, 2 snarl (open + teeth), 3 grin.
// glowEyes: gold iris (blitzer).  faceY: shift the whole face down (thrower).
static void face(QPainter &p, qreal browTilt, int mouth, bool glowEyes, qreal faceY = 0)
{
    for (int side = -1; side <= 1; side += 2) {
        const qreal ex = 48 + side * 6.5;
        // almond eye
        QPainterPath eye;
        eye.moveTo(ex - 4, 30 + faceY);
        eye.cubicTo(ex - 2, 27.4 + faceY, ex + 2, 27.4 + faceY, ex + 4, 30 + faceY);
        eye.cubicTo(ex + 2, 32.4 + faceY, ex - 2, 32.4 + faceY, ex - 4, 30 + faceY);
        eye.closeSubpath();
        p.setBrush(art::kWhite);
        outlinePen(p, 1.2);
        p.drawPath(eye);
        p.setPen(Qt::NoPen);
        p.setBrush(glowEyes ? QColor(0xff, 0xc4, 0x3a) : art::kEye);
        p.drawEllipse(QRectF(ex - 2.1, 28.2 + faceY, 4.2, 3.8));
        if (glowEyes) {
            p.setBrush(art::kEye);
            p.drawEllipse(QRectF(ex - 1, 29.2 + faceY, 2, 2));
        }
        // thick brow: inner end (toward the nose) drops when angry
        const qreal innerX = ex - side * 5;
        const qreal outerX = ex + side * 5;
        const qreal innerY = 25.8 + faceY + browTilt;
        const qreal outerY = 25.8 + faceY - browTilt;
        QPainterPath brow;
        brow.moveTo(innerX, innerY);
        brow.lineTo(outerX, outerY);
        brow.lineTo(outerX, outerY + 2.4);
        brow.lineTo(innerX, innerY + 2.4);
        brow.closeSubpath();
        p.setBrush(art::kHair);
        p.setPen(Qt::NoPen);
        p.drawPath(brow);
    }

    // Nose
    outlinePen(p, 1.5, art::kSkinShade);
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(48, 32 + faceY), QPointF(46.8, 37.5 + faceY));
    p.drawLine(QPointF(46.8, 37.5 + faceY), QPointF(49.4, 37.5 + faceY));

    // Mouth
    if (mouth == 2) { // snarl: open mouth + teeth
        QPainterPath m;
        m.moveTo(42, 42 + faceY);
        m.cubicTo(45, 46.5 + faceY, 51, 46.5 + faceY, 54, 42 + faceY);
        m.cubicTo(51, 44.2 + faceY, 45, 44.2 + faceY, 42, 42 + faceY);
        m.closeSubpath();
        p.setBrush(art::kEye);
        outlinePen(p, 1.2);
        p.drawPath(m);
        p.setBrush(art::kWhite);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 3; ++i) {
            qreal x = 43.8 + i * 3.0;
            QPolygonF t;
            t << QPointF(x, 42.8 + faceY) << QPointF(x + 1.8, 42.8 + faceY)
              << QPointF(x + 0.9, 44.6 + faceY);
            p.drawPolygon(t);
        }
    } else if (mouth == 3) { // grin
        outlinePen(p, 1.8, art::kEye);
        p.setBrush(Qt::NoBrush);
        QPainterPath g;
        g.moveTo(43, 43 + faceY);
        g.cubicTo(46, 45.6 + faceY, 50, 45.6 + faceY, 53, 43 + faceY);
        p.drawPath(g);
    } else if (mouth == 1) { // scowl
        outlinePen(p, 1.8, art::kEye);
        p.setBrush(Qt::NoBrush);
        QPainterPath s;
        s.moveTo(43, 43.5 + faceY);
        s.cubicTo(46, 42.2 + faceY, 50, 42.2 + faceY, 53, 43.5 + faceY);
        p.drawPath(s);
    } else { // neutral
        outlinePen(p, 1.8, art::kEye);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(43.5, 43 + faceY), QPointF(52.5, 43 + faceY));
    }
}

// --- Archetypes ----------------------------------------------------------------

// BLOCKER: horned helm, spiked pauldrons, heavy brow, scowl.
void blocker(QPainter &p, bool home)
{
    plinth(p);
    shoulders(p, home, /*spiked=*/true);
    head(p);
    face(p, 1.8, 1, false);

    // Helm dome
    QPainterPath dome;
    dome.moveTo(29, 31);
    dome.cubicTo(29, 9, 67, 9, 67, 31);
    dome.cubicTo(60, 21, 36, 21, 29, 31);
    dome.closeSubpath();
    filled(p, dome, art::kIron, 2.0);
    // rivets
    p.setBrush(art::kIronLight);
    outlinePen(p, 1.0);
    p.drawEllipse(QRectF(37, 15.5, 4, 4));
    p.drawEllipse(QRectF(55, 15.5, 4, 4));
    // big curved horns
    const QColor horn(0xe8, 0xd8, 0xa8);
    for (int side = -1; side <= 1; side += 2) {
        QPainterPath h;
        const qreal sx = 48 + side * 22;
        h.moveTo(sx, 32);
        h.cubicTo(sx + side * 15, 27, sx + side * 16, 12, sx + side * 7, 5);
        h.cubicTo(sx + side * 12, 11, sx + side * 10, 23, sx + side * 3, 27);
        h.closeSubpath();
        filled(p, h, horn, 1.6);
    }
    // cheek lines (weathered)
    outlinePen(p, 1.4, art::kSkinShade);
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(38, 38), QPointF(39.5, 41));
    p.drawLine(QPointF(58, 38), QPointF(56.5, 41));
}

// RUNNER: spiky hair + team headband with tail, focused.
void runner(QPainter &p, bool home)
{
    plinth(p);
    shoulders(p, home, false);
    head(p);
    face(p, 0.7, 0, false);

    // spiky hair
    p.setBrush(art::kHair);
    outlinePen(p, 1.4);
    QPolygonF spikes;
    spikes << QPointF(33, 28) << QPointF(35, 12) << QPointF(40, 20)
           << QPointF(44, 8)  << QPointF(48, 18) << QPointF(52, 8)
           << QPointF(56, 20) << QPointF(61, 12) << QPointF(63, 28);
    QPainterPath hair;
    hair.addPolygon(spikes);
    hair.cubicTo(56, 18, 40, 18, 33, 28);
    hair.closeSubpath();
    p.drawPath(hair);
    // headband + tail
    const QColor band = home ? art::kHomeLight : art::kAwayLight;
    QPainterPath bandP;
    bandP.addRect(30, 21.5, 36, 5.5);
    filled(p, bandP, band, 1.3);
    QPainterPath tail;
    tail.moveTo(65, 22.5);
    tail.cubicTo(76, 24, 80, 32, 76, 40);
    tail.cubicTo(74, 32, 70, 28, 65, 26.5);
    tail.closeSubpath();
    filled(p, tail, band, 1.1);
}

// BLITZER: wild hair, glowing angry eyes, tusks, snarl.
void blitzer(QPainter &p, bool home)
{
    plinth(p);
    shoulders(p, home, true);
    head(p);
    face(p, 2.2, 2, /*glow=*/true);

    // wild hair
    p.setBrush(art::kHair);
    outlinePen(p, 1.4);
    QPolygonF spikes;
    spikes << QPointF(31, 30) << QPointF(27, 12) << QPointF(37, 18)
           << QPointF(39, 4)  << QPointF(46, 15) << QPointF(48, 2)
           << QPointF(50, 15) << QPointF(57, 4)  << QPointF(59, 18)
           << QPointF(69, 12) << QPointF(65, 30);
    QPainterPath hair;
    hair.addPolygon(spikes);
    hair.cubicTo(58, 16, 38, 16, 31, 30);
    hair.closeSubpath();
    p.drawPath(hair);
    // tusks
    const QColor tusk(0xf0, 0xe8, 0xc8);
    for (int side = -1; side <= 1; side += 2) {
        QPainterPath t;
        t.moveTo(48 + side * 6.5, 51);
        t.cubicTo(48 + side * 10, 45, 48 + side * 9, 38, 48 + side * 5.5, 36);
        t.cubicTo(48 + side * 7.5, 42, 48 + side * 4.5, 47, 48 + side * 3.8, 50);
        t.closeSubpath();
        filled(p, t, tusk, 1.1);
    }
}

// CATCHER: team bandana + knot, wide alert eyes, grin.
void catcher(QPainter &p, bool home)
{
    plinth(p);
    shoulders(p, home, false);
    head(p);
    face(p, 0.3, 3, false);

    // bandana cap
    const QColor band = home ? art::kHomeLight : art::kAwayLight;
    QPainterPath cap;
    cap.moveTo(30, 30);
    cap.cubicTo(30, 11, 66, 11, 66, 30);
    cap.cubicTo(58, 22, 38, 22, 30, 30);
    cap.closeSubpath();
    filled(p, cap, band, 1.7);
    // dots
    p.setBrush(art::kIconBg);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(40, 17, 3, 3));
    p.drawEllipse(QRectF(48, 15, 3, 3));
    p.drawEllipse(QRectF(55, 18, 3, 3));
    // knot + tails
    p.setBrush(band);
    outlinePen(p, 1.3);
    p.drawEllipse(QRectF(63, 22, 8, 8));
    QPainterPath t1;
    t1.moveTo(69, 26);
    t1.cubicTo(78, 28, 81, 36, 77, 43);
    t1.cubicTo(77, 35, 74, 31, 68, 30);
    t1.closeSubpath();
    p.drawPath(t1);
    QPainterPath t2;
    t2.moveTo(70, 28);
    t2.cubicTo(81, 32, 85, 41, 83, 49);
    t2.cubicTo(80, 41, 76, 35, 69, 32);
    t2.closeSubpath();
    p.drawPath(t2);
}

// THROWER: full helmet + T-visor, gold crest, calm face behind the bars.
void thrower(QPainter &p, bool home)
{
    plinth(p);
    shoulders(p, home, true);
    head(p);
    face(p, 0.2, 0, false, /*faceY=*/3.0);

    // helmet shell (open face)
    QPainterPath shell;
    shell.moveTo(29, 42);
    shell.cubicTo(27, 8, 69, 8, 67, 42);
    shell.lineTo(61, 42);
    shell.cubicTo(63, 22, 33, 22, 35, 42);
    shell.closeSubpath();
    filled(p, shell, art::kIron, 2.0);
    // crest
    QPainterPath crest;
    crest.addRect(45.5, 9, 5, 15);
    filled(p, crest, art::kGold, 1.3);
    // T-visor bars (over the face)
    p.setBrush(art::kIronLight);
    outlinePen(p, 1.4);
    p.drawRoundedRect(QRectF(34, 27, 28, 5), 2, 2);
    p.drawRoundedRect(QRectF(45.5, 27, 5, 13), 2, 2);
}

void draw(QImage &img, int archetype, bool home)
{
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    switch (archetype) {
    case 0: blocker(p, home); break;
    case 1: runner(p, home); break;
    case 2: blitzer(p, home); break;
    case 3: catcher(p, home); break;
    case 4: thrower(p, home); break;
    }
}

} // namespace bust

// ---------------------------------------------------------------------------
// Icon background for skills / cards (64x64 rounded medallion)
// ---------------------------------------------------------------------------
void drawIconBg(QPainter &p, bool card)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor bg = card ? art::kIconBgCard : art::kIconBg;
    QPainterPath r;
    r.addRoundedRect(QRectF(3, 3, 58, 58), 10, 10);
    p.setPen(QPen(art::kGold, 2.4));
    p.setBrush(bg);
    p.drawPath(r);
    // inner subtle line
    QPainterPath r2;
    r2.addRoundedRect(QRectF(7, 7, 50, 50), 7, 7);
    p.setPen(QPen(art::kGold.darker(160), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(r2);
}

static void symbolPen(QPainter &p, qreal w = 3.0, const QColor &c = art::kGold)
{
    QPen pen(c, w);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
}

// 5-point star centered at c with radius r.
QPolygonF star(QPointF c, qreal r, int points = 5, qreal inner = 0.45)
{
    QPolygonF poly;
    for (int i = 0; i < points * 2; ++i) {
        qreal ang = M_PI * i / points - M_PI / 2;
        qreal rr = (i % 2 == 0) ? r : r * inner;
        poly << QPointF(c.x() + rr * qCos(ang), c.y() + rr * qSin(ang));
    }
    return poly;
}

static QPainterPath polyPath(const QPolygonF &poly)
{
    QPainterPath path;
    path.addPolygon(poly);
    return path;
}

// ---------------------------------------------------------------------------
// SKILL icons — 64x64, symbol centered around (32,32)
// Each function draws ONE skill symbol. Slug -> painter.
// ---------------------------------------------------------------------------
namespace skills {

void block(QPainter &p)          // crossed fists + impact
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawEllipse(QRectF(14, 30, 15, 15));
    p.drawEllipse(QRectF(35, 30, 15, 15));
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawPolygon(star(QPointF(32, 22), 8));
}
void dodge(QPainter &p)          // swoosh arc + arrowhead
{
    symbolPen(p, 3.4);
    p.setBrush(Qt::NoBrush);
    QPainterPath arc;
    arc.moveTo(14, 46);
    arc.cubicTo(18, 24, 34, 16, 48, 20);
    p.drawPath(arc);
    p.setBrush(art::kGold);
    QPolygonF head;
    head << QPointF(52, 21) << QPointF(42, 15) << QPointF(44, 27);
    p.drawPolygon(head);
    // dashed tail hint
    p.setPen(QPen(art::kGold.darker(130), 1.6, Qt::DashLine));
    p.drawLine(QPointF(16, 52), QPointF(30, 50));
}
void mightyBlow(QPainter &p)     // hammer
{
    p.setBrush(art::kIronLight);
    outlinePen(p, 1.8);
    p.drawRoundedRect(QRectF(29, 22, 6, 28), 2, 2);      // handle
    p.setBrush(art::kIron);
    p.drawRoundedRect(QRectF(16, 14, 32, 12), 3, 3);     // head
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawPolygon(star(QPointF(46, 40), 7));             // impact
}
void nerves(QPainter &p)         // shield + star
{
    QPainterPath sh;
    sh.moveTo(32, 12);
    sh.lineTo(48, 18);
    sh.lineTo(48, 34);
    sh.cubicTo(48, 46, 40, 52, 32, 54);
    sh.cubicTo(24, 52, 16, 46, 16, 34);
    sh.lineTo(16, 18);
    sh.closeSubpath();
    filled(p, sh, art::kIron, 2.0);
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawPolygon(star(QPointF(32, 32), 9));
}
void catchSkill(QPainter &p)     // open hand
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawRoundedRect(QRectF(20, 30, 24, 20), 6, 6);     // palm
    for (int i = 0; i < 4; ++i)                          // fingers
        p.drawRoundedRect(QRectF(20 + i * 6.2, 14 + (i == 0 || i == 3 ? 4 : 0), 5, 18), 2.5, 2.5);
}
void sureHands(QPainter &p)      // hand + star
{
    catchSkill(p);
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawPolygon(star(QPointF(32, 12), 7));
}
void anchor(QPainter &p)
{
    symbolPen(p, 3.2);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(27, 10, 10, 10));               // ring
    p.drawLine(QPointF(32, 20), QPointF(32, 46));        // shaft
    p.drawLine(QPointF(22, 28), QPointF(42, 28));        // stock
    QPainterPath arc;                                     // flukes
    arc.moveTo(16, 38);
    arc.cubicTo(18, 50, 46, 50, 48, 38);
    p.drawPath(arc);
    p.drawLine(QPointF(16, 38), QPointF(20, 44));
    p.drawLine(QPointF(48, 38), QPointF(44, 44));
}
void loner(QPainter &p)         // single footprint
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawEllipse(QRectF(22, 28, 20, 26));               // sole
    for (int i = 0; i < 4; ++i)                          // toes
        p.drawEllipse(QRectF(23 + i * 5.4, 14 + (i == 0 || i == 3 ? 5 : 0), 4.6, 4.6));
}
void frenzy(QPainter &p)        // lightning bolt
{
    QPolygonF bolt;
    bolt << QPointF(36, 10) << QPointF(20, 36) << QPointF(30, 36)
         << QPointF(26, 54) << QPointF(46, 26) << QPointF(34, 26);
    filled(p, polyPath(bolt), art::kGoldBright, 1.8);
}
void assassin(QPainter &p)      // dagger
{
    p.setBrush(art::kIronLight);
    outlinePen(p, 1.8);
    QPolygonF blade;
    blade << QPointF(32, 8) << QPointF(37, 34) << QPointF(27, 34);
    p.drawPolygon(blade);
    p.setBrush(art::kGold);
    p.drawRoundedRect(QRectF(22, 34, 20, 5), 2, 2);      // guard
    p.setBrush(art::kHair);
    p.drawRoundedRect(QRectF(29, 39, 6, 14), 2, 2);      // grip
    p.setBrush(art::kGold);
    p.drawEllipse(QRectF(27.5, 53, 9, 5));               // pommel
}
void piledriver(QPainter &p)    // star impact + down arrow
{
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawPolygon(star(QPointF(32, 42), 12));
    symbolPen(p, 3.4);
    p.drawLine(QPointF(32, 10), QPointF(32, 22));
    QPolygonF head;
    head << QPointF(32, 28) << QPointF(25, 18) << QPointF(39, 18);
    p.setBrush(art::kGold);
    outlinePen(p, 1.6);
    p.drawPolygon(head);
}
void sidestep(QPainter &p)      // S-curve arrow
{
    symbolPen(p, 3.4);
    p.setBrush(Qt::NoBrush);
    QPainterPath s;
    s.moveTo(16, 18);
    s.cubicTo(44, 18, 44, 32, 20, 34);
    s.cubicTo(2, 36, 20, 48, 46, 46);
    p.drawPath(s);
    p.setBrush(art::kGold);
    outlinePen(p, 1.6);
    QPolygonF head;
    head << QPointF(50, 47) << QPointF(40, 41) << QPointF(42, 52);
    p.drawPolygon(head);
}
void stiffSpring(QPainter &p)   // spring + up arrow
{
    symbolPen(p, 2.8);
    p.setBrush(Qt::NoBrush);
    QPainterPath spring;
    spring.moveTo(22, 48);
    for (int i = 0; i < 3; ++i) {
        spring.lineTo(42, 44 - i * 4);
        spring.lineTo(22, 40 - i * 4);
    }
    p.drawPath(spring);
    p.drawLine(QPointF(32, 30), QPointF(32, 14));
    p.setBrush(art::kGold);
    outlinePen(p, 1.6);
    QPolygonF head;
    head << QPointF(32, 8) << QPointF(25, 18) << QPointF(39, 18);
    p.drawPolygon(head);
}
void juggernaut(QPainter &p)    // big rock + star
{
    QPolygonF rock;
    rock << QPointF(16, 44) << QPointF(20, 28) << QPointF(34, 22)
         << QPointF(48, 30) << QPointF(46, 46);
    filled(p, polyPath(rock), art::kIron, 2.0);
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawPolygon(star(QPointF(40, 16), 7));
}
void tackle(QPainter &p)        // gripping claw
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawEllipse(QRectF(18, 24, 28, 26));               // fist
    for (int i = 0; i < 3; ++i) {                        // claws down
        QPainterPath c;
        qreal x = 22 + i * 8;
        c.moveTo(x, 48);
        c.cubicTo(x - 1, 56, x + 4, 56, x + 3, 50);
        c.cubicTo(x + 3, 54, x + 1, 55, x + 1, 51);
        c.closeSubpath();
        p.drawPath(c);
    }
}
void passSkill(QPainter &p)     // football + arrow
{
    p.save();
    p.translate(24, 36);
    p.rotate(-30);
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawEllipse(QRectF(-12, -7, 24, 14));
    p.setPen(QPen(art::kOutline, 1.2));
    p.drawLine(QPointF(-5, 0), QPointF(5, 0));
    p.drawLine(QPointF(-3, -2), QPointF(-3, 2));
    p.drawLine(QPointF(0, -2), QPointF(0, 2));
    p.drawLine(QPointF(3, -2), QPointF(3, 2));
    p.restore();
    symbolPen(p, 3.0);
    p.drawLine(QPointF(34, 26), QPointF(50, 14));
    p.setBrush(art::kGold);
    outlinePen(p, 1.6);
    QPolygonF head;
    head << QPointF(52, 12) << QPointF(42, 10) << QPointF(46, 20);
    p.drawPolygon(head);
}
void foul(QPainter &p)          // flag + !
{
    symbolPen(p, 3.0);
    p.drawLine(QPointF(20, 14), QPointF(20, 52));
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    QPolygonF flag;
    flag << QPointF(21, 14) << QPointF(46, 20) << QPointF(21, 30);
    p.drawPolygon(flag);
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(47, 34, 5, 12), 2, 2);
    p.drawEllipse(QRectF(47, 50, 5, 5));
}
void claws(QPainter &p)         // three slash marks
{
    symbolPen(p, 3.6, art::kGoldBright);
    for (int i = 0; i < 3; ++i) {
        qreal x = 20 + i * 10;
        QPainterPath slash;
        slash.moveTo(x - 4, 14);
        slash.cubicTo(x + 2, 26, x + 2, 38, x - 4, 50);
        p.drawPath(slash);
    }
}
void mauling(QPainter &p)       // fanged jaw
{
    p.setBrush(art::kGold);
    outlinePen(p, 2.0);
    QPainterPath jaw;
    jaw.moveTo(14, 26);
    jaw.cubicTo(24, 34, 40, 34, 50, 26);
    jaw.cubicTo(46, 44, 18, 44, 14, 26);
    jaw.closeSubpath();
    p.drawPath(jaw);
    p.setBrush(art::kWhite);
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 5; ++i) {                        // fangs
        qreal x = 19 + i * 7;
        QPolygonF f;
        f << QPointF(x, 31) << QPointF(x + 5, 31) << QPointF(x + 2.5, 41);
        p.drawPolygon(f);
    }
}
void wrestle(QPainter &p)       // two grappling hooks
{
    symbolPen(p, 3.0);
    p.setBrush(Qt::NoBrush);
    for (int side = -1; side <= 1; side += 2) {
        p.drawLine(QPointF(32 + side * 14, 46), QPointF(32 + side * 14, 28));
        QPainterPath h;
        h.moveTo(32 + side * 22, 24);
        h.cubicTo(32 + side * 20, 14, 32 + side * 8, 14, 32 + side * 8, 22);
        p.drawPath(h);
    }
}
void regeneration(QPainter &p)  // cross in circle
{
    p.setBrush(art::kIron);
    outlinePen(p, 2.0, art::kGold);
    p.drawEllipse(QRectF(12, 12, 40, 40));
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(27, 18, 10, 28), 3, 3);
    p.drawRoundedRect(QRectF(18, 27, 28, 10), 3, 3);
}
void unsteady(QPainter &p)      // wobble line
{
    symbolPen(p, 3.4);
    p.setBrush(Qt::NoBrush);
    QPainterPath w;
    w.moveTo(18, 50);
    w.cubicTo(14, 40, 26, 36, 22, 28);
    w.cubicTo(18, 20, 30, 16, 26, 10);
    p.drawPath(w);
    p.drawLine(QPointF(42, 50), QPointF(38, 40));
    p.drawLine(QPointF(44, 34), QPointF(40, 26));
}
void stunty(QPainter &p)        // small figure
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.6);
    p.drawEllipse(QRectF(27, 26, 10, 10));               // head
    p.drawRoundedRect(QRectF(25, 36, 14, 14), 4, 4);     // body
    p.setPen(QPen(art::kGold.darker(140), 1.6));
    p.drawLine(QPointF(12, 50), QPointF(52, 50));        // ground
}
void flight(QPainter &p)        // wings
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    for (int side = -1; side <= 1; side += 2) {
        QPainterPath wing;
        wing.moveTo(32 + side * 2, 40);
        wing.cubicTo(32 + side * 12, 18, 32 + side * 26, 16, 32 + side * 28, 24);
        wing.cubicTo(32 + side * 22, 24, 32 + side * 20, 28, 32 + side * 24, 34);
        wing.cubicTo(32 + side * 18, 34, 32 + side * 16, 38, 32 + side * 2, 40);
        wing.closeSubpath();
        p.drawPath(wing);
    }
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawPolygon(star(QPointF(32, 48), 6));
}
void boneHead(QPainter &p)      // skull
{
    p.setBrush(art::kWhite);
    outlinePen(p, 2.0);
    p.drawEllipse(QRectF(18, 12, 28, 28));               // cranium
    p.drawRoundedRect(QRectF(24, 36, 16, 10), 3, 3);     // jaw
    p.setBrush(art::kEye);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(23, 22, 7, 8));                 // sockets
    p.drawEllipse(QRectF(34, 22, 7, 8));
    p.setBrush(art::kWhite);
    p.setPen(QPen(art::kEye, 1.4));
    p.drawEllipse(QRectF(29, 31, 6, 4));                 // nose
    // teeth lines
    p.setPen(QPen(art::kEye, 1.2));
    for (int i = 0; i < 3; ++i)
        p.drawLine(QPointF(27 + i * 5, 37), QPointF(27 + i * 5, 45));
}
void thickSkull(QPainter &p)    // riveted helm
{
    QPainterPath dome;
    dome.moveTo(16, 44);
    dome.cubicTo(16, 14, 48, 14, 48, 44);
    dome.closeSubpath();
    filled(p, dome, art::kIron, 2.0);
    p.setBrush(art::kGold);
    outlinePen(p, 1.2);
    for (int i = 0; i < 4; ++i)
        p.drawEllipse(QRectF(22 + i * 6.5, 24, 4, 4));
    p.setBrush(art::kIronLight);
    p.drawRoundedRect(QRectF(14, 44, 36, 7), 3, 3);
}
void kick(QPainter &p)          // boot
{
    p.setBrush(art::kGold);
    outlinePen(p, 2.0);
    QPainterPath boot;
    boot.moveTo(24, 14);
    boot.lineTo(38, 14);
    boot.lineTo(38, 38);
    boot.cubicTo(38, 48, 52, 46, 52, 52);
    boot.lineTo(24, 52);
    boot.closeSubpath();
    p.drawPath(boot);
    p.setBrush(art::kHair);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(24, 14, 14, 6), 2, 2);      // cuff
}
void kickMate(QPainter &p)      // boot + up-right arrow
{
    kick(p);
    symbolPen(p, 3.0);
    p.drawLine(QPointF(16, 44), QPointF(30, 30));
    p.setBrush(art::kGoldBright);
    outlinePen(p, 1.6);
    QPolygonF head;
    head << QPointF(34, 27) << QPointF(24, 27) << QPointF(27, 37);
    p.drawPolygon(head);
}
void noKick(QPainter &p)        // boot + X
{
    kick(p);
    symbolPen(p, 3.6, art::kGoldBright);
    p.drawLine(QPointF(34, 12), QPointF(48, 26));
    p.drawLine(QPointF(48, 12), QPointF(34, 26));
}
void carry(QPainter &p)         // hand under a small figure
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.6);
    p.drawEllipse(QRectF(26, 16, 12, 12));               // small head
    p.drawRoundedRect(QRectF(23, 28, 18, 14), 4, 4);     // small body
    // big hand underneath
    p.setBrush(art::kGoldBright);
    QPainterPath hand;
    hand.moveTo(16, 46);
    hand.cubicTo(24, 40, 40, 40, 48, 46);
    hand.cubicTo(44, 54, 20, 54, 16, 46);
    hand.closeSubpath();
    p.drawPath(hand);
}
void strongPassing(QPainter &p) // football + star above
{
    p.save();
    p.translate(32, 40);
    p.rotate(-30);
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawEllipse(QRectF(-14, -8, 28, 16));
    p.setPen(QPen(art::kOutline, 1.2));
    p.drawLine(QPointF(-6, 0), QPointF(6, 0));
    p.restore();
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawPolygon(star(QPointF(32, 16), 9));
}
void starShow(QPainter &p)      // big star
{
    p.setBrush(art::kGoldBright);
    outlinePen(p, 2.0);
    p.drawPolygon(star(QPointF(32, 32), 18));
}
void bloodlust(QPainter &p)     // blood drop
{
    QPainterPath drop;
    drop.moveTo(32, 10);
    drop.cubicTo(40, 26, 46, 34, 46, 42);
    drop.arcTo(QRectF(18, 30, 28, 24), -0, 180);
    drop.closeSubpath();
    filled(p, drop, art::kGold, 2.0);
}
void accurate(QPainter &p)      // target
{
    symbolPen(p, 2.6);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(14, 14, 36, 36));
    p.drawEllipse(QRectF(22, 22, 20, 20));
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(28, 28, 8, 8));
}
void bullseye(QPainter &p)      // crosshair
{
    symbolPen(p, 2.6);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(14, 14, 36, 36));
    p.drawLine(QPointF(32, 8), QPointF(32, 18));
    p.drawLine(QPointF(32, 46), QPointF(32, 56));
    p.drawLine(QPointF(8, 32), QPointF(18, 32));
    p.drawLine(QPointF(46, 32), QPointF(56, 32));
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(29, 29, 6, 6));
}
void hatred(QPainter &p)        // angry eye
{
    p.setBrush(art::kWhite);
    outlinePen(p, 2.0);
    QPainterPath eye;
    eye.moveTo(14, 32);
    eye.cubicTo(24, 22, 40, 22, 50, 32);
    eye.cubicTo(40, 42, 24, 42, 14, 32);
    eye.closeSubpath();
    p.drawPath(eye);
    p.setBrush(art::kGold);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(26, 26, 12, 12));
    p.setBrush(art::kEye);
    p.drawEllipse(QRectF(30, 29, 5, 5));
    symbolPen(p, 3.4);
    p.drawLine(QPointF(18, 18), QPointF(30, 22));        // brow
    p.drawLine(QPointF(46, 18), QPointF(34, 22));
}
void eyeGouge(QPainter &p)      // eye + slash
{
    hatred(p);
    symbolPen(p, 3.6, art::kGoldBright);
    p.drawLine(QPointF(12, 48), QPointF(52, 14));
}
void hypnoticGaze(QPainter &p)  // swirl eye
{
    symbolPen(p, 2.6);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(14, 22, 36, 20));
    QPainterPath swirl;
    swirl.moveTo(32, 32);
    swirl.cubicTo(40, 32, 40, 26, 32, 26);
    swirl.cubicTo(24, 26, 26, 38, 34, 38);
    swirl.cubicTo(42, 38, 44, 30, 38, 24);
    p.drawPath(swirl);
    symbolPen(p, 2.0, art::kGoldBright);
    for (int i = 0; i < 5; ++i) {                          // lashes
        qreal a = M_PI * (0.15 + i * 0.18);
        p.drawLine(QPointF(32 + 19 * qCos(a), 32 - 10 * qSin(a)),
                   QPointF(32 + 23 * qCos(a), 32 - 12 * qSin(a)));
    }
}
void takeRoot(QPainter &p)      // trunk + roots
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawRoundedRect(QRectF(28, 12, 8, 30), 3, 3);        // trunk
    symbolPen(p, 2.6);
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(32, 42), QPointF(20, 52));
    p.drawLine(QPointF(32, 42), QPointF(32, 54));
    p.drawLine(QPointF(32, 42), QPointF(44, 52));
    p.drawLine(QPointF(32, 20), QPointF(24, 14));
    p.drawLine(QPointF(32, 24), QPointF(40, 16));
}
void drunkard(QPainter &p)      // mug
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawRoundedRect(QRectF(20, 20, 20, 28), 3, 3);
    QPainterPath handle;
    handle.moveTo(40, 26);
    handle.cubicTo(50, 26, 50, 42, 40, 42);
    p.setBrush(Qt::NoBrush);
    symbolPen(p, 3.0);
    p.drawPath(handle);
    // foam
    p.setBrush(art::kWhite);
    outlinePen(p, 1.4);
    p.drawEllipse(QRectF(18, 14, 9, 8));
    p.drawEllipse(QRectF(26, 12, 10, 9));
    p.drawEllipse(QRectF(34, 14, 9, 8));
}
void taunt(QPainter &p)         // open hand + lines
{
    catchSkill(p);
    symbolPen(p, 2.4, art::kGoldBright);
    p.drawLine(QPointF(12, 20), QPointF(8, 14));
    p.drawLine(QPointF(52, 20), QPointF(56, 14));
}
void sprint(QPainter &p)        // running figure + speed lines
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.6);
    p.drawEllipse(QRectF(30, 14, 10, 10));
    p.drawRoundedRect(QRectF(28, 24, 10, 16), 3, 3);
    symbolPen(p, 3.0);
    p.drawLine(QPointF(10, 24), QPointF(22, 24));
    p.drawLine(QPointF(6, 32), QPointF(22, 32));
    p.drawLine(QPointF(10, 40), QPointF(22, 40));
}
void standFirm(QPainter &p)     // boot on ground
{
    kick(p);
    symbolPen(p, 3.0);
    p.drawLine(QPointF(12, 54), QPointF(54, 54));
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawPolygon(star(QPointF(16, 20), 6));
}
void defensive(QPainter &p)     // shield (bigger, plain)
{
    nerves(p);
}
void breakTackle(QPainter &p)   // broken chain
{
    symbolPen(p, 3.2);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(12, 26, 14, 12), 6, 6);
    p.drawRoundedRect(QRectF(38, 26, 14, 12), 6, 6);
    // broken middle
    p.drawLine(QPointF(26, 30), QPointF(31, 26));
    p.drawLine(QPointF(33, 36), QPointF(38, 32));
    p.drawLine(QPointF(31, 26), QPointF(35, 22));
}
void strongArm(QPainter &p)     // bicep
{
    p.setBrush(art::kGold);
    outlinePen(p, 2.0);
    QPainterPath arm;
    arm.moveTo(18, 50);
    arm.cubicTo(16, 32, 24, 22, 36, 20);
    arm.cubicTo(46, 18, 50, 24, 48, 30);
    arm.cubicTo(44, 34, 40, 34, 38, 40);
    arm.cubicTo(34, 40, 30, 44, 32, 50);
    arm.closeSubpath();
    p.drawPath(arm);
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(42, 26, 10, 10));
}
void indomitable(QPainter &p)   // star in shield outline
{
    symbolPen(p, 2.6);
    p.setBrush(Qt::NoBrush);
    QPainterPath sh;
    sh.moveTo(32, 10);
    sh.lineTo(50, 16);
    sh.lineTo(50, 34);
    sh.cubicTo(50, 46, 42, 52, 32, 54);
    sh.cubicTo(22, 52, 14, 46, 14, 34);
    sh.lineTo(14, 16);
    sh.closeSubpath();
    p.drawPath(sh);
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawPolygon(star(QPointF(32, 32), 11));
}
void leader(QPainter &p)        // crown
{
    p.setBrush(art::kGold);
    outlinePen(p, 2.0);
    QPolygonF crown;
    crown << QPointF(14, 46) << QPointF(14, 24) << QPointF(24, 34)
          << QPointF(32, 18) << QPointF(40, 34) << QPointF(50, 24) << QPointF(50, 46);
    p.drawPolygon(crown);
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(29, 40, 6, 6));
}
void oldPro(QPainter &p)        // laurel + star
{
    symbolPen(p, 2.4);
    p.setBrush(Qt::NoBrush);
    QPainterPath arcL;
    arcL.moveTo(20, 46);
    arcL.cubicTo(10, 32, 14, 20, 26, 14);
    p.drawPath(arcL);
    QPainterPath arcR;
    arcR.moveTo(44, 46);
    arcR.cubicTo(54, 32, 50, 20, 38, 14);
    p.drawPath(arcR);
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawPolygon(star(QPointF(32, 32), 10));
}
void generic(QPainter &p)       // scroll (unknown skill)
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawRoundedRect(QRectF(20, 14, 24, 36), 3, 3);
    p.setBrush(art::kGold.darker(130));
    p.drawEllipse(QRectF(16, 12, 8, 8));
    p.drawEllipse(QRectF(16, 44, 8, 8));
    p.drawEllipse(QRectF(40, 12, 8, 8));
    p.drawEllipse(QRectF(40, 44, 8, 8));
    p.setPen(QPen(art::kOutline, 1.2));
    p.drawLine(QPointF(25, 24), QPointF(39, 24));
    p.drawLine(QPointF(25, 32), QPointF(39, 32));
    p.drawLine(QPointF(25, 40), QPointF(35, 40));
}

} // namespace skills

// ---------------------------------------------------------------------------
// CARD icons (inducements) — 64x64
// ---------------------------------------------------------------------------
namespace cards {

void beguilingBracers(QPainter &p)  // two bracers with gems
{
    for (int i = 0; i < 2; ++i) {
        qreal x = 16 + i * 20;
        p.setBrush(art::kIron);
        outlinePen(p, 1.8);
        p.drawRoundedRect(QRectF(x, 22, 14, 24), 4, 4);
        p.setBrush(art::kGoldBright);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(x + 4, 30, 6, 6));
    }
}
void beltInvulner(QPainter &p)      // belt + buckle
{
    p.setBrush(art::kHair);
    outlinePen(p, 1.8);
    p.drawRoundedRect(QRectF(10, 26, 44, 12), 3, 3);
    p.setBrush(art::kGold);
    p.drawRoundedRect(QRectF(26, 22, 12, 20), 3, 3);
    p.setBrush(art::kIconBg);
    p.drawEllipse(QRectF(30, 29, 4, 6));
}
void blatantFoul(QPainter &p)       // blindfolded eye + !
{
    p.setBrush(art::kWhite);
    outlinePen(p, 2.0);
    QPainterPath eye;
    eye.moveTo(14, 30);
    eye.cubicTo(24, 22, 40, 22, 50, 30);
    eye.cubicTo(40, 38, 24, 38, 14, 30);
    eye.closeSubpath();
    p.drawPath(eye);
    p.setBrush(art::kHair);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(12, 26, 40, 8), 3, 3);       // blindfold
    p.setBrush(art::kGoldBright);
    p.drawRoundedRect(QRectF(47, 42, 5, 10), 2, 2);
    p.drawEllipse(QRectF(47, 54, 5, 4));
}
void chopBlock(QPainter &p)         // fist over helmet
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawRoundedRect(QRectF(22, 10, 18, 16), 5, 5);      // fist
    for (int i = 0; i < 3; ++i)                            // knuckles
        p.drawLine(QPointF(26 + i * 5, 12), QPointF(26 + i * 5, 24));
    p.setBrush(art::kIron);
    p.drawEllipse(QRectF(24, 34, 16, 16));                // helmet
    p.drawLine(QPointF(32, 26), QPointF(32, 32));         // motion
}
void custardPie(QPainter &p)        // pie + splat
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawChord(QRectF(14, 30, 36, 24), 0, 180);          // dish
    p.setBrush(art::kWhite);
    p.drawChord(QRectF(18, 22, 28, 20), 0, 180);          // pie
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(46, 16, 6, 6));                  // splat
    p.drawEllipse(QRectF(50, 26, 4, 4));
}
void distract(QPainter &p)          // dazzled eye (star over the pupil)
{
    p.setBrush(art::kWhite);
    outlinePen(p, 2.0);
    QPainterPath eye;
    eye.moveTo(14, 32);
    eye.cubicTo(24, 24, 40, 24, 50, 32);
    eye.cubicTo(40, 40, 24, 40, 14, 32);
    eye.closeSubpath();
    p.drawPath(eye);
    p.setBrush(art::kGoldBright);
    outlinePen(p, 1.4);
    p.drawPolygon(star(QPointF(32, 32), 10));
}
void fawndoughs(QPainter &p)        // headband + feather
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawRoundedRect(QRectF(12, 30, 40, 10), 4, 4);      // band
    QPainterPath feather;
    feather.moveTo(40, 30);
    feather.cubicTo(48, 18, 54, 16, 56, 20);
    feather.cubicTo(52, 24, 46, 28, 42, 32);
    feather.closeSubpath();
    filled(p, feather, art::kGoldBright, 1.4);
}
void forceShield(QPainter &p)       // force circle + bolt
{
    symbolPen(p, 2.6);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(12, 12, 40, 40));
    symbolPen(p, 2.0, art::kGoldBright);
    p.drawArc(QRectF(18, 18, 28, 28), 30 * 16, 120 * 16);
    QPolygonF bolt;
    bolt << QPointF(34, 20) << QPointF(26, 34) << QPointF(32, 34)
         << QPointF(30, 44) << QPointF(40, 30) << QPointF(34, 30);
    p.setBrush(art::kGoldBright);
    outlinePen(p, 1.2);
    p.drawPolygon(bolt);
}
void gikta(QPainter &p)             // bear head
{
    p.setBrush(art::kHair);
    outlinePen(p, 2.0);
    p.drawEllipse(QRectF(18, 18, 28, 26));                // head
    p.drawEllipse(QRectF(16, 14, 10, 10));                // ears
    p.drawEllipse(QRectF(40, 14, 10, 10));
    p.setBrush(art::kGold);
    p.drawRoundedRect(QRectF(27, 34, 10, 8), 3, 3);       // snout
    p.setBrush(art::kEye);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(24, 26, 4, 5));
    p.drawEllipse(QRectF(36, 26, 4, 5));
}
void glovesHolding(QPainter &p)     // two fists gripping
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawRoundedRect(QRectF(12, 26, 16, 16), 5, 5);
    p.drawRoundedRect(QRectF(36, 26, 16, 16), 5, 5);
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(28, 30, 8, 8));                  // ball held
}
void codpiece(QPainter &p)          // plate on belt (tasteful!)
{
    p.setBrush(art::kHair);
    outlinePen(p, 1.8);
    p.drawRoundedRect(QRectF(12, 28, 40, 8), 3, 3);       // belt
    p.setBrush(art::kIron);
    QPainterPath plate;
    plate.moveTo(27, 30);
    plate.lineTo(37, 30);
    plate.lineTo(35, 46);
    plate.cubicTo(34, 50, 30, 50, 29, 46);
    plate.closeSubpath();
    p.drawPath(plate);
    p.setBrush(art::kGold);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(30, 34, 4, 4));
}
void greasedShoes(QPainter &p)      // sliding shoes
{
    for (int i = 0; i < 2; ++i) {
        qreal x = 12 + i * 22;
        p.setBrush(art::kGold);
        outlinePen(p, 1.6);
        QPainterPath shoe;
        shoe.moveTo(x, 30);
        shoe.lineTo(x + 10, 30);
        shoe.lineTo(x + 10, 42);
        shoe.cubicTo(x + 10, 48, x + 20, 46, x + 20, 50);
        shoe.lineTo(x, 50);
        shoe.closeSubpath();
        p.drawPath(shoe);
    }
    symbolPen(p, 2.2, art::kGoldBright);                  // slip lines
    p.drawLine(QPointF(14, 56), QPointF(30, 56));
    p.drawLine(QPointF(38, 56), QPointF(54, 56));
}
void explodingRunes(QPainter &p)    // skull + sparks
{
    p.setBrush(art::kWhite);
    outlinePen(p, 1.8);
    p.drawEllipse(QRectF(20, 20, 24, 24));
    p.setBrush(art::kEye);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(25, 28, 5, 6));
    p.drawEllipse(QRectF(34, 28, 5, 6));
    symbolPen(p, 2.0, art::kGoldBright);                  // sparks
    for (int i = 0; i < 6; ++i) {
        qreal a = M_PI * i / 3.0;
        p.drawLine(QPointF(32 + 15 * qCos(a), 32 + 15 * qSin(a)),
                   QPointF(32 + 21 * qCos(a), 32 + 21 * qSin(a)));
    }
}
void illegalSub(QPainter &p)        // crossed arrows + figure
{
    symbolPen(p, 3.0);
    p.drawLine(QPointF(16, 18), QPointF(48, 50));
    p.drawLine(QPointF(48, 18), QPointF(16, 50));
    p.setBrush(art::kGold);
    outlinePen(p, 1.4);
    p.drawEllipse(QRectF(26, 10, 12, 12));                // head
    p.drawRoundedRect(QRectF(24, 22, 16, 20), 4, 4);      // body
}
void inertiaDamper(QPainter &p)     // figure + brake X on motion
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.6);
    p.drawEllipse(QRectF(26, 12, 10, 10));
    p.drawRoundedRect(QRectF(24, 22, 14, 20), 4, 4);
    symbolPen(p, 3.4, art::kGoldBright);
    p.drawLine(QPointF(42, 30), QPointF(54, 42));
    p.drawLine(QPointF(54, 30), QPointF(42, 42));
}
void kickingBoots(QPainter &p)      // two boots + star
{
    for (int i = 0; i < 2; ++i) {
        qreal x = 12 + i * 24;
        p.setBrush(art::kGold);
        outlinePen(p, 1.6);
        QPainterPath boot;
        boot.moveTo(x, 18);
        boot.lineTo(x + 10, 18);
        boot.lineTo(x + 10, 40);
        boot.cubicTo(x + 10, 48, x + 20, 46, x + 20, 50);
        boot.lineTo(x, 50);
        boot.closeSubpath();
        p.drawPath(boot);
    }
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawPolygon(star(QPointF(32, 12), 7));
}
void luckyCharm(QPainter &p)        // clover
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    for (int i = 0; i < 4; ++i) {
        qreal a = M_PI / 4 + M_PI * i / 2;
        p.drawEllipse(QRectF(32 + 8 * qCos(a) - 8, 32 + 8 * qSin(a) - 8, 16, 16));
    }
    p.drawLine(QPointF(32, 38), QPointF(36, 54));         // stem
}
void magicGloves(QPainter &p)       // glove + star
{
    skills::catchSkill(p);
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawPolygon(star(QPointF(48, 14), 8));
}
void pitTrap(QPainter &p)           // pit + falling figure
{
    symbolPen(p, 3.0);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(12, 24, 40, 28), 4, 4);
    p.setBrush(art::kGold);
    outlinePen(p, 1.4);
    p.drawEllipse(QRectF(28, 28, 8, 8));                  // head falling
    p.drawLine(QPointF(32, 36), QPointF(32, 46));
}
void rabbitsFoot(QPainter &p)       // foot + bone
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawEllipse(QRectF(20, 26, 22, 24));
    p.drawLine(QPointF(31, 26), QPointF(31, 12));         // bone shaft
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(26, 8, 7, 7));
    p.drawEllipse(QRectF(33, 8, 7, 7));
}
void spikedBall(QPainter &p)        // spiky ball
{
    symbolPen(p, 2.4);
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawEllipse(QRectF(18, 18, 28, 28));
    for (int i = 0; i < 8; ++i) {
        qreal a = M_PI * i / 4.0;
        QPolygonF spike;
        qreal bx = 32 + 13 * qCos(a), by = 32 + 13 * qSin(a);
        spike << QPointF(bx, by)
              << QPointF(32 + 19 * qCos(a + 0.18), 32 + 19 * qSin(a + 0.18))
              << QPointF(32 + 19 * qCos(a - 0.18), 32 + 19 * qSin(a - 0.18));
        p.drawPolygon(spike);
    }
}
void stolenPlaybook(QPainter &p)    // book + claw
{
    p.setBrush(art::kHair);
    outlinePen(p, 1.8);
    p.drawRoundedRect(QRectF(16, 16, 24, 32), 3, 3);
    p.setPen(QPen(art::kGold, 1.4));
    p.drawLine(QPointF(21, 24), QPointF(35, 24));
    p.drawLine(QPointF(21, 32), QPointF(35, 32));
    p.drawLine(QPointF(21, 40), QPointF(31, 40));
    // claw
    p.setBrush(art::kGoldBright);
    outlinePen(p, 1.6);
    QPainterPath claw;
    claw.moveTo(46, 20);
    claw.cubicTo(52, 28, 50, 38, 44, 44);
    claw.cubicTo(48, 36, 46, 28, 42, 24);
    claw.closeSubpath();
    p.drawPath(claw);
}
void wandSmashing(QPainter &p)      // wand + star
{
    symbolPen(p, 4.0);
    p.drawLine(QPointF(16, 50), QPointF(42, 24));
    p.setBrush(art::kGoldBright);
    outlinePen(p, 1.6);
    p.drawPolygon(star(QPointF(48, 16), 9));
}
void witchesBrew(QPainter &p)       // cauldron + bubbles
{
    p.setBrush(art::kIron);
    outlinePen(p, 2.0);
    QPainterPath pot;
    pot.moveTo(16, 30);
    pot.cubicTo(16, 52, 48, 52, 48, 30);
    pot.closeSubpath();
    p.drawPath(pot);
    p.drawRoundedRect(QRectF(14, 27, 36, 6), 3, 3);
    p.setBrush(art::kGoldBright);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(22, 18, 7, 7));
    p.drawEllipse(QRectF(32, 14, 9, 9));
    p.drawEllipse(QRectF(42, 19, 6, 6));
}
void genericCard(QPainter &p)       // card with ?
{
    p.setBrush(art::kGold);
    outlinePen(p, 1.8);
    p.drawRoundedRect(QRectF(18, 12, 28, 40), 4, 4);
    p.setBrush(art::kIconBg);
    p.drawRoundedRect(QRectF(22, 16, 20, 32), 3, 3);
    p.setBrush(art::kGoldBright);
    p.setPen(QPen(art::kGoldBright, 2.6));
    QPainterPath q;
    q.moveTo(28, 26);
    q.cubicTo(28, 20, 38, 20, 37, 26);
    q.cubicTo(36, 30, 31, 30, 31, 34);
    p.drawPath(q);
    p.setBrush(Qt::NoBrush);
    QPen dotPen(art::kGoldBright, 5.0);
    dotPen.setCapStyle(Qt::RoundCap);
    p.setPen(dotPen);
    p.drawPoint(QPointF(31, 41));
}

} // namespace cards

// ---------------------------------------------------------------------------
// Slug + registry
// ---------------------------------------------------------------------------
static QString slug(const QString &name)
{
    QString s = name.toLower().simplified();
    QString out;
    for (QChar c : s)
        out += (c.isLetterOrNumber() ? c : QLatin1Char('_'));
    while (out.contains("__"))
        out.replace("__", "_");
    while (out.startsWith(QLatin1Char('_')))
        out.remove(0, 1);
    while (out.endsWith(QLatin1Char('_')))
        out.chop(1);
    return out;
}


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString outDir = QStringLiteral("assets");
    if (argc > 1)
        outDir = QString::fromLocal8Bit(argv[1]);
    QDir out(outDir);
    if (!out.mkpath(QStringLiteral("players")) || !out.mkpath(QStringLiteral("skills")) ||
        !out.mkpath(QStringLiteral("cards"))) {
        std::fprintf(stderr, "cannot create %s\n", outDir.toUtf8().constData());
        return 1;
    }

    int count = 0;

    // NOTE: player faces live in assets/faces (one-off generator, committed).

    // --- Skills ----------------------------------------------------------------
    const QHash<QString, std::function<void(QPainter &)>> skillPainters = {
        { "block", skills::block },
        { "dodge", skills::dodge },
        { "mighty_blow", skills::mightyBlow },
        { "nerves_of_steel", skills::nerves },
        { "catch", skills::catchSkill },
        { "sure_hands", skills::sureHands },
        { "anchor", skills::anchor },
        { "loner", skills::loner },
        { "frenzy", skills::frenzy },
        { "assassin", skills::assassin },
        { "piledriver", skills::piledriver },
        { "sidestep", skills::sidestep },
        { "stiff_spring", skills::stiffSpring },
        { "juggernaut", skills::juggernaut },
        { "tackle", skills::tackle },
        { "pass", skills::passSkill },
        { "foul", skills::foul },
        { "claws", skills::claws },
        { "savage_mauling", skills::mauling },
        { "wrestle", skills::wrestle },
        { "regeneration", skills::regeneration },
        { "unsteady", skills::unsteady },
        { "stunty", skills::stunty },
        { "lethal_flight", skills::flight },
        { "bone_head", skills::boneHead },
        { "thick_skull", skills::thickSkull },
        { "kick", skills::kick },
        { "kick_team_mate", skills::kickMate },
        { "no_kicks_left", skills::noKick },
        { "i_ll_carry_you", skills::carry },
        { "strong_passing_game", skills::strongPassing },
        { "star_of_the_show", skills::starShow },
        { "bloodlust", skills::bloodlust },
        { "accurate", skills::accurate },
        { "bullseye", skills::bullseye },
        { "hatred", skills::hatred },
        { "eye_gouge", skills::eyeGouge },
        { "hypnotic_gaze", skills::hypnoticGaze },
        { "take_root", skills::takeRoot },
        { "drunkard", skills::drunkard },
        { "taunt", skills::taunt },
        { "sprint", skills::sprint },
        { "stand_firm", skills::standFirm },
        { "defensive", skills::defensive },
        { "guard", skills::defensive },
        { "break_tackle", skills::breakTackle },
        { "strong_arm", skills::strongArm },
        { "indomitable", skills::indomitable },
        { "leader", skills::leader },
        { "old_pro", skills::oldPro },
        { "pro", skills::oldPro },
    };
    // The full skill names the server actually sends (for the name->slug map).
    const QStringList skillNames = {
        QStringLiteral("Block"), QStringLiteral("Dodge"), QStringLiteral("Mighty Blow"),
        QStringLiteral("Mighty Blows"), QStringLiteral("Nerves of Steel"),
        QStringLiteral("Catch"), QStringLiteral("Sure Hands"), QStringLiteral("Anchor"),
        QStringLiteral("Loner"), QStringLiteral("Frenzy"), QStringLiteral("Assassin"),
        QStringLiteral("Piledriver"), QStringLiteral("Sidestep"), QStringLiteral("Stiff Spring"),
        QStringLiteral("Juggernaut"), QStringLiteral("Tackle"), QStringLiteral("Pass"),
        QStringLiteral("Foul"), QStringLiteral("Claws"), QStringLiteral("Savage Mauling"),
        QStringLiteral("Wrestle"), QStringLiteral("Regeneration"), QStringLiteral("Unsteady"),
        QStringLiteral("Stunty"), QStringLiteral("Lethal Flight"), QStringLiteral("Bone Head"),
        QStringLiteral("Thick Skull"), QStringLiteral("Kick"), QStringLiteral("Kick Team-Mate"),
        QStringLiteral("No Kicks Left"), QStringLiteral("I'll Carry You"),
        QStringLiteral("Strong Passing Game"), QStringLiteral("Star of the Show"),
        QStringLiteral("Bloodlust"), QStringLiteral("Accurate"), QStringLiteral("Bullseye"),
        QStringLiteral("Hatred"), QStringLiteral("Eye Gouge"), QStringLiteral("Hypnotic Gaze"),
        QStringLiteral("Take Root"), QStringLiteral("Drunkard"), QStringLiteral("Taunt"),
        QStringLiteral("Sprint"), QStringLiteral("Stand Firm"), QStringLiteral("Defensive"),
        QStringLiteral("Guard"), QStringLiteral("Break Tackle"), QStringLiteral("Strong Arm"),
        QStringLiteral("Indomitable"), QStringLiteral("Leader"), QStringLiteral("Old Pro"),
        QStringLiteral("Pro"),
    };
    for (auto it = skillPainters.cbegin(); it != skillPainters.cend(); ++it) {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        drawIconBg(p, false);
        it.value()(p);
        const QString file = QStringLiteral("skills/%1.png").arg(it.key());
        if (!img.save(out.filePath(file)))
            std::fprintf(stderr, "FAIL %s\n", file.toUtf8().constData());
        ++count;
    }
    // generic fallback
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        drawIconBg(p, false);
        skills::generic(p);
        img.save(out.filePath(QStringLiteral("skills/generic.png")));
        ++count;
    }

    // --- Cards --------------------------------------------------------------------
    const QHash<QString, std::function<void(QPainter &)>> cardPainters = {
        { "beguiling_bracers", cards::beguilingBracers },
        { "belt_of_invulnerability", cards::beltInvulner },
        { "blatant_foul", cards::blatantFoul },
        { "distract", cards::distract },
        { "chop_block", cards::chopBlock },
        { "custard_pie", cards::custardPie },
        { "fawndough_s_headband", cards::fawndoughs },
        { "force_shield", cards::forceShield },
        { "gikta_s_strength_of_da_bear", cards::gikta },
        { "gloves_of_holding", cards::glovesHolding },
        { "good_old_magic_codpiece", cards::codpiece },
        { "greased_shoes", cards::greasedShoes },
        { "gromskull_s_exploding_runes", cards::explodingRunes },
        { "illegal_substitution", cards::illegalSub },
        { "inertia_damper", cards::inertiaDamper },
        { "kicking_boots", cards::kickingBoots },
        { "lucky_charm", cards::luckyCharm },
        { "magic_gloves_of_jark_longarm", cards::magicGloves },
        { "pit_trap", cards::pitTrap },
        { "rabbit_s_foot", cards::rabbitsFoot },
        { "spiked_ball", cards::spikedBall },
        { "stolen_playbook", cards::stolenPlaybook },
        { "wand_of_smashing", cards::wandSmashing },
        { "witch_s_brew", cards::witchesBrew },
    };
    const QStringList cardNames = {
        QStringLiteral("Beguiling Bracers"), QStringLiteral("Belt of Invulnerability"),
        QStringLiteral("Blatant Foul"), QStringLiteral("Chop Block"),
        QStringLiteral("Distract"),
        QStringLiteral("Custard Pie"), QStringLiteral("Fawndough's Headband"),
        QStringLiteral("Force Shield"), QStringLiteral("Gikta's Strength of da Bear"),
        QStringLiteral("Gloves of Holding"), QStringLiteral("Good Old Magic Codpiece"),
        QStringLiteral("Greased Shoes"), QStringLiteral("Gromskull's Exploding Runes"),
        QStringLiteral("Illegal Substitution"), QStringLiteral("Inertia Damper"),
        QStringLiteral("Kicking Boots"), QStringLiteral("Lucky Charm"),
        QStringLiteral("Magic Gloves of Jark Longarm"), QStringLiteral("Pit Trap"),
        QStringLiteral("Rabbit's Foot"), QStringLiteral("Spiked Ball"),
        QStringLiteral("Stolen Playbook"), QStringLiteral("Wand of Smashing"),
        QStringLiteral("Witch's Brew"),
    };
    for (auto it = cardPainters.cbegin(); it != cardPainters.cend(); ++it) {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        drawIconBg(p, true);
        it.value()(p);
        const QString file = QStringLiteral("cards/%1.png").arg(it.key());
        if (!img.save(out.filePath(file)))
            std::fprintf(stderr, "FAIL %s\n", file.toUtf8().constData());
        ++count;
    }
    {
        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        drawIconBg(p, true);
        cards::genericCard(p);
        img.save(out.filePath(QStringLiteral("cards/generic.png")));
        ++count;
    }

    // --- Name maps (text, one "Name<TAB>slug" per line) ---------------------------
    {
        QFile f(out.filePath(QStringLiteral("skill_names.txt")));
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        for (const QString &name : skillNames) {
            QString s = slug(name);
            if (!skillPainters.contains(s))
                s = (s == QStringLiteral("mighty_blows")) ? QStringLiteral("mighty_blow")
                                                           : QStringLiteral("generic");
            f.write((name + QLatin1Char('\t') + s + QLatin1Char('\n')).toUtf8());
        }
        f.close();
        QFile c(out.filePath(QStringLiteral("card_names.txt")));
        c.open(QIODevice::WriteOnly | QIODevice::Truncate);
        for (const QString &name : cardNames) {
            QString s = slug(name);
            if (!cardPainters.contains(s))
                s = "generic";
            c.write((name + QLatin1Char('\t') + s + QLatin1Char('\n')).toUtf8());
        }
        c.close();
    }

    std::printf("generated %d assets in %s\n", count, outDir.toUtf8().constData());
    return 0;
}
