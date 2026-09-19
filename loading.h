#pragma once
// 加载：磁盘扫描、缓存索引与全量清单的读写、.xse 解密取表、结果导出为 xlsx，以及后台加载线程。
#include <QThread>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QList>
#include <QFile>
#include <QDir>
#include <QSettings>
#include <QMutex>
#include <fstream>
#include <iterator>
#include <QMetaType>
#include <functional>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

#include "common.h"
#include "theme.h"
#include "data_model.h"
#include "search_engine.h"
#include "xse_codec.h"
#include "miniz.h"
#include "xlsx_reader.h"
#include "xls_reader.h"
#include "csv_reader.h"
#include "docx_reader.h"

// ---- 函数声明 ----
bool readAllBytes(const std::string& path, std::vector<uint8_t>& out);
bool _rI32(const uint8_t*& p, const uint8_t* end, int32_t& v);
bool _rI64(const uint8_t*& p, const uint8_t* end, int64_t& v);
bool collectDiskFiles(const std::string& dataDir, std::vector<DiskFile>& out);
void _wI32(std::string& o, int32_t v);
void _wI64(std::string& o, int64_t v);
bool writeInventory(const std::string& path, const std::vector<DiskFile>& files);
bool readInventory(const std::string& path, std::map<std::string, std::pair<int64_t, int64_t>>& out, int64_t& ts);
bool xseToSheets(const std::string& path, const std::string& pwd, std::vector<SheetData>& sheets, bool& pwdEnabled);
std::string _esc(const std::string& s);
std::string _colRef(int idx);
bool exportXlsxTo(const std::vector<SearchResult>& sel, const std::string& outPath);

// ---- 后台线程 ----
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
