#pragma once
// 自定义控件：文件夹按钮、窗口按钮、卡片标题栏、加载细条、可折叠卡片、状态着色与悬停着色。
// 说明：全部为 QSS 主题下的自绘控件；配色取自 theme.h 的 Proto，动效走 animations.h，不写死颜色与时长。
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPointer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QColor>
#include <QString>
#include <functional>

#include "common.h"
#include "theme.h"
#include "animations.h"

// 着色种类：主按钮 / 工具栏按钮 / 侧栏项
enum HoverTintKind { HtPlain = 0, HtPrimary = 1, HtTb = 2, HtNav = 3 };

class FolderBtn : public QPushButton {
public:
    FolderBtn() {
        setFixedSize(40, 40); setCursor(Qt::PointingHandCursor); setToolTip(T("打开数据文件夹"));
        setAttribute(Qt::WA_Hover, true);
    }
    void setTheme(const QColor& fg, const QColor& fgHover, const QColor& hoverBg) {
        m_fg = fg; m_fgHover = fgHover; m_hoverBg = hoverBg; update();
    }
protected:
    void enterEvent(QEnterEvent* e) override { m_hv.to(this, true, [this](qreal) { update(); }); QPushButton::enterEvent(e); }
    void leaveEvent(QEvent* e) override { m_hv.to(this, false, [this](qreal) { update(); }); QPushButton::leaveEvent(e); }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        const qreal h = isDown() ? 1.0 : m_hv.v;   // 按下即时到底，hover 才走过渡
        const bool hover = h > 0.001;
        if (hover) {
            p.setPen(Qt::NoPen);
            p.setBrush(isDown() ? m_hoverBg.darker(112) : mixColor(Qt::transparent, m_hoverBg, h));
            p.drawRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0), 8, 8);
        }
        // 极简线性文件夹：左上标签 → 斜切 → 主体，圆角描边，无填充
        const qreal w = 19, h2 = 14;
        const qreal x = (width() - w) / 2.0, y = (height() - h2) / 2.0 + 1;
        const qreal r = 2.5;
        QPainterPath path;
        path.moveTo(x, y + h2 - r + 2);
        path.lineTo(x, y + 2);
        path.quadTo(x, y, x + r, y);
        path.lineTo(x + w * 0.36, y);
        path.lineTo(x + w * 0.48, y + 3);          // 标签斜切
        path.lineTo(x + w - r, y + 3);
        path.quadTo(x + w, y + 3, x + w, y + 3 + r);
        path.lineTo(x + w, y + h2 - r + 2);
        path.quadTo(x + w, y + h2 + 2, x + w - r, y + h2 + 2);
        path.lineTo(x + r, y + h2 + 2);
        path.quadTo(x, y + h2 + 2, x, y + h2 - r + 2);
        path.closeSubpath();
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(mixColor(m_fg, m_fgHover, h), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(path);
    }
private:
    HoverT m_hv;
    QColor m_fg = QColor("#6A6A72"), m_fgHover = QColor("#1D1D24"), m_hoverBg = QColor("#E9EDF3");
};

class WinBtn : public QPushButton {
public:
    enum Kind { Min, Max, Close };
    WinBtn(Kind k) : m_kind(k) {
        setFixedSize(46, 34); setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }
    void setTheme(const QColor& fg, const QColor& hoverBg, const QColor& closeHover) {
        m_fg = fg; m_hoverBg = hoverBg; m_closeHover = closeHover; update();
    }
    void setRestore(bool r) { m_restore = r; update(); }
protected:
    void enterEvent(QEnterEvent* e) override { m_hv.to(this, true, [this](qreal) { update(); }); QPushButton::enterEvent(e); }
    void leaveEvent(QEvent* e) override { m_hv.to(this, false, [this](qreal) { update(); }); QPushButton::leaveEvent(e); }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        const qreal h = m_hv.v;
        QColor bg = Qt::transparent, fg = m_fg;
        if (m_kind == Close) fg = mixColor(m_fg, QColor("#FFFFFF"), h);
        bg = mixColor(Qt::transparent, (m_kind == Close) ? m_closeHover : m_hoverBg, h);
        if (bg.alpha() > 0) p.fillRect(rect(), bg);
        p.setPen(QPen(fg, 1.6));
        int cx = width() / 2, cy = height() / 2;
        switch (m_kind) {
        case Min:   p.drawLine(cx - 7, cy, cx + 7, cy); break;
        case Max:
            if (m_restore) { p.drawRect(cx - 7, cy - 7, 13, 13); p.drawRect(cx - 3, cy - 3, 10, 10); }
            else p.drawRect(cx - 7, cy - 7, 14, 14);
            break;
        case Close: p.drawLine(cx - 6, cy - 6, cx + 6, cy + 6); p.drawLine(cx - 6, cy + 6, cx + 6, cy - 6); break;
        }
    }
