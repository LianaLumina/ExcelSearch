// 自检钩子的支撑方法（B10 从 app_window.h 的类内内联定义搬出，逻辑一字未改）。
// 这些方法**只服务于无头自检与离屏截图**，不参与产品运行时路径：
//   demoMigrate        —— --migrate：强制跑一次注册表搬迁并把摘要交给 --out
//   demoCloseDialogPixmap —— --page closedlg：渲染关闭方式对话框（平时是模态的，截不到）
//   demoCollapse / demoHover / forceRowHover —— 截图用：折叠指定卡片 / 强制 hover 终态
//   demoAdvUnlock      —— --advpwd：填入口令并尝试解锁（验证密码迁移成哈希后仍可解锁）
//   firstHitKey        —— --search 输出首条命中身份键，供 --block 链式引用
//   applyMarkCli       —— --block / --mark / --clearmarks 的无头实现
//   settleAnim         —— 等子控件动效落定（或只等指定毫秒抓中间帧）
//   waitForLoad        —— 等后台加载结束（带超时上界，超时置 m_loadTimedOut）
// ⚠️ 这些方法名被 tools/selfcheck.ps1 与截图脚本按名字调用，**改名会直接打断自检链路**。
#include "app_window.h"
#include "dialogs.h"
#include "loading.h"
#include <QLineEdit>
#include <QSystemTrayIcon>
#include <QToolTip>

