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


// ============================================================================
// 控件动效总纲（V0.3.2 新增）
// ----------------------------------------------------------------------------
// 设计原则（改这段之前先读，改完再回去读 docs/动效设计.md）：
//   1) **只作用在子控件上**。顶级无边框窗口开了 WA_TranslucentBackground，Win10 下做
//      尺寸/位置动画会闪（历史已确认，用户选择"质感优先、接受静止"），所以窗口本体永远静止，
//      所有过渡都发生在窗口内部的控件上。
//   2) **颜色一律取自 Proto 令牌**（hover/pressed/text/sub/accent/closeHover…），不硬编码；
//      深浅色 + 5 种强调色下都要成立。
//   3) 克制：时长 90–240ms，曲线分三类（进入用 emphasis-decel、退出用 accel、强调用轻回弹），
//      幅度 ≤ 10px；只表达"层级 / 因果 / 状态"，不做装饰性动作，不给每个控件发明一套新效果。
//   4) **必须能一键关闭**：环境变量 `UI_PROTO_NO_ANIM=1` 或 CLI `--no-anim`。
//      关闭后 animMs() 一律返回 0 → 动效"直接落终态"（不装 effect、不起动画），
//      因此离屏截图与自检可复现、与动效前逐像素一致。
// ★回迁注意：动效与 makeProto()/qssFor() 的令牌强绑定，回迁时要么整段搬走、要么整段不搬，
//   不要只搬一半（只搬画面不搬令牌会出现"深浅色下颜色对不上"）。详见 docs/回迁标注.md 第 19/20 条。
// ============================================================================

// —— 动效令牌：时长档位（所有效果只许用这三档，层次感来自档位差而不是随手取值）——
// —— 错峰（stagger）：现代感的来源——不是所有东西同时动，而是"容器先到、内容随后"——
// —— 各效果时长（都由上面的档位表达）——

// —— 曲线库：三类曲线一处定义，全项目共用（改这里 = 全局动效性格一起变）——
// 用 cubic-bezier / 自定义弹簧表达，零依赖（只用到 QEasingCurve 自带能力）。
// enter（EmphasizedDecel，Material 3 的"进场"曲线）：起步快、尾巴长 → 比 OutCubic 更"利落又不生硬"
// standard（Standard，位移/颜色通用）：两端都平滑，适合"从 A 状态到 B 状态"
// exit（EmphasizedAccel）：起步慢、尾巴快 → 退出动作"化开走掉"，不拖泥带水
// count（数值滚动专用，OutExpo）：起步极快、尾巴很长 —— 数字用它比位移曲线更有"滚上去"的观感；
// 它不表达物理位移，所以不跟位移类共用曲线。
static inline QEasingCurve curveCount() { return QEasingCurve(QEasingCurve::OutExpo); }
// spring（二阶阻尼，ζ=0.7 / ω=16）：约 4.6% 过冲、峰值在过程 30% 处 → 有生命力但不夸张。
// 只用在**位置/旋转**这类能表达"过冲"的属性上；颜色一律不用弹簧（颜色过冲会看成闪）。

// 令牌色线性插值（含 alpha）：自绘控件与"QSS 覆盖"两条路径共用它，
// 保证动画两端点颜色一定来自 Proto，不会出现中间态硬编码色。
// 半透明令牌色写进 QSS 必须用 rgba()：QColor::name() 会丢掉 alpha。
// 注：hover 过渡是**全程不透明**的（见 backDropOf），这个函数留给以后需要半透明底色的场景。
[[maybe_unused]] static inline QString rgbaCss(const QColor& c) {
    return QString("rgba(%1,%2,%3,%4)")
        .arg(c.red()).arg(c.green()).arg(c.blue())
        .arg(QString::number(c.alphaF(), 'f', 3));
}

// ============================================================================
// 自绘动效效果：淡入 + 位移（正负都行）
// ----------------------------------------------------------------------------
// 为什么不用"动画布局上边距"（第一版的做法）：那会**每帧触发整棵子树的 relayout**
//   ——页面里挂着结果表，每帧重排一次既费又会引起轻微抖动，而且只能往一个方向位移。
// 这里改成在 effect 内部平移渲染结果：布局完全不动、方向可正可负、也没有边距累加的坑。
// 用法：动画期间挂上，结束立刻摘掉（长期挂着会改渲染路径、伤性能）。
// 关闭动效时根本不安装 —— 渲染路径与动效前逐像素一致。
// ============================================================================

// 子控件进入动效：淡入 + 轻微位移（curveEnter）。
//   dyPx  ：位移幅度（正 = 从下方滑入；负 = 从上方滑入；0 = 只淡入）
//   child ：可选"随后到"的子控件 —— 容器先到约 30%，内容再淡入（层级因果，现代感的来源）
// 关闭动效（dur<=0）时**什么都不做**：终态本就"没有位移、没有 effect"。

// 一次性抖动（用于"密码错误"这类明确的失败反馈）：横向阻尼振荡后归位，不碰布局。
// 挂在控件的 FadeSlideEffect 上做，因此和淡入/位移共用同一套机制。

// 自绘控件的 hover 进度驱动：0 ↔ 1 的 QVariantAnimation（默认 standard 曲线），每帧回调 apply(v)。
// 关闭动效时直接落终态（0 或 1），因此在事件循环里不留任何定时器。
// curve 可换：位置/旋转类属性用 curveSpring()，颜色类一律用 curveStandard()（颜色过冲会被看成"闪"）。


static bool readAllBytes(const std::string& path, std::vector<uint8_t>& out) {
    namespace fs = std::filesystem;
    std::ifstream f(fs::u8path(path), std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return !out.empty();
}

static bool _rI32(const uint8_t*& p, const uint8_t* end, int32_t& v) { if (end - p < 4) return false; memcpy(&v, p, 4); p += 4; return true; }
static bool _rI64(const uint8_t*& p, const uint8_t* end, int64_t& v) { if (end - p < 8) return false; memcpy(&v, p, 8); p += 8; return true; }

// 数据目录里所有可加载文件的磁盘信息（文件名/路径/mtime原始计数/size）
static bool collectDiskFiles(const std::string& dataDir, std::vector<DiskFile>& out) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir(fs::u8path(dataDir));
    if (!fs::is_directory(dir, ec)) return false;
    for (auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec)) continue;
        std::string fn = e.path().filename().u8string();
        std::string ext; { std::string s = e.path().extension().u8string(); for (char& ch : s) ch = (char)tolower((unsigned char)ch); ext = s; }
        if (ext != ".xlsx" && ext != ".xls" && ext != ".csv" && ext != ".docx" && ext != ".xse") continue;
        DiskFile d; d.fn = fn; d.path = e.path().u8string();
        std::error_code ec2;
        auto ftime = fs::last_write_time(e.path(), ec2);
        if (!ec2) d.mtime = (int64_t)ftime.time_since_epoch().count();   // 原始 file_clock 计数值：稳定可复现
        std::error_code ec3;
        d.size = (int64_t)e.file_size(ec3);
        out.push_back(std::move(d));
    }
    return true;
}

// 缓存清单文件（cache.inv）：记录写入时刻数据目录「全量」文件(fn,mtime,size)，作为缓存是否可复用的唯一凭据。
// 只在清单逐项精确相等（数量/文件名/mtime/size 全部一致）时才允许复用索引，绝不因 .xse 跳过态而放宽。
static void _wI32(std::string& o, int32_t v) { o.append(reinterpret_cast<const char*>(&v), 4); }
static void _wI64(std::string& o, int64_t v) { o.append(reinterpret_cast<const char*>(&v), 8); }
static bool writeInventory(const std::string& path, const std::vector<DiskFile>& files) {
    std::string out; out.append("EXIV1", 5);
    _wI64(out, (int64_t)time(nullptr));
    _wI32(out, (int32_t)files.size());
    for (const auto& d : files) {
        _wI32(out, (int32_t)d.fn.size()); out.append(d.fn);
        _wI64(out, d.mtime); _wI64(out, d.size);
    }
    std::ofstream f(std::filesystem::u8path(path), std::ios::binary);
    if (!f) return false;
    f.write(out.data(), (std::streamsize)out.size());
    return (bool)f;
}
static bool readInventory(const std::string& path,
                          std::map<std::string, std::pair<int64_t, int64_t>>& out, int64_t& ts) {
    std::vector<uint8_t> data;
    if (!readAllBytes(path, data)) return false;
    if (data.size() < 5 + 8 + 4 || memcmp(data.data(), "EXIV1", 5) != 0) return false;
    const uint8_t* p = data.data(); const uint8_t* end = data.data() + data.size();
    p += 5;
    if (!_rI64(p, end, ts)) return false;
    int32_t cnt; if (!_rI32(p, end, cnt)) return false;
    if (cnt < 0 || cnt > 100000) return false;
    for (int32_t i = 0; i < cnt; i++) {
        int32_t len; if (!_rI32(p, end, len)) return false;
        if (len < 0 || len > 1024 * 1024 || p + len > end) return false;
        std::string fn(reinterpret_cast<const char*>(p), len); p += len;
        int64_t mt, sz;
        if (!_rI64(p, end, mt)) return false;
        if (!_rI64(p, end, sz)) return false;
        out[fn] = { mt, sz };
    }
    return true;
}

// 解密 .xse -> SheetData 列表（worker 与主线程共用；pwdEnabled 输出该文件是否启用附加密码）
static bool xseToSheets(const std::string& path, const std::string& pwd,
                        std::vector<SheetData>& sheets, bool& pwdEnabled) {
    std::vector<uint8_t> container;
    if (!readAllBytes(path, container)) return false;
    if (container.size() < 5 || memcmp(container.data(), "XSE1", 4) != 0) return false;
    std::vector<uint8_t> plain; int64_t m = 0, s = 0; std::string e; bool pe = false;
    if (!xse::decryptData(container, pwd, plain, &m, &s, &e, &pe)) { pwdEnabled = pe; return false; }
    pwdEnabled = pe;
    std::vector<XseFileEntry> entries;
    if (!xse::deserializePayload(plain, entries)) return false;
    for (const auto& ent : entries) {
        for (const auto& [nm, rows] : ent.sheets) {
            SheetData sd; sd.name = nm; sd.rows = rows;
            auto it = ent.headers.find(nm); if (it != ent.headers.end()) sd.headers = it->second;
            sheets.push_back(std::move(sd));
        }
    }
    return !sheets.empty();
}

