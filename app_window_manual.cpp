// 使用说明书窗口相关的 AppWindow 方法（B2 从 app_window.h 的类内内联定义搬出，逻辑一字未改）。
// 内容来源与门禁语义见 manual_window.h；这里只做"主窗口 → 说明书窗口"的粘合与自检钩子。
#include "app_window.h"
// ---- 使用说明书（版式与交互对齐 MAA「公告」框）----
// 打开说明书；若用户勾了「下次说明书更新前不再显示」，则记住**当前内容版本**（哈希）——
// 内容一旦更新（哈希变化），下次启动会自动恢复提示（与 MAA 的 DoNotShowAgain 语义一致）。
void AppWindow::openManual() {
    if (m_manualWin) { m_manualWin->raise(); m_manualWin->activateWindow(); return; }   // 已开着就置前，不重复弹
    const QString md = manualText();
    if (md.isEmpty()) { showToast(T("未找到说明书内容（MANUAL.md）")); return; }
    // 独立窗口（非模态）：勾了「下次更新前不再展示」才记住当前内容版本，内容一更新就恢复提示
    auto* win = new ManualDialog(m_dark, m_accent, md, [this, md](bool noRemind) {
        if (!noRemind) return;
        QSettings s(QString::fromUtf8(m_settingsPath.c_str()), QSettings::IniFormat);
        s.setValue("manual/dismissedHash", manualHash(md));
    }, this);
    win->setAttribute(Qt::WA_DeleteOnClose);
    m_manualWin = win;
    connect(win, &QObject::destroyed, this, [this] { m_manualWin = nullptr; });
    win->show();
    win->raise();
    win->activateWindow();
}
// 启动时检查：未永久关闭、且当前内容版本 ≠ 上次已关闭的版本 → 弹出
void AppWindow::maybeShowManual() {
    if (m_manualNeverShow) return;
    const QString md = manualText();
    if (md.isEmpty()) return;
    QSettings s(QString::fromUtf8(m_settingsPath.c_str()), QSettings::IniFormat);
    if (s.value("manual/dismissedHash", "").toString() == manualHash(md)) return;
    openManual();
}
// 自检钩子：--page manual 渲染使用说明书窗口
QPixmap AppWindow::demoManualPixmap() {
    ManualDialog dlg(m_dark, m_accent, manualText(), [](bool) {}, this);
    dlg.show();
    QCoreApplication::processEvents();
    QPixmap pm = dlg.grab();
    dlg.hide();
    return pm;
}
// 自检：真实控件里检查目录栏是否会截断标题（--report 用；会短暂显示一次说明书窗口）
QString AppWindow::manualNavReport() {
    ManualDialog dlg(m_dark, m_accent, manualText(), [](bool) {}, this);
    dlg.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    const QString r = dlg.navReport();
    const QString mode = dlg.modeReport();
    // 门禁自检：未读完 → 复选灰、关不掉、按钮变调侃；滚到底 → 复选可用、可关
    const QString g0 = QStringLiteral("chk=%1,gate=%2")
        .arg(dlg.testChkEnabled() ? 1 : 0).arg(dlg.testGateOpen() ? 1 : 0);
    const QString nag = dlg.testNagOnce();
    dlg.testScrollBottom();
    QCoreApplication::processEvents();
    const QString g1 = QStringLiteral("chk=%1,gate=%2")
        .arg(dlg.testChkEnabled() ? 1 : 0).arg(dlg.testGateOpen() ? 1 : 0);
    const QString rects = dlg.relRects();
    const QRect normal = dlg.geometry();
    dlg.testToggleMax();   // 最大化 → 应等于屏幕可用区域（不盖任务栏）
    QCoreApplication::processEvents();
    const QRect maxed = dlg.geometry();
    const QRect avail = dlg.screen() ? dlg.screen()->availableGeometry() : QRect();
    dlg.testToggleMax();   // 还原 → 应回到原尺寸
    QCoreApplication::processEvents();
    const QRect back = dlg.geometry();
    const bool closed = dlg.testTryClose();   // 读到底后点确认应当真的关掉（放最后，关窗后不再操作）
    dlg.hide();
    return mode + QStringLiteral("|nav=") + r
        + QStringLiteral("|normal=%1x%2").arg(normal.width()).arg(normal.height())
        + QStringLiteral("|max=%1x%2").arg(maxed.width()).arg(maxed.height())
        + QStringLiteral("|avail=%1x%2").arg(avail.width()).arg(avail.height())
        + QStringLiteral("|maxEqAvail=%1").arg(maxed == avail && !avail.isEmpty() ? QStringLiteral("yes") : QStringLiteral("no"))
        + QStringLiteral("|restore=%1x%2").arg(back.width()).arg(back.height())
        + QStringLiteral("|gate0=[%1]").arg(g0) + QStringLiteral("|nag=[%1]").arg(nag)
        + QStringLiteral("|gateRead=[%1]").arg(g1)
        + QStringLiteral("|closedAfterRead=%1").arg(closed ? 1 : 0)
        + QStringLiteral("|rects=") + rects;
}
