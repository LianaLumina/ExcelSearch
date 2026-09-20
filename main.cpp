// V0.3.2 —— ExcelSearch 正式版 Qt UI（MAA 风格；由 V0.3.1.5 Qt 原型迁移而来）
// 接真 + 后台加载 + 配置持久化：core 接进 Qt，后台线程加载带进度；
// 主题/强调色/模糊开关/共享路径 存进 exe 旁 config.ini，启动恢复。
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QFrame>
#include <QMouseEvent>
#include <QWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QScrollArea>
#include <QButtonGroup>
#include <QColorDialog>
#include <QToolTip>
#include <QMenu>
#include <QTimer>
#include <QAction>
#include <QDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QCryptographicHash>
#include <QScrollBar>
#include <QFile>
#include <QKeyEvent>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QPainterPath>
#include <QLocalServer>
#include <QLocalSocket>
#include <QComboBox>
#include <QDateTime>
#include <QRadioButton>
#include <rapidfuzz/fuzz.hpp>   // 列名模糊兜底校验（与核心 fuzzySearch 同一套评分）
#include <QThread>
#include <QEventLoop>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QPixmap>
#include <QScreen>   // 说明书窗口最大化用 screen()->availableGeometry()
#include <QColor>
#include <QString>
#include <QPainter>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QPointer>
#include <QEnterEvent>
#include <functional>
#include <cmath>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <map>
#include <tuple>
#include <algorithm>
#include <ctime>
#include <cstring>

#include "search_engine.h"
#include "xlsx_reader.h"
#include "xls_reader.h"
#include "csv_reader.h"
#include "docx_reader.h"
#include "xse_codec.h"
#include "miniz.h"
#include "common.h"
#include "theme.h"
#include "animations.h"
#include "widgets.h"
#include "dialogs.h"
#include "manual_window.h"
#include "data_model.h"
#include "loading.h"
#include "app_window.h"
#include "config_crypto.h"   // 加固：config.ini 敏感值加密 / 管理密码哈希（--cfgselftest 用）



// 自检钩子输出：本程序是 GUI 子系统（不分配控制台），所以钩子结果统一写文件而不是 stdout。
// 用法：--search 工日 --out out.txt / --colprobe 核定工日 --out out.txt
static void writeHookOut(const QString& path, const QString& text) {
    if (path.isEmpty()) return;
    std::ofstream o(path.toLocal8Bit().constData());
    if (o) o << text.toUtf8().constData();
}