private:
    HoverT m_hv;
    Kind m_kind; bool m_restore = false;
    QColor m_fg = QColor("#808089"), m_hoverBg = QColor("#E9EDF3"), m_closeHover = QColor("#E5484D");
};

class StateTint : public QObject {
public:
    using Fn = std::function<QColor()>;
    // bgFrom/bgTo：hover 底色两端；pressColor：按下时继续混向的目标；fgFrom/fgTo：文字色；
    // bdFrom/bdTo：边框色（输入框焦点用）；radiusFrom/To：圆角（<0 = 不写，避免影响布局无关但会改外观）；
    // focusTrigger：true = 由 FocusIn/FocusOut 驱动（输入框），false = Enter/Leave + 按下（按钮）。
    StateTint(QWidget* w, QString sel, Fn bgFrom, Fn bgTo, Fn pressColor = nullptr,
              Fn fgFrom = nullptr, Fn fgTo = nullptr, Fn bdFrom = nullptr, Fn bdTo = nullptr,
              qreal radiusFrom = -1, qreal radiusTo = -1, bool focusTrigger = false)
        : QObject(w), m_w(w), m_sel(std::move(sel)),
          m_bgFrom(std::move(bgFrom)), m_bgTo(std::move(bgTo)), m_press(std::move(pressColor)),
          m_fgFrom(std::move(fgFrom)), m_fgTo(std::move(fgTo)),
          m_bdFrom(std::move(bdFrom)), m_bdTo(std::move(bdTo)),
          m_rFrom(radiusFrom), m_rTo(radiusTo), m_focus(focusTrigger) {
        w->installEventFilter(this);
        w->setAttribute(Qt::WA_Hover, true);
    }
    // 主题 / 强调色变了：状态中也要立刻换成新令牌色（否则会残留旧主题的过渡色）
    void refresh() { if (m_hover > 0.001 || m_pressT > 0.001) applyState(); }
protected:
    bool eventFilter(QObject*, QEvent* e) override {
        switch (e->type()) {
        case QEvent::Enter:   if (!m_focus) toHover(true); break;
        case QEvent::Leave:   if (!m_focus) { toHover(false); toPress(false); } break;
        case QEvent::FocusIn:  if (m_focus) toHover(true); break;
        case QEvent::FocusOut: if (m_focus) toHover(false); break;
        case QEvent::MouseButtonPress:
            if (!m_focus && static_cast<QMouseEvent*>(e)->button() == Qt::LeftButton) toPress(true);
            break;
        case QEvent::MouseButtonRelease:
            if (!m_focus && static_cast<QMouseEvent*>(e)->button() == Qt::LeftButton) toPress(false);
            break;
        default: break;
        }
        return false;
    }
private:
    void toHover(bool on) {
        const int dur = animMs(kHoverAnimMs);
        if (dur <= 0) { m_hover = on ? 1.0 : 0.0; applyState(); return; }
        if (!m_hoverAnim) {
            m_hoverAnim = new QVariantAnimation(this);
            QObject::connect(m_hoverAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
                m_hover = v.toDouble();
                applyState();
            });
        }
        m_hoverAnim->setEasingCurve(on ? curveStandard() : curveExit());
        m_hoverAnim->stop();
        m_hoverAnim->setDuration(dur);
        m_hoverAnim->setStartValue(m_hover);
        m_hoverAnim->setEndValue(on ? 1.0 : 0.0);
        m_hoverAnim->start();
    }
    // 按下：比 hover 更快（kPressAnimMs），反馈要"立刻跟手"
    void toPress(bool on) {
        const int dur = animMs(kPressAnimMs);
        if (dur <= 0) { m_pressT = on ? 1.0 : 0.0; applyState(); return; }
        if (!m_pressAnim) {
            m_pressAnim = new QVariantAnimation(this);
            m_pressAnim->setEasingCurve(curveStandard());
            QObject::connect(m_pressAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
                m_pressT = v.toDouble();
                applyState();
            });
        }
        m_pressAnim->stop();
        m_pressAnim->setDuration(dur);
        m_pressAnim->setStartValue(m_pressT);
        m_pressAnim->setEndValue(on ? 1.0 : 0.0);
        m_pressAnim->start();
    }
    void applyState() {
        if (!m_w) return;
        // 勾选态（顶部标签的"当前页"）不参与：`:checked` 的强调色是状态表达，不能被 hover/按下冲掉
        auto* btn = qobject_cast<QAbstractButton*>(m_w.data());
        const bool frozen = (btn && btn->isChecked());
        const qreal h = frozen ? 0.0 : m_hover;
        const qreal p = frozen ? 0.0 : m_pressT;
        if (h <= 0.001 && p <= 0.001) { m_w->setStyleSheet(QString()); return; }   // 回到全局 QSS
        QString body;
        if (m_bgFrom && m_bgTo) {
            QColor c = mixColor(m_bgFrom(), m_bgTo(), h);
            if (m_press && p > 0.001) c = mixColor(c, m_press(), p);
            body += "background:" + c.name() + ";";   // 全程不透明，见 backDropOf
        }
        if (m_fgFrom && m_fgTo) body += "color:" + mixColor(m_fgFrom(), m_fgTo(), h).name() + ";";
        if (m_bdFrom && m_bdTo) body += "border-color:" + mixColor(m_bdFrom(), m_bdTo(), h).name() + ";";
        if (m_rFrom >= 0.0 && m_rTo >= 0.0)
            body += QString("border-radius:%1px;").arg(m_rFrom + (m_rTo - m_rFrom) * h, 0, 'f', 1);
        // 两条规则一起挂（都只作用于这个控件自身）：
        //   ① `sel:hover` —— 真机上 hover 态生效，与 qssFor 里那条**同选择器**、控件级更靠后 → 压得住；
        //   ② `sel`       —— 兜底，不依赖 QSS 对 hover 态的判定（离屏/无鼠标环境下 ① 判定不出来，
        //                     自检因此也能截到 hover 终态）；焦点/按下态同理。
        m_w->setStyleSheet(m_sel + ":hover { " + body + " } " + m_sel + " { " + body + " }");
    }
    QPointer<QWidget> m_w;
    QString m_sel;
    Fn m_bgFrom, m_bgTo, m_press, m_fgFrom, m_fgTo, m_bdFrom, m_bdTo;
    qreal m_rFrom = -1, m_rTo = -1;
    bool m_focus = false;
    qreal m_hover = 0.0, m_pressT = 0.0;
    QVariantAnimation* m_hoverAnim = nullptr;
    QVariantAnimation* m_pressAnim = nullptr;
};