static std::string _esc(const std::string& s) { std::string r; for (char c : s) { if (c == '&') r += "&amp;"; else if (c == '<') r += "&lt;"; else if (c == '>') r += "&gt;"; else if (c == '"') r += "&quot;"; else r += c; } return r; }
static std::string _colRef(int idx) { std::string r; idx++; while (idx > 0) { idx--; r = char('A' + (idx % 26)) + r; idx /= 26; } return r; }
static bool exportXlsxTo(const std::vector<SearchResult>& sel, const std::string& outPath) {
    if (sel.empty()) return false;
    std::set<int> allCols; int maxCol = 0;
    for (const auto& r : sel) { for (const auto& [c, _] : r.rowCells) { allCols.insert(c); if (c > maxCol) maxCol = c; } }
    std::vector<int> cols(allCols.begin(), allCols.end());
    std::vector<std::string> ss; std::map<std::string, int> ssIdx;
    auto getIdx = [&](const std::string& v) -> int { auto it = ssIdx.find(v); if (it != ssIdx.end()) return it->second; int i = (int)ss.size(); ss.push_back(v); ssIdx[v] = i; return i; };
    auto headers = sel.front().headers;
    std::string sheet = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>\n";
    sheet += "<row r=\"1\">";
    for (int ci = 0; ci < (int)cols.size(); ci++) { int c = cols[ci]; std::string v = (c < (int)headers.size() && !headers[c].empty()) ? headers[c] : ""; sheet += "<c r=\"" + _colRef(ci) + "1\" t=\"s\"><v>" + std::to_string(getIdx(v)) + "</v></c>"; }
    sheet += "</row>\n";
    int rowOut = 2;
    for (const auto& r : sel) {
        sheet += "<row r=\"" + std::to_string(rowOut) + "\">";
        for (int ci = 0; ci < (int)cols.size(); ci++) { int c = cols[ci]; auto it = r.rowCells.find(c); if (it == r.rowCells.end() || it->second.empty()) continue; sheet += "<c r=\"" + _colRef(ci) + std::to_string(rowOut) + "\" t=\"s\"><v>" + std::to_string(getIdx(it->second)) + "</v></c>"; }
        sheet += "</row>\n"; rowOut++;
    }
    sheet += "</sheetData></worksheet>";
    std::string ssXml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" count=\"" + std::to_string(ss.size()) + "\" uniqueCount=\"" + std::to_string(ss.size()) + "\">";
    for (const auto& s : ss) ssXml += "<si><t xml:space=\"preserve\">" + _esc(s) + "</t></si>";
    ssXml += "</sst>";
    std::string ct = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/><Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/><Override PartName=\"/xl/sharedStrings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml\"/><Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/></Types>";
    std::string rels = "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>";
    std::string wbRels = "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/><Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings\" Target=\"sharedStrings.xml\"/><Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/></Relationships>";
    std::string wb = "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets><sheet name=\"Sheet1\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>";
    std::string styles = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><fonts count=\"1\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts><fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill><fill><patternFill patternType=\"gray125\"/></fill></fills><borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders><cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs><cellXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/></cellXfs></styleSheet>";
    mz_zip_archive z; memset(&z, 0, sizeof(z));
    if (!mz_zip_writer_init_heap(&z, 0, 0)) return false;
    mz_zip_writer_add_mem(&z, "[Content_Types].xml", ct.c_str(), ct.size(), MZ_BEST_COMPRESSION);
    mz_zip_writer_add_mem(&z, "_rels/.rels", rels.c_str(), rels.size(), MZ_BEST_COMPRESSION);
    mz_zip_writer_add_mem(&z, "xl/workbook.xml", wb.c_str(), wb.size(), MZ_BEST_COMPRESSION);
    mz_zip_writer_add_mem(&z, "xl/_rels/workbook.xml.rels", wbRels.c_str(), wbRels.size(), MZ_BEST_COMPRESSION);
    mz_zip_writer_add_mem(&z, "xl/worksheets/sheet1.xml", sheet.c_str(), sheet.size(), MZ_BEST_COMPRESSION);
    mz_zip_writer_add_mem(&z, "xl/sharedStrings.xml", ssXml.c_str(), ssXml.size(), MZ_BEST_COMPRESSION);
    mz_zip_writer_add_mem(&z, "xl/styles.xml", styles.c_str(), styles.size(), MZ_BEST_COMPRESSION);
    void* out = nullptr; size_t os = 0; mz_zip_writer_finalize_heap_archive(&z, &out, &os); mz_zip_writer_end(&z);
    if (!out || os == 0) return false;
    std::ofstream f(std::filesystem::u8path(outPath), std::ios::binary);
    if (!f) { mz_free(out); return false; }
    f.write((const char*)out, (std::streamsize)os); f.close(); mz_free(out);
    return true;
}

class LoadWorker : public QThread {
    Q_OBJECT
public:
    std::string dataDir;
    bool addPwdEnabled = false;
    std::string addPwd;
    std::vector<Doc> docs;
    std::vector<std::pair<std::string, std::string>> pendingXse;   // 需要附加密码的 .xse (fn, path)
    // ★产品化（首次运行体验）：把"没读进来的文件"记下来，加载结束后汇总提示。
    //   否则用户把文件丢进 data\ 却没生效时，完全不知道发生了什么。
    std::vector<std::string> failed;      // 读取失败（格式不符 / 文件损坏 / 加密文件无法解密）
    std::vector<std::string> emptyData;   // 能打开但没有有效数据（空文件 / 只有空表）
    // 共享模式：目录不可达（主机不可达/无权限/路径不存在）。置位后主线程走缓存兜底，
    // 且**绝不回写缓存**——否则断网一次就会把好缓存覆盖成空索引。
    bool unreachable = false;
    void run() override {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path dir(fs::u8path(dataDir));
        std::vector<std::pair<std::string, std::string>> files;
        if (fs::is_directory(dir, ec)) {
            for (auto& e : fs::directory_iterator(dir, ec)) {
                if (!e.is_regular_file(ec)) continue;
                std::string fn = e.path().filename().u8string();
                std::string ext; { std::string s = e.path().extension().u8string(); for (char& ch : s) ch = (char)tolower((unsigned char)ch); ext = s; }
                if (ext == ".xlsx" || ext == ".xls" || ext == ".csv" || ext == ".docx" || ext == ".xse") files.push_back({ fn, e.path().u8string() });
            }
        }
        if (ec || !fs::is_directory(dir, ec)) { unreachable = true; return; }   // 探测失败：交给主线程兜底
        int total = (int)files.size();
        for (int i = 0; i < total; i++) {
            const std::string& fn = files[i].first;
            const std::string& path = files[i].second;
            std::vector<SheetData> sheets; bool ok = false;
            bool needPwd = false;
            std::string ext; { std::string s = path; size_t p = s.rfind('.'); ext = (p == std::string::npos) ? "" : s.substr(p); for (char& ch : ext) ch = (char)tolower((unsigned char)ch); }
            if (ext == ".xlsx")      { XlsxReader r; ok = r.open(path); if (ok) sheets = r.getSheets(); }
            else if (ext == ".xls")  { XlsReader r;  ok = r.open(path); if (ok) sheets = r.getSheets(); }
            else if (ext == ".csv")  { CsvReader r;  ok = r.open(path); if (ok) sheets = r.getSheets(); }
            else if (ext == ".docx") { DocxReader r; ok = r.open(path); if (ok) sheets = r.getSheets(); }
            else if (ext == ".xse") {
                bool pwdEnabled = false;
                std::string pwd = addPwdEnabled ? addPwd : "";
                ok = xseToSheets(path, pwd, sheets, pwdEnabled);
                if (!ok && pwdEnabled) { pendingXse.push_back({ fn, path }); needPwd = true; }
            }
            if (ok && !sheets.empty()) docs.push_back({ fn, sheets });
            else if (needPwd) { /* 待主线程补问附加密码，不算失败 */ }
            else if (ok) emptyData.push_back(fn);   // 打开成功但没有任何有效数据
            else failed.push_back(fn);              // 真正读取失败
            emit progress(i + 1, total, QString::fromUtf8(fn.c_str()));
        }
    }
signals:
    void progress(int done, int total, QString file);
};

// 共享目录连通性探测：UNC 的 exists() 可能阻塞数秒到数十秒，必须放在 worker 线程，
// 否则界面会像卡死。结果通过 finished 回主线程，自动串行"测试 → 成功才强制重载"。
class ProbeWorker : public QThread {
    Q_OBJECT
public:
    std::string path;
    bool ok = false;
    std::string detail;   // 失败原因（区分"路径不存在/主机不可达"与"拒绝访问"）
    void run() override {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path p(fs::u8path(path));
        const bool isDir = fs::is_directory(p, ec);
        ok = isDir && !ec;
        if (ok) { detail = "OK"; return; }
        // 尽量把"没找到"和"拒绝访问"分开，避免用户查错方向
        std::error_code ec2;
        const bool exists = fs::exists(p, ec2);
        if (!exists && !ec2)            detail = "路径不存在";
        else if (ec2 && ec2.value() == 5) detail = "拒绝访问（权限不足）";
        else if (!exists)               detail = "主机或路径不可达";
        else                            detail = "无法访问（" + ec.message() + "）";
    }
signals:
    void finished();
};


// 按钮"实际压在什么颜色上"（panelBg 或卡片 card）。
// 为什么需要它：透明底按钮（#themeBtn / #tbBtn / #navBtn 的常态）做 hover 过渡时，如果往 QSS 里写
// **半透明**背景，Qt 会把它合成到一个不由我们控制的中介底色上 —— 实测中间帧偏成深灰（#BBBBBF），
// 也就是 hover 会"闪一下"。所以这里取真实底色，把「透明 → hover 色」做成**两个不透明色之间的插值**，
// 全程 alpha=1，颜色走向可预测、可截图核对。
// ★回迁注意：按钮若挪到别的容器（不是 #panel / #card），这里要跟着补容器判定。
// 取出容器里"随后到"的内容控件（cardFrame 建卡片时登记在 animChild 属性上）。
// 用于父子错峰：容器先到、内容延迟 kStaggerMs 再淡入 —— 层级因果比"一起淡入"清楚得多。


// 自绘文件夹图标按钮（替换原来的「导出 xlsx」文字按钮）：
// 现代极简风格 —— 只描边、不上色，颜色跟随主题文字色（hover 变亮 + 淡底色）。
// 动效：hover 底色与描边色都由 HoverT 驱动做过渡（令牌色插值），按下仍是即时反馈。

// 标题栏三键（最小化/最大化/关闭）：hover 底色与前景色走过渡；关闭键的红色取自 Proto.closeHover
// 令牌（此前是硬编码 #E5484D，深色下与令牌不一致 —— 顺手对齐，取消 hover 后无任何视觉差异）。

