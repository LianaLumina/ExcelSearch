// 搜索历史相关的 AppWindow 方法（B3 从 app_window.h 的类内内联定义搬出，逻辑一字未改）。
// 语义：只有一级搜索记录历史；时效 0 = 关闭程序后删除（不落盘）；下拉与设置页共用这一份数据。
#include "app_window.h"
#include "dialogs.h"
#include <QComboBox>
#include <QDateTime>
#include <QLineEdit>
#include <QMenu>

// ---- 搜索历史 ----
// 按当前时效清理过期条目（时效 = 0「关闭程序后删除」时不做时间清理）
void  AppWindow::purgeHistory() {
    if (m_histTtlMin <= 0) return;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 ttl = (qint64)m_histTtlMin * 60;
    m_history.erase(std::remove_if(m_history.begin(), m_history.end(),
        [&](const HistItem& h) { return h.ts + ttl <= now; }), m_history.end());
}
// 只有一级搜索会调用这里（二级筛选不记录）
void  AppWindow::addHistory(const QString& kw) {
    if (!m_histRecord || kw.isEmpty()) return;
    for (auto it = m_history.begin(); it != m_history.end(); ) {
        if (it->kw == kw) it = m_history.erase(it); else ++it;   // 同词去重（提到最前）
    }
    m_history.insert(m_history.begin(), { kw, QDateTime::currentSecsSinceEpoch() });
    if ((int)m_history.size() > kHistStoreMax) m_history.resize(kHistStoreMax);
    saveSettings();
    refreshHistoryUI();
}
void  AppWindow::clearHistory() {
    m_history.clear();
    saveSettings();
    refreshHistoryUI();
}
// 下拉（最近 m_histShow 条）与设置页列表（全部）统一刷新；显示条数为 0 时隐藏「历史」按钮
void  AppWindow::refreshHistoryUI() {
    if (m_histBtn) m_histBtn->setVisible(m_histShow > 0);
    if (m_histList) {
        m_histList->clear();
        for (size_t i = 0; i < m_history.size(); i++) {
            const QString when = QDateTime::fromSecsSinceEpoch(m_history[i].ts).toString("MM-dd HH:mm");
            m_histList->addItem(when + T("　") + m_history[i].kw);
        }
    }
}
void  AppWindow::showHistoryMenu() {
    purgeHistory();
    if (m_histShow <= 0) { showToast(T("历史下拉已设为「不显示」，可在 设置 → 搜索历史 中调整")); return; }
    const int n = qMin((int)m_history.size(), m_histShow);
    if (n == 0) { showToast(T("暂无搜索历史")); return; }
    QMenu menu(this);
    menu.setStyleSheet(qssFor(makeProto(m_dark, m_accent)));
    for (int i = 0; i < n; i++) menu.addAction(m_history[i].kw);
    QAction* chosen = m_histBtn ? menu.exec(m_histBtn->mapToGlobal(QPoint(0, m_histBtn->height()))) : nullptr;
    if (chosen && m_searchEdit) { m_searchEdit->setText(chosen->text()); doSearch(); }
}
QWidget*  AppWindow::makeHistorySec() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(10);
    auto* hint = new QLabel(T("只有「一级搜索」会记录历史，二级筛选不记录。历史最多保留 20 条，"
                              "下拉最多显示 10 条，其余可在此查看并回填。"));
    hint->setObjectName("statLabel"); hint->setWordWrap(true); v->addWidget(hint);

    v->addWidget(new QLabel(T("下拉显示数量")));
    m_histShowCombo = new QComboBox;
    m_histShowCombo->addItem(T("不显示"), 0);
    for (int i = 1; i <= kHistShowMax; i++) m_histShowCombo->addItem(T("最近 ") + QString::number(i) + T(" 条"), i);
    m_histShowCombo->setCurrentIndex(qBound(0, m_histShow, kHistShowMax));
    v->addWidget(m_histShowCombo);

    v->addWidget(new QLabel(T("删除时效")));
    m_histTtlCombo = new QComboBox;
    m_histTtlCombo->addItem(T("10 分钟"), 10);
    m_histTtlCombo->addItem(T("30 分钟"), 30);
    m_histTtlCombo->addItem(T("1 小时"), 60);
    m_histTtlCombo->addItem(T("6 小时"), 360);
    m_histTtlCombo->addItem(T("12 小时"), 720);
    m_histTtlCombo->addItem(T("关闭程序后删除（不保存）"), 0);
    {
        int idx = m_histTtlCombo->findData(m_histTtlMin);
        m_histTtlCombo->setCurrentIndex(idx >= 0 ? idx : m_histTtlCombo->count() - 1);
    }
    v->addWidget(m_histTtlCombo);

    v->addWidget(new QLabel(T("全部历史（双击可回填并重新搜索）")));
    m_histList = new QListWidget; m_histList->setMinimumHeight(150); m_histList->setMaximumHeight(200);
    v->addWidget(m_histList);
    enableRowHoverAnim(m_histList, makeProto(m_dark, m_accent), &m_rowDelegates);
    auto* row = new QHBoxLayout; row->setSpacing(10);
    auto* useBtn = new QPushButton(T("使用所选")); useBtn->setObjectName("primaryBtn");
    auto* delBtn = new QPushButton(T("删除所选")); delBtn->setObjectName("themeBtn");
    auto* clrBtn = new QPushButton(T("清空全部")); clrBtn->setObjectName("themeBtn");
    row->addWidget(useBtn); row->addWidget(delBtn); row->addWidget(clrBtn); row->addStretch();
    v->addLayout(row);

    connect(m_histShowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!m_histShowCombo) return;
        m_histShow = m_histShowCombo->currentData().toInt();
        saveSettings(); refreshHistoryUI();
    });
    connect(m_histTtlCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!m_histTtlCombo) return;
        m_histTtlMin = m_histTtlCombo->currentData().toInt();
        purgeHistory(); saveSettings(); refreshHistoryUI();
        showToast(m_histTtlMin == 0 ? T("已设为关闭程序后删除（不保存）")
                                    : (T("超过 ") + QString::number(m_histTtlMin) + T(" 分钟的历史将自动删除")));
    });
    auto useSel = [this] {
        if (!m_histList) return;
        const int r = m_histList->currentRow();
        if (r < 0 || r >= (int)m_history.size()) { showToast(T("请先选择一条历史")); return; }
        if (m_searchEdit) { m_searchEdit->setText(m_history[r].kw); doSearch(); }
    };
    connect(useBtn, &QPushButton::clicked, this, useSel);
    connect(m_histList, &QListWidget::itemDoubleClicked, this, [useSel](QListWidgetItem*) { useSel(); });
    connect(delBtn, &QPushButton::clicked, this, [this] {
        if (!m_histList) return;
        const int r = m_histList->currentRow();
        if (r < 0 || r >= (int)m_history.size()) { showToast(T("请先选择一条历史")); return; }
        m_history.erase(m_history.begin() + r);
        saveSettings(); refreshHistoryUI();
        showToast(T("已删除该条历史"));
    });
    connect(clrBtn, &QPushButton::clicked, this, [this] {
        if (m_history.empty()) { showToast(T("当前没有历史记录")); return; }
        bool ok = ConfirmDialog::ask(this, m_dark, m_accent, T("清空历史"),
            T("此操作将会删除全部 %1 条搜索历史，你确认要这么做吗？").arg(m_history.size()), T("清空"));
        if (!ok) return;
        clearHistory();
        showToast(T("已清空搜索历史"));
    });
    refreshHistoryUI();
    v->addStretch();
    return w;
}