class CardHeader : public QWidget {
    Q_OBJECT
public:
    CardHeader(const QString& title, bool collapsible, QWidget* parent = nullptr)
        : QWidget(parent), m_collapsible(collapsible) {
        auto* h = new QHBoxLayout(this);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(0);
        auto* t = new QLabel(title);
        t->setObjectName("cardTitle");                       // 复用 QSS：字体/颜色与原来完全一致
        t->setAttribute(Qt::WA_TransparentForMouseEvents, true);   // 点击落在标题栏本身
        h->addWidget(t);
        h->addStretch();
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        if (m_collapsible) { setCursor(Qt::PointingHandCursor); setAttribute(Qt::WA_Hover, true); }
    }
    void setTheme(const Proto& p) { m_hover = p.hover; m_chev = p.sub; m_accent = p.accent; update(); }
    // 折叠进度：0 = 展开（箭头朝下 ▾）、1 = 收起（箭头朝右 ▸）
    void setScratch(qreal t) { m_chevT = t; update(); }
signals:
    void toggled();
protected:
    void enterEvent(QEnterEvent* e) override { if (m_collapsible) m_hv.to(this, true, [this](qreal) { update(); }); QWidget::enterEvent(e); }
    void leaveEvent(QEvent* e) override { if (m_collapsible) m_hv.to(this, false, [this](qreal) { update(); }); QWidget::leaveEvent(e); }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (m_collapsible && e->button() == Qt::LeftButton && rect().contains(e->position().toPoint())) emit toggled();
        QWidget::mouseReleaseEvent(e);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        if (m_hv.v > 0.001) {   // hover 高亮：表达"这一条可点"（没有阴影/变换可用，颜色是最诚实的提示）
            QColor c = m_hover;
            c.setAlphaF(c.alphaF() * m_hv.v);
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        }
        if (!m_collapsible) return;
        p.save();
        p.translate(width() - 11.0, height() / 2.0);
        p.rotate(-90.0 * m_chevT);
        p.setPen(Qt::NoPen);
        p.setBrush(mixColor(m_chev, m_accent, m_hv.v));   // 箭头随 hover 靠向强调色
        QPolygonF tri; tri << QPointF(-4, -2) << QPointF(4, -2) << QPointF(0, 2.5);
        p.drawPolygon(tri);
        p.restore();
    }