// ============================================================================
// QSS 控件的 hover 颜色过渡（按钮）
// ----------------------------------------------------------------------------
// 为什么这么做：QSS 自身不支持 transition，hover 只能瞬间换色。这里在 hover 期间往**控件自身的
// 样式表**挂一条与 qssFor() 同名（必要时加 `:hover`）的选择器规则来覆盖 background/color，
// 并用 QVariantAnimation 在两端令牌色之间插值。
// 四条硬约束（改选择器前必读）：
//   ① 选择器的"主体"必须与 qssFor() 里那条**一致** —— `#themeBtn` 在 qssFor 里没有专属规则、
//      只有通用 `QPushButton:hover`，所以它的选择器要用 `QPushButton`；否则优先级压不住，动画不可见；
//   ② 离开（或动画结束）立刻清空控件级样式表 → 回到全局 QSS，换主题/换强调色不受影响；
//   ③ 勾选态（顶部标签当前页）不参与过渡，`:checked` 的强调色是状态表达，不能被 hover 冲掉；
//   ④ UI_PROTO_NO_ANIM=1 时**根本不安装** —— hover 与动效前逐像素一致。
// 不在覆盖范围内的（见 docs/动效设计.md「刻意不做」）：checkable 的 #themeToggle（选中态是强调色底）、
// 强调色色块（自带样式表）、QMenu 菜单项（QMenu 的自绘路径插不进过渡，菜单寿命极短，瞬时高亮更跟手）。
// ============================================================================

// 状态过渡（QSS 控件通用）：hover / 按下 / 焦点 三种状态的**颜色与圆角**过渡。
// ----------------------------------------------------------------------------
// 为什么这么做：QSS 没有 transition。这里在状态期间往**控件自身的样式表**挂一条与 qssFor() 同名
// （必要时加 `:hover`）的选择器规则来覆盖 background/color/border-color/border-radius，并用
// QVariantAnimation 在两端令牌色之间插值；状态结束立刻清空控件级样式表 → 回到全局 QSS。
// 硬约束（改选择器/加控件前必读）：
//   ① 选择器主体必须与 qssFor() 里那条一致 —— `#themeBtn` 在 qssFor 里没有专属规则、只吃通用
//      `QPushButton:hover`，所以它的选择器就用 `QPushButton`；否则优先级压不住，动画不可见；
//   ② 颜色**全程不透明**：透明底按钮的"透明"用 backDropOf() 取真实底色代替（半透明背景在 QSS 里
//      会被合成到不受控的中介底色上，实测中间帧偏成深灰，hover 会"闪一下"）；
//   ③ 勾选态（顶部标签"当前页"）不参与过渡：`:checked` 的强调色是状态表达，不能被 hover 冲掉；
//   ④ UI_PROTO_NO_ANIM=1 时**根本不安装** —— 与动效前逐像素一致。
// 颜色用 curveStandard 进出（颜色过冲会被看成"闪"）；位移/旋转类才用 curveSpring()。
// ============================================================================

// 给一个 QSS 按钮挂状态过渡（kind 决定两端令牌色与圆角，选择器由调用方给全）。
// proto 每帧重新求值 → 换主题 / 换强调色 / 状态中切主题都跟着变。
// 注意所有端点色都**不透明**（透明底用 backDropOf 取真实底色代替），理由见 backDropOf。
// ============================================================================
// 可折叠卡片 + 卡片标题栏（搜索页「数据概览」/「搜索结果」）
// ----------------------------------------------------------------------------
// 交互：点标题栏展开/收起，内容体做**高度动画**（180ms OutCubic），标题栏箭头随进度旋转 90°。
// 为什么标题仍用 QLabel#cardTitle：QSS 的字体/颜色/行高原样复用，布局高度与动效前逐像素一致
//   （自己 drawText 就得复刻字体，稍不留神整体位移 1–2px）。
// 折叠状态**不持久化**：不新增配置键 → 配置格式与回迁面完全不变（见 docs/回迁标注.md 第 19 条）。
// ============================================================================

// ============================================================================
// B4 加载进度条（不确定进度）
// ----------------------------------------------------------------------------
// 只表达"后台在干活"这一因果：一条 2px 强调色细条带渐变尾，从面板顶部滑过。
// **不假装知道百分比** —— 真实进度（x/y 个文件）仍在状态栏文字里。
// 叠加在面板顶部、不进布局 → 不改变任何控件的位置（进布局会让整页在加载时上下跳）。
// 关闭动效时**完全不出现**：它本身就是一个运动提示（功能性进度由状态栏文字承担），
// 而且这样截图无论何时都拍不到它。
// ============================================================================


// 附加密码输入对话框（MAA 风格，与主界面主题统一：深/浅色 + 圆角 + 主色按钮）

// 行详情对话框（MAA 风格，与主界面主题统一：深/浅色 + 圆角 + 主色按钮）

// 确认对话框（MAA 风格，替代原生 QMessageBox —— 原生外观与无边框窗口冲突）

// 关闭方式选择对话框（MAA 风格）：点 × 时让用户选「直接关闭 / 最小化到托盘」，
// 左下角可选「不再询问」——勾选后按本次选择记住，不再弹。

// ============================================================================
// 使用说明书窗口（版式与交互对齐 MAA 的「公告」框；实现为本项目自有代码）
// ----------------------------------------------------------------------------
// 版式：左「章节列表」+ 右「可滚动正文」；左下为**装饰位（默认留白，预留图片接口）**；
//       右下「☐ 下次说明书更新前不再显示」+「确认」。
// 交互（与 MAA 一致）：
//   · 启动时若「当前说明书内容 ≠ 上次已关闭的版本」且未永久关闭 → 自动弹出；
//   · **必须滚动到底**才能关闭；未读到底时点确认会依次出现几句调侃，连续点 20 次以上才放行；
//   · 勾「下次说明书更新前不再显示」→ 记住当前内容版本，**内容更新后自动恢复提示**；
//   · 「设置 → 通用设置」另有永久开关（对应 MAA 的「不显示公告」）。
// 内容来源：exe 同目录的 `MANUAL.md`（可外部替换，无需重编译）→ 缺省用内嵌副本 :/manual.md
// 装饰图片接口：exe 同目录放 `说明书插图.png` 即自动显示（未放则留白）。
// ============================================================================
// 按 `### 标题` 分节；首项固定为「全部内容」（与 MAA 公告的 ALL 项一致）
// 体检：哪些章节在真实控件里会需要横向滚动（= 内容比窗口宽 → 会被裁掉，必须改文案）
// 使用说明书窗口（版式与交互对齐 MAA 的「公告」框）
// ----------------------------------------------------------------------------
// 独立顶层窗口（非模态）：可与其他窗口并用；右上角是**最大化/还原**（不是关闭），
// 无边框窗口自己支持「拖动标题区移动 + 双击标题区最大化」；关闭走「确认」按钮。
// 版式：左「章节导航」（禁止横向滚动条，标题已缩短）+ 右 Markdown 正文 +
//       左下装饰位（默认留白；exe 旁放 `说明书插图.png` 即自动显示）+ 右下「☐ 下次更新前不再展示」+「确认」。
// 交互（与 MAA 一致）：必须**滚动到底**才能关；未读完点确认依次出现调侃文案，连续点 20 次以上放行；
//       勾「下次更新前不再展示」→ 记住当前内容版本，内容更新后自动恢复提示（Alt+F4 同样受门禁约束）。
// 内容来源：exe 旁 `MANUAL.md`（可外部替换）→ 内嵌副本 :/manual.md 兜底。


// ============================================================================
// 屏蔽 / 标记存储（V0.3.2）
// 身份键与 V0.3.0 完全一致 —— (文件名, 工作表, 行号)：便于回迁与数据互导。
// 持久化文本格式亦沿用原版：block/entries = "fn|sheet|row"，mark/entries = "fn|sheet|row|color"。
// 注意：屏蔽/标记属 UI 层概念，不进索引、不进 cache.dat，因此不影响缓存命中判定。
// ============================================================================

// 管理门禁：普通管理密码可修改；超管密码硬编码、不可修改，供开发者维护时直达
// ★敏感口令：**构建期注入**（仓库内不保存真值）
//   真值放在本机 local_secrets.cmake（已被 .gitignore 忽略），由 CMakeLists.txt 注入为编译宏。
//   未注入时的行为：超管通道**关闭**（不匹配任何输入）；默认管理密码退回弱默认值。
#ifndef ES_SUPER_PASSWORD
#define ES_SUPER_PASSWORD ""
#endif
#ifndef ES_DEFAULT_ADMIN_PASSWORD
#define ES_DEFAULT_ADMIN_PASSWORD "excelsearch"
#endif
static const char* kSuperPassword = ES_SUPER_PASSWORD;
static const char* kDefaultAdminPassword = ES_DEFAULT_ADMIN_PASSWORD;

// 标记 5 色（索引沿用原版顺序：0红 1紫 2蓝 3绿 4黄）。
// 呈现为「行首色条」而非原版的整行文字变色 —— 深色主题下更干净，也不会和斑马纹/选中蓝打架。
// 由色名反查色索引（原版 GetColorIndexByName），供「已标记红色」这类关键词使用

// ============================================================================
// 可配置智能列：结果表总列数 3..7。
// 固定三列（不可改动）：序号（首）、文件名（次）、匹配内容（末）；
// 中间为用户自定义列，来源可以是内建元数据（工作表 / 行号），也可以是任意工作簿表头关键词。
// 表头关键词按「表头包含 + 精确匹配加权」选列；都不命中时再用 rapidfuzz 兜底，
// 兜底分数仍低于阈值 → 视为「列不存在」。
// ============================================================================

// ★回迁注意：原版结果列表是固定 7 列（含硬编码的 核定工日/工时代码）；此处改为可配置 3–7 列，
//   默认只放内建元数据（工作表/行号），避免数据里没有该表头时出现空列。详见 docs/回迁标注.md 第 5 条。
// 缺省布局：只放「内建元数据」两列（工作表 / 行号），任何数据下都必然有效。
// 注意不要把「核定工日」「工时代码」之类硬编码进默认值 —— 数据里没这一列时默认就会出现两个空列；
// 这类智能列交给用户在「智能列设置」里按实际表头配置。

// ============================================================================
// 搜索历史：只有「一级搜索」记录历史，二级筛选一律不记录（避免同一关键词反复入栈）。
// 存储上限 20 条；下拉最多显示 10 条，其余到「搜索历史」设置页查看/回填。
// 时效 = 条目存活多久后删除：最短 10 分钟，最长「关闭程序后删除」（该档完全不落盘）。
// ============================================================================
// 模糊补充的护栏：精确命中已达此数量时不再做模糊补充（见 doSearch 里的性能修复说明）
// 项目开源许可证名称（发行时在此一处填写，界面与文档共用；留空则界面显示"（待定）"）
// 例：return QStringLiteral("MIT");
static QString projectLicense() { return QStringLiteral("GPL-3.0"); }


// ============================================================================
// 结果表 / 设置列表的行委托（定义在 MarkStore 之后：要复用标记 5 色 kMarkColors）
// ----------------------------------------------------------------------------
// 职责一：行 hover 的颜色过渡（QSS 不支持 transition，只有自绘才能做过渡）。
//   颜色取 Proto.hover 令牌；进度由 HoverT 驱动（关闭动效时直接落终态）。
// 职责二：结果表首列的标记色条（原 MarkBarDelegate 的职责，绘制完全不变；
//   docs/回迁标注.md 第 7 条提到的 MarkBarDelegate = 本类）。
// 列表与表格的差异：列表项有 8px 圆角，故先自绘圆角底再交给基类画文字；
//   表格有斑马纹（基类会用 backgroundBrush 铺满整行），改成设置 backgroundBrush 由基类铺。
// ============================================================================

