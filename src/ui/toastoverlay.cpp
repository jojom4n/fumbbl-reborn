#include "ui/toastoverlay.h"

#include <QFont>
#include <QFontMetricsF>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>

namespace {
// "rgba(r, g, b, a)" for widget stylesheets (QColor::name() drops alpha).
QString rgba(const QColor &c)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(c.alpha());
}
} // namespace

ToastOverlay::ToastOverlay(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("toastOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void ToastOverlay::showCenter(const QString &text, int visibleMs)
{
    if (text.isEmpty())
        return;
    // The label sizes itself inside makeToast (its own sizeHint — the exact
    // font/style it renders with); center it on the FINAL width so the
    // centering can never drift from the rendered size.
    QLabel *label =
        makeToast(text, QColor(24, 24, 30, 215), 0, 36, visibleMs, false);
    label->move(qMax(8, (width() - label->width()) / 2), 36);
}

void ToastOverlay::showEdge(const QString &text, const QColor &accent,
                            int visibleMs)
{
    if (text.isEmpty())
        return;
    // CUMULATIVE stacking: the new pill sits below the ones still live
    // (36 + sum(height + 6 gap)); a fading pill's slot is reused.
    int y = 36;
    for (const QPair<QLabel *, int> &e : m_edgeStack)
        y += e.second + 6;
    QLabel *label = makeToast(text, accent, 20, y, visibleMs, true);
    ++m_edgeCount;
    m_edgeStack.append({label, label->height()});
}

QLabel *ToastOverlay::makeToast(const QString &text, const QColor &bg, int x,
                             int y, int visibleMs, bool edge)
{
    auto *label = new QLabel(text, this);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    QFont f = font();
    f.setBold(true);
    f.setPointSizeF(edge ? 11 : 20);
    label->setFont(f);
    // The padding is the SINGLE SOURCE for both the stylesheet and the
    // size math (they must never drift apart).
    const int kPadX = 16, kPadY = 6;
    label->setStyleSheet(QStringLiteral(
        "background: %1; color: #ffffff; border-radius: 8px; padding: %2px %3px;")
        .arg(rgba(bg)).arg(kPadY).arg(kPadX));
    const QFontMetricsF fm(f);
    // The border must HUG the text: same font as the label renders with,
    // the exact number of lines, and the longest line — never wider than
    // the text (a wrapped pill used to stretch to the stage width) and
    // never shorter (user-reported, 4th time: "name (9)" lost its closing
    // parenthesis; special characters that fall back to another font are
    // covered by the +2px safety margin). Full text always shown (no
    // elision): a text wider than the stage word-wraps instead of
    // clipping.
    const int maxW = edge ? width() - 48 : width() - 16;
    int w, h;
    if (maxW > 0 && fm.horizontalAdvance(text) + 2 * kPadX > maxW) {
        // Word-wrap: the LINE COUNT/height comes from the label's own
        // layout (sizeHint with word wrap at maxW — the same engine that
        // renders, per-glyph grid rounding included), so the border has
        // the exact number of lines. The width is the LONGEST LINE
        // (metrics block) — the pill hugs the text instead of stretching
        // to the stage width — plus a few px for the grid rounding of
        // that line. The content width stays >= the longest line, so the
        // label's own re-wrap can only merge lines, never add them.
        label->setWordWrap(true);
        label->setGeometry(x, y, maxW, maxW);
        const QSize hint = label->sizeHint();
        const QRectF block = fm.boundingRect(
            QRectF(0, 0, maxW - 2 * kPadX, 100000),
            Qt::TextWordWrap | Qt::TextDontClip, text);
        w = qMin(int(qCeil(block.width())) + 2 * kPadX + 4, maxW);
        h = hint.height() + 2;
    } else {
        // Single line: the label's OWN sizeHint — measured by the same
        // layout engine that renders (QFontMetricsF's float advance runs
        // a few px SHORT of the grid-rounded rendered text, which is how
        // the last glyph got clipped). +2px safety margin for special
        // characters that fall back to another font.
        QSize s = label->sizeHint();
        s.setWidth(s.width() + 2);
        if (maxW > 0)
            s.setWidth(qMin(s.width(), maxW));
        s.setHeight(s.height() + 2);
        w = s.width();
        h = s.height();
    }
    label->setGeometry(x, y, w, h);
    label->show();
    raise();

    auto *effect = new QGraphicsOpacityEffect(label);
    label->setGraphicsEffect(effect);
    auto *anim = new QPropertyAnimation(effect, "opacity", label);
    anim->setDuration(300);
    auto *timer = new QTimer(label);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, label, [anim, effect]() {
        anim->setStartValue(effect->opacity());
        anim->setEndValue(0.0);
        anim->start();
    });
    QObject::connect(anim, &QAbstractAnimation::finished, this,
                     [this, label, edge]() {
        if (edge) {
            if (m_edgeCount > 0)
                --m_edgeCount;
            for (int i = m_edgeStack.size() - 1; i >= 0; --i)
                if (m_edgeStack.at(i).first == label)
                    m_edgeStack.removeAt(i);
        }
        label->deleteLater();
    });
    timer->start(visibleMs);
    return label;
}
