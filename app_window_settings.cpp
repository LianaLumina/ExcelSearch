// 设置页外壳、关于页与预留实用工具页的 AppWindow 方法（B7 从 app_window.h 的类内内联定义搬出，逻辑一字未改）。
// 组成：分区切换 onSettingsSectionChanged（进入「高级设置」未解锁则显示解锁层、离开重新上锁）、
//       设置页外壳 makeSettingsPage（左分区列表 + 右内容栈）、关于页 makeAboutSec、
//       预留实用工具页 makeUtilPage 与注册入口 registerUtilTool（kEnableUtilTab=false，当前不启用）。
#include "app_window.h"

// 设置页分区切换：进入「高级设置」若未解锁则显示解锁层；离开则重新上锁
void AppWindow::onSettingsSectionChanged(int row) {
    if (row == m_advSecIndex) {
        const int want = m_advUnlocked ? 1 : 0;
        const bool gateChanged = m_advGateStack && m_advGateStack->currentIndex() != want;
        if (m_advGateStack) m_advGateStack->setCurrentIndex(want);
        if (m_advUnlocked) {
            refreshBlockAdmin();
        } else {
            if (m_advPwdEdit) { m_advPwdEdit->clear(); m_advPwdEdit->setFocus(); }
            if (m_advErrLabel) m_advErrLabel->clear();
        }
        if (gateChanged) advGateAnim();   // 解锁层 / 真实内容进出都淡入
    } else if (m_advUnlocked) {
        m_advUnlocked = false;   // 离开「高级设置」→ 重新上锁
        m_adminIsSuper = false;
    }
}
// 【预留接口】注册一个实用工具；makeUtilPage() 会自动把它排进左侧列表
void AppWindow::registerUtilTool(const QString& title, std::function<QWidget*()> make) {
    m_utilTools.push_back({ title, make });
}
// 【预留接口】实用工具页：空表时给明确空态，避免以后启用时出现"白页"被当成 bug
QWidget* AppWindow::makeUtilPage() {
    auto* page = new QWidget; auto* h = new QHBoxLayout(page); h->setContentsMargins(0, 0, 0, 0); h->setSpacing(14);
    if (m_utilTools.empty()) {
        auto* empty = new QLabel(T("暂无实用工具。\n低频辅助功能会集中在这里，不占用主界面。"));
        empty->setObjectName("statLabel"); empty->setAlignment(Qt::AlignCenter); empty->setWordWrap(true);
        h->addWidget(empty, 1);
        return page;
    }
    auto* nav = new QListWidget; nav->setFixedWidth(168);
    for (const auto& t : m_utilTools) nav->addItem(t.title);
    h->addWidget(nav);
    enableRowHoverAnim(nav, makeProto(m_dark, m_accent), &m_rowDelegates);
    auto* stack = new QStackedWidget;
    for (const auto& t : m_utilTools) stack->addWidget(cardFrame(t.title, t.make()));
    h->addWidget(stack, 1);
    connect(nav, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);
    nav->setCurrentRow(0);
    return page;
}
QWidget* AppWindow::makeSettingsPage() {
    auto* page = new QWidget; auto* h = new QHBoxLayout(page); h->setContentsMargins(0, 0, 0, 0); h->setSpacing(14);
    m_navList = new QListWidget; m_navList->setFixedWidth(168);
    for (const auto& s : m_secs) m_navList->addItem(s.title);
    h->addWidget(m_navList);
    enableRowHoverAnim(m_navList, makeProto(m_dark, m_accent), &m_rowDelegates);
    m_secStack = new QStackedWidget;
    for (const auto& s : m_secs) m_secStack->addWidget(cardFrame(s.title, s.make()));
    h->addWidget(m_secStack, 1);
    connect(m_navList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_secStack) {
            // 设置分区切换：淡入 + 轻微上移（只对真正换了分区时做）
            const bool changed = (m_secStack->currentIndex() != row);
            m_secStack->setCurrentIndex(row);
            if (changed) enterAnim(m_secStack->currentWidget(), kSecAnimMs, kEnterSlidePx, animChildOf(m_secStack->currentWidget()));
        }
        onSettingsSectionChanged(row);
    });
    for (int i = 0; i < (int)m_secs.size(); i++) if (m_secs[i].title == T("高级设置")) m_advSecIndex = i;
    m_navList->setCurrentRow(0);
    return page;
}
QWidget* AppWindow::makeAboutSec() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(8);
    v->addWidget(new QLabel(T("Excel 表格关键字搜索工具")));
    auto* ver = new QLabel(T("版本 v0.3.2 · Qt6")); ver->setObjectName("statLabel");
    v->addWidget(ver);
    v->addWidget(new QLabel(T("作者 / 发行者：觉心恋影")));
    v->addWidget(new QLabel(projectLicense().isEmpty() ? T("开源许可：（待定）") : (T("开源许可：") + projectLicense())));
    v->addWidget(new QLabel(T("本程序以动态链接方式使用 Qt（LGPL-3.0）；第三方组件许可见 licenses 目录。")));
    v->addWidget(new QLabel(T("界面视觉风格参考自 MAA / MaaWpfGui 与 MaaEnd。")));
    auto* row = new QHBoxLayout;
    auto* manBtn = new QPushButton(T("使用说明书")); manBtn->setObjectName("primaryBtn");
    manBtn->setCursor(Qt::PointingHandCursor);
    connect(manBtn, &QPushButton::clicked, this, [this] { openManual(); });
    row->addWidget(manBtn);
    auto* btn = new QPushButton(T("查看开源声明")); btn->setObjectName("themeBtn");
    connect(btn, &QPushButton::clicked, this, [] {
        const QString dir = QCoreApplication::applicationDirPath() + "/licenses";
        QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(dir).exists() ? dir : QCoreApplication::applicationDirPath()));
    });
    row->addWidget(btn); row->addStretch();
    v->addLayout(row);
    v->addStretch();
    return w;
}
