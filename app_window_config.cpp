// 配置持久化与数据加载流水线的 AppWindow 方法（B8 从 app_window.h 的类内内联定义搬出，逻辑一字未改）。
// 组成：apply（把当前配置落到界面：QSS + 强调色 + 状态过渡）；migrateFromRegistry（V0.3.0 注册表一次性搬迁）；
//       loadSettings / saveSettings（config.ini 读写）；loadData / tryLoadCache / loadCacheFallback /
//       onLoadFinished（后台加载、缓存命中与回退、收尾）；briefNames / emptySourceHint（提示文案）。
// ★ 本文件是"config.ini 敏感值加密加固"的落点（见 CONFIG-HARDENING-PLAN.md）。
#include "app_window.h"

void AppWindow::apply() {
    auto p = makeProto(m_dark, m_accent);
    setStyleSheet(qssFor(p));
    // 占位符颜色不受 QSS 控制（走 QPalette::PlaceholderText），深浅色下都显式设一遍
    if (m_searchEdit) {
        QPalette pal = m_searchEdit->palette();
        pal.setColor(QPalette::PlaceholderText, p.sub);
        m_searchEdit->setPalette(pal);
    }
    if (m_themeBtn) m_themeBtn->setText(m_dark ? T("☀ 浅色") : T("🌙 深色"));
    if (m_darkBtn) m_darkBtn->setChecked(m_dark);
    if (m_lightBtn) m_lightBtn->setChecked(!m_dark);
    if (m_maxBtn) { m_maxBtn->setTheme(p.text, p.hover, p.closeHover); m_maxBtn->setRestore(m_maxed); }
    if (m_minBtn) m_minBtn->setTheme(p.text, p.hover, p.closeHover);
    if (m_closeBtn) m_closeBtn->setTheme(p.text, p.hover, p.closeHover);
    if (m_folderBtn) m_folderBtn->setTheme(p.sub, p.text, p.hover);
    if (m_loadBar) m_loadBar->setTheme(p);
    // 动效控件跟随主题刷新：自绘标题栏 / 行委托 / QSS 按钮的 hover 过渡端点色。
    // 必须在这里统一刷新 —— hover 中途切主题时，过渡色不能残留旧主题的令牌值。
    for (auto* c : m_collapseCards) c->setTheme(p);
    for (auto* d : m_rowDelegates) d->setTheme(p);
    for (auto* s : m_stateTints) s->refresh();
    refreshAccentButtons();   // 强调色选中态跟随主题色刷新
    // 图标随强调色联动（托盘 + 任务栏）
    const QIcon ic = makeAppIcon();
    setWindowIcon(ic);
    if (m_tray) {
        m_tray->setIcon(ic);
        if (m_trayMenu) m_trayMenu->setStyleSheet(qssFor(p));
    }
}
bool AppWindow::migrateFromRegistry(bool force, QString* detail) {
    auto summarize = [&](const QString& s) { if (detail) *detail = s; };
    QSettings ini(QString::fromUtf8(m_settingsPath.c_str()), QSettings::IniFormat);
    if (!force && ini.value("meta/registryMigrated", false).toBool()) {
        summarize("migrated=0 reason=already-marked\n"); return false;
    }
    QSettings reg("HKEY_CURRENT_USER\\Software\\ExcelSearch", QSettings::NativeFormat);
    const QStringList keys = reg.allKeys();
    const QStringList wanted = { "Password","DarkMode","AdditionalPwdEnabled","AdditionalPwd",
                                 "ShareMode","SharePath","BlockedFiles","BlockedEntries",
                                 "MarkedEntries","SearchHistory1" };
    bool any = false;
    for (const QString& w : wanted) if (keys.contains(w)) { any = true; break; }
    if (!any) { summarize("migrated=0 reason=no-old-settings\n"); return false; }

    int nFiles = 0, nEntries = 0, nMarked = 0, nHist = 0;
    if (keys.contains("Password"))            m_adminPassword = reg.value("Password").toString().toUtf8().toStdString();
    if (keys.contains("DarkMode"))            m_dark = (reg.value("DarkMode").toInt() == 1);
    if (keys.contains("AdditionalPwdEnabled")) m_addPwdEnabled = (reg.value("AdditionalPwdEnabled").toInt() == 1);
    if (keys.contains("AdditionalPwd"))       m_addPwd = reg.value("AdditionalPwd").toString().toUtf8().toStdString();
    if (keys.contains("ShareMode"))           m_shareMode = (reg.value("ShareMode").toInt() == 1);
    if (keys.contains("SharePath")) {
        const QString sp = reg.value("SharePath").toString();
        if (!sp.isEmpty()) m_sharePath = sp.toUtf8().toStdString();
    }
    for (const QString& f : reg.value("BlockedFiles").toStringList()) {
        const QString t = f.trimmed();
        if (!t.isEmpty()) { m_marks.blockedFiles.insert(t.toUtf8().toStdString()); nFiles++; }
    }
    for (const QString& ln : reg.value("BlockedEntries").toStringList()) {
        auto p = ln.split('|');   // fn|sheet|row
        if (p.size() == 3) { m_marks.blockedEntries.insert({ p[0].toUtf8().toStdString(), p[1].toUtf8().toStdString(), p[2].toInt() }); nEntries++; }
    }
    for (const QString& ln : reg.value("MarkedEntries").toStringList()) {
        auto p = ln.split('|');   // fn|sheet|row|color
        if (p.size() == 4) { m_marks.marked[{ p[0].toUtf8().toStdString(), p[1].toUtf8().toStdString(), p[2].toInt() }] = p[3].toInt(); nMarked++; }
    }
    // 历史：原版与本版同为「最新在前」，直接按顺序搬；原版无时间戳 → 按 1 分钟间隔倒推生成。
    // ⚠️ 新版默认 hist/ttl=0 = 「关闭程序后删除（不落盘）」，而原版历史是长期保留的；
    //    若照默认走，老用户的历史等于白搬 —— 因此**只在确有历史可搬时**把时效设为最长档
    //    （12 小时 = 下拉里的 720 分钟），最接近原版"长期保留"的体感。
    const QStringList h1 = reg.value("SearchHistory1").toStringList();
    QStringList h1keep;
    for (const QString& s : h1) { const QString t = s.trimmed(); if (!t.isEmpty()) h1keep << t; }
    if (!h1keep.isEmpty()) {
        if (m_histTtlMin == 0) m_histTtlMin = 720;
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        m_history.clear();
        for (int i = 0; i < h1keep.size() && (int)m_history.size() < kHistStoreMax; i++) {
            m_history.push_back({ h1keep[i], now - (qint64)i * 60 });
            nHist++;
        }
    }
    saveSettings();
    QSettings ini2(QString::fromUtf8(m_settingsPath.c_str()), QSettings::IniFormat);
    ini2.setValue("meta/registryMigrated", true);
    ini2.setValue("meta/migratedFrom", "V0.3.0-registry");
    m_migratedFromOld = (nFiles + nEntries + nMarked + nHist) > 0;
    summarize(QString("migrated=1 from=V0.3.0-registry dark=%1 adminPwd=%2 addPwdEnabled=%3 addPwd=%4 share=%5 blockedFiles=%6 blockedEntries=%7 marked=%8 history=%9\n")
              .arg(m_dark ? 1 : 0).arg(m_adminPassword.empty() ? 0 : 1).arg(m_addPwdEnabled ? 1 : 0)
              .arg(m_addPwd.empty() ? 0 : 1).arg(m_shareMode ? 1 : 0)
              .arg(nFiles).arg(nEntries).arg(nMarked).arg(nHist));
    return true;
}
void AppWindow::loadSettings() {
    QSettings s(QString::fromUtf8(m_settingsPath.c_str()), QSettings::IniFormat);
    m_dark = s.value("ui/dark", false).toBool();
    m_accent = QColor(s.value("ui/accent", "#326cf3").toString());
    m_fuzzyEnabled = s.value("search/fuzzy", true).toBool();
    // ★回迁注意：默认「标准模式」(standard)，原版行为(就地逐级缩)作为可选项 chain
    m_chainMode = (s.value("filter/mode", "standard").toString() == "chain");
    m_addPwdEnabled = s.value("crypto/addPwdEnabled", false).toBool();
    m_addPwd = s.value("crypto/addPwd", "").toString().toUtf8().toStdString();
    m_sharePath = s.value("share/path", QString::fromUtf8("\\\\server\\share\\excel_search")).toString().toUtf8().toStdString();
    // ★回迁注意：共享模式（数据源切换）与离线模式并存；原版是 g_shareMode + g_dataFolder 覆盖
    m_shareMode = s.value("share/enabled", false).toBool();
    // 关闭行为：ask(默认) / close / tray
    {
        const QString ca = s.value("ui/closeAction", "ask").toString();
        m_closeAction = (ca == "close") ? 1 : (ca == "tray" ? 2 : 0);
    }
    m_marks.load(s);
    m_manualNeverShow = s.value("manual/neverShow", false).toBool();
    m_adminPassword = s.value("admin/password", QString::fromUtf8(kDefaultAdminPassword)).toString().toUtf8().toStdString();
    // 智能列配置：换行分隔的来源列表。
    // 键「不存在」→ 用默认布局（原版 7 列）；键存在但为空 → 用户主动只保留固定 3 列。
    if (s.contains("smartcols/list")) {
        std::vector<std::string> cols;
        for (const QString& ln : s.value("smartcols/list").toString().split('\n', Qt::SkipEmptyParts)) {
            const QString t = ln.trimmed();
            if (!t.isEmpty()) cols.push_back(t.toUtf8().toStdString());
        }
        if (cols.size() > (size_t)(kMaxResultCols - 3)) cols.resize(kMaxResultCols - 3);
        m_extraCols = cols;
    } else {
        m_extraCols = defaultExtraCols();
    }
    // 搜索历史：显示条数 / 时效 / 条目（时效为 0 时不落盘，也就不读）
    m_histShow = qBound(0, s.value("hist/show", 5).toInt(), kHistShowMax);
    m_histTtlMin = s.value("hist/ttl", 0).toInt();
    if (m_histTtlMin != 0 && m_histTtlMin < kHistTtlMinMinute) m_histTtlMin = kHistTtlMinMinute;
    m_history.clear();
    if (m_histTtlMin != 0) {
        for (const QString& ln : s.value("hist/items").toString().split('\n', Qt::SkipEmptyParts)) {
            const int tab = ln.indexOf('\t');
            if (tab <= 0) continue;
            bool ok = false;
            const qint64 ts = ln.left(tab).toLongLong(&ok);
            const QString kw = ln.mid(tab + 1);
            if (ok && !kw.isEmpty()) m_history.push_back({ kw, ts });
        }
        purgeHistory();   // 载入时先按当前时效清一遍
    }
}
void AppWindow::saveSettings() {
    QSettings s(QString::fromUtf8(m_settingsPath.c_str()), QSettings::IniFormat);
    s.setValue("ui/dark", m_dark);
    s.setValue("ui/accent", m_accent.name());
    s.setValue("search/fuzzy", m_fuzzyEnabled);
    s.setValue("filter/mode", m_chainMode ? "chain" : "standard");   // ★回迁注意：见 loadSettings
    s.setValue("crypto/addPwdEnabled", m_addPwdEnabled);
    s.setValue("crypto/addPwd", QString::fromUtf8(m_addPwd.c_str()));
    s.setValue("share/path", QString::fromUtf8(m_sharePath.c_str()));
    s.setValue("share/enabled", m_shareMode);
    s.setValue("ui/closeAction", m_closeAction == 1 ? "close" : (m_closeAction == 2 ? "tray" : "ask"));
    s.setValue("manual/neverShow", m_manualNeverShow);
    m_marks.save(s);
    s.setValue("admin/password", QString::fromUtf8(m_adminPassword.c_str()));
    {
        QStringList cols;
        for (const auto& c : m_extraCols) cols << QString::fromUtf8(c.c_str());
        s.setValue("smartcols/list", cols.join('\n'));
    }
    // 搜索历史：时效为「关闭程序后删除」时不写任何条目（不做留存）
    s.setValue("hist/show", m_histShow);
    s.setValue("hist/ttl", m_histTtlMin);
    if (m_histTtlMin == 0) {
        s.setValue("hist/items", QString());
    } else {
        QStringList its;
        for (const auto& h : m_history) its << (QString::number(h.ts) + '\t' + h.kw);
        s.setValue("hist/items", its.join('\n'));
    }
    s.sync();
}
void AppWindow::loadData(bool forceReload) {
    if (m_worker && m_worker->isRunning()) return;
    // B4：任何一次"加载/重新加载"都给一次进度反馈 —— **包括缓存命中的瞬时加载**。
    //   返工记录：一开始只在"真的起了后台 worker"的分支里点亮细条，结果本机缓存命中时
    //   （cache=hit，加载只要几毫秒）细条永远不出现，等于没做。现在配合"最短显示窗口"，
    //   缓存命中也给一次 260ms 的扫过（不闪一下就没，也不假装知道进度）。
    if (m_loadBar) m_loadBar->start();
    // 注意：这里**不再 updateStats()** —— 加载期间统计数字保留上一次的值。
    //   语义：这三个数字表示"当前索引的规模"，而加载中索引本来就是空的（0）也没意义，
    //   显示 0 只会让人以为数据被清了；等新数据到位再由 tryLoadCache()/onLoadFinished()
    //   统一刷新（并向上滚动）。索引状态本身该清还是要清（m_engine.clear() 影响的是搜索，不是显示）。
    m_engine.clear(); m_loadedFiles = 0; m_skipped = 0;
    m_usedCache = false;
    if (m_status) m_status->setText(m_shareMode ? T("正在连接共享目录…") : T("正在加载数据…"));
    if (!forceReload && tryLoadCache()) {   // 缓存命中：直接复用上次索引（强制重载时跳过）
        if (m_loadBar) m_loadBar->stop();
        return;
    }
    auto* wk = new LoadWorker; wk->dataDir = dataSourceDir(); wk->addPwdEnabled = m_addPwdEnabled; wk->addPwd = m_addPwd; m_worker = wk;
    connect(wk, &LoadWorker::progress, this, [this](int done, int total, QString file) {
        if (m_status) m_status->setText(T("正在加载 ") + QString::number(done) + T("/") + QString::number(total) + T("：") + file);
    });
    connect(wk, &LoadWorker::finished, this, &AppWindow::onLoadFinished);
    wk->start();
}
bool AppWindow::tryLoadCache() {
    // 凭据 = cache.inv 里的「全量清单」；与当前磁盘逐项精确相等才可复用
    std::map<std::string, std::pair<int64_t, int64_t>> inv;
    int64_t ts = 0;
    if (!readInventory(m_cacheInvPath, inv, ts)) return false;
    if ((int64_t)time(nullptr) - ts > cacheTtlSec()) return false;   // TTL 过期（离线 10 天 / 共享 30 小时）
    std::vector<DiskFile> disk; collectDiskFiles(dataSourceDir(), disk);
    if (disk.size() != inv.size()) return false;   // 数量必须完全一致（含新增/删除）
    for (const auto& d : disk) {
        auto it = inv.find(d.fn);
        if (it == inv.end()) return false;   // 磁盘有、清单没有 → 新增文件
        if (it->second.first != d.mtime || it->second.second != d.size) return false;   // 内容/时间被改动
    }
    if (!m_engine.loadFromFile(m_cachePath)) return false;
    // ★回迁注意：命中判定用的是 cache.inv「全量清单」（含被附加密码跳过的 .xse），
    //   原版只比 loadedFiles，有 skip 文件时永不命中。回迁必须把 cache.inv 机制一并搬运。
    //   详见 docs/回迁标注.md 第 4 条。
    m_usedCache = true;
    m_loadedFiles = (int)m_engine.getFiles().size();
    m_skipped = 0;   // 重算跳过数：磁盘 .xse 中未进索引的个数（附加密码跳过态在缓存恢复后依旧成立）
    auto isXse = [](const std::string& f) { size_t d = f.rfind('.'); return d != std::string::npos && (f.substr(d) == ".xse" || f.substr(d) == ".XSE"); };
    std::set<std::string> inIndex; for (const auto& f : m_engine.getFiles()) inIndex.insert(f);
    for (const auto& d : disk) if (isXse(d.fn) && !inIndex.count(d.fn)) m_skipped++;
    buildColMaps();
    updateStats();
    if (m_loadedFiles == 0 && m_engine.getFiles().empty()) {
        // 首次运行的典型情形：程序目录下 data\ 是空的 → 必须明确告诉用户文件该放哪
        if (m_status) m_status->setText(emptySourceHint());
        showToast(emptySourceHint(), 12000);
    } else if (m_status) {
        m_status->setText(T("已加载(缓存) ") + QString::number(m_loadedFiles) + T(" 个文件 · 共 ") +
                          QString::number((qulonglong)m_engine.getEntryCount()) + T(" 条记录"));
    }
    return true;
}
QString AppWindow::briefNames(const std::vector<std::string>& names) {
    QString s;
    for (size_t i = 0; i < names.size() && i < 3; i++) { if (i) s += T("、"); s += u8(names[i]); }
    if (names.size() > 3) s += T(" 等 ") + QString::number((qulonglong)names.size()) + T(" 个");
    return s;
}
// 数据源为空 / 不可用时的提示语（模式相关：离线模式不能说成"共享目录"）
QString AppWindow::emptySourceHint() const {
    return m_shareMode
        ? T("共享目录为空：") + u8(dataSourceDir()) + T("　请把 Excel / Word / CSV 文件放入该目录后点「重新加载」")
        : T("数据目录为空：") + u8(dataSourceDir()) + T("　请把 Excel / Word / CSV 文件放入该文件夹后点「重新加载」");
}
void AppWindow::loadCacheFallback() {
    // ★产品化：文案必须区分模式 —— 离线模式下说"共享目录不可达"会让人完全摸不着头脑（冒烟发现 B）
    const QString badSrc = m_shareMode ? T("共享目录不可达") : T("本地数据目录不存在或不可访问");
    if (!m_engine.loadFromFile(m_cachePath)) {
        m_loadedFiles = 0; updateStats();
        if (m_status) m_status->setText(badSrc + (m_shareMode ? T("，且无可用缓存数据（请检查共享路径与网络）")
                                                             : T("，且无可用缓存数据（请检查数据目录后点「重新加载」）")));
        setShareStatus(m_shareMode ? T("不可达 · 无缓存") : T("本地目录不可用 · 无缓存"));
        return;
    }
    m_usedCache = true;
    m_loadedFiles = (int)m_engine.getFiles().size();
    m_skipped = 0;
    buildColMaps();
    updateStats();
    int64_t ts = 0; std::map<std::string, std::pair<int64_t, int64_t>> inv;
    readInventory(m_cacheInvPath, inv, ts);
    const qint64 ageH = ts > 0 ? (QDateTime::currentSecsSinceEpoch() - ts) / 3600 : -1;
    const QString age = ageH < 0 ? T("时间未知") : (QString::number((qulonglong)ageH) + T(" 小时前"));
    if (m_status) m_status->setText(badSrc + T("，已使用缓存数据（") + age + T("）· 共 ")
                                   + QString::number((qulonglong)m_engine.getEntryCount()) + T(" 条记录"));
    setShareStatus((m_shareMode ? T("不可达") : T("本地目录不可用")) + T(" · 已用缓存（") + age + T("）"));
}
void AppWindow::onLoadFinished() {
    if (!m_worker) return;
    if (m_loadBar) m_loadBar->stop();   // B4：解析结束就收（含不可达兜底路径）
    // ★共享模式不可达旁路：目录探测失败 → 直接加载 cache.dat（跳过 cache.inv 清单校验），
    //   并且**绝不回写缓存、绝不把视图清空**。否则断网一次就会把好缓存覆盖成空索引。
    if (m_worker->unreachable) {
        m_worker->deleteLater(); m_worker = nullptr;
        loadCacheFallback();
        return;
    }
    for (const auto& d : m_worker->docs) m_engine.addFile(d.fn, d.sheets);
    m_loadedFiles = (int)m_worker->docs.size();
    int skipped = 0;
    if (m_allowPrompt) {
        // 主线程补问附加密码（跨线程不能弹框，这里在主线程处理）
        for (auto& p : m_worker->pendingXse) {
            bool added = false;
            bool first = true;
            while (true) {
                PasswordDialog dlg(m_dark, m_accent, T("该加密文件需要附加密码"),
                                   first ? u8(p.first) : T("附加密码错误，请重试。"), this);
                first = false;
                if (dlg.exec() != QDialog::Accepted) break;   // 取消
                std::vector<SheetData> sheets; bool pe = false;
                if (xseToSheets(p.second, dlg.password().toUtf8().toStdString(), sheets, pe)) {
                    m_sessionAddPwd = dlg.password().toUtf8().toStdString();
                    m_engine.addFile(p.first, sheets); m_loadedFiles++; added = true; break;
                }
            }
            if (!added) skipped++;
        }
    } else {
        skipped = (int)m_worker->pendingXse.size();
    }
    std::vector<DiskFile> disk; std::map<std::string, int64_t> mt, sz;
    collectDiskFiles(m_dataDir, disk);
    for (const auto& d : disk) { mt[d.fn] = d.mtime; sz[d.fn] = d.size; }
    m_engine.setFileMeta(mt, sz);   // 记录磁盘元数据，供缓存一致性校验
    updateStats();
    const int failedN = (int)m_worker->failed.size();
    {   // 保留失败文件清单（worker 稍后会被 deleteLater，届时拿不到）
        QStringList fl;
        for (const auto& fn : m_worker->failed) fl << QString::fromUtf8(fn.c_str());
        m_failedCount = failedN;
        m_failedList = fl.join(QStringLiteral(","));
    }
    const int emptyN  = (int)m_worker->emptyData.size();
    if (m_status) {
        if (m_loadedFiles == 0 && failedN == 0 && emptyN == 0 && skipped == 0) {
            m_status->setText(emptySourceHint());   // 数据源为空（首次运行的典型情形）
        } else {
            QString msg = T("已加载 ") + QString::number(m_loadedFiles) + T(" 个文件 · 共 ") + QString::number((qulonglong)m_engine.getEntryCount()) + T(" 条记录");
            if (skipped > 0) msg += T("（") + QString::number(skipped) + T(" 个加密文件需附加密码未加载）");
            if (failedN > 0) msg += T("（") + QString::number(failedN) + T(" 个文件读取失败：") + briefNames(m_worker->failed) + T("）");
            if (emptyN  > 0) msg += T("（") + QString::number(emptyN)  + T(" 个文件无有效数据）");
            m_status->setText(msg);
        }
    }
    // 空数据源 → 显著提示一次（用户丢进文件却没生效时最需要这个）；有读取失败也提示
    if (m_loadedFiles == 0) showToast(emptySourceHint(), 12000);
    else if (failedN > 0) showToast(T("有 ") + QString::number(failedN) + T(" 个文件读取失败：") + briefNames(m_worker->failed), 8000);
    m_skipped = skipped;
    // 先写索引块 cache.dat，再写全量清单 cache.inv 作为「提交标记」：清单在则索引必定完整
    m_engine.saveToFile(m_cachePath);
    writeInventory(m_cacheInvPath, disk);
    buildColMaps();
    m_worker->deleteLater(); m_worker = nullptr;
}