// 自检钩子：--migrate 强制执行一次「注册表 → config.ini」搬迁，摘要写 --out（供验证/客服排障）
QString AppWindow::demoMigrate() {
    QString detail;
    migrateFromRegistry(true, &detail);
    return detail;
}
QPixmap AppWindow::demoCloseDialogPixmap() {
    CloseDialog d(m_dark, m_accent, this);
    d.show();
    QCoreApplication::processEvents();
    QPixmap pm = d.grab();
    d.close();
    return pm;
}
// --collapse 0,1：折叠搜索页第 0/1 张卡片（0=数据概览 1=搜索结果）。走的就是点击标题栏那条路径。
void AppWindow::demoCollapse(const QString& spec) {
    for (const QString& s : spec.split(',', Qt::SkipEmptyParts)) {
        bool ok = false;
        const int i = s.trimmed().toInt(&ok);
        if (ok && i >= 0 && i < (int)m_collapseCards.size()) m_collapseCards[i]->setCollapsed(true, true);
    }
}
// --hover <名>：把某个控件强制摆到 hover 终态，用于离屏验证"过渡两端色与 QSS 一致"。
//   primaryBtn/themeBtn/tbBtn/navBtn = QSS 按钮；sec = 设置列表项；row = 结果表行；
//   folder/close = 自绘按钮；card = 卡片标题栏。真实鼠标下这些状态由 Enter/Leave 驱动。
void AppWindow::demoHover(const QString& what) {
    // sec[行号] = 设置列表项（默认第 1 项，避开选中项）；row[行号] = 结果表行（默认第 0 行）
    if (what == "loadbar") { if (m_loadBar) m_loadBar->start(); return; }   // B4：进度条呈现帧（截图用）
    // B5：密码错误的失败反馈呈现帧（走真实解锁路径，只喂一个错密码；不改配置、不落盘）
    if (what == "shakewrong") { if (m_advPwdEdit) { m_advPwdEdit->setText(T("__wrong__")); tryAdvUnlock(); } return; }
    if (what.startsWith("sec")) { forceRowHover(m_navList, what.size() > 3 ? what.mid(3).toInt() : 1); return; }
    if (what.startsWith("row")) { forceRowHover(m_table, what.size() > 3 ? what.mid(3).toInt() : 0); return; }
    QWidget* w = nullptr;
    // 名字末尾带数字 = 取第 N 个**可见**的同名按钮（默认第 1 个），例：navBtn2 = 第二个顶部标签
    QString kind = what; int nth = 1;
    while (!kind.isEmpty() && kind.at(kind.size() - 1).isDigit()) kind.chop(1);
    if (kind != what) nth = what.mid(kind.size()).toInt();
    if (kind == "primaryBtn" || kind == "themeBtn" || kind == "tbBtn" || kind == "navBtn") {
        // 关键：必须挑**可见**的那一个 —— 同名按钮在隐藏页里也有一堆（设置页各分区），
        // 挑到隐藏的等于没挑（合成 Enter 会被送到不可见控件上，截图毫无变化）。
        int seen = 0;
        for (auto* b : findChildren<QPushButton*>()) {
            if (b->objectName() != kind || !b->isVisible()) continue;
            if (++seen == nth) { w = b; break; }
        }
    } else if (what == "folder") w = m_folderBtn;
    else if (what == "close")    w = m_closeBtn;
    else if (what == "card")     { if (!m_collapseCards.empty()) w = m_collapseCards[0]->header(); }
    if (!w) return;
    // QSS 的 :hover 只看 WA_UnderMouse（真实鼠标时由 Qt 维护），离屏下要手动置位；
    // QSS 控件的状态过渡（StateTint）挂在事件过滤器上，靠这个合成 Enter 启动。
    // 注意：关闭动效时 StateTint 根本没安装，QSS 控件的 hover 就交回给 QSS 自己 ——
    // 离屏下没有真实鼠标，截不到该态（这是"关闭动效后与动效前一致"的必然结果）。
    w->setAttribute(Qt::WA_UnderMouse, true);
    QEnterEvent enter(QPointF(1, 1), QPointF(1, 1), QPointF(1, 1));
    QApplication::sendEvent(w, &enter);
    w->update();
}
void AppWindow::forceRowHover(QAbstractItemView* v, int row) {
    if (!v) return;
    if (auto* d = qobject_cast<RowDelegate*>(v->itemDelegate())) d->setHoverRow(row);
}
// 自检钩子：--advpwd <口令>：把口令填进解锁框并尝试解锁（用来验证"迁移成哈希后仍能正常解锁"）
void AppWindow::demoAdvUnlock(const char* pw) {
    if (!m_advPwdEdit) return;
    m_advPwdEdit->setText(QString::fromUtf8(pw));
    tryAdvUnlock();
}
// 首条命中行的身份键 "fn|sheet|row"，供无头自检链式引用（--search 输出 → --block 使用）
QString AppWindow::firstHitKey() const {
    if (m_results.empty()) return QString();
    const auto& r = m_results[0];
    return QString::fromUtf8((r.filename + "|" + r.sheetName + "|" + std::to_string(r.row)).c_str());
}
// 无头自检钩子：--block fn|sheet|row（或 --block fn 屏蔽整个文件）、--mark fn|sheet|row|color、--clearmarks
void AppWindow::applyMarkCli(const QString& blockSpec, const QString& markSpec, bool clearAll) {
    if (clearAll) m_marks.clearAll();
    if (!blockSpec.isEmpty()) {
        auto p = blockSpec.split('|');
        if (p.size() == 3) m_marks.blockedEntries.insert({ p[0].toUtf8().toStdString(), p[1].toUtf8().toStdString(), p[2].toInt() });
        else if (p.size() == 1) m_marks.blockedFiles.insert(p[0].toUtf8().toStdString());
    }
    if (!markSpec.isEmpty()) {
        auto p = markSpec.split('|');
        if (p.size() == 4) m_marks.marked[{ p[0].toUtf8().toStdString(), p[1].toUtf8().toStdString(), p[2].toInt() }] = p[3].toInt();
    }
    saveSettings();
}
// 离屏截图 / 自检用：等子控件动效落定，避免拍到动画中间态。
// ms > 0 时只等指定毫秒 —— 用于**故意抓动画中间帧**（证明动效确实在动，不是瞬变）。
// 默认 kSettleMs 必须覆盖**最长的一条动效链**（数值滚动 420ms + 最大错峰 80ms + 页面/分区 160ms
// + 余量）。以后加了更长的动效要同步调大 kSettleMs —— 否则截图会拍到中间态
// （返工记录：默认值还停在旧的 420ms 时，把索引记录 43846 拍成了 43844）。
// 关闭动效时是空操作（本来就没有中间态）。
void AppWindow::settleAnim(int ms) {
    if (!g_noAnim) {
        QEventLoop loop;
        QTimer::singleShot(ms > 0 ? ms : kSettleMs, &loop, &QEventLoop::quit);
        loop.exec();
    }
    QCoreApplication::processEvents();
}
void AppWindow::waitForLoad(int timeoutMs) {
    // 注意：worker 可能已经结束（例如共享目录瞬时不可达时几微秒就返回），此时 finished 的
    // 队列槽（onLoadFinished / 缓存兜底）还没跑，必须泵一次事件循环，否则自检会读到中间态。
    if (!m_worker || !m_worker->isRunning()) { QCoreApplication::processEvents(); return; }
    QEventLoop loop;
    connect(m_worker, &LoadWorker::finished, &loop, &QEventLoop::quit);
    QTimer timeoutTimer; timeoutTimer.setSingleShot(true);
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(timeoutMs);
    loop.exec();
    if (m_worker && m_worker->isRunning()) m_loadTimedOut = true;   // 超时：调用方据此报错
    QCoreApplication::processEvents();
}
qulonglong AppWindow::demoFilter(const char* kw) {
    if (m_filterEdit) { m_filterEdit->setText(QString::fromUtf8(kw)); doFilter(); }
    return (qulonglong)m_results.size();
}

