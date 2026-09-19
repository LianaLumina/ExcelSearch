#pragma once
// 使用说明书窗口：左侧章节导航 + 右侧 Markdown 正文 + 左下插图位 + 底部提示复选框与确认按钮。
// 内容来源：程序目录的 MANUAL.md（可外部替换）→ 内嵌副本 :/manual.md 兜底。
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QListWidget>
#include <QTextBrowser>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScreen>
#include <QWindow>
#include <QFile>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QVector>
#include <QPair>
#include <QStringList>
#include <QColor>
#include <QString>
#include <functional>

#include "common.h"
#include "theme.h"
#include "animations.h"
#include "widgets.h"

static const char* kManualNags[3] = {
    "还没看完呢，往下翻翻～", "后面还有内容，别急着关～", "真的不看一下吗？就一点点～"
};
QString manualText();
QVector<QPair<QString, QString>> manualSections(const QString& md);
QString manualOverflowSections();
QString manualHash(const QString& md);

class ManualDialog : public QWidget {
public:
    ManualDialog(bool dark, const QColor& accent, const QString& md, std::function<void(bool)> onClosed, QWidget* parent = nullptr)
        : QWidget(parent), m_onClosed(std::move(onClosed)) {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);   // 独立顶层窗口；无边框（自绘标题区，与主界面一致，不出现原生标题栏）
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowTitle(T("使用说明书"));
        setStyleSheet(qssFor(makeProto(dark, accent)));
        const Proto p = makeProto(dark, accent);
        m_sections = manualSections(md);

        auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0);
        auto* panel = new QWidget; panel->setObjectName("panel");
        auto* pv = new QVBoxLayout(panel); pv->setContentsMargins(16, 10, 16, 14); pv->setSpacing(10);
        auto* tb = new QHBoxLayout;
        auto* title = new QLabel(T("使用说明书")); title->setObjectName("appTitle"); tb->addWidget(title);
        tb->addStretch();
        m_maxBtn = new WinBtn(WinBtn::Max);    // 右上角：最大化/还原（本窗不做关闭按钮）
        m_maxBtn->setTheme(p.text, p.hover, p.closeHover);
        connect(m_maxBtn, &QPushButton::clicked, this, [this] { toggleMax(); });
        tb->addWidget(m_maxBtn);
        pv->addLayout(tb);

        auto* bodyRow = new QHBoxLayout; bodyRow->setSpacing(12);
        auto* leftCol = new QVBoxLayout; leftCol->setSpacing(8);
        m_list = new QListWidget; m_list->setFixedWidth(176);
        m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);   // 目录栏不出现横向拖动条
        m_list->setTextElideMode(Qt::ElideRight);
        for (const auto& s : m_sections) m_list->addItem(s.first);
        leftCol->addWidget(m_list, 1);
        m_deco = new QLabel; m_deco->setObjectName("manualDeco");
        m_deco->setFixedHeight(142); m_deco->setAlignment(Qt::AlignCenter);
        const QString decoPath = QCoreApplication::applicationDirPath() + "/说明书插图.png";
        if (QFile::exists(decoPath)) {
            QPixmap pm(decoPath);
            if (!pm.isNull()) m_deco->setPixmap(pm.scaled(m_deco->width(), m_deco->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        leftCol->addWidget(m_deco);
        bodyRow->addLayout(leftCol);
        m_view = new QTextBrowser; m_view->setObjectName("manualView"); m_view->setOpenExternalLinks(true);
        // 横向滚动条一律去掉：正文强制按窗口宽度换行（宁可把文案改短也不出横条）
        m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setLineWrapMode(QTextEdit::WidgetWidth);
        bodyRow->addWidget(m_view, 1);
        pv->addLayout(bodyRow, 1);

        auto* bot = new QHBoxLayout;
        m_noRemind = new QCheckBox(T("下次更新前不再展示"));
        m_noRemind->setEnabled(false);   // 与 MAA 一致：读完之前不可勾选
        bot->addStretch();
        bot->addWidget(m_noRemind);
        m_btn = new QPushButton(T("确认")); m_btn->setObjectName("primaryBtn"); m_btn->setMinimumWidth(108);
        m_btn->setCursor(Qt::PointingHandCursor);
        connect(m_btn, &QPushButton::clicked, this, [this] { tryClose(); });
        bot->addWidget(m_btn);
        bot->addStretch();   // 与 MAA 一致：复选与确认居中成组
        pv->addLayout(bot);
        root->addWidget(panel);
        resize(880, 566);

        connect(m_list, &QListWidget::currentRowChanged, this, [this](int r) { showSection(r); });
        connect(m_view->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) { checkScrolled(); });
        if (m_list->count()) m_list->setCurrentRow(0);
    }
    bool noRemindChecked() const { return m_noRemind && m_noRemind->isChecked(); }
    void testToggleMax() { toggleMax(); }   // 供自检：验证最大化/还原几何
    // 门禁自检钩子（与 --report 配合，替代人工点击）
    bool testGateOpen() const { return gateOpen(); }
    bool testChkEnabled() const { return m_noRemind && m_noRemind->isEnabled(); }
    QString testNagOnce() { nag(); return m_btn ? m_btn->text() : QString(); }
    void testScrollBottom() {
        if (!m_view) return;
        QScrollBar* sb = m_view->verticalScrollBar();
        sb->setValue(sb->maximum());
        checkScrolled();
    }
    bool testTryClose() { tryClose(); return !isVisible(); }   // 供自检：确认按钮是否真的关掉了窗口
    // 供自检：控件在窗口内的相对位置（便于自动化点击；坐标系与 --report 的窗口一致）
    QString relRects() const {
        auto rel = [this](const QWidget* w) {
            if (!w) return QStringLiteral("-");
            const QPoint p = w->mapTo(this, QPoint(0, 0));
            return QStringLiteral("%1,%2,%3x%4").arg(p.x()).arg(p.y()).arg(w->width()).arg(w->height());
        };
        return QStringLiteral("chk:%1;btn:%2;view:%3").arg(rel(m_noRemind)).arg(rel(m_btn)).arg(rel(m_view));
    }
    // 供自检：目录栏里是否有标题被省略号截断（横向滚动条已关闭 → 截断就等于看不全）
    QString navReport() const {
        if (!m_list) return QStringLiteral("none");
        const int avail = m_list->viewport() ? m_list->viewport()->width() : m_list->width();
        QFontMetrics fm(m_list->font());
        QStringList cut;
        for (int i = 0; i < m_list->count(); ++i) {
            const QString s = m_list->item(i)->text();
            if (fm.horizontalAdvance(s) + 22 > avail) cut << s;   // 22 ≈ 条目左右内边距
        }
        return cut.isEmpty() ? QStringLiteral("none") : cut.join(QStringLiteral(" / "));
    }
    // 供自检：窗口形态（独立顶层窗口 + 右上角是最大化按钮 + 两条滚动条都关闭 + 复选文案）
    QString modeReport() const {
        const bool top = (windowType() == Qt::Window);   // 独立顶层窗口（Qt::Dialog 含 Window 位，不能用位与判断）
        const bool frameless = (windowFlags() & Qt::FramelessWindowHint) != 0;
        const QRect fg = frameGeometry(), geo = geometry();
        return QStringLiteral("%1,btn=%2,viewhbar=%3,listhbar=%4,chk=%5")
            .arg(top ? QStringLiteral("toplevel") : QStringLiteral("NOT-toplevel"))
            .arg(m_maxBtn ? QStringLiteral("max") : QStringLiteral("none"))
            .arg(m_view ? (m_view->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff ? QStringLiteral("off") : QStringLiteral("on")) : QStringLiteral("?"))
            .arg(m_list ? (m_list->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff ? QStringLiteral("off") : QStringLiteral("on")) : QStringLiteral("?"))
            .arg(m_noRemind ? m_noRemind->text() : QString())
            + QStringLiteral(",frameless=%1,frameDelta=%2x%3")
                  .arg(frameless ? QStringLiteral("yes") : QStringLiteral("no"))
                  .arg(fg.width() - geo.width()).arg(fg.height() - geo.height());
    }
protected:
    void closeEvent(QCloseEvent* e) override {   // 关闭也受门禁约束（Alt+F4 同样拦住）
        if (!gateOpen()) { e->ignore(); nag(); return; }
        if (m_onClosed) m_onClosed(noRemindChecked());
        QWidget::closeEvent(e);
    }
    void mousePressEvent(QMouseEvent* e) override {   // 无边框：拖动标题区移动
        if (e->button() == Qt::LeftButton && e->position().y() < kTitleStrip) {
            if (windowHandle()) windowHandle()->startSystemMove();
            return;
        }
        QWidget::mousePressEvent(e);
    }
    void mouseDoubleClickEvent(QMouseEvent* e) override {   // 双击标题区 = 最大化/还原
        if (e->position().y() < kTitleStrip) { toggleMax(); return; }
        QWidget::mouseDoubleClickEvent(e);
    }
    void keyPressEvent(QKeyEvent* e) override {   // Esc 与「确认」同义（同样受门禁约束）
        if (e->key() == Qt::Key_Escape) { tryClose(); return; }
        QWidget::keyPressEvent(e);
    }
private:
    static const int kTitleStrip = 44;   // 顶部拖动/双击区高度
    bool gateOpen() const { return m_readToBottom || noRemindChecked(); }
    void toggleMax() {
        if (m_maxed) { setGeometry(m_normalGeo); m_maxed = false; }
        else {
            m_normalGeo = geometry();
            const QRect av = screen() ? screen()->availableGeometry() : QRect(0, 0, 1280, 800);
            setGeometry(av); m_maxed = true;      // 无边框窗口自己最大化到"可用区域"（不盖任务栏）
        }
        if (m_maxBtn) m_maxBtn->setRestore(m_maxed);
    }
    void showSection(int idx) {
        if (idx < 0 || idx >= (int)m_sections.size()) return;
        m_view->setMarkdown(m_sections[idx].second);
        m_view->verticalScrollBar()->setValue(0);
        // 注意：读完标记**不随切换章节重置**——与 MAA 的 HasEverScrolledToBottom 一致（读过一次即长期有效）
        QTimer::singleShot(0, this, [this] { checkScrolled(); });   // 内容不足一屏时视为已读完
    }
    void checkScrolled() {
        QScrollBar* sb = m_view->verticalScrollBar();
        if (sb->maximum() <= 0 || sb->value() >= sb->maximum() - 10) {
            if (!m_readToBottom) {
                m_readToBottom = true;
                if (m_noRemind) m_noRemind->setEnabled(true);   // 读完才允许勾「不再展示」（同 MAA）
                if (m_btn && m_nag > 0) m_btn->setText(T("确认"));   // 读完后按钮文案复原（同 MAA）
            }
        }
    }
    void nag() {   // 未读完时的调侃（与 MAA 一致：三句之后每次追加 "?"，20 次以上放行）
        if (m_nag < 3) m_btn->setText(QString::fromUtf8(kManualNags[m_nag++]));
        else { m_btn->setText(m_btn->text() + QStringLiteral("?")); if (++m_stubborn > 20) { m_readToBottom = true; close(); } }
    }
    void tryClose() {
        if (gateOpen()) { close(); return; }
        nag();
    }
    QVector<QPair<QString, QString>> m_sections;
    std::function<void(bool)> m_onClosed;
    QListWidget* m_list = nullptr; QTextBrowser* m_view = nullptr; QLabel* m_deco = nullptr;
    QCheckBox* m_noRemind = nullptr; QPushButton* m_btn = nullptr; WinBtn* m_maxBtn = nullptr;
    bool m_readToBottom = false; int m_nag = 0; int m_stubborn = 0;
    bool m_maxed = false; QRect m_normalGeo;
};