private:
    bool m_collapsible = false;
    qreal m_chevT = 0.0;
    HoverT m_hv;
    QColor m_hover = QColor("#E9EDF3"), m_chev = QColor("#6A6A72"), m_accent = QColor("#326cf3");
};

class LoadBar : public QWidget {
    Q_OBJECT
public:
    explicit LoadBar(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFixedHeight(2);
        hide();
        m_minTimer = new QTimer(this);
        m_minTimer->setSingleShot(true);
        QObject::connect(m_minTimer, &QTimer::timeout, this, [this] { if (m_pendingStop) hideNow(); });
    }
    void setTheme(const Proto& p) { m_accent = p.accent; update(); }
    void start() {
        if (g_noAnim) return;   // 关闭动效：不给运动提示
        m_pendingStop = false;
        m_minTimer->start(kLoadBarMinMs);   // 最短显示窗口：不让"瞬时加载"的细条闪一下就没
        show(); raise();
        if (!m_anim) {
            m_anim = new QVariantAnimation(this);
            m_anim->setDuration(1100);
            m_anim->setLoopCount(-1);            // 不确定进度：一直循环
            m_anim->setEasingCurve(QEasingCurve::Linear);
            QObject::connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
                m_pv = v.toDouble();
                update();
            });
        }
        m_anim->stop();
        m_anim->setStartValue(0.0);
        m_anim->setEndValue(1.0);
        m_anim->start();
    }
    void stop() {
        // 请求收工，但至少要显示完 kLoadBarMinMs（否则缓存命中的加载会闪一下，看着像故障）
        m_pendingStop = true;
        if (!m_minTimer->isActive()) hideNow();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        if (width() <= 0) return;
        QPainter p(this);
        const qreal segW = width() * 0.28;
        const qreal x = -segW + m_pv * (width() + segW);
        QLinearGradient g(x, 0, x + segW, 0);   // 两端透明、中段强调色 → 现代感的"扫过"光带
        QColor a = m_accent; a.setAlphaF(0.0);
        QColor b = m_accent; b.setAlphaF(0.9);
        g.setColorAt(0.0, a);
        g.setColorAt(0.5, b);
        g.setColorAt(1.0, a);
        p.fillRect(QRectF(x, 0, segW, height()), g);
    }
private:
    void hideNow() { if (m_anim) m_anim->stop(); hide(); }
    QColor m_accent = QColor("#326cf3");
    qreal m_pv = 0.0;
    QVariantAnimation* m_anim = nullptr;
    QTimer* m_minTimer = nullptr;
    bool m_pendingStop = false;
};