// 自检钩子：--cfgselftest —— 验证 config.ini 加固用的加密与口令哈希（**纯内存，不读写 config.ini**）。
// 覆盖：密钥装载、加解密往返（含中文/换行/空串）、明文兼容、**错误密钥必须解不开**、
//       **篡改密文必须被 GCM 认证拒绝**、管理密码哈希（正确/错误/旧版明文迁移识别）。
// 注意：结果写文件（GUI 子系统无控制台），用法 `--cfgselftest --out r.txt`。
static QString cfgRunSelfTest() {
    QString out;
    int pass = 0, fail = 0;
    auto check = [&](const char* name, bool ok) {
        out += QStringLiteral("  %1 %2\n").arg(ok ? "PASS" : "FAIL", QString::fromUtf8(name));
        if (ok) ++pass; else ++fail;
    };

    // 1) 密钥装载（首次生成 + 回填 keyblob）
    bool created = false;
    std::string blob1, blobJunk;
    const bool created1 = cfgcrypto::LoadOrCreateKey(std::string(), &created, &blob1);
    check("key-create", created1 && created && blob1.size() > 40);
    check("key-ready", cfgcrypto::KeyReady());

    // 2) 往返（中文 + 换行 + 空串）
    const std::string plain = "p@ss-中文密码-\n第二行";
    const std::string enc = cfgcrypto::Protect(plain);
    check("protect-nonempty", !enc.empty() && cfgcrypto::IsProtected(enc));
    std::string back; bool wasProtected = false;
    const bool okRound = cfgcrypto::Unprotect(enc, &back, &wasProtected);
    check("roundtrip-equal", okRound && wasProtected && back == plain);
    const std::string encEmpty = cfgcrypto::Protect(std::string());
    std::string backEmpty; bool wpEmpty = false;
    check("roundtrip-empty", cfgcrypto::Unprotect(encEmpty, &backEmpty, &wpEmpty) && backEmpty.empty() && wpEmpty);

    // 3) 旧版明文兼容（原样返回，且不标记为密文）
    std::string legacyOut; bool legacyProtected = true;
    const bool okLegacy = cfgcrypto::Unprotect("plain-legacy-value", &legacyOut, &legacyProtected);
    check("legacy-passthrough", okLegacy && !legacyProtected && legacyOut == "plain-legacy-value");

    // 4) 错误密钥必须解不开（换一把新密钥后再试）
    bool created2 = false;
    cfgcrypto::ResetKeyForTest();
    const bool okNewKey = cfgcrypto::LoadOrCreateKey(std::string(), &created2, &blobJunk) && created2;
    std::string wrongOut; bool wpWrong = false;
    const bool okWrong = cfgcrypto::Unprotect(enc, &wrongOut, &wpWrong);
    check("wrong-key-fails", okNewKey && !okWrong && wpWrong);
    // 恢复原密钥（后续测试与真实使用都基于它）
    bool created3 = true;
    const bool restored = cfgcrypto::LoadOrCreateKey(blob1, &created3, &blobJunk);
    check("key-restore", restored && !created3);

    // 5) 篡改密文必须被拒绝（改一个 hex 位）
    std::string tampered = enc;
    const size_t pos = tampered.size() - 3;   // 落在密文尾部
    tampered[pos] = (tampered[pos] == 'a') ? 'b' : 'a';
    std::string tamperOut; bool wpTamper = false;
    const bool okTamper = cfgcrypto::Unprotect(tampered, &tamperOut, &wpTamper);
    check("tamper-rejected", !okTamper && wpTamper);

    // 6) 管理密码哈希
    const std::string h = cfgcrypto::HashPassword("abc123");
    bool isLegacy = true;
    check("pwd-hash-format", h.rfind("pbkdf2$200000$", 0) == 0);
    check("pwd-verify-ok", cfgcrypto::VerifyPassword("abc123", h, &isLegacy) && !isLegacy);
    check("pwd-verify-bad", !cfgcrypto::VerifyPassword("abc124", h, &isLegacy));
    check("pwd-verify-empty", !cfgcrypto::VerifyPassword(std::string(), h, &isLegacy));
    bool legacyFlag = false;
    check("pwd-legacy-plain", cfgcrypto::VerifyPassword("old", "old", &legacyFlag) && legacyFlag);

    out.prepend(QStringLiteral("cfgselftest=%1\npass=%2 fail=%3\n")
                    .arg(fail == 0 ? "ok" : "fail").arg(pass).arg(fail));
    return out;
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    // ★回迁：应用名固定为 ExcelSearch —— 决定配置/缓存目录 = %APPDATA%\ExcelSearch\
    //   （与原版 V0.3.0 的 g_configFolder 同一目录，用户设置不搬家；不要设 organizationName，否则会多套一层）
    QApplication::setApplicationName("ExcelSearch");
    // 统一程序图标：窗口 / 任务栏 / 托盘 全部用 exe 内嵌的 app.ico（与资源管理器一致）
    QApplication::setWindowIcon(QIcon(":/app.ico"));
    QString shot; bool toSettings = false, toDark = false; QString report, exp;
    QString blockSpec, markSpec, searchKw; bool clearMarks = false;
    QString filterKw;
    QString colProbe;
    QString shotKw; bool shotKwSet = false; bool toLight = false;
    bool toAdv = false; int advSec = -1; int setSec = -1; int tabIdx = -1;
    bool toCloseDlg = false;
    bool toManual = false;   // --page manual：渲染使用说明书窗口
    QString collapseSpec, hoverWhat;   // 截图用：折叠指定卡片 / 强制 hover 终态（见 demoCollapse/demoHover）
    QString midMs;                     // 截图用：--mid <毫秒> 只等指定时长（抓动画中间帧）
    bool reloadBeforeShot = false;     // 截图用：--reload 截图前再触发一次"重新加载"
    QString shareSpec; bool shareSet = false, shareOff = false;
    QString hookOut;   // --out <file>：自检钩子结果写文件（GUI 子系统无控制台）
    bool migrateNow = false;   // --migrate：强制跑一次旧版设置搬迁
    bool cfgSelfTest = false;  // --cfgselftest：配置加密/口令哈希自测（纯内存）
    // 动效总开关：**必须在构造 AppWindow 之前生效**（否则可能已经起过动画）。
    // 环境变量 EXCELSEARCH_NO_ANIM=1（推荐，离屏截图/自检用）或 CLI --no-anim，二选一。
    // 注：沿用原型期的旧名 UI_PROTO_NO_ANIM 作为兼容别名（现有脚本/文档仍可用）。
    g_noAnim = qEnvironmentVariableIntValue("EXCELSEARCH_NO_ANIM") > 0
            || qEnvironmentVariableIntValue("UI_PROTO_NO_ANIM") > 0;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--shot") == 0 && i + 1 < argc) shot = QString::fromLocal8Bit(argv[i + 1]);
        if (std::strcmp(argv[i], "--page") == 0 && i + 1 < argc && std::strcmp(argv[i + 1], "settings") == 0) toSettings = true;
        if (std::strcmp(argv[i], "--page") == 0 && i + 1 < argc && std::strcmp(argv[i + 1], "adv") == 0) toAdv = true;
        if (std::strcmp(argv[i], "--page") == 0 && i + 1 < argc && std::strcmp(argv[i + 1], "closedlg") == 0) toCloseDlg = true;
        if (std::strcmp(argv[i], "--page") == 0 && i + 1 < argc && std::strcmp(argv[i + 1], "manual") == 0) toManual = true;
        if (std::strcmp(argv[i], "--advsec") == 0 && i + 1 < argc) advSec = atoi(argv[i + 1]);
        if (std::strcmp(argv[i], "--sec") == 0 && i + 1 < argc) setSec = atoi(argv[i + 1]);
        if (std::strcmp(argv[i], "--tab") == 0 && i + 1 < argc) tabIdx = atoi(argv[i + 1]);
        if (std::strcmp(argv[i], "--share") == 0 && i + 1 < argc) { shareSpec = QString::fromLocal8Bit(argv[i + 1]); shareSet = true; }
        if (std::strcmp(argv[i], "--shareoff") == 0) { shareOff = true; shareSet = true; }
        if (std::strcmp(argv[i], "--migrate") == 0) migrateNow = true;
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) hookOut = QString::fromLocal8Bit(argv[i + 1]);
        if (std::strcmp(argv[i], "--dark") == 0) toDark = true;
        if (std::strcmp(argv[i], "--report") == 0 && i + 1 < argc) report = QString::fromLocal8Bit(argv[i + 1]);
        if (std::strcmp(argv[i], "--exportdemo") == 0 && i + 1 < argc) exp = QString::fromLocal8Bit(argv[i + 1]);
        if (std::strcmp(argv[i], "--block") == 0 && i + 1 < argc) blockSpec = QString::fromLocal8Bit(argv[i + 1]);
        if (std::strcmp(argv[i], "--mark") == 0 && i + 1 < argc) markSpec = QString::fromLocal8Bit(argv[i + 1]);
        if (std::strcmp(argv[i], "--clearmarks") == 0) clearMarks = true;
        if (std::strcmp(argv[i], "--search") == 0 && i + 1 < argc) searchKw = QString::fromLocal8Bit(argv[i + 1]);
        if (std::strcmp(argv[i], "--filter") == 0 && i + 1 < argc) filterKw = QString::fromLocal8Bit(argv[i + 1]);
        if (std::strcmp(argv[i], "--colprobe") == 0 && i + 1 < argc) colProbe = QString::fromLocal8Bit(argv[i + 1]);
        if (std::strcmp(argv[i], "--kw") == 0) {
            shotKwSet = true;   // 裸 --kw 视为空关键词：空输入框，用于查看占位符
            if (i + 1 < argc && argv[i + 1][0] != '-') shotKw = QString::fromLocal8Bit(argv[++i]);
        }
        if (std::strcmp(argv[i], "--light") == 0) toLight = true;
        if (std::strcmp(argv[i], "--collapse") == 0 && i + 1 < argc) collapseSpec = QString::fromLocal8Bit(argv[i + 1]);
        if (std::strcmp(argv[i], "--hover") == 0 && i + 1 < argc) hoverWhat = QString::fromLocal8Bit(argv[i + 1]);
        if (std::strcmp(argv[i], "--mid") == 0 && i + 1 < argc) midMs = QString::fromLocal8Bit(argv[i + 1]);
        if (std::strcmp(argv[i], "--reload") == 0) reloadBeforeShot = true;
        if (std::strcmp(argv[i], "--no-anim") == 0) g_noAnim = true;   // 等价 UI_PROTO_NO_ANIM=1
        if (std::strcmp(argv[i], "--cfgselftest") == 0) cfgSelfTest = true;
    }
    // 自检钩子：配置加固自测（纯内存，不碰 config.ini，也不受单实例闸门影响）
    if (cfgSelfTest) {
        writeHookOut(hookOut, cfgRunSelfTest());
        return 0;
    }
    // ================= 单实例闸门 =================
    // 为什么必须有：多实例会同时读写同一份 config.ini 与同一对 cache.dat/cache.inv。
    // 缓存是"两个文件配对"的（cache.inv 的清单 + cache.dat 的数据），写入非原子，
    // 交错后可能留下"inv 来自 A、dat 已被 B 覆盖"的撕裂对 —— A 启动时清单恰好匹配，
    // 于是把 B 的数据当成自己的加载，缓存还显示命中，**静默给出错误结果**。
    // 因此这里强制运行时唯一：第二个实例把已运行的窗口唤起来，然后自己退出。
    static const char* kInstanceKey = "ExcelSearch.V0.3.2.single";
    const bool isHookMode = !report.isEmpty() || !exp.isEmpty() || !shot.isEmpty() || !searchKw.isEmpty()
                          || !colProbe.isEmpty() || shareSet || clearMarks
                          || !blockSpec.isEmpty() || !markSpec.isEmpty();
    QLocalSocket instanceProbe;
    instanceProbe.connectToServer(kInstanceKey);
    if (instanceProbe.waitForConnected(300)) {   // 已有实例在跑
        if (isHookMode) {   // 自检钩子：明确报错退出，绝不去和运行中的实例抢缓存/配置
            const QString diag = "error=another-instance-running\n";
            if (!hookOut.isEmpty()) writeHookOut(hookOut, diag);
            if (!report.isEmpty()) writeHookOut(report, diag);
        } else {            // 普通重复启动：让已运行的实例把窗口显示出来，再自己退出
            instanceProbe.write("show");
            instanceProbe.flush();
            instanceProbe.waitForBytesWritten(500);
        }
        return 0;
    }
    // 截图确定性：关掉光标闪烁。焦点输入框的文字光标本来就会闪（500ms 周期），
    // 截图会随机带上/不带那 1×14px 的竖线（实测差 14 个像素），让"逐像素对比"变成掷骰子。
    // 只影响截图路径，不改变任何交互行为。
    if (!shot.isEmpty()) QApplication::setCursorFlashTime(0);
    QLocalServer::removeServer(kInstanceKey);   // 清理上次异常退出可能残留的名字
    QLocalServer* instanceServer = new QLocalServer(&app);
    instanceServer->listen(kInstanceKey);

    AppWindow w;
    w.show();
    // 启动后检查使用说明书：内容与上次关闭的版本不同才弹（与 MAA 公告一致）
    QTimer::singleShot(1200, &w, [&w] { w.maybeShowManual(); });
    // 重复启动时，已运行的实例收到 "show" → 把窗口从托盘/后台唤到前台
    QObject::connect(instanceServer, &QLocalServer::newConnection, &w, [instanceServer, &w] {
        while (QLocalSocket* s = instanceServer->nextPendingConnection()) {
            s->waitForReadyRead(200);
            if (s->readAll().contains("show")) w.showFromTray();
            s->disconnectFromServer();
            s->deleteLater();
        }
    });
    if (migrateNow) { writeHookOut(hookOut, w.demoMigrate()); return 0; }   // 自检钩子：强制搬迁旧版设置
    if (!blockSpec.isEmpty() || !markSpec.isEmpty() || clearMarks) {   // 自检钩子：写入屏蔽/标记并持久化
        w.applyMarkCli(blockSpec, markSpec, clearMarks);
        return 0;
    }
    if (shareSet) {   // 自检钩子：切换数据源模式（只改配置，不加载）
        w.applyShareCli(shareSpec, shareOff);
        return 0;
    }
    if (!colProbe.isEmpty()) {   // 自检钩子：列名解析（空 = 列不存在，UI 会提示重输）
        w.setAllowPrompt(false); w.waitForLoad();
        const QString r = w.demoResolveColumn(colProbe.toUtf8().constData());
        writeHookOut(hookOut, QString("input=%1 resolved=%2 => %3\n")
            .arg(colProbe, r, r.isEmpty() ? "NOT-FOUND" : "OK"));
        return 0;
    }
    if (!searchKw.isEmpty()) {   // 自检钩子：输出搜索命中数与被屏蔽过滤数
        w.setAllowPrompt(false); w.waitForLoad();
        const qulonglong hits = w.demoHits(searchKw.toUtf8().constData());
        QString out = QString("hits=%1 blocked=%2 first=%3\n")
            .arg(hits).arg(w.demoBlocked()).arg(w.firstHitKey());
        if (!filterKw.isEmpty()) {   // 逗号分隔 = 依次筛选（等价"改条件再点筛选"）
            for (const QString& k : filterKw.split(',', Qt::SkipEmptyParts)) {
                const QString kk = k.trimmed();
                out += QString("filtered[%1]=%2\n").arg(kk).arg(w.demoFilter(kk.toUtf8().constData()));
            }
        }
        writeHookOut(hookOut, out);
        return 0;
    }
    if (!report.isEmpty()) {   // 无头自检：输出 已加载文件/索引条数/需密码跳过数/缓存/屏蔽标记规模
        w.setAllowPrompt(false); w.waitForLoad();
        std::ofstream o(report.toLocal8Bit().constData());
        o << "files=" << w.loadedCount() << "\nentries=" << w.entryCount() << "\nskipped=" << w.skippedCount()
          << "\ncache=" << (w.usedCache() ? "hit" : "miss")
          << "\nfailedFiles=" << w.failedFileCountC()
          << "\nfailedList=" << w.failedListC().toUtf8().constData()
          << "\nloadTimeout=" << (w.loadTimedOutC() ? "1" : "0")
          << "\nblockedEntries=" << w.blockedEntryCount() << "\nblockedFiles=" << w.blockedFileCount()
          << "\nmarked=" << w.markedCount()
          << "\nhist=" << w.historyCount() << "\nhistShow=" << w.historyShow() << "\nhistTtl=" << w.historyTtl()
          << "\nfilterMode=" << w.filterMode()
          << "\nshareMode=" << (w.shareEnabled() ? "on" : "off") << "\nsharePath=" << w.sharePathC()
          << "\ntray=" << (w.trayActive() ? "on" : "off")
          << "\nappIcon=" << (w.appIconIsResource() ? "resource" : "fallback")
          << "\nmanualText=" << (manualText().isEmpty() ? "empty" : "ok")
          << "\nmanualHash=" << manualHash(manualText()).left(8).toUtf8().constData()
          << "\nmanualNever=" << (w.manualNeverShowC() ? "1" : "0")
          << "\nmanualOverflow=" << manualOverflowSections().toUtf8().constData()
          << "\nmanualNav=" << w.manualNavReport().toUtf8().constData()   // 目录栏标题是否被截断
          << "\nmanualSections=" << manualSections(manualText()).size()
          << "\ncloseAction=" << w.closeActionName()
          << "\nanim=" << w.animName() << "\n";   // 新增字段（既有字段含义未变）：证明动效开关真的生效
        o.close();
        return 0;
    }
    if (!exp.isEmpty()) {   // 导出自检：把搜索"工日"的结果导出成 xlsx
        w.setAllowPrompt(false); w.waitForLoad(); w.demoSearch("工日");
        int ok = w.exportAllTo(exp.toLocal8Bit().constData());
        std::ofstream o(std::string(exp.toLocal8Bit().constData()) + ".ok"); o << (ok ? "OK" : "FAIL") << "\n"; o.close();
        return 0;
    }
    if (!shot.isEmpty()) {
        w.setAllowPrompt(false);   // 离屏截图：不弹附加密码框
        if (toDark) w.setDark(true);
        if (toLight) w.setDark(false);
        w.waitForLoad();
        if (toSettings) { w.goSettingsPage(); w.goSection(setSec >= 0 ? setSec : 0); }
        else if (toCloseDlg) { w.demoCloseDialogPixmap().save(shot); return 0; }   // 关闭方式对话框
        else if (toManual) { w.demoManualPixmap().save(shot); return 0; }   // 使用说明书窗口
        else if (toAdv) { w.goAdvPage(advSec >= 0); if (advSec >= 0) w.goAdvSection(advSec); }   // 截图用：无 --advsec 时停在解锁层
        else if (shotKwSet) { if (!shotKw.isEmpty()) w.demoSearch(shotKw.toUtf8().constData()); }   // --kw "" → 空输入框(看占位符)
        else w.demoSearch("工日");
        if (tabIdx >= 0) w.demoTab(tabIdx);   // 可选：切到指定顶级标签
        // 截图用：复现「已有数据 → 点重新加载」。先 settle 一次让界面稳定（模拟人已经看了一会儿），
        // 再走真实路径触发加载 —— 否则启动时那次数值滚动还在飞，会把「重新加载」的效果盖住。
        if (reloadBeforeShot) { w.settleAnim(); w.forceReloadForDemo(); }
        if (!collapseSpec.isEmpty()) w.demoCollapse(collapseSpec);   // 截图用：折叠指定卡片
        if (!hoverWhat.isEmpty()) w.demoHover(hoverWhat);            // 截图用：强制 hover 终态
        // 等动效落定再截：否则会拍到淡入/位移的中间态（加 UI_PROTO_NO_ANIM=1 时是空操作，直接就是终态）
        if (!midMs.isEmpty()) w.settleAnim(midMs.toInt()); else w.settleAnim();
        QPixmap pm = w.grab();
        pm.save(shot);
        return 0;
    }
    return app.exec();
}


