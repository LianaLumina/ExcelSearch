#include "animations.h"
#include "common.h"
#include <QTimer>

bool g_noAnim = false;              // true = 关闭全部动效（UI_PROTO_NO_ANIM=1 / --no-anim）

QEasingCurve curveEnter() {
    QEasingCurve c;
    c.addCubicBezierSegment(QPointF(0.05, 0.7), QPointF(0.1, 1.0), QPointF(1.0, 1.0));
    return c;
}

QEasingCurve curveStandard() {
    QEasingCurve c;
    c.addCubicBezierSegment(QPointF(0.2, 0.0), QPointF(0.0, 1.0), QPointF(1.0, 1.0));
    return c;
}

QEasingCurve curveExit() {
    QEasingCurve c;
    c.addCubicBezierSegment(QPointF(0.3, 0.0), QPointF(0.8, 0.15), QPointF(1.0, 1.0));
    return c;
}

QEasingCurve curveSpring() {
    QEasingCurve c;
    c.setCustomType([](qreal t) -> qreal {
        if (t <= 0.0) return 0.0;
        if (t >= 1.0) return 1.0;
        const qreal zeta = 0.7, omega = 16.0;
        const qreal wd = omega * std::sqrt(1.0 - zeta * zeta);
        return 1.0 - std::exp(-zeta * omega * t) * (std::cos(wd * t) + (zeta * omega / wd) * std::sin(wd * t));
    });
    return c;
}

QColor mixColor(const QColor& a, const QColor& b, qreal t) {
    t = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

void enterAnim(QWidget* w, int ms, qreal dyPx, QWidget* child) {
    if (!w) return;
    const int dur = animMs(ms);
    if (dur <= 0) return;
    auto* eff = new FadeSlideEffect;
    eff->setFrame(0.0, 0.0, dyPx);
    w->setGraphicsEffect(eff);
    FadeSlideEffect* childEff = nullptr;
    if (child) {
        childEff = new FadeSlideEffect;
        childEff->setFrame(0.0, 0.0, dyPx * 0.75);
        child->setGraphicsEffect(childEff);
    }
    auto* a = new QVariantAnimation(eff);
    a->setDuration(dur);
    a->setEasingCurve(curveEnter());
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    QObject::connect(a, &QVariantAnimation::valueChanged, eff, [eff, childEff, dyPx](const QVariant& v) {
        const qreal p = v.toDouble();
        eff->setFrame(p, 0.0, (1.0 - p) * dyPx);
        if (childEff) {   // 内容随后到：容器走到 30% 才开始，用标准曲线收尾
            const qreal cp = curveStandard().valueForProgress(qBound(0.0, (p - 0.3) / 0.7, 1.0));
            childEff->setFrame(cp, 0.0, (1.0 - cp) * dyPx * 0.75);
        }
    });
    QObject::connect(a, &QVariantAnimation::finished, w, [w, eff, child, childEff] {
        // 延迟一拍再摘：此刻仍在 finished 回调里，直接删 effect 会把它的子动画一起删掉。
        // 用 identity 判断避开"动画中途又被新动画顶掉"的情况。
        QTimer::singleShot(0, w, [w, eff, child, childEff] {
            if (w->graphicsEffect() == eff) w->setGraphicsEffect(nullptr);
            if (child && child->graphicsEffect() == childEff) child->setGraphicsEffect(nullptr);
        });
    });
    a->start();
}

void shakeAnim(QWidget* w, qreal amp, int ms, qreal cycles) {
    if (!w) return;
    const int dur = animMs(ms);
    if (dur <= 0) return;
    auto* eff = new FadeSlideEffect;
    w->setGraphicsEffect(eff);
    auto* a = new QVariantAnimation(eff);
    a->setDuration(dur);
    a->setEasingCurve(QEasingCurve::Linear);   // 阻尼振荡的形状由公式给，不再叠曲线
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    QObject::connect(a, &QVariantAnimation::valueChanged, eff, [eff, amp, cycles](const QVariant& v) {
        const qreal t = v.toDouble();
        const qreal dx = amp * std::sin(2.0 * M_PI * cycles * t) * (1.0 - t);   // 振幅随时间衰减
        eff->setFrame(1.0, dx, 0.0);
    });
    QObject::connect(a, &QVariantAnimation::finished, w, [w, eff] {
        QTimer::singleShot(0, w, [w, eff] { if (w->graphicsEffect() == eff) w->setGraphicsEffect(nullptr); });
    });
    a->start();
}

QWidget* animChildOf(QWidget* w) {
    if (!w) return nullptr;
    return qobject_cast<QWidget*>(w->property("animChild").value<QObject*>());
}