class CollapseCard : public QFrame {
    Q_OBJECT
public:
    CollapseCard(const QString& title, QWidget* body) : QFrame(nullptr), m_body(body) {
        setObjectName("card");
        auto* v = new QVBoxLayout(this);
        v->setContentsMargins(18, 16, 18, 18);   // 与 cardFrame() 完全一致，保证静止态逐像素相同
        v->setSpacing(14);
        m_header = new CardHeader(title, true);
        v->addWidget(m_header);
        v->addWidget(body, 1);
        connect(m_header, &CardHeader::toggled, this, [this] { setCollapsed(!m_collapsed, true); });
    }
    void setTheme(const Proto& p) { m_header->setTheme(p); }
    bool collapsed() const { return m_collapsed; }
    CardHeader* header() const { return m_header; }   // 自检/截图用：强制标题栏 hover
    void setCollapsed(bool c, bool animate) {
        if (c == m_collapsed) return;
        if (c) {   // 收起前先量当前实际高度（窗口可被缩放，不能用陈旧值）
            m_bodyFull = m_body->height() > 0 ? m_body->height() : m_body->sizeHint().height();
            m_cardFull = height() > 0 ? height() : (m_bodyFull + m_header->height());
        }
        m_collapsed = c;
        const int dur = animate ? animMs(kCardAnimMs) : 0;
        if (dur <= 0) { applyScratch(c ? 1.0 : 0.0); return; }   // 关闭动效：直接落终态
        // B2 父子编排：内容体挂一个 effect，让它"随后到"（展开）/ "先走"（收起）——
        //   容器与内容不同时动，层级因果比"整体缩放"清楚得多。
        if (!m_bodyEff) {
            m_bodyEff = new FadeSlideEffect;
            m_body->setGraphicsEffect(m_bodyEff);
        }
        if (!m_anim) {
            m_anim = new QVariantAnimation(this);
            m_anim->setEasingCurve(QEasingCurve::Linear);   // 形状由 applyScratch 里的曲线分别给
            QObject::connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { applyScratch(v.toDouble()); });
            QObject::connect(m_anim, &QVariantAnimation::finished, this, [this] {
                applyScratch(m_collapsed ? 1.0 : 0.0);   // 精确落终态
                // 摘掉 effect（延迟一拍 + identity 判断：此刻还在回调里，直接删不安全）
                QTimer::singleShot(0, this, [this] {
                    if (m_body && m_bodyEff && m_body->graphicsEffect() == m_bodyEff) {
                        m_body->setGraphicsEffect(nullptr);
                        m_bodyEff = nullptr;
                    }
                });
            });
        }
        m_anim->stop();
        m_anim->setDuration(dur);
        m_anim->setStartValue(m_scratch);
        m_anim->setEndValue(c ? 1.0 : 0.0);
        m_anim->start();
    }
private:
    // raw：0 = 完全展开，1 = 完全收起（动画的原始进度，不含曲线）。
    // 容器高度走 curveEnter（利落），箭头走 curveSpring（过冲 ~4.6% → 收放时有"回弹"的活感），
    // 内容体在容器开到 35% 之后才淡入（展开）/ 在收到 35% 之前就淡出（收起）。
    void applyScratch(qreal raw) {
        m_scratch = raw;
        const qreal t = curveEnter().valueForProgress(raw);
        m_header->setScratch(curveSpring().valueForProgress(raw));
        const qreal open = 1.0 - t;
        if (m_bodyEff) {
            const qreal cp = curveStandard().valueForProgress(qBound(0.0, (open - 0.35) / 0.65, 1.0));
            m_bodyEff->setFrame(cp, 0.0, (1.0 - cp) * 6.0);
        }
        const int bodyH = (int)qRound(m_bodyFull * open);
        m_body->setMaximumHeight(t >= 0.999 ? 0 : bodyH);
        const int cardH = (int)qRound(m_cardFull - m_bodyFull * t);
        if (t <= 0.001 || t >= 0.999) {
            setMaximumHeight(t >= 0.999 ? cardH : QWIDGETSIZE_MAX);
            if (t <= 0.001) m_body->setMaximumHeight(QWIDGETSIZE_MAX);
        } else {
            setMaximumHeight(cardH);
        }
        updateGeometry();
    }
    CardHeader* m_header = nullptr;
    QWidget* m_body = nullptr;
    QVariantAnimation* m_anim = nullptr;
    FadeSlideEffect* m_bodyEff = nullptr;   // 只在折叠动画期间存在
    bool m_collapsed = false;
    qreal m_scratch = 0.0;   // 当前折叠进度
    int m_bodyFull = 0;      // 展开态内容体高度
    int m_cardFull = 0;      // 展开态卡片高度
};

StateTint* attachStateTint(QPushButton* b, const QString& sel, int kind, std::function<Proto()> proto);