// 给一个列表装上行 hover 过渡（沿用 MarkBarDelegate 的用法：只补画，不改默认绘制）
// 注：原 `MarkBarDelegate` 的"行首标记色条"职责已并入上面的 RowDelegate（绘制逻辑一字未改），
//     保留这段指向是为了让回迁时能对上 docs/回迁标注.md 第 7 条里提到的类名。

// ============================================================================
// 【预留接口】实用工具标签页 —— 当前不启用，只保留注册通道
// ----------------------------------------------------------------------------
// 分类规则（回迁/后续开发都按这个判，别再另起一套）：
//   1) 顶部标签 = 主流程页：搜索 / 设置 /（预留）实用工具；
//   2) 「实用工具」放：使用频率明显低于主流程（搜索·筛查·查看·导出·屏蔽·标记）
//      的辅助功能，例如统计报表(已决定不迁移，若复活则放这里)、批量转换、数据体检等；
//   3) 需要密码的管理类内容仍然只放「设置 → 高级设置」，不要混进实用工具。
// 启用方式（将来只需两步，不要新建第二套标签注册逻辑）：
//   a) 把下面的 kEnableUtilTab 置为 true；
//   b) 在构造函数里对每个工具调用一次 registerUtilTool(标题, 工厂)。
//      makeUtilPage() 会自动生成"左列表 + 右内容栈"，无需改布局代码。
// ★回迁注意：若不搬这个标签，至少要把"分类规则"搬进说明书，避免以后把低频功能随手塞回
//   主界面或设置页；若搬，则连注册方式一起搬。详见 docs/回迁标注.md 第 10 条。
// ============================================================================
static const bool kEnableUtilTab = false;          // ← 预留开关：当前明确不启用
static QString utilTabTitle() { return T("实用工具"); }

// 行悬停动效：需要 RowDelegate 的完整定义，故留在本文件（跟随 RowDelegate 一起拆分）

