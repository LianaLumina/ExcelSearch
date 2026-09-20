// 可配置智能列相关的 AppWindow 方法（B4 从 app_window.h 的类内内联定义搬出，逻辑一字未改）。
// 组成：列选择评分 buildColMap / 表头收集 / 输入解析（内建元数据 → 表头包含 → rapidfuzz 兜底）/
//       取值 cellValueFor / 设置页「智能列设置」子卡与增删改动作。
#include "app_window.h"

// 智能列：按「表头包含关键词 + 精确匹配加权」选列（对齐原版 BuildColMap 的评分规则）
std::map<std::pair<std::string, std::string>, int>  AppWindow::buildColMap(const std::string& target) const {
    std::map<std::pair<std::string, std::string>, int> colMap;
    const auto& hd = m_engine.getHeaders();
    for (const auto& fn : m_engine.getFiles()) {
        auto fit = hd.find(fn);
        if (fit == hd.end()) continue;
        for (const auto& [sn, hdrs] : fit->second) {
            int bestCol = -1, bestScore = -1;
            for (int i = 0; i < (int)hdrs.size(); i++) {
                if (hdrs[i].find(target) == std::string::npos) continue;
                int score = 10000 - (int)hdrs[i].size();
                if (hdrs[i] == target) score += 5000;
                if (score > bestScore) { bestScore = score; bestCol = i; }
            }
            colMap[{fn, sn}] = bestCol;   // -1 = 该表没有这一列
        }
    }
    return colMap;
}
// 去重收集全部工作簿表头（供下拉候选与校验）
QStringList  AppWindow::allHeaderNames() const {
    std::set<std::string> uniq;
    for (const auto& [fn, sheets] : m_engine.getHeaders())
        for (const auto& [sn, hdrs] : sheets)
            for (const auto& h : hdrs) if (!h.empty()) uniq.insert(h);
    QStringList out;
    for (const auto& h : uniq) out << u8(h);
    return out;
}
// 把用户输入解析成「实际可用的列来源」，找不到返回空串。校验与取值共用同一套规则，避免
// 「校验通过但列取不到值」的不一致：
//   1) 内建元数据（工作表 / 行号）原样通过；
//   2) 表头「包含」该文字 → 直接用该表头；
//   3) 否则取 rapidfuzz 相似度最高的表头，分数 ≥ 阈值才认（这就是「模糊分数过低则视为不存在」）。
std::string  AppWindow::resolveColumnSource(const std::string& text) const {
    if (text.empty()) return std::string();
    if (isMetaCol(text)) return text;
    double best = 0; std::string bestH;
    for (const auto& [fn, sheets] : m_engine.getHeaders()) {
        for (const auto& [sn, hdrs] : sheets) {
            for (const auto& h : hdrs) {
                if (h.empty()) continue;
                if (h.find(text) != std::string::npos) return h;
                double s = rapidfuzz::fuzz::ratio(h, text, kColFuzzyMin);
                if (s > best) { best = s; bestH = h; }
            }
        }
    }
    return (best >= kColFuzzyMin) ? bestH : std::string();
}
// 取某结果行在指定来源列上的显示值
std::string  AppWindow::cellValueFor(const SearchResult& r, const std::string& source) const {
    if (source == kMetaSheetCol) return r.sheetName;
    if (source == kMetaRowCol)   return std::to_string(r.row);
    auto it = m_colMapCache.find(source);
    if (it == m_colMapCache.end()) it = m_colMapCache.emplace(source, buildColMap(source)).first;
    auto cit = it->second.find({ r.filename, r.sheetName });
    if (cit == it->second.end() || cit->second < 0) return std::string();
    auto vit = r.rowCells.find(cit->second);
    return vit == r.rowCells.end() ? std::string() : vit->second;
}
QWidget*  AppWindow::makeSmartColSec() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(10);
    auto* hint = new QLabel(T("结果表列可自定义：固定 3 列（序号 / 文件名 / 匹配内容）不可改动，其余 0–4 列由你配置，"
                              "总列数 3–7。列来源可以是内建元数据（工作表 / 行号），也可以是任意工作簿表头里的列名。"));
    hint->setObjectName("statLabel"); hint->setWordWrap(true); v->addWidget(hint);
    v->addWidget(new QLabel(T("当前列（自上而下即结果表从左到右）")));
    m_colList = new QListWidget; m_colList->setMinimumHeight(160); m_colList->setMaximumHeight(220);
    v->addWidget(m_colList);
    enableRowHoverAnim(m_colList, makeProto(m_dark, m_accent), &m_rowDelegates);
    v->addWidget(new QLabel(T("要显示的列名")));
    m_colCombo = new QComboBox; m_colCombo->setEditable(true);
    m_colCombo->setInsertPolicy(QComboBox::NoInsert);
    if (m_colCombo->lineEdit()) m_colCombo->lineEdit()->setPlaceholderText(T("选择或输入列名…"));
    v->addWidget(m_colCombo);
    m_colErr = new QLabel(""); m_colErr->setObjectName("lockErr"); m_colErr->setWordWrap(true); v->addWidget(m_colErr);
    auto* row = new QHBoxLayout; row->setSpacing(10);
    auto* addBtn = new QPushButton(T("添加列")); addBtn->setObjectName("primaryBtn");
    auto* updBtn = new QPushButton(T("更新所选")); updBtn->setObjectName("themeBtn");
    auto* delBtn = new QPushButton(T("删除所选")); delBtn->setObjectName("themeBtn");
    auto* defBtn = new QPushButton(T("恢复默认")); defBtn->setObjectName("themeBtn");
    row->addWidget(addBtn); row->addWidget(updBtn); row->addWidget(delBtn); row->addWidget(defBtn); row->addStretch();
    v->addLayout(row);
    // 选中自定义列时，把它的来源回填到输入框，便于修改
    connect(m_colList, &QListWidget::currentRowChanged, this, [this](int r) {
        if (!m_colCombo) return;
        int idx = r - 2;   // 前两行是固定的序号/文件名
        if (idx >= 0 && idx < (int)m_extraCols.size()) m_colCombo->setCurrentText(u8(m_extraCols[idx]));
    });
    connect(addBtn, &QPushButton::clicked, this, [this] { smartColAdd(); });
    connect(updBtn, &QPushButton::clicked, this, [this] { smartColUpdate(); });
    connect(delBtn, &QPushButton::clicked, this, [this] { smartColRemove(); });
    connect(defBtn, &QPushButton::clicked, this, [this] {
        m_extraCols = defaultExtraCols();
        smartColApplied(T("已恢复默认列"));
    });
    smartColRefresh();
    v->addStretch();
    return w;
}
// 校验并解析输入 → 存「实际表头名」；模糊命中到别的列时给出提示
bool  AppWindow::smartColValidateInput(std::string& out, QString& note) {
    const QString text = m_colCombo ? m_colCombo->currentText().trimmed() : QString();
    if (text.isEmpty()) { smartColError(T("请输入列名")); return false; }
    const std::string input = text.toUtf8().toStdString();
    const std::string resolved = resolveColumnSource(input);
    if (resolved.empty()) {
        smartColError(T("所输入的列不存在，请确认后重新输入"));
        return false;
    }
    out = resolved;
    note.clear();
    if (resolved != input) note = T("已按模糊匹配对应到列：") + u8(resolved);   // 例：核定工日 → 工日
    if (m_colErr) m_colErr->clear();
    return true;
}
void  AppWindow::smartColError(const QString& msg) {
    if (m_colErr) m_colErr->setText(msg);
    showToast(msg);
}
void  AppWindow::smartColAdd() {
    if ((int)m_extraCols.size() >= kMaxResultCols - 3) { smartColError(T("最多 7 列，请先删除一列")); return; }
    std::string src; QString note;
    if (!smartColValidateInput(src, note)) return;
    m_extraCols.push_back(src);
    if (!note.isEmpty() && m_colErr) m_colErr->setText(note);
    smartColApplied(note.isEmpty() ? (T("已添加列：") + u8(src)) : note);
}
void  AppWindow::smartColUpdate() {
    if (!m_colList) return;
    int idx = m_colList->currentRow() - 2;
    if (idx < 0 || idx >= (int)m_extraCols.size()) { smartColError(T("请先选中一个自定义列（固定列不可改动）")); return; }
    std::string src; QString note;
    if (!smartColValidateInput(src, note)) return;
    m_extraCols[idx] = src;
    if (!note.isEmpty() && m_colErr) m_colErr->setText(note);
    smartColApplied(note.isEmpty() ? (T("已更新为：") + u8(src)) : note);
}
void  AppWindow::smartColRemove() {
    if (!m_colList) return;
    int idx = m_colList->currentRow() - 2;
    if (idx < 0 || idx >= (int)m_extraCols.size()) { smartColError(T("请先选中一个自定义列（固定列不可改动）")); return; }
    if ((int)m_extraCols.size() <= 0) return;
    m_extraCols.erase(m_extraCols.begin() + idx);
    smartColApplied(T("已删除该列"));
}
// 配置变更后的统一收尾：保存 → 重建表列 → 刷新设置页预览
void  AppWindow::smartColApplied(const QString& toast) {
    saveSettings();
    applyColumns();
    smartColRefresh();
    showToast(toast);
}
void  AppWindow::smartColRefresh() {
    if (m_colList) {
        m_colList->clear();
        m_colList->addItem(T("序号　【固定】"));
        m_colList->addItem(T("文件名　【固定】"));
        for (const auto& s : m_extraCols) m_colList->addItem(u8(s) + T("　【自定义】"));
        m_colList->addItem(T("匹配内容　【固定】"));
    }
    if (m_colCombo) {
        const QString cur = m_colCombo->currentText();
        m_colCombo->clear();
        m_colCombo->addItem(QString::fromUtf8(kMetaSheetCol));
        m_colCombo->addItem(QString::fromUtf8(kMetaRowCol));
        for (const QString& h : allHeaderNames()) m_colCombo->addItem(h);
        m_colCombo->setCurrentText(cur);
    }
}
