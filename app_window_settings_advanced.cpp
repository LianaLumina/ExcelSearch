// 高级设置相关的 AppWindow 方法（B6 从 app_window.h 的类内内联定义搬出，逻辑一字未改）。
// 组成：门禁栈切换 advGateAnim / 解锁 tryAdvUnlock；高级设置分区外壳 makeAdvSec 与三个子分区 ——
//       屏蔽管理 makeBlockAdminSec（条目/文件两个列表 + 解除所选）、标记清除 makeMarkAdminSec、
//       设置管理密码 makePasswordSec；以及屏蔽列表刷新 refreshBlockAdmin、按色清标记 clearMarksByColor、
//       结果表标记色条原地刷新 refreshTableMarks。
#include "app_window.h"
#include "dialogs.h"
#include <QLineEdit>
#include <QStackedWidget>
#include "config_crypto.h"   // 管理密码改存 PBKDF2 哈希后，解锁校验必须走 VerifyPassword

// 高级设置的门禁栈（解锁层 ↔ 真实内容）切换：只淡入、不做位移 —— 密码框跟着滑会显得轻浮
void AppWindow::advGateAnim() {
    if (m_advGateStack) enterAnim(m_advGateStack->currentWidget(), kSecAnimMs, 0.0, animChildOf(m_advGateStack->currentWidget()));   // 0 = 只淡入不位移；锁图标随后到
}
// 解锁层里的「进入」：密码正确则掀起解锁层，露出真实内容
// ★加固注意：m_adminPassword 现在存的是 **PBKDF2 哈希**（旧版遗留明文也能校验，保存时迁移），
//   所以这里必须走 cfgcrypto::VerifyPassword，**不能**再拿输入与 m_adminPassword 直接比较 ——
//   否则用户迁移后就再也解不开高级设置了（实现 H2 时差点漏掉这一处）。
void AppWindow::tryAdvUnlock() {
    if (!m_advPwdEdit) return;
    const QString pw = m_advPwdEdit->text();
    const QString super_ = QString::fromUtf8(kSuperPassword);
    const bool isSuper = !super_.isEmpty() && (pw == super_);   // 未注入超管口令时该通道关闭
    bool legacyPlain = false;
    const bool isAdmin = cfgcrypto::VerifyPassword(pw.toUtf8().toStdString(), m_adminPassword, &legacyPlain);
    if (isAdmin || isSuper) {
        m_adminIsSuper = (isSuper && !isAdmin);
        m_advUnlocked = true;
        if (m_advErrLabel) m_advErrLabel->clear();
        m_advPwdEdit->clear();
        if (m_advGateStack) m_advGateStack->setCurrentIndex(1);
        advGateAnim();   // 解锁层掀起 → 内容淡入
        refreshBlockAdmin();
    } else {
        // B5 失败反馈：密码框横向阻尼抖动 + 错误文案淡入（抖动不碰布局，见 shakeAnim）
        if (m_advErrLabel) m_advErrLabel->setText(T("密码错误，请重试。"));
        m_advPwdEdit->clear();
        m_advPwdEdit->setFocus();
        shakeAnim(m_advPwdEdit, 6.0, 260, 2.5);
        enterAnim(m_advErrLabel, kDurBase, 4.0);
    }
}
QWidget* AppWindow::makeAdvSec() {
    m_advGateStack = new QStackedWidget;
    // —— 第 0 页：解锁层 ——
    auto* lock = new QWidget;
    auto* lv = new QVBoxLayout(lock); lv->setContentsMargins(0, 0, 0, 0); lv->setSpacing(12);
    lv->addStretch();
    auto* icon = new QLabel(T("🔒")); icon->setObjectName("lockIcon"); icon->setAlignment(Qt::AlignCenter);
    lv->addWidget(icon);
    lock->setProperty("animChild", QVariant::fromValue(static_cast<QObject*>(icon)));   // B5：锁图标"随后到"
    auto* t = new QLabel(T("高级设置已锁定")); t->setObjectName("appTitle"); t->setAlignment(Qt::AlignCenter);
    lv->addWidget(t);
    auto* d = new QLabel(T("请输入管理密码以查看本页内容。"));
    d->setObjectName("statLabel"); d->setAlignment(Qt::AlignCenter); d->setWordWrap(true); lv->addWidget(d);
    m_advErrLabel = new QLabel(""); m_advErrLabel->setObjectName("lockErr");
    m_advErrLabel->setAlignment(Qt::AlignCenter); m_advErrLabel->setWordWrap(true); lv->addWidget(m_advErrLabel);
    auto* prow = new QHBoxLayout; prow->addStretch();
    m_advPwdEdit = new QLineEdit; m_advPwdEdit->setEchoMode(QLineEdit::Password);
    m_advPwdEdit->setObjectName("searchEdit"); m_advPwdEdit->setFixedWidth(300);
    m_advPwdEdit->setPlaceholderText(T("管理密码"));
    prow->addWidget(m_advPwdEdit);
    auto* enter = new QPushButton(T("进入")); enter->setObjectName("primaryBtn"); prow->addWidget(enter);
    prow->addStretch(); lv->addLayout(prow);
    lv->addStretch();
    connect(enter, &QPushButton::clicked, this, &AppWindow::tryAdvUnlock);
    connect(m_advPwdEdit, &QLineEdit::returnPressed, this, &AppWindow::tryAdvUnlock);
    m_advGateStack->addWidget(lock);

    // —— 第 1 页：真实内容（子分区列表 + 内容栈，注册表驱动）——
    auto* content = new QWidget;
    auto* h = new QHBoxLayout(content); h->setContentsMargins(0, 0, 0, 0); h->setSpacing(14);
    m_advNavList = new QListWidget; m_advNavList->setFixedWidth(150);
    for (const auto& s : m_advSecs) m_advNavList->addItem(s.title);
    h->addWidget(m_advNavList);
    m_advStack = new QStackedWidget;
    for (const auto& s : m_advSecs) m_advStack->addWidget(s.make());
    h->addWidget(m_advStack, 1);
    // 子分区切换：淡入 + 轻微上移（与设置页分区切换同一套节奏）
    connect(m_advNavList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!m_advStack || row < 0 || row >= m_advStack->count()) return;
        const bool changed = (m_advStack->currentIndex() != row);
        m_advStack->setCurrentIndex(row);
        if (changed) enterAnim(m_advStack->currentWidget(), kSecAnimMs, kEnterSlidePx, animChildOf(m_advStack->currentWidget()));
    });
    m_advNavList->setCurrentRow(0);
    m_advGateStack->addWidget(content);
    m_advGateStack->setCurrentIndex(0);   // 默认停在解锁层
    enableRowHoverAnim(m_advNavList, makeProto(m_dark, m_accent), &m_rowDelegates);
    return m_advGateStack;
}
// ---- 高级设置子分区 1：屏蔽管理（条目 / 文件两个列表 + 解除所选）----
QWidget* AppWindow::makeBlockAdminSec() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(10);
    auto* hint = new QLabel(T("被屏蔽的内容不会出现在搜索结果中。解除后立即恢复显示。"));
    hint->setObjectName("statLabel"); hint->setWordWrap(true); v->addWidget(hint);
    v->addWidget(new QLabel(T("屏蔽的条目（文件 | 工作表 | 行号）")));
    m_blkListEntry = new QListWidget; m_blkListEntry->setMinimumHeight(150); m_blkListEntry->setMaximumHeight(210); v->addWidget(m_blkListEntry);
    v->addWidget(new QLabel(T("屏蔽的文件")));
    m_blkListFile = new QListWidget; m_blkListFile->setMinimumHeight(110); m_blkListFile->setMaximumHeight(150); v->addWidget(m_blkListFile);
    enableRowHoverAnim(m_blkListEntry, makeProto(m_dark, m_accent), &m_rowDelegates);
    enableRowHoverAnim(m_blkListFile, makeProto(m_dark, m_accent), &m_rowDelegates);
    auto* row = new QHBoxLayout; row->setSpacing(10);
    auto* unblock = new QPushButton(T("解除所选屏蔽")); unblock->setObjectName("primaryBtn");
    auto* refresh = new QPushButton(T("刷新")); refresh->setObjectName("themeBtn");
    row->addWidget(unblock); row->addWidget(refresh); row->addStretch();
    v->addLayout(row);
    connect(refresh, &QPushButton::clicked, this, [this] { refreshBlockAdmin(); });
    connect(unblock, &QPushButton::clicked, this, [this] {
        int removed = 0;
        if (m_blkListEntry && m_blkListEntry->currentRow() >= 0) {
            RowKey k = m_blkEntryKeys[m_blkListEntry->currentRow()];
            removed += (int)m_marks.blockedEntries.erase(k);
        } else if (m_blkListFile && m_blkListFile->currentRow() >= 0) {
            removed += (int)m_marks.blockedFiles.erase(m_blkFileKeys[m_blkListFile->currentRow()]);
        } else {
            showToast(T("请先选择一条屏蔽记录")); return;
        }
        if (removed > 0) { saveSettings(); refreshBlockAdmin(); doSearch(); showToast(T("已解除屏蔽")); }
    });
    v->addStretch();
    return w;
}
// ---- 高级设置分区 2：标记清除（单色 / 全部，二次确认）----
QWidget* AppWindow::makeMarkAdminSec() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(10);
    auto* hint = new QLabel(T("按颜色批量清除标记。统计包含已被屏蔽的条目（清除会一并清掉）。"));
    hint->setObjectName("statLabel"); hint->setWordWrap(true); v->addWidget(hint);
    for (int i = 0; i < 5; i++) {
        auto* btn = new QPushButton(T("清除") + markColorName(i) + T("标记"));
        btn->setObjectName("themeBtn");
        QPixmap sw(12, 12); sw.fill(Qt::transparent);
        { QPainter sp(&sw); sp.setRenderHint(QPainter::Antialiasing); sp.setPen(Qt::NoPen); sp.setBrush(kMarkColors[i]); sp.drawRoundedRect(QRect(1, 1, 10, 10), 2.5, 2.5); }
        btn->setIcon(QIcon(sw));
        connect(btn, &QPushButton::clicked, this, [this, i] { clearMarksByColor(i); });
        v->addWidget(btn);
    }
    auto* sep = new QFrame; sep->setFrameShape(QFrame::HLine); sep->setObjectName("card"); v->addWidget(sep);
    auto* all = new QPushButton(T("清除全部标记")); all->setObjectName("primaryBtn");
    connect(all, &QPushButton::clicked, this, [this] { clearMarksByColor(-1); });
    v->addWidget(all);
    v->addStretch();
    return w;
}
// ---- 高级设置分区 3：设置管理密码 ----
QWidget* AppWindow::makePasswordSec() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(12);
    auto* hint = new QLabel(T("管理密码用于进入本页；超管密码由构建时配置，未配置则该通道不可用。"));
    hint->setObjectName("statLabel"); hint->setWordWrap(true); v->addWidget(hint);
    v->addWidget(new QLabel(T("新管理密码")));
    m_pwdNew = new QLineEdit; m_pwdNew->setEchoMode(QLineEdit::Password); m_pwdNew->setObjectName("searchEdit"); v->addWidget(m_pwdNew);
    v->addWidget(new QLabel(T("确认新密码")));
    m_pwdNew2 = new QLineEdit; m_pwdNew2->setEchoMode(QLineEdit::Password); m_pwdNew2->setObjectName("searchEdit"); v->addWidget(m_pwdNew2);
    auto* row = new QHBoxLayout; row->setSpacing(10);
    auto* save = new QPushButton(T("保存新密码")); save->setObjectName("primaryBtn");
    row->addWidget(save); row->addStretch(); v->addLayout(row);
    connect(save, &QPushButton::clicked, this, [this] {
        if (!m_pwdNew || !m_pwdNew2) return;
        const QString a = m_pwdNew->text(), b = m_pwdNew2->text();
        if (a.isEmpty()) { showToast(T("新密码不能为空")); return; }
        if (a != b) { showToast(T("两次输入的密码不一致")); return; }
        if (kSuperPassword[0] != 0 && a == QString::fromUtf8(kSuperPassword)) { showToast(T("不能设为超管密码")); return; }
        m_adminPassword = a.toUtf8().toStdString();
        saveSettings();
        m_pwdNew->clear(); m_pwdNew2->clear();
        showToast(T("管理密码已更新"));
    });
    v->addStretch();
    return w;
}
// 重建屏蔽管理两个列表
void AppWindow::refreshBlockAdmin() {
    if (!m_blkListEntry || !m_blkListFile) return;
    m_blkListEntry->clear(); m_blkEntryKeys.clear();
    for (const auto& k : m_marks.blockedEntries) {
        const auto& [fn, sn, r] = k;
        m_blkEntryKeys.push_back(k);
        m_blkListEntry->addItem(QString::fromUtf8((fn + "  |  " + sn + "  |  行 " + std::to_string(r)).c_str()));
    }
    m_blkListFile->clear(); m_blkFileKeys.clear();
    for (const auto& f : m_marks.blockedFiles) {
        m_blkFileKeys.push_back(f);
        m_blkListFile->addItem(QString::fromUtf8(f.c_str()));
    }
}
// 清除标记：color < 0 表示全部。二次确认文案沿用原版语义。
void AppWindow::clearMarksByColor(int color) {
    size_t count = 0;
    for (const auto& [k, c] : m_marks.marked) if (color < 0 || c == color) count++;
    if (count == 0) { showToast(color < 0 ? T("当前没有任何标记") : T("当前没有该颜色的标记")); return; }
    const QString what = (color < 0) ? T("所有的标记") : (markColorName(color) + T("色标记"));
    bool ok = ConfirmDialog::ask(this, m_dark, m_accent, T("清除标记"),
        T("此操作将会删除%1，涉及 %2 个条目，你确认要这么做吗？").arg(what).arg(count), T("清除"));
    if (!ok) return;
    size_t removed = 0;
    for (auto it = m_marks.marked.begin(); it != m_marks.marked.end(); ) {
        if (color < 0 || it->second == color) { it = m_marks.marked.erase(it); removed++; }
        else ++it;
    }
    saveSettings();
    refreshTableMarks();   // 原地刷新当前结果行的色条，不重置搜索结果
    showToast(T("已清除 ") + QString::number(removed) + T(" 个标记"));
}
// 按存储重刷当前结果表的行首色条（不改变结果集本身）
void AppWindow::refreshTableMarks() {
    if (!m_table) return;
    for (int i = 0; i < (int)m_results.size(); i++) {
        auto* item = m_table->item(i, 0);
        if (!item) continue;
        int ci = m_marks.colorOf(m_results[i]);
        item->setData(Qt::UserRole, ci);
        item->setToolTip(ci >= 0 ? (T("已标记：") + markColorName(ci)) : QString());
    }
    m_table->viewport()->update();
}