class AppWindow : public QWidget {
    bool m_dark = false;
    QColor m_accent = QColor("#326cf3");
    bool m_maxed = false;
    QPushButton *m_themeBtn = nullptr, *m_lightBtn = nullptr, *m_darkBtn = nullptr;
    QStackedWidget* m_stack = nullptr;
    QListWidget* m_navList = nullptr;
    QStackedWidget* m_secStack = nullptr;
    // 高级设置（门禁页）：分区同样由列表驱动，加新内容只需往 m_advSecs 追加一项
    std::vector<SecDef> m_advSecs;
    QListWidget* m_advNavList = nullptr;
    QStackedWidget* m_advStack = nullptr;
    QStackedWidget* m_advGateStack = nullptr;   // 0 = 解锁层（盖住内容）1 = 真实内容
    QLineEdit* m_advPwdEdit = nullptr;
    QLabel*    m_advErrLabel = nullptr;
    int  m_advSecIndex = -1;       // 「高级设置」在 m_secs 中的下标
    int  m_curPage = 0;
    bool m_advUnlocked = false;    // 离开该分区即置回 false（重新上锁）
    bool m_adminIsSuper = false;
    std::string m_adminPassword = kDefaultAdminPassword;
    QListWidget *m_blkListEntry = nullptr, *m_blkListFile = nullptr;
    std::vector<RowKey> m_blkEntryKeys;      // 与 m_blkListEntry 行一一对应
    std::vector<std::string> m_blkFileKeys;
    QLineEdit *m_pwdNew = nullptr, *m_pwdNew2 = nullptr;
    WinBtn *m_minBtn = nullptr, *m_maxBtn = nullptr, *m_closeBtn = nullptr;
    std::vector<QPushButton*> m_navBtns;
    std::vector<QColor> m_accents = { QColor("#326cf3"), QColor("#8b5cf6"), QColor("#10b981"), QColor("#f59e0b"), QColor("#ef4444") };
    std::vector<QPushButton*> m_accentBtns;   // 强调色色块（选中项要加描边，故持引用统一刷新）
    std::vector<CardDef> m_cards;
    std::vector<PageDef> m_pages;
    // 【预留】实用工具标签的工具注册表：加工具 = registerUtilTool 一次（当前为空，kEnableUtilTab=false）
    std::vector<SecDef> m_utilTools;
    std::vector<SecDef>  m_secs;
    SearchEngine m_engine;
    std::vector<SearchResult> m_results;
    MarkStore m_marks;
    int m_lastBlocked = 0;   // 本次搜索被屏蔽过滤掉的条数（供状态提示/后续 toast）
    // 智能列映射：(文件,工作表) -> 列号。本步只用「任务标题」（标记筛选态下替换「匹配内容」）；
    // buildColMap 是通用的，后续要加「核定工日」「工时代码」只是多调一次。
    std::map<std::pair<std::string, std::string>, int> m_renWuColMap;
    // 可配置智能列（中间列，0..4 个）；结果表顺序 = 序号 | 文件名 | [自定义列…] | 匹配内容
    std::vector<std::string> m_extraCols = defaultExtraCols();
    // source -> (文件,工作表) -> 列号 的惰性缓存（数据重载时清空；mutable 以便在 const 取值函数里填充）
    mutable std::map<std::string, std::map<std::pair<std::string, std::string>, int>> m_colMapCache;
    QListWidget* m_colList = nullptr;      // 智能列设置：当前列预览
    QComboBox*   m_colCombo = nullptr;     // 智能列设置：可编辑下拉（内建 + 已检测表头）
    QLabel*      m_colErr = nullptr;       // 智能列设置：校验错误提示
    // 搜索历史
    std::vector<HistItem> m_history;       // 最新在前
    int m_histShow = 5;                    // 下拉显示条数 0..10（0 = 不显示）
    int m_histTtlMin = 0;                  // 时效（分钟）；0 = 关闭程序后删除（不落盘）
    bool m_histRecord = true;              // 无头自检模式下关闭记录，避免污染用户历史
    QPushButton* m_histBtn = nullptr;
    QListWidget* m_histList = nullptr;
    QComboBox *m_histShowCombo = nullptr, *m_histTtlCombo = nullptr;
    QTimer* m_histTimer = nullptr;
    std::string m_dataDir;
    // 共享模式（★回迁注意：原版 DoFilter 无关，这里是数据源切换 + 不可达兜底，详见 docs/回迁标注.md 第 12 条）
    bool m_shareMode = false;                 // true = 数据源为共享目录（只读）
    std::string m_sharePath = "\\\\server\\share\\excel_search";   // UNC 路径（已规范化）
    bool m_forceReload = false;               // 强制重载：跳过缓存命中
    QRadioButton *m_modeOfflineBtn = nullptr, *m_modeShareBtn = nullptr;
    QLineEdit* m_shareEdit = nullptr;         // 共享设置里的路径输入
    QLabel* m_shareStatus = nullptr;          // 共享设置里的状态行
    QString m_shareStatusText;                // 当前共享状态（模式/来源/测试结果）
    ProbeWorker* m_probe = nullptr;           // 连通性探测（worker，避免 UNC 阻塞 UI）
    // 托盘（★回迁注意：对齐原版 V0.3.0.1「隐藏到托盘」——点 × 只隐藏，真退出走托盘菜单）
    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_trayMenu = nullptr;
    bool m_trayHintShown = false;             // 本次启动首次隐藏是否已气泡提示
    bool m_migratedFromOld = false;           // 本次启动是否刚完成「V0.3.0 注册表 → config.ini」搬迁
    bool m_manualNeverShow = false;           // 启动时永久不提示说明书（对应 MAA 的 DoNotShow）
    bool m_quitting = false;                  // true = 真的在退出（closeEvent 放行）
    // 关闭行为：0=每次询问 1=直接关闭 2=最小化到托盘（可在 设置 → 通用设置 调整）
    int m_closeAction = 0;
    QRadioButton *m_closeAskBtn = nullptr, *m_closeDirectBtn = nullptr, *m_closeTrayBtn = nullptr;
    QCheckBox* m_manualChk = nullptr;   // 通用设置：启动时不再提示说明书
    ManualDialog* m_manualWin = nullptr;   // 说明书独立窗口（同一时刻只开一个；随主窗销毁）
    std::string m_settingsPath;
    std::string m_cachePath;
    std::string m_cacheInvPath;
    bool m_usedCache = false;
    QTableWidget* m_table = nullptr;
    // —— 动效（V0.3.2）涉及的控件登记表：换主题/换强调色时统一刷新，避免各写一套刷新逻辑 ——
    std::vector<CollapseCard*> m_collapseCards;   // 搜索页可折叠卡片（标题栏自绘，需 setTheme）
    std::vector<RowDelegate*>  m_rowDelegates;    // 行 hover 过渡委托（结果表 + 各设置列表）
    std::vector<StateTint*>    m_stateTints;      // QSS 控件的状态过渡（按钮 hover/按下 + 输入框焦点）
    std::map<QPushButton*, QVariantAnimation*> m_statAnims;   // 数值滚动动画（A2），每控件一条
    LoadBar* m_loadBar = nullptr;   // B4 加载进度细条（叠加在面板顶部，不进布局）
    QWidget* m_panel = nullptr;     // #panel（进度条挂它下面，手动定位）
    QPushButton *m_statFiles = nullptr, *m_statIndex = nullptr, *m_statHit = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    FolderBtn* m_folderBtn = nullptr;       // 自绘文件夹图标：打开当前数据源目录
    QLineEdit* m_filterEdit = nullptr;      // 二级筛选输入框
    bool m_markedFilterActive = false;      // 当前结果是否来自「已标记」筛选（供二级颜色精筛）
    // ★回迁注意：二级筛选的「基准」必须是一级搜索结果快照，绝不能就地缩（原版 V0.3.0 的 DoFilter 是
    //   在 g_results 上原地筛选并覆盖，改条件会在旧结果里继续缩；本版改为从基准重算）。
    std::vector<SearchResult> m_baseResults;   // 一级搜索结果快照（已过屏蔽过滤）
    std::vector<QString>      m_filterChain;   // 已生效的筛选条件；标准模式恒为 0/1 条
    bool m_chainMode = false;                  // false=标准模式(默认) true=逐级模式
    QString m_baseStatus;                      // 一级搜索时的状态栏文案（清除筛选后还原它）
    QRadioButton *m_modeStdBtn = nullptr, *m_modeChainBtn = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_toast = nullptr;         // 左下角临时提示（等价原版 g_hToast）
    QTimer* m_toastTimer = nullptr;
    // toast 动效（淡入淡出 + 上浮）：effect 只在显示期间挂着，隐藏时摘掉
    QVariantAnimation* m_toastAnim = nullptr;
    QGraphicsOpacityEffect* m_toastEff = nullptr;
    bool m_toastFading = false;
    bool m_fuzzyEnabled = true;
    int m_lastExact = 0, m_lastFuzzy = 0;
    QCheckBox* m_fuzzyBox = nullptr;
    QCheckBox* m_encChk = nullptr;
    QLineEdit* m_encEdit = nullptr;
    int m_loadedFiles = 0;
    int m_skipped = 0;   // 需要附加密码未加载的加密文件数
    LoadWorker* m_worker = nullptr;
    bool m_addPwdEnabled = false;      // 附加密码（加密设置）
    std::string m_addPwd;
    std::string m_sessionAddPwd;       // 本次会话已输入的附加密码
    bool m_allowPrompt = true;         // 是否允许弹附加密码框（离屏截图时关闭）
public:
    AppWindow() {
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
    void goSettingsPage() { switchPage(1); }
    void goSection(int i) { if (m_navList) m_navList->setCurrentRow(i); }
    // 高级设置：截图/自检用（unlock=true 直接解锁到真实内容；否则停在解锁层）
    void goAdvPage(bool unlock = false) {
        switchPage(1);                              // 先确保在「设置」页
        if (m_navList && m_advSecIndex >= 0) {
            m_navList->setCurrentRow(m_advSecIndex);           // 再切到「高级设置」分区（此刻仍是锁定态）
            if (unlock) { m_advUnlocked = true; onSettingsSectionChanged(m_advSecIndex); }
        }
    }
    int advSectionCount() const { return (int)m_advSecs.size(); }
    // 自检钩子：--page closedlg 时渲染关闭方式对话框（它平时是模态的，没法直接截）
    // 自检钩子：--migrate 强制执行一次「注册表 → config.ini」搬迁，摘要写 --out（供验证/客服排障）
    QString demoMigrate() {
        QString detail;
        migrateFromRegistry(true, &detail);
        return detail;
    }
    // ---- 使用说明书（版式与交互对齐 MAA「公告」框）----
    // 打开说明书；若用户勾了「下次说明书更新前不再显示」，则记住**当前内容版本**（哈希）——
    // 内容一旦更新（哈希变化），下次启动会自动恢复提示（与 MAA 的 DoNotShowAgain 语义一致）。
    void openManual() {
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
    void maybeShowManual() {
        if (m_manualNeverShow) return;
        const QString md = manualText();
        if (md.isEmpty()) return;
        QSettings s(QString::fromUtf8(m_settingsPath.c_str()), QSettings::IniFormat);
        if (s.value("manual/dismissedHash", "").toString() == manualHash(md)) return;
        openManual();
    }
    // 自检钩子：--page manual 渲染使用说明书窗口
    QPixmap demoManualPixmap() {
        ManualDialog dlg(m_dark, m_accent, manualText(), [](bool) {}, this);
        dlg.show();
        QCoreApplication::processEvents();
        QPixmap pm = dlg.grab();
        dlg.hide();
        return pm;
    }
    // 自检：真实控件里检查目录栏是否会截断标题（--report 用；会短暂显示一次说明书窗口）
    QString manualNavReport() {
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
    QPixmap demoCloseDialogPixmap() {
        CloseDialog d(m_dark, m_accent, this);
        d.show();
        QCoreApplication::processEvents();
        QPixmap pm = d.grab();
        d.close();
        return pm;
    }
    void demoTab(int i) { switchPage(i); }   // 自检钩子：--tab N 切到第 N 个顶级标签
    // 自检钩子：--reload 用。走的就是「重新加载」按钮那条路径（loadData，非强制），
    // 用来离屏复现"已有数据 → 重新加载"时的加载反馈与统计数字复位。
    void forceReloadForDemo() { loadData(); }
    // —— 以下两个是**截图/自检专用**入口（只影响本次渲染，不写配置、不改业务行为）——
    // --collapse 0,1：折叠搜索页第 0/1 张卡片（0=数据概览 1=搜索结果）。走的就是点击标题栏那条路径。
    void demoCollapse(const QString& spec) {
        for (const QString& s : spec.split(',', Qt::SkipEmptyParts)) {
            bool ok = false;
            const int i = s.trimmed().toInt(&ok);
            if (ok && i >= 0 && i < (int)m_collapseCards.size()) m_collapseCards[i]->setCollapsed(true, true);
        }
    }
    // --hover <名>：把某个控件强制摆到 hover 终态，用于离屏验证"过渡两端色与 QSS 一致"。
    //   primaryBtn/themeBtn/tbBtn/navBtn = QSS 按钮；sec = 设置列表项；row = 结果表行；
    //   folder/close = 自绘按钮；card = 卡片标题栏。真实鼠标下这些状态由 Enter/Leave 驱动。
    void demoHover(const QString& what) {
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
    void forceRowHover(QAbstractItemView* v, int row) {
        if (!v) return;
        if (auto* d = qobject_cast<RowDelegate*>(v->itemDelegate())) d->setHoverRow(row);
    }

    void goAdvSection(int i) { if (m_advNavList) m_advNavList->setCurrentRow(i); }
    // 高级设置的门禁栈（解锁层 ↔ 真实内容）切换：只淡入、不做位移 —— 密码框跟着滑会显得轻浮
    void advGateAnim() {
        if (m_advGateStack) enterAnim(m_advGateStack->currentWidget(), kSecAnimMs, 0.0, animChildOf(m_advGateStack->currentWidget()));   // 0 = 只淡入不位移；锁图标随后到
    }
    // 设置页分区切换：进入「高级设置」若未解锁则显示解锁层；离开则重新上锁
    void onSettingsSectionChanged(int row) {
        if (row == m_advSecIndex) {
            const int want = m_advUnlocked ? 1 : 0;
            const bool gateChanged = m_advGateStack && m_advGateStack->currentIndex() != want;
            if (m_advGateStack) m_advGateStack->setCurrentIndex(want);
            if (m_advUnlocked) {
                refreshBlockAdmin();
            } else {
                if (m_advPwdEdit) { m_advPwdEdit->clear(); m_advPwdEdit->setFocus(); }
                if (m_advErrLabel) m_advErrLabel->clear();
            }
            if (gateChanged) advGateAnim();   // 解锁层 / 真实内容进出都淡入
        } else if (m_advUnlocked) {
            m_advUnlocked = false;   // 离开「高级设置」→ 重新上锁
            m_adminIsSuper = false;
        }
    }
    // 解锁层里的「进入」：密码正确则掀起解锁层，露出真实内容
    void tryAdvUnlock() {
        if (!m_advPwdEdit) return;
        const QString pw = m_advPwdEdit->text();
        const QString super_ = QString::fromUtf8(kSuperPassword);
        const bool isSuper = !super_.isEmpty() && (pw == super_);   // 未注入超管口令时该通道关闭
        if (pw == QString::fromUtf8(m_adminPassword.c_str()) || isSuper) {
            m_adminIsSuper = (isSuper && pw != QString::fromUtf8(m_adminPassword.c_str()));
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
    void switchPage(int idx) {
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
    void demoSearch(const char* kw) { if (m_searchEdit) { m_searchEdit->setText(QString::fromUtf8(kw)); doSearch(); } }
    qulonglong demoHits(const char* kw) { demoSearch(kw); return (qulonglong)m_results.size(); }
    // 自检钩子：--colprobe 打印列名解析结果（空串 = UI 会提示「所输入的列不存在」）
    QString demoResolveColumn(const char* text) const { return u8(resolveColumnSource(text)); }
    // 自检钩子：--search <一级> --filter <二级> 时返回二级筛选后的条数
    qulonglong demoFilter(const char* kw) {
        if (m_filterEdit) { m_filterEdit->setText(QString::fromUtf8(kw)); doFilter(); }
        return (qulonglong)m_results.size();
    }
    // 首条命中行的身份键 "fn|sheet|row"，供无头自检链式引用（--search 输出 → --block 使用）
    QString firstHitKey() const {
        if (m_results.empty()) return QString();
        const auto& r = m_results[0];
        return QString::fromUtf8((r.filename + "|" + r.sheetName + "|" + std::to_string(r.row)).c_str());
    }
    int demoBlocked() const { return m_lastBlocked; }
    // 无头自检钩子：--block fn|sheet|row（或 --block fn 屏蔽整个文件）、--mark fn|sheet|row|color、--clearmarks
    void applyMarkCli(const QString& blockSpec, const QString& markSpec, bool clearAll) {
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
    int blockedEntryCount() const { return (int)m_marks.blockedEntries.size(); }
    int blockedFileCount() const { return (int)m_marks.blockedFiles.size(); }
    int markedCount() const { return (int)m_marks.marked.size(); }
    int historyCount() const { return (int)m_history.size(); }
    int historyShow() const { return m_histShow; }
    int historyTtl() const { return m_histTtlMin; }
    const char* filterMode() const { return m_chainMode ? "chain" : "standard"; }
    // 自检钩子：--share <UNC 路径> 打开共享模式并保存；--shareoff 关回离线模式
    void applyShareCli(const QString& path, bool off) {
        if (off) { m_shareMode = false; }
        else { m_shareMode = true; if (!path.isEmpty()) m_sharePath = normalizeSharePath(path).toUtf8().toStdString(); }
        saveSettings();
    }
    bool shareEnabled() const { return m_shareMode; }
    // 自检：托盘是否真的可用（无托盘环境应返回 false，此时 × 仍按普通关闭处理）
    bool trayActive() const { return m_tray && m_tray->isVisible(); }
    bool manualNeverShowC() const { return m_manualNeverShow; }
    const char* closeActionName() const { return m_closeAction == 1 ? "close" : (m_closeAction == 2 ? "tray" : "ask"); }
    // 动效开关状态（--report 新增字段 anim=on|off，用来证明开关真的生效；既有字段含义不变）
    const char* animName() const { return g_noAnim ? "off" : "on"; }
    // 离屏截图 / 自检用：等子控件动效落定，避免拍到动画中间态。
    // ms > 0 时只等指定毫秒 —— 用于**故意抓动画中间帧**（证明动效确实在动，不是瞬变）。
    // 默认 kSettleMs 必须覆盖**最长的一条动效链**（数值滚动 420ms + 最大错峰 80ms + 页面/分区 160ms
    // + 余量）。以后加了更长的动效要同步调大 kSettleMs —— 否则截图会拍到中间态
    // （返工记录：默认值还停在旧的 420ms 时，把索引记录 43846 拍成了 43844）。
    // 关闭动效时是空操作（本来就没有中间态）。
    void settleAnim(int ms = 0) {
        if (!g_noAnim) {
            QEventLoop loop;
            QTimer::singleShot(ms > 0 ? ms : kSettleMs, &loop, &QEventLoop::quit);
            loop.exec();
        }
        QCoreApplication::processEvents();
    }
    const char* sharePathC() const { return m_sharePath.c_str(); }
    void setAllowPrompt(bool v) { m_allowPrompt = v; m_histRecord = v; }   // 无头自检同时关闭历史记录
    // 打开当前数据源目录：离线 = 程序旁 data\，共享 = 共享目录（与原版「打开文件夹」同义）
    void openDataFolder() {
        const QString dir = u8(dataSourceDir());
        if (!QDir(dir).exists()) { showToast(T("文件夹不存在：") + dir); return; }
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dir))) showToast(T("无法打开文件夹：") + dir);
    }
    // ---- 托盘（对齐原版 V0.3.0.1：点 × 隐藏到托盘，真退出走托盘菜单）----
    // 程序图标：**统一用 exe 内嵌的 app.ico**（`app_icon.qrc` 嵌入 → `:/app.ico`）。
    //   早期版本这里是按强调色程序化绘制，导致「资源管理器（读 exe 内嵌图标）」与
    //   「任务栏/托盘（读代码绘制）」显示**两个不同图标**（用户反馈）。现三处统一。
    //   程序化绘制保留为**兜底**：万一 Qt 的 ico 插件缺失导致资源加载失败，也不至于出现空白图标。
    static bool appIconIsResource() { return !QIcon(":/app.ico").isNull(); }
    QIcon makeAppIcon() const {
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
    void setupTray() {
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
    void showFromTray() {
        showNormal();
        raise();
        activateWindow();
    }
    void quitApp() {
        m_quitting = true;
        if (m_tray) m_tray->hide();   // 清理托盘图标（等价原版 WM_DESTROY 的 NIM_DELETE）
        QApplication::quit();
    }
    // 通用设置里改动关闭选项后同步单选框（对话框勾了"不再询问"也要回写）
    void refreshCloseRadios() {
        if (!m_closeAskBtn) return;
        (m_closeAction == 0 ? m_closeAskBtn : (m_closeAction == 1 ? m_closeDirectBtn : m_closeTrayBtn))->setChecked(true);
    }
    void hideToTray() {
        hide();
        if (m_tray && !m_trayHintShown) {   // 本次启动首次隐藏才气泡提示
            m_trayHintShown = true;
            m_tray->showMessage(windowTitle(), T("已最小化到托盘，双击图标可恢复"), QSystemTrayIcon::Information, 4000);
        }
    }
    // ---- 共享模式 ----
    std::string dataSourceDir() const { return m_shareMode ? m_sharePath : m_dataDir; }
    void setShareStatus(const QString& s) { m_shareStatusText = s; if (m_shareStatus) m_shareStatus->setText(s); }
    // 规范化共享路径：去首尾空白、去尾部反斜杠（保留 "\\host\share" 的前导）
    static QString normalizeSharePath(const QString& raw) {
        QString s = raw.trimmed();
        while (s.size() > 2 && s.endsWith('\\')) s.chop(1);
        return s;
    }
    // 启动连通性探测（worker）；ok 回调在主线程执行
    void startProbe(std::function<void(bool)> onDone) {
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
    void testShareConnection() {
        if (m_sharePath.empty()) { setShareStatus(T("请先填写共享路径")); return; }
        setShareStatus(T("正在测试连接…"));
        startProbe([](bool) {});
    }
    // 手动「强制重载」：忽略缓存；仅共享模式下先验连通性（失败则不重载）
    void forceReloadNow() {
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
    void doForceReload() {
        m_forceReload = true;
        loadData(/*forceReload=*/true);
        m_forceReload = false;
    }
    // 路径改完（编辑结束）触发：规范化 → 保存 → 自动"测试连接 →（成功）强制重载"
    void onSharePathEdited() {
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
    void setShareMode(bool on) {
        if (m_shareMode == on) return;
        if (on && m_sharePath.empty()) { setShareStatus(T("请先填写共享路径")); return; }
        m_shareMode = on;
        saveSettings();
        m_engine.clear(); m_loadedFiles = 0; m_skipped = 0; updateStats();
        setShareStatus(on ? T("已切换到共享模式，正在重新加载…") : T("已切换到离线模式，正在重新加载…"));
        loadData();
    }
    int exportAllTo(const char* file) { return exportXlsxTo(m_results, file) ? 1 : 0; }
    int loadedCount() const { return m_loadedFiles; }
    int skippedCount() const { return m_skipped; }
    qulonglong entryCount() const { return (qulonglong)m_engine.getEntryCount(); }
    bool usedCache() const { return m_usedCache; }
    void setDark(bool d) { m_dark = d; apply(); }
    void waitForLoad() {
        // 注意：worker 可能已经结束（例如共享目录瞬时不可达时几微秒就返回），此时 finished 的
        // 队列槽（onLoadFinished / 缓存兜底）还没跑，必须泵一次事件循环，否则自检会读到中间态。
        if (!m_worker || !m_worker->isRunning()) { QCoreApplication::processEvents(); return; }
        QEventLoop loop; connect(m_worker, &LoadWorker::finished, &loop, &QEventLoop::quit); loop.exec();
        QCoreApplication::processEvents();
    }
    // 给界面上的 QSS 控件统一挂状态过渡：按钮（hover/按下/圆角）+ 输入框（焦点边框）。
    // 放在 buildUi() 之后扫描一遍 —— 以后新增按钮只要沿用同名 objectName 就自动获得过渡，
    // 不需要在每个工厂里重复写一行（注册表驱动的老规矩）。
    // 选择器主体必须与 qssFor() 里的那条一致，否则控件级规则压不住全局规则。
    void setupStateTints() {
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
    void apply() {
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
protected:
    // 点 × 的三种行为（可在 设置 → 通用设置 调整，默认「每次询问」）：
    //   每次询问 → 弹关闭方式对话框（左下角「不再询问」可记住本次选择）
    //   直接关闭 → 真退出（清理托盘图标）
    //   最小化到托盘 → 只隐藏窗口，进程常驻
    void closeEvent(QCloseEvent* e) override {
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
    void mousePressEvent(QMouseEvent* e) override {
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
    void mouseDoubleClickEvent(QMouseEvent* e) override {
        if ((int)e->position().y() < kTitleH) { toggleMax(); return; }
        QWidget::mouseDoubleClickEvent(e);
    }    // B4：进度条手动定位在标题栏下方一条（居中留 16px 边距，与内容对齐）
    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        if (m_loadBar) m_loadBar->setGeometry(16, kTitleH, std::max(0, width() - 32), m_loadBar->height());
    }
private:
    void toggleMax() {
        if (isMaximized()) { showNormal(); m_maxed = false; } else { showMaximized(); m_maxed = true; }
        apply();
    }
    void flipTheme() { m_dark = !m_dark; saveSettings(); apply(); }
    void setAccent(const QColor& c) { m_accent = c; saveSettings(); apply(); }
    // 强调色色块：统一 28px 圆点；当前生效的那个加一圈"主题文字色"描边，否则看不出选的是哪个
    void refreshAccentButtons() {
        if (m_accentBtns.empty()) return;
        const QColor ring = makeProto(m_dark, m_accent).text;
        for (size_t i = 0; i < m_accentBtns.size() && i < m_accents.size(); i++) {
            const bool sel = (m_accents[i] == m_accent);
            m_accentBtns[i]->setStyleSheet(QString("background:%1; border-radius:14px; border:2px solid %2;")
                .arg(m_accents[i].name(), sel ? ring.name() : QString("transparent")));
        }
    }
    void pickCustomAccent() { QColor c = QColorDialog::getColor(m_accent, this, T("选择强调色")); if (c.isValid()) setAccent(c); }

    static QString u8(const std::string& s) { return QString::fromUtf8(s.c_str()); }

    // ================= 从 V0.3.0 注册表搬迁设置（一次性） =================
    // 旧版设置存在 HKCU\Software\ExcelSearch；本版改用 %APPDATA%\ExcelSearch\config.ini。
    // 不搬的话老用户升级后主题 / 屏蔽 / 标记 / 搜索历史 / 共享路径全丢。策略：
    //   · 只在「从未搬迁过」（config.ini 无 meta/registryMigrated）且注册表确有可迁键时执行一次；
    //   · **注册表只读、原样保留**（便于回退到旧版）；任何异常都不影响启动；
    //   · 废弃项一律不迁：Title / IconPath / BlurPercent / AlphaPercent（见回迁清单第 15 条）；
    //     原版的 DataPath 是**死键**（源码里从未读取，实测确认），同样不迁；
    //   · 原版有两级历史（SearchHistory1=一级搜索，SearchHistory2=二级筛选）；本版只保留一级历史，
    //     故只迁 SearchHistory1 并丢弃 SearchHistory2（本版无对应功能）。
    // 说明：屏蔽/标记的文本格式与原版**逐字节一致**（fn|sheet|row / fn|sheet|row|color），直接复用。
    bool migrateFromRegistry(bool force, QString* detail = nullptr) {
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
    void loadSettings() {
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
    void saveSettings() {
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

    // 共享模式：缓存 TTL 单独缩短为 30 小时（共享数据由分享机管理员维护，避免长期陈旧）
    long long cacheTtlSec() const { return m_shareMode ? (30LL * 3600) : (10LL * 24 * 3600); }
    void loadData(bool forceReload = false) {
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
    bool tryLoadCache() {
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
    // 共享目录不可达：直接吃缓存（不校验清单，因为清单也读不到），并明确标注数据来源与新鲜度
    // 文件名列表 → 简短提示语（最多列 3 个，其余归并成"等 N 个"）
    static QString briefNames(const std::vector<std::string>& names) {
        QString s;
        for (size_t i = 0; i < names.size() && i < 3; i++) { if (i) s += T("、"); s += u8(names[i]); }
        if (names.size() > 3) s += T(" 等 ") + QString::number((qulonglong)names.size()) + T(" 个");
        return s;
    }
    // 数据源为空 / 不可用时的提示语（模式相关：离线模式不能说成"共享目录"）
    QString emptySourceHint() const {
        return m_shareMode
            ? T("共享目录为空：") + u8(dataSourceDir()) + T("　请把 Excel / Word / CSV 文件放入该目录后点「重新加载」")
            : T("数据目录为空：") + u8(dataSourceDir()) + T("　请把 Excel / Word / CSV 文件放入该文件夹后点「重新加载」");
    }
    void loadCacheFallback() {
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
    void onLoadFinished() {
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
    // 智能列：按「表头包含关键词 + 精确匹配加权」选列（对齐原版 BuildColMap 的评分规则）
    std::map<std::pair<std::string, std::string>, int> buildColMap(const std::string& target) const {
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
    void buildColMaps() { m_renWuColMap = buildColMap("任务标题"); m_colMapCache.clear(); }
    // ---- 可配置智能列 ----
    static bool isMetaCol(const std::string& s) { return s == kMetaSheetCol || s == kMetaRowCol; }
    // 去重收集全部工作簿表头（供下拉候选与校验）
    QStringList allHeaderNames() const {
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
    std::string resolveColumnSource(const std::string& text) const {
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
    std::string cellValueFor(const SearchResult& r, const std::string& source) const {
        if (source == kMetaSheetCol) return r.sheetName;
        if (source == kMetaRowCol)   return std::to_string(r.row);
        auto it = m_colMapCache.find(source);
        if (it == m_colMapCache.end()) it = m_colMapCache.emplace(source, buildColMap(source)).first;
        auto cit = it->second.find({ r.filename, r.sheetName });
        if (cit == it->second.end() || cit->second < 0) return std::string();
        auto vit = r.rowCells.find(cit->second);
        return vit == r.rowCells.end() ? std::string() : vit->second;
    }
    // 按当前配置重建结果表列（固定三列夹住自定义列），并重填数据
    void applyColumns() {
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
    std::vector<SearchResult> buildMarkResults(int colorFilter) const {
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
    // ---- 搜索历史 ----
    // 按当前时效清理过期条目（时效 = 0「关闭程序后删除」时不做时间清理）
    void purgeHistory() {
        if (m_histTtlMin <= 0) return;
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const qint64 ttl = (qint64)m_histTtlMin * 60;
        m_history.erase(std::remove_if(m_history.begin(), m_history.end(),
            [&](const HistItem& h) { return h.ts + ttl <= now; }), m_history.end());
    }
    // 只有一级搜索会调用这里（二级筛选不记录）
    void addHistory(const QString& kw) {
        if (!m_histRecord || kw.isEmpty()) return;
        for (auto it = m_history.begin(); it != m_history.end(); ) {
            if (it->kw == kw) it = m_history.erase(it); else ++it;   // 同词去重（提到最前）
        }
        m_history.insert(m_history.begin(), { kw, QDateTime::currentSecsSinceEpoch() });
        if ((int)m_history.size() > kHistStoreMax) m_history.resize(kHistStoreMax);
        saveSettings();
        refreshHistoryUI();
    }
    void clearHistory() {
        m_history.clear();
        saveSettings();
        refreshHistoryUI();
    }
    // 下拉（最近 m_histShow 条）与设置页列表（全部）统一刷新；显示条数为 0 时隐藏「历史」按钮
    void refreshHistoryUI() {
        if (m_histBtn) m_histBtn->setVisible(m_histShow > 0);
        if (m_histList) {
            m_histList->clear();
            for (size_t i = 0; i < m_history.size(); i++) {
                const QString when = QDateTime::fromSecsSinceEpoch(m_history[i].ts).toString("MM-dd HH:mm");
                m_histList->addItem(when + T("　") + m_history[i].kw);
            }
        }
    }
    void showHistoryMenu() {
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
    // A2 数值滚动：把统计数字从"当前显示值"滚到目标值（curveCount = OutExpo）。
    // 四条要点：
    //   ① 结束**精确**落到目标整数（绝不停在中间值）——数字是有业务含义的，不能只求好看；
    //   ② 滚动期间按目标文本预留最小宽度（只到 advance，不超过 sizeHint → 不会顶动布局），
    //      结束复位 → 静止态与关闭动效时逐像素一致；
    //   ③ 同一控件再次触发时从"当前显示值"续滚（不跳回 0）；
    //   ④ 关闭动效（dur<=0）时直接 setText。
    void setStat(QPushButton* btn, qulonglong target, int delayMs = 0) {
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
    // 三个数字之间的错峰：已加载文件 → 索引记录 → 本次命中（0 / 40 / 80ms）
    void setHitStat(qulonglong n) { setStat(m_statHit, n, 2 * kStaggerMs); }
    void updateStats() {
        if (m_statFiles) setStat(m_statFiles, (qulonglong)m_loadedFiles, 0);
        if (m_statIndex) setStat(m_statIndex, (qulonglong)m_engine.getEntryCount(), kStaggerMs);
    }
    void doSearch() {
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
    void showHitTip() { QToolTip::showText(QCursor::pos(), QString(T("精确 %1 条 · 模糊 %2 条")).arg(m_lastExact).arg(m_lastFuzzy), this); }
    // ★回迁注意：二级筛选从「一级结果快照」逐级重算，绝不就地缩。
    //   原版 V0.3.0 的 DoFilter 是在 g_results 上原地 filterResults 并覆盖，导致"改个条件会在
    //   旧结果里继续缩"；本版统一为「条件链 + 从基准重算」，标准模式链长恒为 1。
    void applyFilterChain() {
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
    void doFilter() {
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
    void clearFilter() {
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
    void showDetail(int row) {
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
    void exportSelected() {
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
    // 左下角提示气泡：淡入 + 上浮（150ms OutCubic），到时淡出再隐藏。
    // 关闭动效时退化成原来的"直接显示/直接隐藏"，位置与时长都不变。
    void showToast(const QString& text, int ms = 10000) {
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
    void stopToastAnim() {
        if (m_toastAnim) m_toastAnim->stop();
        m_toastFading = false;
    }
    // 到时隐藏：先淡出再 hide（关闭动效时直接 hide）
    void hideToastAnim() {
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
    std::vector<int> selectedRows() const {
        std::vector<int> sel;
        if (!m_table) return sel;
        for (const auto& idx : m_table->selectionModel()->selectedRows()) sel.push_back(idx.row());
        std::sort(sel.begin(), sel.end());
        return sel;
    }
    // 结果表右键菜单：屏蔽 / 导出 / 行详情（标记子菜单在第 3 步接入）
    void onTableContextMenu(const QPoint& pos) {
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
    void markSelected(const std::vector<int>& sel, int color) {
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
    void blockSelectedEntries(const std::vector<int>& sel) {
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
    void blockSelectedFiles(const std::vector<int>& sel) {
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
    void fillTable(const std::vector<SearchResult>& res) {
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
    QWidget* statCell(const QString& label, QPushButton** valOut) {
        auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(2);
        auto* lv = new QLabel(label); lv->setObjectName("statLabel");
        auto* lv2 = new QPushButton("0"); lv2->setObjectName("statVal"); lv2->setFlat(true);
        if (valOut) *valOut = lv2;
        v->addWidget(lv); v->addWidget(lv2);
        return w;
    }
    QWidget* makeStatCard() {
        auto* body = new QWidget; auto* v = new QVBoxLayout(body); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(4);
        auto* stats = new QHBoxLayout; stats->setSpacing(24);
        stats->addWidget(statCell(T("已加载文件"), &m_statFiles));
        stats->addWidget(statCell(T("索引记录"), &m_statIndex));
        stats->addWidget(statCell(T("本次命中"), &m_statHit));
        if (m_statHit) connect(m_statHit, &QPushButton::clicked, this, &AppWindow::showHitTip);
        v->addLayout(stats);
        return body;
    }
    QWidget* makeResultCard() {
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
    QWidget* makeSearchPage() {
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
    QWidget* cardFrame(const QString& title, QWidget* body) {
        auto* f = new QFrame; f->setObjectName("card");
        auto* v = new QVBoxLayout(f); v->setContentsMargins(18, 16, 18, 18); v->setSpacing(14);
        auto* t = new QLabel(title); t->setObjectName("cardTitle");
        v->addWidget(t); v->addWidget(body);
        // 登记"随后到"的内容控件：分区切换时卡片（容器）先到、内容再淡入（错峰 kStaggerMs）
        f->setProperty("animChild", QVariant::fromValue(static_cast<QObject*>(body)));
        return f;
    }
    // 高级设置分区：外层是一个「解锁层 + 真实内容」两页栈。
    // 未解锁时只显示解锁层（盖住内容，看不到任何设置项）；密码正确后切到真实内容页，再离开该分区即上锁。
    // ★回迁注意：原版是搜索框隐藏入口关键词 + 弹窗密码；此处改为设置页内分区 + 解锁层，入口形式不同。
    //   详见 docs/回迁标注.md 第 6 条。
    QWidget* makeAdvSec() {
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
    QWidget* makeBlockAdminSec() {
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
    QWidget* makeMarkAdminSec() {
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
    QWidget* makePasswordSec() {
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
    void refreshBlockAdmin() {
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
    void clearMarksByColor(int color) {
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
    void refreshTableMarks() {
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
    QWidget* makeSmartColSec() {
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
    bool smartColValidateInput(std::string& out, QString& note) {
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
    void smartColError(const QString& msg) {
        if (m_colErr) m_colErr->setText(msg);
        showToast(msg);
    }
    void smartColAdd() {
        if ((int)m_extraCols.size() >= kMaxResultCols - 3) { smartColError(T("最多 7 列，请先删除一列")); return; }
        std::string src; QString note;
        if (!smartColValidateInput(src, note)) return;
        m_extraCols.push_back(src);
        if (!note.isEmpty() && m_colErr) m_colErr->setText(note);
        smartColApplied(note.isEmpty() ? (T("已添加列：") + u8(src)) : note);
    }
    void smartColUpdate() {
        if (!m_colList) return;
        int idx = m_colList->currentRow() - 2;
        if (idx < 0 || idx >= (int)m_extraCols.size()) { smartColError(T("请先选中一个自定义列（固定列不可改动）")); return; }
        std::string src; QString note;
        if (!smartColValidateInput(src, note)) return;
        m_extraCols[idx] = src;
        if (!note.isEmpty() && m_colErr) m_colErr->setText(note);
        smartColApplied(note.isEmpty() ? (T("已更新为：") + u8(src)) : note);
    }
    void smartColRemove() {
        if (!m_colList) return;
        int idx = m_colList->currentRow() - 2;
        if (idx < 0 || idx >= (int)m_extraCols.size()) { smartColError(T("请先选中一个自定义列（固定列不可改动）")); return; }
        if ((int)m_extraCols.size() <= 0) return;
        m_extraCols.erase(m_extraCols.begin() + idx);
        smartColApplied(T("已删除该列"));
    }
    // 配置变更后的统一收尾：保存 → 重建表列 → 刷新设置页预览
    void smartColApplied(const QString& toast) {
        saveSettings();
        applyColumns();
        smartColRefresh();
        showToast(toast);
    }
    void smartColRefresh() {
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
    QWidget* makeHistorySec() {
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
    // 【预留接口】注册一个实用工具；makeUtilPage() 会自动把它排进左侧列表
    void registerUtilTool(const QString& title, std::function<QWidget*()> make) {
        m_utilTools.push_back({ title, make });
    }
    // 【预留接口】实用工具页：空表时给明确空态，避免以后启用时出现"白页"被当成 bug
    QWidget* makeUtilPage() {
        auto* page = new QWidget; auto* h = new QHBoxLayout(page); h->setContentsMargins(0, 0, 0, 0); h->setSpacing(14);
        if (m_utilTools.empty()) {
            auto* empty = new QLabel(T("暂无实用工具。\n低频辅助功能会集中在这里，不占用主界面。"));
            empty->setObjectName("statLabel"); empty->setAlignment(Qt::AlignCenter); empty->setWordWrap(true);
            h->addWidget(empty, 1);
            return page;
        }
        auto* nav = new QListWidget; nav->setFixedWidth(168);
        for (const auto& t : m_utilTools) nav->addItem(t.title);
        h->addWidget(nav);
        enableRowHoverAnim(nav, makeProto(m_dark, m_accent), &m_rowDelegates);
        auto* stack = new QStackedWidget;
        for (const auto& t : m_utilTools) stack->addWidget(cardFrame(t.title, t.make()));
        h->addWidget(stack, 1);
        connect(nav, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);
        nav->setCurrentRow(0);
        return page;
    }
    QWidget* makeSettingsPage() {
        auto* page = new QWidget; auto* h = new QHBoxLayout(page); h->setContentsMargins(0, 0, 0, 0); h->setSpacing(14);
        m_navList = new QListWidget; m_navList->setFixedWidth(168);
        for (const auto& s : m_secs) m_navList->addItem(s.title);
        h->addWidget(m_navList);
        enableRowHoverAnim(m_navList, makeProto(m_dark, m_accent), &m_rowDelegates);
        m_secStack = new QStackedWidget;
        for (const auto& s : m_secs) m_secStack->addWidget(cardFrame(s.title, s.make()));
        h->addWidget(m_secStack, 1);
        connect(m_navList, &QListWidget::currentRowChanged, this, [this](int row) {
            if (m_secStack) {
                // 设置分区切换：淡入 + 轻微上移（只对真正换了分区时做）
                const bool changed = (m_secStack->currentIndex() != row);
                m_secStack->setCurrentIndex(row);
                if (changed) enterAnim(m_secStack->currentWidget(), kSecAnimMs, kEnterSlidePx, animChildOf(m_secStack->currentWidget()));
            }
            onSettingsSectionChanged(row);
        });
        for (int i = 0; i < (int)m_secs.size(); i++) if (m_secs[i].title == T("高级设置")) m_advSecIndex = i;
        m_navList->setCurrentRow(0);
        return page;
    }
    // 通用设置：目前只有「关闭选项设置」（关闭行为 + 是否询问），后续通用项也放这里
    // 通用设置：外观（原「界面设置」并入）+ 关闭选项设置。设置项变多后按"通用"归并，减少左侧标签数量
    QWidget* makeGeneralSec() {
        auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(16);
        // —— 外观 ——
        v->addWidget(new QLabel(T("主题")));
        auto* trow = new QHBoxLayout; trow->setSpacing(10);
        m_lightBtn = new QPushButton(T("浅色")); m_lightBtn->setObjectName("themeToggle"); m_lightBtn->setCheckable(true);
        m_darkBtn = new QPushButton(T("深色")); m_darkBtn->setObjectName("themeToggle"); m_darkBtn->setCheckable(true);
        QButtonGroup* tbg = new QButtonGroup(this); tbg->addButton(m_lightBtn); tbg->addButton(m_darkBtn);
        connect(m_lightBtn, &QPushButton::clicked, this, [this] { if (m_dark) flipTheme(); });
        connect(m_darkBtn, &QPushButton::clicked, this, [this] { if (!m_dark) flipTheme(); });
        trow->addWidget(m_lightBtn); trow->addWidget(m_darkBtn); trow->addStretch();
        v->addLayout(trow);
        v->addWidget(new QLabel(T("强调色")));
        auto* arow = new QHBoxLayout; arow->setSpacing(10);
        for (const auto& a : m_accents) {
            auto* b = new QPushButton; b->setFixedSize(28, 28); b->setCursor(Qt::PointingHandCursor);
            b->setToolTip(a.name());
            b->setProperty("noHoverTint", true);   // 自带样式表（选中描边），不参与 QSS 状态过渡
            QColor cc = a; connect(b, &QPushButton::clicked, this, [this, cc] { setAccent(cc); });
            m_accentBtns.push_back(b);
            arow->addWidget(b);
        }
        refreshAccentButtons();   // 统一 28px 圆点 + 当前项描边（否则看不出选的是哪个）
        auto* custom = new QPushButton(T("自定义…")); custom->setObjectName("themeBtn");
        connect(custom, &QPushButton::clicked, this, &AppWindow::pickCustomAccent);
        arow->addWidget(custom); arow->addStretch();
        v->addLayout(arow);

        auto* sep = new QFrame; sep->setFrameShape(QFrame::HLine); sep->setObjectName("card"); v->addWidget(sep);

        // —— 关闭选项设置 ——
        v->addWidget(new QLabel(T("关闭选项设置")));
        m_closeAskBtn = new QRadioButton(T("每次询问（关闭时选择关闭方式）"));
        m_closeDirectBtn = new QRadioButton(T("直接关闭程序"));
        m_closeTrayBtn = new QRadioButton(T("最小化到托盘"));
        // 先设初值再连信号，避免构造期触发保存
        refreshCloseRadios();
        QButtonGroup* bg = new QButtonGroup(this);
        bg->addButton(m_closeAskBtn); bg->addButton(m_closeDirectBtn); bg->addButton(m_closeTrayBtn);
        v->addWidget(m_closeAskBtn);
        v->addWidget(m_closeDirectBtn);
        v->addWidget(m_closeTrayBtn);
        connect(m_closeAskBtn, &QRadioButton::toggled, this, [this](bool on) { if (on) { m_closeAction = 0; saveSettings(); } });
        connect(m_closeDirectBtn, &QRadioButton::toggled, this, [this](bool on) { if (on) { m_closeAction = 1; saveSettings(); } });
        connect(m_closeTrayBtn, &QRadioButton::toggled, this, [this](bool on) { if (on) { m_closeAction = 2; saveSettings(); } });
        // 说明书提示开关（对应 MAA 公告的「不显示公告」；内容更新后仍会恢复提示）
        m_manualChk = new QCheckBox(T("启动时不再提示说明书（说明书更新后恢复提示）"));
        m_manualChk->setChecked(m_manualNeverShow);
        v->addWidget(m_manualChk);
        connect(m_manualChk, &QCheckBox::toggled, this, [this](bool on) { m_manualNeverShow = on; saveSettings(); });
        v->addStretch();
        return w;
    }
    // 搜索设置（原「模糊搜索」分区）：模糊搜索 + 筛选模式，自上而下排布
    QWidget* makeSearchSec() {
        auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(14);
        v->addWidget(new QLabel(T("模糊搜索")));
        auto* desc = new QLabel(T("勾选后：精确搜索无结果时会补充模糊匹配（rapidfuzz），并与精确结果合并展示。\n关闭后：只保留精确匹配结果。"));
        desc->setObjectName("statLabel"); desc->setWordWrap(true);
        v->addWidget(desc);
        m_fuzzyBox = new QCheckBox(T("启用模糊搜索（默认开）"));
        m_fuzzyBox->setChecked(m_fuzzyEnabled);
        connect(m_fuzzyBox, &QCheckBox::toggled, this, [this](bool on) { m_fuzzyEnabled = on; saveSettings(); doSearch(); });
        v->addWidget(m_fuzzyBox);

        auto* sep = new QFrame; sep->setFrameShape(QFrame::HLine); sep->setObjectName("card"); v->addWidget(sep);

        v->addWidget(new QLabel(T("筛选模式")));
        // 说明放在各自模式的下方（缩进），而不是先给一段总简介再摆两个选项 —— 用户要对着选项看
        auto addModeDesc = [&](const QString& text) {
            auto* d = new QLabel(text);
            d->setObjectName("statLabel"); d->setWordWrap(true);
            d->setContentsMargins(24, 0, 0, 0);   // 缩进到所属单选之下
            v->addWidget(d);
        };
        m_modeStdBtn = new QRadioButton(T("标准模式（每次基于一级搜索结果重新筛）"));
        m_modeChainBtn = new QRadioButton(T("逐级模式（在上一次筛选结果里继续筛）"));
        // 先设初值再连信号，避免构造期触发保存/toast
        (m_chainMode ? m_modeChainBtn : m_modeStdBtn)->setChecked(true);
        QButtonGroup* mbg = new QButtonGroup(this); mbg->addButton(m_modeStdBtn); mbg->addButton(m_modeChainBtn);
        v->addWidget(m_modeStdBtn);
        addModeDesc(T("每次筛选都基于一级搜索结果重新筛，改条件就是换条件。\n"
                      "例：搜「工日」得 54 条 → 输入「辅助」得 26 条；把「辅助」改成「检修」→ 仍在 54 条里筛"));
        v->addWidget(m_modeChainBtn);
        addModeDesc(T("在上一次筛选结果里继续筛，可逐级下钻。\n"
                      "例：搜「工日」得 54 条 → 输入「辅助」得 26 条；再输入「张三」→ 在 26 条里筛出 5 条"));
        connect(m_modeStdBtn, &QRadioButton::toggled, this, [this](bool on) {
            if (!on) return;
            m_chainMode = false; saveSettings();
            showToast(T("已切换为「标准模式」：每次筛选都基于一级搜索结果"));
        });
        connect(m_modeChainBtn, &QRadioButton::toggled, this, [this](bool on) {
            if (!on) return;
            m_chainMode = true; saveSettings();
            showToast(T("已切换为「逐级模式」：在上一次筛选结果里继续筛"));
        });
        v->addStretch();
        return w;
    }
    QWidget* makeEncSec() {
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
    QWidget* makeShareSec() {
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
    QWidget* makeAboutSec() {
        auto* w = new QWidget; auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(8);
        v->addWidget(new QLabel(T("Excel 表格关键字搜索工具")));
        auto* ver = new QLabel(T("版本 v0.3.2 · Qt6")); ver->setObjectName("statLabel");
        v->addWidget(ver);
        v->addWidget(new QLabel(T("作者 / 发行者：觉心恋影")));
        v->addWidget(new QLabel(projectLicense().isEmpty() ? T("开源许可：（待定）") : (T("开源许可：") + projectLicense())));
        v->addWidget(new QLabel(T("本程序以动态链接方式使用 Qt（LGPL-3.0）；第三方组件许可见 licenses 目录。")));
        v->addWidget(new QLabel(T("界面视觉风格参考自 MAA / MaaWpfGui 与 MaaEnd。")));
        auto* row = new QHBoxLayout;
        auto* manBtn = new QPushButton(T("使用说明书")); manBtn->setObjectName("primaryBtn");
        manBtn->setCursor(Qt::PointingHandCursor);
        connect(manBtn, &QPushButton::clicked, this, [this] { openManual(); });
        row->addWidget(manBtn);
        auto* btn = new QPushButton(T("查看开源声明")); btn->setObjectName("themeBtn");
        connect(btn, &QPushButton::clicked, this, [] {
            const QString dir = QCoreApplication::applicationDirPath() + "/licenses";
            QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(dir).exists() ? dir : QCoreApplication::applicationDirPath()));
        });
        row->addWidget(btn); row->addStretch();
        v->addLayout(row);
        v->addStretch();
        return w;
    }
    void buildUi() {
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
};

// 自检钩子输出：本程序是 GUI 子系统（不分配控制台），所以钩子结果统一写文件而不是 stdout。
// 用法：--search 工日 --out out.txt / --colprobe 核定工日 --out out.txt
static void writeHookOut(const QString& path, const QString& text) {
    if (path.isEmpty()) return;
    std::ofstream o(path.toLocal8Bit().constData());
    if (o) o << text.toUtf8().constData();
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


#include "main.moc"
