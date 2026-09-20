// 共享模式与附加加密相关的 AppWindow 方法（B5 从 app_window.h 的类内内联定义搬出，逻辑一字未改）。
// 共享模式：UNC 数据源（只读）+ 连通性探测（worker，避免 UNC 阻塞 UI）+ 路径规范化 + 强制重载；
// 附加加密：.xse 附加密码的启用与保存；两者共同构成设置页的「共享设置」「加密设置」两张子卡。
#include "app_window.h"
#include <QMessageBox>

// 自检钩子：--share <UNC 路径> 打开共享模式并保存；--shareoff 关回离线模式
void AppWindow::applyShareCli(const QString& path, bool off) {
    if (off) { m_shareMode = false; }
    else { m_shareMode = true; if (!path.isEmpty()) m_sharePath = normalizeSharePath(path).toUtf8().toStdString(); }
    saveSettings();
}
// 规范化共享路径：去首尾空白、去尾部反斜杠（保留 "\\host\share" 的前导）
QString AppWindow::normalizeSharePath(const QString& raw) {
    QString s = raw.trimmed();
    while (s.size() > 2 && s.endsWith('\\')) s.chop(1);
    return s;
}
// 启动连通性探测（worker）；ok 回调在主线程执行
void AppWindow::startProbe(std::function<void(bool)> onDone) {
    if (m_probe && m_probe->isRunning()) return;
    auto* pw = new ProbeWorker; pw->path = m_sharePath; m_probe = pw;
    connect(pw, &ProbeWorker::finished, this, [this, onDone] {
        const bool ok = m_probe && m_probe->ok;
        const QString detail = m_probe ? QString::fromUtf8(m_probe->detail.c_str()) : QString();
        if (m_probe) { m_probe->deleteLater(); m_probe = nullptr; }
        setShareStatus(ok ? T("连接正常 · ") + u8(m_sharePath) : (T("连接失败：") + detail));
        onDone(ok);
    });
    pw->start();
}
// 手动「测试连接」
void AppWindow::testShareConnection() {
    if (m_sharePath.empty()) { setShareStatus(T("请先填写共享路径")); return; }
    setShareStatus(T("正在测试连接…"));
    startProbe([](bool) {});
}
// 手动「强制重载」：忽略缓存；仅共享模式下先验连通性（失败则不重载）
void AppWindow::forceReloadNow() {
    if (m_shareMode) {
        setShareStatus(T("正在测试连接…"));
        startProbe([this](bool ok) {
            if (!ok) { setShareStatus(T("连接失败，已取消强制重载")); return; }   // 静默拒绝执行
            doForceReload();
        });
        return;
    }
    doForceReload();
}
void AppWindow::doForceReload() {
    m_forceReload = true;
    loadData(/*forceReload=*/true);
    m_forceReload = false;
}
// 路径改完（编辑结束）触发：规范化 → 保存 → 自动"测试连接 →（成功）强制重载"
void AppWindow::onSharePathEdited() {
    if (!m_shareEdit) return;
    const QString norm = normalizeSharePath(m_shareEdit->text());
    if (norm == QString::fromUtf8(m_sharePath.c_str())) return;   // 没变，不折腾
    m_shareEdit->setText(norm);
    m_sharePath = norm.toUtf8().toStdString();
    saveSettings();
    if (!m_shareMode) { setShareStatus(T("路径已保存（离线模式下不生效）")); return; }
    if (m_sharePath.empty()) { setShareStatus(T("共享路径为空")); return; }
    setShareStatus(T("正在测试连接…"));
    startProbe([this](bool ok) {
        if (!ok) return;    // 静默拒绝：不弹窗、不重载，状态行已显示失败原因
        doForceReload();
    });
}
void AppWindow::setShareMode(bool on) {
    if (m_shareMode == on) return;
    if (on && m_sharePath.empty()) { setShareStatus(T("请先填写共享路径")); return; }
    m_shareMode = on;
    saveSettings();
    m_engine.clear(); m_loadedFiles = 0; m_skipped = 0; m_failedCount = 0; m_failedList.clear(); m_loadTimedOut = false; updateStats();
    setShareStatus(on ? T("已切换到共享模式，正在重新加载…") : T("已切换到离线模式，正在重新加载…"));
    loadData();
}
QWidget* AppWindow::makeEncSec() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(14);
    v->addWidget(new QLabel(T("附加密码（加密文件 .xse）")));
    auto* desc = new QLabel(T("启用后，带附加密码的 .xse 加密文件可用此密码自动解密；未启用则由用户在加载时输入。"));
    desc->setObjectName("statLabel"); desc->setWordWrap(true);
    v->addWidget(desc);
    m_encChk = new QCheckBox(T("启用附加密码"));
    m_encChk->setChecked(m_addPwdEnabled);
    v->addWidget(m_encChk);
    v->addWidget(new QLabel(T("附加密码")));
    m_encEdit = new QLineEdit(u8(m_addPwd)); m_encEdit->setEchoMode(QLineEdit::Password);
    m_encEdit->setEnabled(m_addPwdEnabled);
    v->addWidget(m_encEdit);
    connect(m_encChk, &QCheckBox::toggled, this, [this](bool on) { if (m_encEdit) m_encEdit->setEnabled(on); });
    auto* save = new QPushButton(T("保存")); save->setObjectName("primaryBtn");
    connect(save, &QPushButton::clicked, this, [this] {
        if (!m_encChk || !m_encEdit) return;
        m_addPwdEnabled = m_encChk->isChecked();
        m_addPwd = m_encEdit->text().toUtf8().toStdString();
        if (m_addPwdEnabled && m_addPwd.empty()) { QMessageBox::warning(this, T("提示"), T("已启用附加密码但未输入密码")); return; }
        saveSettings();
    });
    auto* row = new QHBoxLayout; row->addWidget(save); row->addStretch();
    v->addLayout(row);
    v->addStretch();
    return w;
}
QWidget* AppWindow::makeShareSec() {
    auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(12);
    // 按用户要求：本页不放说明性文字（对普通用户不友好、易造成混乱），只留功能标签与操作
    v->addWidget(new QLabel(T("数据来源")));
    m_modeOfflineBtn = new QRadioButton(T("离线模式（使用程序同目录的 data\\）"));
    m_modeShareBtn = new QRadioButton(T("共享模式（使用下面的共享目录）"));
    // 先设初值再连信号，避免构造期触发切换/重载
    (m_shareMode ? m_modeShareBtn : m_modeOfflineBtn)->setChecked(true);
    QButtonGroup* mbg = new QButtonGroup(this);
    mbg->addButton(m_modeOfflineBtn); mbg->addButton(m_modeShareBtn);
    v->addWidget(m_modeOfflineBtn); v->addWidget(m_modeShareBtn);
    connect(m_modeOfflineBtn, &QRadioButton::toggled, this, [this](bool on) { if (on) setShareMode(false); });
    connect(m_modeShareBtn, &QRadioButton::toggled, this, [this](bool on) { if (on) setShareMode(true); });

    auto* sep = new QFrame; sep->setFrameShape(QFrame::HLine); sep->setObjectName("card"); v->addWidget(sep);

    v->addWidget(new QLabel(T("共享目录（UNC 路径）")));
    m_shareEdit = new QLineEdit(u8(m_sharePath));
    m_shareEdit->setPlaceholderText(T("\\\\主机名\\共享名\\子目录"));
    m_shareEdit->setClearButtonEnabled(true);
    v->addWidget(m_shareEdit);
    connect(m_shareEdit, &QLineEdit::editingFinished, this, [this] { onSharePathEdited(); });

    auto* row = new QHBoxLayout; row->setSpacing(10);
    auto* testBtn = new QPushButton(T("测试连接")); testBtn->setObjectName("themeBtn");
    auto* forceBtn = new QPushButton(T("强制重载")); forceBtn->setObjectName("primaryBtn");
    row->addWidget(testBtn); row->addWidget(forceBtn); row->addStretch();
    v->addLayout(row);
    connect(testBtn, &QPushButton::clicked, this, [this] { testShareConnection(); });
    connect(forceBtn, &QPushButton::clicked, this, [this] {
        bool ok = ConfirmDialog::ask(this, m_dark, m_accent, T("强制重载"),
            m_shareMode ? T("将忽略缓存、重新从共享目录读取全部文件并重建索引。\n共享目录文件多时可能较慢，确认继续？")
                        : T("将忽略缓存、重新从本地 data\\ 读取全部文件并重建索引。确认继续？"),
            T("重载"));
        if (ok) forceReloadNow();
    });

    m_shareStatus = new QLabel(""); m_shareStatus->setObjectName("statLabel"); m_shareStatus->setWordWrap(true);
    v->addWidget(m_shareStatus);
    setShareStatus(m_shareMode ? (T("共享模式 · ") + u8(m_sharePath)) : T("离线模式 · ") + u8(m_dataDir));
    v->addStretch();
    return w;
}
