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
    QFont f = font();
    f.setPointSizeF(20);
    f.setBold(true);
    // Same width formula as makeToast (advance CEILED + 32px padding,
    // capped at the stage width with word-wrap) so the centering matches
    // the label's real size.
    const int maxW = width() - 16;
    const int w =
        qMin(int(qCeil(QFontMetricsF(f).horizontalAdvance(text))) + 32, maxW);
    makeToast(text, QColor(24, 24, 30, 215), qMax(8, (width() - w) / 2), 36,
              visibleMs, false);
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
    label->setStyleSheet(QStringLiteral(
        "background: %1; color: #ffffff; border-radius: 8px; padding: 6px 16px;")
        .arg(rgba(bg)));
    const QFontMetricsF fm(f);
    // Full text always shown (no elision): a text wider than the stage
    // word-wraps to a two-line toast instead of clipping (user-reported:
    // the pills were too small for their text).
    const qreal textW = fm.horizontalAdvance(text);
    const int maxW = edge ? width() - 48 : width() - 16;
    int w, h;
    if (textW + 32 > maxW && maxW > 0) {
        label->setWordWrap(true);
        w = maxW;
        h = edge ? 46 : 58; // two lines
    } else {
        // CEIL the advance: int() truncation left the last glyph short by
        // a fraction of a pixel (and the padding is exactly 16px per side).
        w = int(qCeil(textW)) + 32;
        h = edge ? 30 : 44;
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