// —— B12-step2：从类内搬出的单行成员函数（逻辑一字未改）——
    int AppWindow::advSectionCount() const { return (int)m_advSecs.size(); }
    void AppWindow::demoSearch(const char* kw) { if (m_searchEdit) { m_searchEdit->setText(QString::fromUtf8(kw)); doSearch(); } }
    qulonglong AppWindow::demoHits(const char* kw) { demoSearch(kw); return (qulonglong)m_results.size(); }
    QString AppWindow::demoResolveColumn(const char* text) const { return u8(resolveColumnSource(text)); }
    int AppWindow::failedFileCountC() const { return m_failedCount; }
    QString AppWindow::failedListC() const { return m_failedList; }
    bool AppWindow::loadTimedOutC() const { return m_loadTimedOut; }
    int AppWindow::cfgEncFailC() const { return m_cfgEncFail; }
    bool AppWindow::cfgKeyReadyC() const { return m_cfgKeyReady; }
    int AppWindow::demoBlocked() const { return m_lastBlocked; }
    int AppWindow::blockedEntryCount() const { return (int)m_marks.blockedEntries.size(); }
    int AppWindow::blockedFileCount() const { return (int)m_marks.blockedFiles.size(); }
    int AppWindow::markedCount() const { return (int)m_marks.marked.size(); }
    int AppWindow::historyCount() const { return (int)m_history.size(); }
    int AppWindow::historyShow() const { return m_histShow; }
    int AppWindow::historyTtl() const { return m_histTtlMin; }
    const char* AppWindow::filterMode() const { return m_chainMode ? "chain" : "standard"; }
    bool AppWindow::shareEnabled() const { return m_shareMode; }
    bool AppWindow::trayActive() const { return m_tray && m_tray->isVisible(); }
    bool AppWindow::manualNeverShowC() const { return m_manualNeverShow; }
    const char* AppWindow::closeActionName() const { return m_closeAction == 1 ? "close" : (m_closeAction == 2 ? "tray" : "ask"); }
    const char* AppWindow::animName() const { return g_noAnim ? "off" : "on"; }
    const char* AppWindow::sharePathC() const { return m_sharePath.c_str(); }
    int AppWindow::exportAllTo(const char* file) { return exportXlsxTo(m_results, file) ? 1 : 0; }
    int AppWindow::loadedCount() const { return m_loadedFiles; }
    int AppWindow::skippedCount() const { return m_skipped; }
    qulonglong AppWindow::entryCount() const { return (qulonglong)m_engine.getEntryCount(); }
    bool AppWindow::usedCache() const { return m_usedCache; }
    void AppWindow::setHitStat(qulonglong n) { setStat(m_statHit, n, 2 * kStaggerMs); }
    void AppWindow::showHitTip() { QToolTip::showText(QCursor::pos(), QString(T("精确 %1 条 · 模糊 %2 条")).arg(m_lastExact).arg(m_lastFuzzy), this); }
