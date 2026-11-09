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
    const int w = int(QFontMetricsF(f).horizontalAdvance(text)) + 48;
    makeToast(text, QColor(24, 24, 30, 215), qMax(8, (width() - w) / 2), 36,
              visibleMs, false);
}

void ToastOverlay::showEdge(const QString &text, const QColor &accent,
                            int visibleMs)
{
    if (text.isEmpty())
        return;
    const int y = 36 + m_edgeCount * 34;
    ++m_edgeCount;
    makeToast(text, accent, 20, y, visibleMs, true);
}

void ToastOverlay::makeToast(const QString &text, const QColor &bg, int x,
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
    const int w = int(fm.horizontalAdvance(text)) + 32;
    const int h = edge ? 30 : 44;
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
        if (edge && m_edgeCount > 0)
            --m_edgeCount;
        label->deleteLater();
    });
    timer->start(visibleMs);
}
