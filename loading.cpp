#include "loading.h"

bool readAllBytes(const std::string& path, std::vector<uint8_t>& out) {
    namespace fs = std::filesystem;
    std::ifstream f(fs::u8path(path), std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return !out.empty();
}

bool _rI32(const uint8_t*& p, const uint8_t* end, int32_t& v) { if (end - p < 4) return false; memcpy(&v, p, 4); p += 4; return true; }

bool _rI64(const uint8_t*& p, const uint8_t* end, int64_t& v) { if (end - p < 8) return false; memcpy(&v, p, 8); p += 8; return true; }

bool collectDiskFiles(const std::string& dataDir, std::vector<DiskFile>& out) {
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

void _wI32(std::string& o, int32_t v) { o.append(reinterpret_cast<const char*>(&v), 4); }

void _wI64(std::string& o, int64_t v) { o.append(reinterpret_cast<const char*>(&v), 8); }

bool writeInventory(const std::string& path, const std::vector<DiskFile>& files) {
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

bool readInventory(const std::string& path,
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

bool xseToSheets(const std::string& path, const std::string& pwd,
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

std::string _esc(const std::string& s) { std::string r; for (char c : s) { if (c == '&') r += "&amp;"; else if (c == '<') r += "&lt;"; else if (c == '>') r += "&gt;"; else if (c == '"') r += "&quot;"; else r += c; } return r; }

std::string _colRef(int idx) { std::string r; idx++; while (idx > 0) { idx--; r = char('A' + (idx % 26)) + r; idx /= 26; } return r; }

bool exportXlsxTo(const std::vector<SearchResult>& sel, const std::string& outPath) {
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
