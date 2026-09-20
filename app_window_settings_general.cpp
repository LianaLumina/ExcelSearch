// 通用设置页与其中搜索设置子卡的 AppWindow 方法（B7 从 app_window.h 的类内内联定义搬出，逻辑一字未改）。
// 通用设置：外观（原「界面设置」并入）+ 关闭选项；搜索设置子卡：模糊开关与筛选模式等。
#include "app_window.h"

// 通用设置：目前只有「关闭选项设置」（关闭行为 + 是否询问），后续通用项也放这里
// 通用设置：外观（原「界面设置」并入）+ 关闭选项设置。设置项变多后按"通用"归并，减少左侧标签数量
QWidget* AppWindow::makeGeneralSec() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(16);
    // —— 外观 ——
    v->addWidget(new QLabel(T("主题")));
    auto* trow = new QHBoxLayout; trow->setSpacing(10);
    m_lightBtn = new QPushButton(T("浅色")); m_lightBtn->setObjectName("themeToggle"); m_lightBtn->setCheckable(true);
    m_darkBtn = new QPushButton(T("深色")); m_darkBtn->setObjectName("themeToggle"); m_darkBtn->setCheckable(true);
    QButtonGroup* tbg = new QButtonGroup(this); tbg->addButton(m_lightBtn); tbg->addButton(m_darkBtn);
    connect(m_lightBtn, &QPushButton::clicked, this, [this] { if (m_dark) flipTheme(); });
    connect(m_darkBtn, &QPushButton::clicked, this, [this] { if (!m_dark) flipTheme(); });
    trow->addWidget(m_lightBtn); trow->addWidget(m_darkBtn); trow->addStretch();
    v->addLayout(trow);
    v->addWidget(new QLabel(T("强调色")));
    auto* arow = new QHBoxLayout; arow->setSpacing(10);
    for (const auto& a : m_accents) {
        auto* b = new QPushButton; b->setFixedSize(28, 28); b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(a.name());
        b->setProperty("noHoverTint", true);   // 自带样式表（选中描边），不参与 QSS 状态过渡
        QColor cc = a; connect(b, &QPushButton::clicked, this, [this, cc] { setAccent(cc); });
        m_accentBtns.push_back(b);
        arow->addWidget(b);
    }
    refreshAccentButtons();   // 统一 28px 圆点 + 当前项描边（否则看不出选的是哪个）
    auto* custom = new QPushButton(T("自定义…")); custom->setObjectName("themeBtn");
    connect(custom, &QPushButton::clicked, this, &AppWindow::pickCustomAccent);
    arow->addWidget(custom); arow->addStretch();
    v->addLayout(arow);

    auto* sep = new QFrame; sep->setFrameShape(QFrame::HLine); sep->setObjectName("card"); v->addWidget(sep);

    // —— 关闭选项设置 ——
    v->addWidget(new QLabel(T("关闭选项设置")));
    m_closeAskBtn = new QRadioButton(T("每次询问（关闭时选择关闭方式）"));
    m_closeDirectBtn = new QRadioButton(T("直接关闭程序"));
    m_closeTrayBtn = new QRadioButton(T("最小化到托盘"));
    // 先设初值再连信号，避免构造期触发保存
    refreshCloseRadios();
    QButtonGroup* bg = new QButtonGroup(this);
    bg->addButton(m_closeAskBtn); bg->addButton(m_closeDirectBtn); bg->addButton(m_closeTrayBtn);
    v->addWidget(m_closeAskBtn);
    v->addWidget(m_closeDirectBtn);
    v->addWidget(m_closeTrayBtn);
    connect(m_closeAskBtn, &QRadioButton::toggled, this, [this](bool on) { if (on) { m_closeAction = 0; saveSettings(); } });
    connect(m_closeDirectBtn, &QRadioButton::toggled, this, [this](bool on) { if (on) { m_closeAction = 1; saveSettings(); } });
    connect(m_closeTrayBtn, &QRadioButton::toggled, this, [this](bool on) { if (on) { m_closeAction = 2; saveSettings(); } });
    // 说明书提示开关（对应 MAA 公告的「不显示公告」；内容更新后仍会恢复提示）
    m_manualChk = new QCheckBox(T("启动时不再提示说明书（说明书更新后恢复提示）"));
    m_manualChk->setChecked(m_manualNeverShow);
    v->addWidget(m_manualChk);
    connect(m_manualChk, &QCheckBox::toggled, this, [this](bool on) { m_manualNeverShow = on; saveSettings(); });
    v->addStretch();
    return w;
}
// 搜索设置（原「模糊搜索」分区）：模糊搜索 + 筛选模式，自上而下排布
QWidget* AppWindow::makeSearchSec() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(14);
    v->addWidget(new QLabel(T("模糊搜索")));
    auto* desc = new QLabel(T("勾选后：精确搜索无结果时会补充模糊匹配（rapidfuzz），并与精确结果合并展示。\n关闭后：只保留精确匹配结果。"));
    desc->setObjectName("statLabel"); desc->setWordWrap(true);
    v->addWidget(desc);
    m_fuzzyBox = new QCheckBox(T("启用模糊搜索（默认开）"));
    m_fuzzyBox->setChecked(m_fuzzyEnabled);
    connect(m_fuzzyBox, &QCheckBox::toggled, this, [this](bool on) { m_fuzzyEnabled = on; saveSettings(); doSearch(); });
    v->addWidget(m_fuzzyBox);

    auto* sep = new QFrame; sep->setFrameShape(QFrame::HLine); sep->setObjectName("card"); v->addWidget(sep);

    v->addWidget(new QLabel(T("筛选模式")));
    // 说明放在各自模式的下方（缩进），而不是先给一段总简介再摆两个选项 —— 用户要对着选项看
    auto addModeDesc = [&](const QString& text) {
        auto* d = new QLabel(text);
        d->setObjectName("statLabel"); d->setWordWrap(true);
        d->setContentsMargins(24, 0, 0, 0);   // 缩进到所属单选之下
        v->addWidget(d);
    };
    m_modeStdBtn = new QRadioButton(T("标准模式（每次基于一级搜索结果重新筛）"));
    m_modeChainBtn = new QRadioButton(T("逐级模式（在上一次筛选结果里继续筛）"));
    // 先设初值再连信号，避免构造期触发保存/toast
    (m_chainMode ? m_modeChainBtn : m_modeStdBtn)->setChecked(true);
    QButtonGroup* mbg = new QButtonGroup(this); mbg->addButton(m_modeStdBtn); mbg->addButton(m_modeChainBtn);
    v->addWidget(m_modeStdBtn);
    addModeDesc(T("每次筛选都基于一级搜索结果重新筛，改条件就是换条件。\n"
                  "例：搜「工日」得 54 条 → 输入「辅助」得 26 条；把「辅助」改成「检修」→ 仍在 54 条里筛"));
    v->addWidget(m_modeChainBtn);
    addModeDesc(T("在上一次筛选结果里继续筛，可逐级下钻。\n"
                  "例：搜「工日」得 54 条 → 输入「辅助」得 26 条；再输入「张三」→ 在 26 条里筛出 5 条"));
    connect(m_modeStdBtn, &QRadioButton::toggled, this, [this](bool on) {
        if (!on) return;
        m_chainMode = false; saveSettings();
        showToast(T("已切换为「标准模式」：每次筛选都基于一级搜索结果"));
    });
    connect(m_modeChainBtn, &QRadioButton::toggled, this, [this](bool on) {
        if (!on) return;
        m_chainMode = true; saveSettings();
        showToast(T("已切换为「逐级模式」：在上一次筛选结果里继续筛"));
    });
    v->addStretch();
    return w;
}
