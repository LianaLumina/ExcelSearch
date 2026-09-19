#pragma once
// 动效：缓动曲线、淡入位移动效、抖动、悬停过渡与时长常量（统一在这里调）
#include <QColor>
#include <QString>
#include <QWidget>
#include <QGraphicsEffect>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QPointF>
#include <QListWidget>
#include <QPushButton>
#include <QVector>
#include <vector>
#include "theme.h"

class RowDelegate;   // 行委托（data_model 提供），此处仅用指针

extern bool g_noAnim;

// 动效统一开关：关闭时返回 0，各处据此跳过动画
inline int animMs(int ms) { return g_noAnim ? 0 : ms; }
static const int kDurMicro  = 90;          // 微反馈：按下 / 图标抖动的起步
static const int kDurBase   = 160;         // 标准：页面 / 分区 / 折叠
static const int kDurSlow   = 240;         // 强调：数值滚动 / 结果区入场
static const int kStaggerMs     = 40;      // 父子错峰步进
static const int kRowStaggerMs  = 20;      // 结果表行错峰步进
static const int kRowStaggerMax = 8;       // 参与错峰的最大行数（再多的行直接落终态）
static const int kPageAnimMs   = kDurBase;      // 顶部标签切换：淡入 + 位移
static const int kSecAnimMs    = 150;           // 设置分区 / 高级设置解锁层切换
static const int kEnterSlidePx = 8;             // 切入位移幅度（px）
static const int kCardAnimMs   = 180;           // 可折叠卡片：展开 / 收起高度动画
static const int kHoverAnimMs  = 120;           // hover：底色 / 圆角过渡
static const int kPressAnimMs  = kDurMicro;     // 按下 / 抬起
static const int kFocusAnimMs  = 140;           // 输入框焦点边框过渡
static const int kCountAnimMs  = 420;           // 数值滚动
static const int kToastAnimMs  = 150;           // toast 淡入 / 淡出
static const int kToastRisePx  = 10;            // toast 上浮幅度（px）
static const int kSettleMs     = 760;           // 截图/自检等待上限（覆盖最长的一条动效链）
static const int kLoadBarMinMs = 260;           // 加载细条"最短显示窗口"：缓存命中的瞬时加载也给一次扫过

QEasingCurve curveEnter();
QEasingCurve curveStandard();
QEasingCurve curveExit();
QEasingCurve curveSpring();
QColor mixColor(const QColor& a, const QColor& b, qreal t);
void enterAnim(QWidget* w, int ms, qreal dyPx = kEnterSlidePx, QWidget* child = nullptr);
void shakeAnim(QWidget* w, qreal amp = 6.0, int ms = 260, qreal cycles = 2.5);
QWidget* animChildOf(QWidget* w);

class RowDelegate;   // 行委托：由 data_model 提供，这里只用指针


class FadeSlideEffect : public QGraphicsEffect {
public:
    // 一帧一次：opacity 0..1，dx/dy 为位移像素（正 = 向右/向下，即"从更远处滑进来"）
    void setFrame(qreal opacity, qreal dx, qreal dy) {
        if (qFuzzyCompare(m_opacity + 1.0, opacity + 1.0) && qFuzzyCompare(m_dx + 1.0, dx + 1.0)
            && qFuzzyCompare(m_dy + 1.0, dy + 1.0)) return;
        m_opacity = opacity; m_dx = dx; m_dy = dy;
        update();
    }
protected:
    void draw(QPainter* painter) override {
        if (m_opacity <= 0.001) return;
        // 位移在 effect 内部完成：boundingRectFor 不改，所以画面边缘被自然裁掉——
        // 正好就是"内容从下/上滑入、超出部分先不见"的效果。
        QPoint off;
        const QPixmap pm = sourcePixmap(Qt::LogicalCoordinates, &off, QGraphicsEffect::PadToEffectiveBoundingRect);
        painter->save();
        painter->setOpacity(m_opacity);
        painter->drawPixmap(off + QPoint(qRound(m_dx), qRound(m_dy)), pm);
        painter->restore();
    }
private:
    qreal m_opacity = 1.0, m_dx = 0.0, m_dy = 0.0;
};

struct HoverT {
    qreal v = 0.0;
    QVariantAnimation* anim = nullptr;
    void to(QObject* owner, bool on, const std::function<void(qreal)>& apply, QEasingCurve curve = curveStandard()) {
        const int dur = animMs(kHoverAnimMs);
        if (dur <= 0) {
            if (anim) anim->stop();
            v = on ? 1.0 : 0.0;
            apply(v);
            return;
        }
        if (!anim) {
            anim = new QVariantAnimation(owner);
            QObject::connect(anim, &QVariantAnimation::valueChanged, owner, [this, apply](const QVariant& x) {
                v = x.toDouble();
                apply(v);
            });
        }
        anim->setEasingCurve(curve);
        anim->stop();
        anim->setDuration(dur);
        anim->setStartValue(v);
        anim->setEndValue(on ? 1.0 : 0.0);
        anim->start();
    }
};
