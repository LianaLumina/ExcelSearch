// 搜索页与结果表的 AppWindow 方法（B9 从 app_window.h 的类内内联定义搬出，逻辑一字未改）。
// 组成：结果表列与内容（applyColumns / fillTable / statCell / makeStatCard / makeResultCard /
//       setStat / updateStats）、搜索与筛选（doSearch / applyFilterChain / doFilter / clearFilter /
//       buildMarkResults）、行交互（selectedRows / onTableContextMenu / markSelected /
//       blockSelectedEntries / blockSelectedFiles / showDetail / exportSelected）、搜索页装配 makeSearchPage。
// 说明：showToast（窗口级提示）与 cardFrame（通用卡片工厂，设置页也用）留给 B11 窗口外壳；
//       setHitStat / showHitTip 是单行内联，按约定留在类内。
#include "app_window.h"
#include <QHeaderView>
#include <QMessageBox>

void AppWindow::applyColumns() {
    if (!m_table) return;
    QStringList heads; heads << T("序号") << T("文件名");
    for (const auto& s : m_extraCols) heads << u8(s);
    heads << T("匹配内容");
    m_table->setColumnCount(heads.size());
    m_table->setHorizontalHeaderLabels(heads);
    for (int c = 0; c < heads.size(); c++) {
        int w = (c == 0) ? 56 : (c == 1 ? 180 : (c == heads.size() - 1 ? 220 : 110));
        m_table->setColumnWidth(c, w);
    }
    fillTable(m_results);
}
// 标记筛选：从已标记清单反查行内容重建结果（对齐原版「已标记颜色」分支）
std::vector<SearchResult> AppWindow::buildMarkResults(int colorFilter) const {
    std::vector<SearchResult> out;
    const auto& fd = m_engine.getFileData();
    const auto& hd = m_engine.getHeaders();
    for (const auto& [key, color] : m_marks.marked) {
        if (colorFilter >= 0 && color != colorFilter) continue;
        const auto& [fn, sn, row] = key;
        auto fit = fd.find(fn);            if (fit == fd.end()) continue;
        auto sit = fit->second.find(sn);   if (sit == fit->second.end()) continue;
        auto rit = sit->second.find(row);  if (rit == sit->second.end()) continue;

        SearchResult sr;
        sr.filename = fn; sr.sheetName = sn; sr.row = row;
        sr.rowCells = rit->second;
        sr.col = sr.rowCells.empty() ? 0 : sr.rowCells.begin()->first;
        sr.colName = XlsxReader::colIndexToName(sr.col);
        sr.matchedValue = sr.rowCells.empty() ? "" : sr.rowCells.begin()->second;
        // 标记筛选态：「匹配内容」列改显「任务标题」（该表没有这一列则回落为行首单元格）
        auto rw = m_renWuColMap.find({ fn, sn });
        if (rw != m_renWuColMap.end() && rw->second >= 0) {
            auto cit = sr.rowCells.find(rw->second);
            if (cit != sr.rowCells.end() && !cit->second.empty()) {
                sr.matchedValue = cit->second;
                sr.col = rw->second;
                sr.colName = XlsxReader::colIndexToName(sr.col);
            }
        }
        auto hf = hd.find(fn);
        if (hf != hd.end()) { auto hs = hf->second.find(sn); if (hs != hf->second.end()) sr.headers = hs->second; }
        out.push_back(std::move(sr));
    }
    return out;
}
// A2 数值滚动：把统计数字从"当前显示值"滚到目标值（curveCount = OutExpo）。
// 四条要点：
//   ① 结束**精确**落到目标整数（绝不停在中间值）——数字是有业务含义的，不能只求好看；
//   ② 滚动期间按目标文本预留最小宽度（只到 advance，不超过 sizeHint → 不会顶动布局），
//      结束复位 → 静止态与关闭动效时逐像素一致；
//   ③ 同一控件再次触发时从"当前显示值"续滚（不跳回 0）；
//   ④ 关闭动效（dur<=0）时直接 setText。
void AppWindow::setStat(QPushButton* btn, qulonglong target, int delayMs) {
    if (!btn) return;
    const QString targetText = QString::number(target);
    auto it = m_statAnims.find(btn);
    QVariantAnimation* prev = (it == m_statAnims.end()) ? nullptr : it->second;
    const qulonglong from = btn->property("animVal").isValid() ? btn->property("animVal").toULongLong()
                                                              : btn->text().toULongLong();
    if (prev) prev->stop();
    const int dur = animMs(kCountAnimMs);
    // 只有**变大**才滚动（数字在"到达"）。变小一律直接落值 ——
    //   返工记录：清空索引（重新加载开头会把引擎清掉 → updateStats 写 0）曾被做成"倒数动画"，
    //   于是点一次「重新加载」就看到"索引记录"从 43846 一路滚到 0，像是数据在被删。
    //   复位是状态清零，不是数值到达，不该有过程。
    if (dur <= 0 || from == target || target < from) {
        btn->setMinimumWidth(0);
        btn->setProperty("animVal", QVariant::fromValue(target));
        btn->setText(targetText);
        return;
    }
    auto* a = new QVariantAnimation(btn);
    a->setDuration(dur);
    a->setEasingCurve(curveCount());
    a->setStartValue((double)from);
    a->setEndValue((double)target);
    btn->setMinimumWidth(btn->fontMetrics().horizontalAdvance(targetText));   // 防"数字变长顶动布局"
    QObject::connect(a, &QVariantAnimation::valueChanged, btn, [btn](const QVariant& v) {
        const qulonglong cur = (qulonglong)qMax<qreal>(0.0, (qreal)std::llround(v.toDouble()));
        btn->setProperty("animVal", QVariant::fromValue(cur));
        btn->setText(QString::number(cur));
    });
    QObject::connect(a, &QVariantAnimation::finished, btn, [btn, target, targetText] {
        btn->setMinimumWidth(0);
        btn->setProperty("animVal", QVariant::fromValue(target));
        btn->setText(targetText);   // 精确落定
    });
    m_statAnims[btn] = a;
    if (delayMs > 0) QTimer::singleShot(animMs(delayMs), btn, [a] { a->start(); });
    else a->start();
}
void AppWindow::updateStats() {
    if (m_statFiles) setStat(m_statFiles, (qulonglong)m_loadedFiles, 0);
    if (m_statIndex) setStat(m_statIndex, (qulonglong)m_engine.getEntryCount(), kStaggerMs);
}
void AppWindow::doSearch() {
    if (!m_searchEdit) return;
    // 对齐原版：存在二级筛选结果时不允许直接发起一级搜索，需先清除筛选
    if (m_filterEdit && !m_filterEdit->text().isEmpty()) {
        showToast(T("当前存在二级筛选结果，请先清除筛选或将二级搜索框清空"));
        m_filterEdit->setFocus();
        return;
    }
    std::string keyword = m_searchEdit->text().toUtf8().toStdString();
    if (keyword.empty()) { fillTable({}); m_results.clear(); m_markedFilterActive = false; setHitStat(0); m_lastExact = m_lastFuzzy = 0; if (m_status) m_status->setText(T("请输入搜索关键词")); return; }
    addHistory(m_searchEdit->text().trimmed());   // 仅一级搜索记录历史（二级筛选不记录）
    std::vector<SearchResult> res; int mode = 0;
    // 标记筛选：关键词 "已标记颜色" / "已标记<色名>"（先于正则与普通搜索判定，对齐原版）
    int markColor = -2;   // -2 = 非标记筛选；-1 = 全部已标记；0..4 = 指定颜色
    {
        const std::string mkPrefix = "已标记";
        if (keyword == mkPrefix + "颜色") markColor = -1;
        else if (keyword.rfind(mkPrefix, 0) == 0) {
            int c = markColorIndexByName(keyword.substr(mkPrefix.size()));
            if (c >= 0) markColor = c;
        }
    }
    m_markedFilterActive = (markColor >= -1);
    if (markColor >= -1) {
        res = buildMarkResults(markColor);
        // ★回迁注意：原版该分支不过 FilterBlocked（已屏蔽项仍会出现在「已标记」视图）；此处做了修正。
        //   保留修正时，「标记清除」的计数含被屏蔽条目，需在文案里说明。详见 docs/回迁标注.md 第 2 条。
        m_lastBlocked = (int)m_marks.filterBlocked(res);
        m_lastExact = (int)res.size(); m_lastFuzzy = 0;
        fillTable(res);
        m_results = res;
        m_baseResults = res; m_filterChain.clear();   // ★一级结果作为二级筛选的唯一基准
        setHitStat((qulonglong)res.size());
        if (m_status) {
            QString tail = m_lastBlocked > 0 ? T("（已屏蔽 ") + QString::number(m_lastBlocked) + T(" 条）") : QString();
            m_status->setText((markColor >= 0 ? T("已标记") + markColorName(markColor) : T("已标记颜色"))
                              + T(": ") + QString::number((qulonglong)res.size()) + T(" 条结果") + tail);
            m_baseStatus = m_status->text();
        }
        return;
    }
    if (keyword.rfind("re:", 0) == 0) {
        res = m_engine.regexSearch(keyword.substr(3)); m_lastExact = (int)res.size(); m_lastFuzzy = 0; mode = 2;
    } else {
        std::vector<SearchResult> exact = m_engine.search(keyword);
        res = exact;
        // 模糊补充：★性能修复（冒烟实测）——原来用「fuzzy × exact」双重循环去重，是 O(n×m)，
        //   10 万 × 10 万 ≈ 1e10 次比较，短关键词 + 大索引下会把一次搜索拖到 **75 秒**。
        //   改为：① 精确结果先入 set，模糊补充用 O(1) 查重（整体 O(n+m)）；
        //         ② 精确结果已经很多（≥ kFuzzySupplementMaxExact）时跳过模糊补充 ——
        //            结果集已有几千条，再补近义词对用户没有意义，还白扫一遍全索引。
        if (m_fuzzyEnabled && exact.size() < kFuzzySupplementMaxExact) {
            std::set<std::tuple<std::string, std::string, int>> seen;
            for (const auto& e : exact) seen.insert({ e.filename, e.sheetName, e.row });
            std::vector<SearchResult> fuzzy = m_engine.fuzzySearch(keyword, 60.0);
            for (const auto& f : fuzzy)
                if (seen.insert({ f.filename, f.sheetName, f.row }).second) res.push_back(f);
        }
        m_lastExact = (int)exact.size(); m_lastFuzzy = (int)res.size() - (int)exact.size();
        mode = (m_lastFuzzy > 0) ? 1 : 0;
    }
    m_lastBlocked = (int)m_marks.filterBlocked(res);   // 屏蔽统一在搜索结果上过滤（对齐原版 FilterBlocked）
    fillTable(res);
    m_results = res;
    setHitStat((qulonglong)res.size());
    if (m_status) {
        QString tail = m_lastBlocked > 0 ? T("（已屏蔽 ") + QString::number(m_lastBlocked) + T(" 条）") : QString();
        if (res.empty()) m_status->setText(T("未找到与 '") + u8(keyword) + T("' 匹配的内容"));
        else if (mode == 2) m_status->setText(T("正则搜索 '") + u8(keyword) + T("' 命中 ") + QString::number((qulonglong)res.size()) + T(" 条") + tail);
        else if (mode == 1) m_status->setText(T("搜索 '") + u8(keyword) + T("'（含模糊补充）命中 ") + QString::number((qulonglong)res.size()) + T(" 条") + tail);
        else m_status->setText(T("搜索 '") + u8(keyword) + T("' 命中 ") + QString::number((qulonglong)res.size()) + T(" 条") + tail);
        m_baseStatus = m_status->text();
    }
    m_baseResults = res; m_filterChain.clear();   // ★一级结果作为二级筛选的唯一基准
}
// ★回迁注意：二级筛选从「一级结果快照」逐级重算，绝不就地缩。
//   原版 V0.3.0 的 DoFilter 是在 g_results 上原地 filterResults 并覆盖，导致"改个条件会在
//   旧结果里继续缩"；本版统一为「条件链 + 从基准重算」，标准模式链长恒为 1。
void AppWindow::applyFilterChain() {
    std::vector<SearchResult> cur = m_baseResults;
    for (const auto& cond : m_filterChain) {
        const std::string kw = cond.toUtf8().toStdString();
        if (m_markedFilterActive) {   // 标记筛选态：条件若是色名 → 按颜色精筛
            const int ci = markColorIndexByName(kw);
            if (ci >= 0) {
                std::vector<SearchResult> next;
                for (const auto& r : cur) if (m_marks.colorOf(r) == ci) next.push_back(r);
                cur.swap(next);
                continue;
            }
        }
        cur = SearchEngine::filterResults(cur, kw);
    }
    m_lastBlocked = 0;   // 基准已过屏蔽过滤，逐级重算无需再过滤
    fillTable(cur);
    m_results = cur;
    setHitStat((qulonglong)cur.size());
}
// 二级筛选：标准模式 = 换条件；逐级模式 = 追加条件。两者都从基准重算。
void AppWindow::doFilter() {
    if (!m_filterEdit) return;
    const std::string kw = m_filterEdit->text().toUtf8().toStdString();
    if (kw.empty()) { showToast(T("请输入二级筛选关键词")); return; }
    if (m_baseResults.empty()) {
        showToast(T("一级搜索内容为空，无法使用二级筛选，请先搜索"));
        m_filterEdit->clear();
        return;
    }
    // before 的语义随模式变化：标准模式是基准条数，逐级模式是上一级条数
    const size_t before = m_chainMode ? m_results.size() : m_baseResults.size();
    if (m_chainMode) m_filterChain.push_back(m_filterEdit->text().trimmed());
    else { m_filterChain.clear(); m_filterChain.push_back(m_filterEdit->text().trimmed()); }
    applyFilterChain();
    const size_t after = m_results.size();
    if (m_status) {
        m_status->setText((m_markedFilterActive && markColorIndexByName(kw) >= 0 ? T("颜色筛选 '") : T("筛选 '"))
                          + u8(kw) + T("': ") + QString::number((qulonglong)before) + T(" -> ")
                          + QString::number((qulonglong)after) + T(" 条结果"));
    }
    if (m_filterChain.size() > 1 && m_status) {
        m_status->setText(m_status->text() + T("　[逐级 ") + QString::number((qulonglong)m_filterChain.size()) + T(" 级]"));
    }
    if (after == 0) {
        showToast((m_markedFilterActive && markColorIndexByName(kw) >= 0)
                  ? (T("未筛选到「") + u8(kw) + T("」标记的内容"))
                  : (T("未筛选到含有「") + u8(kw) + T("」的内容")));
    }
}
// 清除二级筛选：清空条件链与输入框，直接从基准还原（不再重跑一级搜索，也不重复记历史）
void AppWindow::clearFilter() {
    if (m_filterEdit) m_filterEdit->clear();
    m_filterChain.clear();
    if (m_baseResults.empty()) {
        m_results.clear(); m_markedFilterActive = false;
        fillTable({});
        setHitStat(0);
        if (m_status) m_status->setText(QString());
        return;
    }
    applyFilterChain();
    if (m_status) m_status->setText(m_baseStatus);
}
void AppWindow::showDetail(int row) {
    if (row < 0 || row >= (int)m_results.size()) return;
    const auto& r = m_results[row];
    QString s;
    s += T("文件：") + u8(r.filename) + T("\n工作表：") + u8(r.sheetName) + T("\n行号：") + QString::number(r.row) + T("\n匹配单元格：") + u8(r.colName) + T(" = ") + u8(r.matchedValue) + T("\n\n");
    int maxCol = 0; for (const auto& [c, _] : r.rowCells) if (c > maxCol) maxCol = c;
    for (int c = 0; c <= maxCol; c++) {
        auto it = r.rowCells.find(c); if (it == r.rowCells.end() || it->second.empty()) continue;
        QString line = T("  ") + u8(XlsxReader::colIndexToName(c));
        if (c < (int)r.headers.size() && !r.headers[c].empty()) line += T(" [") + u8(r.headers[c]) + T("]");
        line += T("：") + u8(it->second) + T("\n"); s += line;
    }
    DetailDialog dlg(m_dark, m_accent, T("行详情 - ") + u8(r.filename), s, this);
    dlg.exec();
}
void AppWindow::exportSelected() {
    if (!m_table) return;
    QModelIndexList rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) { QMessageBox::information(this, T("提示"), T("请先在结果列表中选中要导出的行")); return; }
    std::vector<SearchResult> sel;
    for (auto& idx : rows) { int r = idx.row(); if (r >= 0 && r < (int)m_results.size()) sel.push_back(m_results[r]); }
    if (sel.empty()) { QMessageBox::information(this, T("提示"), T("没有可导出的数据")); return; }
    std::string dir = QCoreApplication::applicationDirPath().toStdString() + "/导出";
    std::filesystem::create_directories(std::filesystem::u8path(dir));
    std::string out = dir + "/导出.xlsx";
    if (exportXlsxTo(sel, out)) QMessageBox::information(this, T("导出成功"), u8(out));
    else QMessageBox::warning(this, T("导出失败"), T("写入文件失败"));
}
std::vector<int> AppWindow::selectedRows() const {
    std::vector<int> sel;
    if (!m_table) return sel;
    for (const auto& idx : m_table->selectionModel()->selectedRows()) sel.push_back(idx.row());
    std::sort(sel.begin(), sel.end());
    return sel;
}
// 结果表右键菜单：屏蔽 / 导出 / 行详情（标记子菜单在第 3 步接入）
void AppWindow::onTableContextMenu(const QPoint& pos) {
    if (!m_table) return;
    int idx = m_table->indexAt(pos).row();
    if (idx < 0 || idx >= (int)m_results.size()) return;
    // 右键落在选区外 → 先选中该行（对齐原版行为）
    if (!m_table->selectionModel()->isRowSelected(idx, QModelIndex())) {
        m_table->clearSelection();
        m_table->selectRow(idx);
    }
    std::vector<int> sel = selectedRows();
    if (sel.empty()) sel.push_back(idx);

    const QString menuQss = qssFor(makeProto(m_dark, m_accent));
    QMenu menu(this);
    menu.setStyleSheet(menuQss);
    QMenu* blockMenu = menu.addMenu(T("屏蔽"));
    blockMenu->setStyleSheet(menuQss);
    QAction* actEntry = blockMenu->addAction(sel.size() > 1 ? T("屏蔽所选条目 (%1)").arg(sel.size()) : T("屏蔽当前条目"));
    QAction* actFile = (sel.size() <= 1) ? blockMenu->addAction(T("屏蔽当前文件")) : nullptr;
    QMenu* markMenu = menu.addMenu(T("标记"));
    markMenu->setStyleSheet(menuQss);
    QAction* markActs[5];
    for (int i = 0; i < 5; i++) {
        markActs[i] = markMenu->addAction(markColorName(i));
        QPixmap sw(12, 12); sw.fill(Qt::transparent);
        { QPainter sp(&sw); sp.setRenderHint(QPainter::Antialiasing); sp.setPen(Qt::NoPen); sp.setBrush(kMarkColors[i]); sp.drawRoundedRect(QRect(1, 1, 10, 10), 2.5, 2.5); }
        markActs[i]->setIcon(QIcon(sw));
    }
    markMenu->addSeparator();
    QAction* actUnmark = markMenu->addAction(T("取消标记"));
    menu.addSeparator();
    QAction* actDetail = menu.addAction(T("行详情"));
    QAction* actExport = menu.addAction(T("导出所选条目 (%1)").arg(sel.size()));

    QAction* chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (!chosen) return;
    if (chosen == actEntry)       blockSelectedEntries(sel);
    else if (chosen == actFile)   blockSelectedFiles(sel);
    else if (chosen == actUnmark) markSelected(sel, -1);
    else if (chosen == actDetail) showDetail(idx);
    else if (chosen == actExport) exportSelected();
    else {
        for (int i = 0; i < 5; i++) if (chosen == markActs[i]) { markSelected(sel, i); break; }
    }
}
// 给所选行打/取消标记；color < 0 表示取消标记。只改行数据不重建表，保留选中状态。
void AppWindow::markSelected(const std::vector<int>& sel, int color) {
    if (!m_table) return;
    int changed = 0;
    for (int s : sel) {
        if (s < 0 || s >= (int)m_results.size()) continue;
        const auto& r = m_results[s];
        RowKey key{ r.filename, r.sheetName, r.row };
        if (color < 0) {
            changed += (int)m_marks.marked.erase(key);
        } else {
            auto it = m_marks.marked.find(key);
            if (it == m_marks.marked.end() || it->second != color) { m_marks.marked[key] = color; changed++; }
        }
        auto* item = m_table->item(s, 0);
        if (item) {
            item->setData(Qt::UserRole, color);
            item->setToolTip(color >= 0 ? (T("已标记：") + markColorName(color)) : QString());
        }
    }
    m_table->viewport()->update();
    if (changed <= 0) return;
    saveSettings();
    showToast(color < 0 ? (T("已取消标记: ") + QString::number(changed) + T(" 条"))
                        : (T("已标记为") + markColorName(color) + T(": ") + QString::number(changed) + T(" 条")));
}
void AppWindow::blockSelectedEntries(const std::vector<int>& sel) {
    int added = 0;
    for (int s : sel) {
        const auto& r = m_results[s];
        if (m_marks.blockedEntries.insert({ r.filename, r.sheetName, r.row }).second) added++;
    }
    if (added == 0) { showToast(T("所选条目均已被屏蔽")); return; }
    bool ok = ConfirmDialog::ask(this, m_dark, m_accent, T("屏蔽条目"),
        T("确认屏蔽 %1 个条目？\n屏蔽后搜索将不再显示这些内容，可在「高级设置 → 屏蔽管理」中解除。").arg(added),
        T("屏蔽"));
    if (!ok) {   // 取消 → 回滚刚才的插入
        for (int s : sel) { const auto& r = m_results[s]; m_marks.blockedEntries.erase({ r.filename, r.sheetName, r.row }); }
        return;
    }
    saveSettings();
    doSearch();
    showToast(T("已屏蔽: ") + QString::number(added) + T(" 条"));
}
void AppWindow::blockSelectedFiles(const std::vector<int>& sel) {
    std::set<std::string> files;
    for (int s : sel) files.insert(m_results[s].filename);
    QStringList names; int added = 0;
    for (const auto& f : files) { if (m_marks.blockedFiles.insert(f).second) { added++; names << u8(f); } }
    if (added == 0) { showToast(T("所选文件均已被屏蔽")); return; }
    bool ok = ConfirmDialog::ask(this, m_dark, m_accent, T("屏蔽文件"),
        T("确认屏蔽以下文件？\n%1\n\n屏蔽后搜索将不再显示这些文件的内容。").arg(names.join("\n")), T("屏蔽"));
    if (!ok) { for (const auto& f : files) m_marks.blockedFiles.erase(f); return; }
    saveSettings();
    doSearch();
    showToast(T("已屏蔽: ") + QString::number(added) + T(" 个文件"));
}
void AppWindow::fillTable(const std::vector<SearchResult>& res) {
    if (!m_table) return;
    const int extra = (int)m_extraCols.size();
    const int lastCol = 2 + extra;              // 匹配内容列
    m_table->setRowCount((int)res.size());
    for (int i = 0; i < (int)res.size(); i++) {
        const auto& r = res[i];
        auto* no0 = new QTableWidgetItem(QString::number(i + 1));
        int ci = m_marks.colorOf(r);   // -1 未标记；0..4 标记色索引（供 MarkBarDelegate 画行首色条）
        no0->setData(Qt::UserRole, ci);
        if (ci >= 0) no0->setToolTip(T("已标记：") + markColorName(ci));
        m_table->setItem(i, 0, no0);
        m_table->setItem(i, 1, new QTableWidgetItem(u8(r.filename)));
        for (int k = 0; k < extra; k++)
            m_table->setItem(i, 2 + k, new QTableWidgetItem(u8(cellValueFor(r, m_extraCols[k]))));
        m_table->setItem(i, lastCol, new QTableWidgetItem(u8(r.matchedValue)));
    }
    // B1：结果集变化后做一次入场错峰（首屏若干行自上而下"洗"进来）
    for (auto* d : m_rowDelegates) if (d->isTableView()) d->startReveal((int)res.size());
}
QWidget* AppWindow::statCell(const QString& label, QPushButton** valOut) {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(2);
    auto* lv = new QLabel(label); lv->setObjectName("statLabel");
    auto* lv2 = new QPushButton("0"); lv2->setObjectName("statVal"); lv2->setFlat(true);
    if (valOut) *valOut = lv2;
    v->addWidget(lv); v->addWidget(lv2);
    return w;
}
QWidget* AppWindow::makeStatCard() {
    auto* body = new QWidget; auto* v = new QVBoxLayout(body); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(4);
    auto* stats = new QHBoxLayout; stats->setSpacing(24);
    stats->addWidget(statCell(T("已加载文件"), &m_statFiles));
    stats->addWidget(statCell(T("索引记录"), &m_statIndex));
    stats->addWidget(statCell(T("本次命中"), &m_statHit));
    if (m_statHit) connect(m_statHit, &QPushButton::clicked, this, &AppWindow::showHitTip);
    v->addLayout(stats);
    return body;
}
QWidget* AppWindow::makeResultCard() {
    auto* table = new QTableWidget(0, 0);   // 列由 applyColumns() 按智能列配置动态建立
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setMinimumSectionSize(52);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    // 行委托：行 hover 颜色过渡 + 首列标记色条（委托也是"行 hover"的唯一绘制者，见 qssFor 的说明）
    auto* tdel = new RowDelegate(table, makeProto(m_dark, m_accent));
    tdel->setMarkBar(true);   // 只有结果表有"行首标记色条"
    table->setItemDelegate(tdel);
    m_rowDelegates.push_back(tdel);
    m_table = table;
    connect(table, &QTableWidget::cellDoubleClicked, this, [this](int, int row) { showDetail(row); });
    connect(table, &QTableWidget::customContextMenuRequested, this, &AppWindow::onTableContextMenu);
    applyColumns();
    return table;
}
QWidget* AppWindow::makeSearchPage() {
    auto* page = new QWidget; auto* v = new QVBoxLayout(page); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(14);
    auto* row = new QHBoxLayout; row->setSpacing(10);
    // 注意：QLineEdit(QString) 参数是「内容」不是占位符 —— 提示语必须用 setPlaceholderText
    m_searchEdit = new QLineEdit; m_searchEdit->setPlaceholderText(T("请输入关键词…"));
    m_searchEdit->setObjectName("searchEdit"); m_searchEdit->setMinimumHeight(40);
    auto* hist = new QPushButton(T("历史▾")); hist->setObjectName("themeBtn");
    m_histBtn = hist;
    connect(hist, &QPushButton::clicked, this, [this] { showHistoryMenu(); });
    auto* searchBtn = new QPushButton(T("搜索")); searchBtn->setObjectName("primaryBtn"); searchBtn->setMinimumHeight(40);
    auto* reload = new QPushButton(T("重新加载")); reload->setObjectName("themeBtn");
    // 打开数据文件夹（自绘文件夹图标；共享模式下打开共享目录）；导出已移入右键菜单
    m_folderBtn = new FolderBtn;
    connect(m_folderBtn, &QPushButton::clicked, this, [this] { openDataFolder(); });
    row->addWidget(m_searchEdit, 1); row->addWidget(hist); row->addWidget(searchBtn); row->addWidget(reload); row->addWidget(m_folderBtn);
    connect(searchBtn, &QPushButton::clicked, this, [this] { doSearch(); });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this] { doSearch(); });
    connect(reload, &QPushButton::clicked, this, [this] { loadData(); });
    v->addLayout(row);
    // 二级筛选：在当前结果集里再筛一层（对齐原版 g_hEditFilter / DoFilter / ClearFilter）
    auto* frow = new QHBoxLayout; frow->setSpacing(10);
    m_filterEdit = new QLineEdit;
    m_filterEdit->setPlaceholderText(T("二级筛选：在当前结果中再筛…"));
    m_filterEdit->setObjectName("searchEdit"); m_filterEdit->setMinimumHeight(34);
    auto* filterBtn = new QPushButton(T("筛选")); filterBtn->setObjectName("themeBtn");
    auto* clearFilterBtn = new QPushButton(T("清除筛选")); clearFilterBtn->setObjectName("themeBtn");
    frow->addWidget(m_filterEdit, 1); frow->addWidget(filterBtn); frow->addWidget(clearFilterBtn);
    connect(filterBtn, &QPushButton::clicked, this, [this] { doFilter(); });
    connect(m_filterEdit, &QLineEdit::returnPressed, this, [this] { doFilter(); });
    connect(clearFilterBtn, &QPushButton::clicked, this, [this] { clearFilter(); });
    v->addLayout(frow);
    auto* sa = new QScrollArea; sa->setWidgetResizable(true);
    auto* cont = new QWidget; auto* cv = new QVBoxLayout(cont); cv->setContentsMargins(0, 0, 0, 0); cv->setSpacing(14);
    // 搜索页两张卡片做成**可折叠**（点标题栏展开/收起，高度动画）。
    // 初始态一律展开 → 静止画面与动效前一致；折叠状态不持久化（不新增配置键）。
    // stretch 因子仍按 CardDef.expand 给（数据概览 0 / 搜索结果 1），折叠时靠高度上限压住。
    for (const auto& c : m_cards) {
        auto* card = new CollapseCard(c.title, c.make());
        card->setTheme(makeProto(m_dark, m_accent));
        m_collapseCards.push_back(card);
        cv->addWidget(card, c.expand ? 1 : 0);
    }
    cv->addStretch();
    sa->setWidget(cont);
    v->addWidget(sa, 1);
    m_status = new QLabel; m_status->setObjectName("status");
    v->addWidget(m_status);
    return page;
}
