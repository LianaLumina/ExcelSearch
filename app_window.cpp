// 窗口外壳与生命周期的 AppWindow 方法（B11 从 app_window.h 的类内内联定义搬出，逻辑一字未改）。
// 组成：构造 AppWindow（成员初始化 + 分区表 + 托盘/定时器挂钩）、页面导航 switchPage / goAdvPage、
//       托盘与关闭流程 setupTray / showFromTray / quitApp / refreshCloseRadios / hideToTray / closeEvent、
//       无边框窗口交互 mousePressEvent / mouseDoubleClickEvent / resizeEvent / toggleMax、
//       主题相关 refreshAccentButtons / setupStateTints、提示气泡 showToast / stopToastAnim / hideToastAnim、
//       通用卡片工厂 cardFrame、界面装配 buildUi、图标 makeAppIcon、打开数据目录 openDataFolder。
// ⚠️ 本文件里 startSystemMove / startSystemResize / toggleMax 是无边框窗口的命脉（见各函数注释），
//    改窗口行为前先读 apply()/buildUi() 与 docs/回迁标注.md 里关于无边框窗口的条目。
#include "app_window.h"
#include <QDesktopServices>
#include <QStandardPaths>

AppWindow::AppWindow() {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    m_settingsPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
    QDir().mkpath(QString::fromUtf8(m_settingsPath.c_str()));
    m_settingsPath += "/config.ini";
    std::string cacheDir = m_settingsPath.substr(0, m_settingsPath.rfind('/'));
    m_cachePath = cacheDir + "/cache.dat";
    m_cacheInvPath = cacheDir + "/cache.inv";
    loadSettings();   // 先读配置，供各控件初始化
    // 旧版（V0.3.0）设置存在注册表 → 首次运行自动搬迁一次，避免老用户升级后设置全丢。
    // 必须放在 buildUi() 之前：界面控件要按搬迁后的值初始化。
    migrateFromRegistry(false);
    m_pages = {
        { T("搜索"), [this] { return makeSearchPage(); } },
        { T("设置"), [this] { return makeSettingsPage(); } },
    };
    m_cards = {
        { T("数据概览"), false, [this] { return makeStatCard(); } },
        { T("搜索结果"), true,  [this] { return makeResultCard(); } },
    };
    // 预留：实用工具标签（当前 kEnableUtilTab=false，不注册任何工具，也就不出现该标签）
    m_utilTools.clear();
    if (kEnableUtilTab) m_pages.push_back({ utilTabTitle(), [this] { return makeUtilPage(); } });
    m_secs = {
        { T("通用设置"), [this] { return makeGeneralSec(); } },   // 外观 + 关闭选项（原「界面设置」已并入）
        { T("搜索设置"), [this] { return makeSearchSec(); } },   // 模糊搜索 + 筛选模式
        { T("搜索历史"), [this] { return makeHistorySec(); } },
        { T("加密设置"), [this] { return makeEncSec(); } },
        { T("共享设置"), [this] { return makeShareSec(); } },
        { T("智能列设置"), [this] { return makeSmartColSec(); } },   // 结果表列数与列内容自定义
        { T("高级设置"), [this] { return makeAdvSec(); } },   // 与其它分区同级；选中时先显示解锁层
        { T("关于我们"), [this] { return makeAboutSec(); } },
    };
    // 高级设置分区：注册表驱动 —— 以后要加新内容，往这个列表追加一项即可
    m_advSecs = {
        { T("屏蔽管理"),   [this] { return makeBlockAdminSec(); } },
        { T("标记清除"),   [this] { return makeMarkAdminSec(); } },
        { T("设置管理密码"), [this] { return makePasswordSec(); } },
    };
    // 注意：m_dataDir 必须在 buildUi() 之前赋值 —— 共享设置等分区工厂在 buildUi 里就会读它。
    m_dataDir = QCoreApplication::applicationDirPath().toStdString() + "/data";
    buildUi();
    m_toast = new QLabel(this);          // 左下角提示气泡（常驻隐藏，等价原版 g_hToast）
    m_toast->setObjectName("toast");
    m_toast->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_toast->hide();
    apply();
    // 刚完成旧版设置搬迁 → 等窗口显示后提示一次（气泡需要界面已就绪，故延后到事件循环）
    if (m_migratedFromOld)
        QTimer::singleShot(600, this, [this] {
            showToast(T("已从旧版本导入设置（主题 / 屏蔽 / 标记 / 搜索历史 / 共享路径）"), 10000);
        });
    setupStateTints();   // 统一给界面上的 QSS 控件挂状态过渡（放在 buildUi 之后扫一遍，见函数注释）
    // B4 加载进度条：挂在面板上、手动定位（不进布局，避免加载时整页上下跳）
    if (m_panel) {
        m_loadBar = new LoadBar(m_panel);
        m_loadBar->setTheme(makeProto(m_dark, m_accent));
        // 先摆一次：隐藏窗口的 resizeEvent 不一定立刻到，这里给初始几何，后续靠 resizeEvent 跟随
        m_loadBar->setGeometry(16, kTitleH, std::max(0, width() - 32), m_loadBar->height());
    }
    // 历史时效清理：每分钟检查一次（时效为「关闭程序后删除」时不启用）
    m_histTimer = new QTimer(this);
    m_histTimer->setInterval(60 * 1000);
    connect(m_histTimer, &QTimer::timeout, this, [this] {
        const size_t before = m_history.size();
        purgeHistory();
        if (m_history.size() != before) { saveSettings(); refreshHistoryUI(); }
    });
    m_histTimer->start();
    setupTray();   // 托盘：点 × 隐藏到托盘，真退出走托盘菜单
    setWindowTitle(T("Excel 表格关键字搜索工具 v0.3.2"));
    resize(1020, 700);
    setMinimumSize(720, 540);
    loadData();
}
void AppWindow::goAdvPage(bool unlock) {
    switchPage(1);                              // 先确保在「设置」页
    if (m_navList && m_advSecIndex >= 0) {
        m_navList->setCurrentRow(m_advSecIndex);           // 再切到「高级设置」分区（此刻仍是锁定态）
        if (unlock) { m_advUnlocked = true; onSettingsSectionChanged(m_advSecIndex); }
    }
}
void AppWindow::switchPage(int idx) {
    if (idx < 0 || idx >= (int)m_pages.size()) return;
    if (idx != 1 && m_advUnlocked) { m_advUnlocked = false; m_adminIsSuper = false; }   // 离开设置页也上锁
    const int prev = m_stack ? m_stack->currentIndex() : -1;
    m_curPage = idx;
    if (m_stack) {
        m_stack->setCurrentIndex(idx);
        // 顶部标签切换：淡入 + 轻微上移（只对**真正换了页**时做，避免原地重复播报）
        // 方向感知：往右切 = 内容从下方进来（上移），往左切 = 从上方进来（下移）——
        // 位移方向与标签的左右顺序一致，用户能"感觉出"页面从哪来（共享轴连续性）
        if (prev != idx) enterAnim(m_stack->currentWidget(), kPageAnimMs, (idx > prev ? 1.0 : -1.0) * kEnterSlidePx);
    }
    if (idx < (int)m_navBtns.size()) m_navBtns[idx]->setChecked(true);
    if (idx == 1) onSettingsSectionChanged(m_navList ? m_navList->currentRow() : -1);
}
void AppWindow::openDataFolder() {
    const QString dir = u8(dataSourceDir());
    if (!QDir(dir).exists()) { showToast(T("文件夹不存在：") + dir); return; }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dir))) showToast(T("无法打开文件夹：") + dir);
}
QIcon AppWindow::makeAppIcon() const {
    const QIcon ic(":/app.ico");
    if (!ic.isNull()) return ic;
    QPixmap pm(64, 64); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(m_accent);
    p.drawRoundedRect(QRectF(2, 2, 60, 60), 14, 14);
    QPen pen(Qt::white, 6); pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(27, 27), 13, 13);
    p.drawLine(QPointF(37, 37), QPointF(50, 50));
    p.end();
    return QIcon(pm);
}
void AppWindow::setupTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;   // 无托盘环境：× 仍按普通关闭处理
    m_tray = new QSystemTrayIcon(makeAppIcon(), this);
    m_tray->setToolTip(windowTitle());
    m_trayMenu = new QMenu(this);
    m_trayMenu->setStyleSheet(qssFor(makeProto(m_dark, m_accent)));
    QAction* actShow = m_trayMenu->addAction(T("显示主界面"));
    m_trayMenu->addSeparator();
    QAction* actExit = m_trayMenu->addAction(T("退出程序"));
    connect(actShow, &QAction::triggered, this, [this] { showFromTray(); });
    connect(actExit, &QAction::triggered, this, [this] { quitApp(); });
    m_tray->setContextMenu(m_trayMenu);
    // 左键单击 / 双击：恢复并置前（原版 WM_LBUTTONUP 行为）
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) showFromTray();
    });
    m_tray->show();
}
void AppWindow::showFromTray() {
    showNormal();
    raise();
    activateWindow();
}
void AppWindow::quitApp() {
    m_quitting = true;
    if (m_tray) m_tray->hide();   // 清理托盘图标（等价原版 WM_DESTROY 的 NIM_DELETE）
    QApplication::quit();
}
// 通用设置里改动关闭选项后同步单选框（对话框勾了"不再询问"也要回写）
void AppWindow::refreshCloseRadios() {
    if (!m_closeAskBtn) return;
    (m_closeAction == 0 ? m_closeAskBtn : (m_closeAction == 1 ? m_closeDirectBtn : m_closeTrayBtn))->setChecked(true);
}
void AppWindow::hideToTray() {
    hide();
    if (m_tray && !m_trayHintShown) {   // 本次启动首次隐藏才气泡提示
        m_trayHintShown = true;
        m_tray->showMessage(windowTitle(), T("已最小化到托盘，双击图标可恢复"), QSystemTrayIcon::Information, 4000);
    }
}
void AppWindow::setupStateTints() {
    if (g_noAnim) return;   // 关闭动效：一个都不装，hover/焦点与动效前完全一致
    const auto protoFn = [this] { return makeProto(m_dark, m_accent); };
    for (auto* b : findChildren<QPushButton*>()) {
        if (b->property("noHoverTint").toBool()) continue;   // 例：强调色色块自带样式表，别抢
        const QString n = b->objectName();
        StateTint* st = nullptr;
        if (n == "primaryBtn")     st = attachStateTint(b, "QPushButton#primaryBtn", HtPrimary, protoFn);
        else if (n == "tbBtn")     st = attachStateTint(b, "QPushButton#tbBtn", HtTb, protoFn);
        else if (n == "navBtn")    st = attachStateTint(b, "QPushButton#navBtn", HtNav, protoFn);
        // #themeBtn 在 qssFor 里没有专属规则，靠通用 `QPushButton` 那条 —— 选择器必须跟着用通用名
        // （样式表挂在控件自身，所以只会影响这一个按钮，不会外溢）
        else if (n == "themeBtn")  st = attachStateTint(b, "QPushButton", HtPlain, protoFn);
        // 说明：#themeToggle（浅色/深色）**故意不挂** —— 它是 checkable，hover 覆盖会盖掉
        //   ":checked" 的强调色底（同优先级时控件级样式表更强），得不偿失。
        if (st) m_stateTints.push_back(st);
    }
    // 输入框焦点过渡：边框色 editBorder → accent（只改 border-color，不动布局）
    for (auto* e : findChildren<QLineEdit*>()) {
        if (qobject_cast<QComboBox*>(e->parentWidget())) continue;   // 下拉框内部的 lineEdit 不碰
        const QString sel = (e->objectName() == "searchEdit") ? "QLineEdit#searchEdit" : "QLineEdit";
        m_stateTints.push_back(new StateTint(e, sel, nullptr, nullptr, nullptr, nullptr, nullptr,
            [this] { return makeProto(m_dark, m_accent).editBorder; },
            [this] { return makeProto(m_dark, m_accent).accent; },
            -1, -1, /*focusTrigger=*/true));
    }
}
void AppWindow::closeEvent(QCloseEvent* e) {
    if (m_quitting) { e->accept(); return; }
    if (!m_tray || !m_tray->isVisible()) { e->accept(); return; }   // 无托盘环境：× 就是关闭，避免关不掉也找不到
    if (m_closeAction == 1) { quitApp(); e->accept(); return; }
    if (m_closeAction == 2) { e->ignore(); hideToTray(); return; }
    // 每次询问
    CloseDialog dlg(m_dark, m_accent, this);
    if (dlg.exec() != QDialog::Accepted) { e->ignore(); return; }   // 取消/Esc：不关闭
    if (dlg.noAsk()) {                                              // 「不再询问」→ 记住本次选择
        m_closeAction = (dlg.choice() == CloseDialog::ToTray) ? 2 : 1;
        saveSettings();
        refreshCloseRadios();
    }
    if (dlg.choice() == CloseDialog::ToTray) { e->ignore(); hideToTray(); }
    else { quitApp(); e->accept(); }
}
void AppWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        const int y = (int)e->position().y();
        if (y < kTitleH) { if (windowHandle()) windowHandle()->startSystemMove(); return; }
        const int w = width(), h = height(), x = (int)e->position().x();
        Qt::Edges ed;
        bool l = x <= 6, r = x >= w - 7, b = y >= h - 7;
        if (l && b) ed = Qt::LeftEdge | Qt::BottomEdge;
        else if (r && b) ed = Qt::RightEdge | Qt::BottomEdge;
        else if (l) ed = Qt::LeftEdge;
        else if (r) ed = Qt::RightEdge;
        else if (b) ed = Qt::BottomEdge;
        else ed = Qt::Edges();
        if (ed != Qt::Edges() && windowHandle()) { windowHandle()->startSystemResize(ed); return; }
    }
    QWidget::mousePressEvent(e);
}
void AppWindow::mouseDoubleClickEvent(QMouseEvent* e) {
    if ((int)e->position().y() < kTitleH) { toggleMax(); return; }
    QWidget::mouseDoubleClickEvent(e);
}    // B4：进度条手动定位在标题栏下方一条（居中留 16px 边距，与内容对齐）
void AppWindow::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    if (m_loadBar) m_loadBar->setGeometry(16, kTitleH, std::max(0, width() - 32), m_loadBar->height());
}
void AppWindow::toggleMax() {
    if (isMaximized()) { showNormal(); m_maxed = false; } else { showMaximized(); m_maxed = true; }
    apply();
}
void AppWindow::refreshAccentButtons() {
    if (m_accentBtns.empty()) return;
    const QColor ring = makeProto(m_dark, m_accent).text;
    for (size_t i = 0; i < m_accentBtns.size() && i < m_accents.size(); i++) {
        const bool sel = (m_accents[i] == m_accent);
        m_accentBtns[i]->setStyleSheet(QString("background:%1; border-radius:14px; border:2px solid %2;")
            .arg(m_accents[i].name(), sel ? ring.name() : QString("transparent")));
    }
}
void AppWindow::showToast(const QString& text, int ms) {
    if (!m_toast) return;
    m_toast->setText(text);
    m_toast->adjustSize();
    const int x = 18;
    const int yEnd = std::max(18, height() - m_toast->height() - 18);
    m_toast->raise(); m_toast->show();
    if (!m_toastTimer) {
        m_toastTimer = new QTimer(this); m_toastTimer->setSingleShot(true);
        connect(m_toastTimer, &QTimer::timeout, this, [this] { hideToastAnim(); });
    }
    m_toastTimer->start(ms);
    const int dur = animMs(kToastAnimMs);
    if (dur <= 0) {   // 关闭动效：直接落终态（不装 effect、不做位移）
        stopToastAnim();
        m_toast->move(x, yEnd);
        if (m_toastEff) { m_toast->setGraphicsEffect(nullptr); m_toastEff = nullptr; }
        return;
    }
    if (!m_toastAnim) {
        m_toastAnim = new QVariantAnimation(this);
        m_toastAnim->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(m_toastAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            const qreal t = v.toDouble();
            if (m_toastEff) m_toastEff->setOpacity(t);
            const int yEnd2 = std::max(18, height() - m_toast->height() - 18);
            m_toast->move(18, yEnd2 + (int)qRound((1.0 - t) * kToastRisePx));
        });
        QObject::connect(m_toastAnim, &QVariantAnimation::finished, this, [this] {
            if (m_toastFading) { m_toast->hide(); m_toast->setGraphicsEffect(nullptr); m_toastEff = nullptr; m_toastFading = false; }
        });
    }
    m_toastFading = false;
    if (!m_toastEff) { m_toastEff = new QGraphicsOpacityEffect(m_toast); m_toast->setGraphicsEffect(m_toastEff); }
    m_toastAnim->stop();
    m_toastAnim->setDuration(dur);
    m_toastAnim->setStartValue(0.0);
    m_toastAnim->setEndValue(1.0);
    m_toastAnim->start();
}
void AppWindow::stopToastAnim() {
    if (m_toastAnim) m_toastAnim->stop();
    m_toastFading = false;
}
// 到时隐藏：先淡出再 hide（关闭动效时直接 hide）
void AppWindow::hideToastAnim() {
    if (!m_toast || !m_toast->isVisible()) return;
    const int dur = animMs(kToastAnimMs);
    if (dur <= 0 || !m_toastAnim) { if (m_toast) m_toast->hide(); return; }
    m_toastFading = true;
    m_toastAnim->stop();
    m_toastAnim->setDuration(dur);
    m_toastAnim->setStartValue(m_toastEff ? m_toastEff->opacity() : 1.0);
    m_toastAnim->setEndValue(0.0);
    m_toastAnim->start();
}
QWidget* AppWindow::cardFrame(const QString& title, QWidget* body) {
    auto* f = new QFrame; f->setObjectName("card");
    auto* v = new QVBoxLayout(f); v->setContentsMargins(18, 16, 18, 18); v->setSpacing(14);
    auto* t = new QLabel(title); t->setObjectName("cardTitle");
    v->addWidget(t); v->addWidget(body);
    // 登记"随后到"的内容控件：分区切换时卡片（容器）先到、内容再淡入（错峰 kStaggerMs）
    f->setProperty("animChild", QVariant::fromValue(static_cast<QObject*>(body)));
    return f;
}
void AppWindow::buildUi() {
    auto* root = new QVBoxLayout(this); root->setContentsMargins(0, 0, 0, 0); root->setSpacing(0);
    auto* panel = new QWidget; panel->setObjectName("panel");
    m_panel = panel;   // B4 的进度条挂在它下面
    auto* pv = new QVBoxLayout(panel); pv->setContentsMargins(16, 4, 16, 12); pv->setSpacing(6);
    auto* tb = new QHBoxLayout; tb->setContentsMargins(4, 6, 4, 6); tb->setSpacing(2);
    auto* logo = new QLabel(T("🔍")); logo->setObjectName("appTitle"); tb->addWidget(logo);
    auto* title = new QLabel(T("Excel 表格关键字搜索工具")); title->setObjectName("appTitle"); tb->addWidget(title);
    tb->addStretch();
    m_themeBtn = new QPushButton(T("🌙 深色")); m_themeBtn->setObjectName("tbBtn");
    m_minBtn = new WinBtn(WinBtn::Min); m_maxBtn = new WinBtn(WinBtn::Max); m_closeBtn = new WinBtn(WinBtn::Close);
    tb->addWidget(m_themeBtn); tb->addWidget(m_minBtn); tb->addWidget(m_maxBtn); tb->addWidget(m_closeBtn);
    pv->addLayout(tb);
    connect(m_themeBtn, &QPushButton::clicked, this, &AppWindow::flipTheme);
    connect(m_minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(m_maxBtn, &QPushButton::clicked, this, [this] { toggleMax(); });
    connect(m_closeBtn, &QPushButton::clicked, this, &QWidget::close);
    auto* nav = new QHBoxLayout; nav->setContentsMargins(4, 0, 4, 0); nav->setSpacing(4);
    QButtonGroup* nbg = new QButtonGroup(this);
    m_stack = new QStackedWidget;
    for (int i = 0; i < (int)m_pages.size(); i++) {
        auto* b = new QPushButton(m_pages[i].title); b->setObjectName("navBtn"); b->setCheckable(true);
        nbg->addButton(b); nav->addWidget(b); m_navBtns.push_back(b);
        int idx = i; connect(b, &QPushButton::clicked, this, [this, idx] { switchPage(idx); });
        m_stack->addWidget(m_pages[i].make());
    }
    nav->addStretch();
    if (!m_navBtns.empty()) m_navBtns[0]->setChecked(true);
    pv->addLayout(nav);
    pv->addWidget(m_stack, 1);
    root->addWidget(panel);
}
