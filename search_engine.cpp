#include "search_engine.h"
#include "pinyin.h"
#include <rapidfuzz/fuzz.hpp>
#include <sstream>
#include <regex>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <set>
#include <tuple>
#include <cstring>
#include "core/file_io.h"

std::string SearchEngine::toLower(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    const auto* data = reinterpret_cast<const unsigned char*>(str.data());
    const auto* dataEnd = data + str.size();
    for (const unsigned char* c = data; c != dataEnd; ++c) {
        unsigned char byte = *c;
        if (byte >= 'A' && byte <= 'Z') {
            result += static_cast<char>(byte + ('a' - 'A'));
        } else {
            result += static_cast<char>(byte);
        }
    }
    return result;
}

bool SearchEngine::matchKeyword(const std::string& text, const std::string& keyword) {
    if (toLower(text).find(toLower(keyword)) != std::string::npos) return true;
    // 拼音首字母匹配（如 "hdgr" 命中 "核定工日"）：仅当关键词非纯中文时尝试
    // 关键词含字母（可能为用户输入的拼音首字母）时，转换文本首字母再匹配
    bool keywordHasAscii = false;
    for (char c : keyword) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) { keywordHasAscii = true; break; }
    }
    if (!keywordHasAscii) return false;
    std::string textInit = pinyin::initials(text);
    if (textInit.empty()) return false;
    std::string kwLower = toLower(keyword);
    return textInit.find(kwLower) != std::string::npos;
}

void SearchEngine::addFile(const std::string& filename, const std::vector<SheetData>& sheets) {
    loadedFiles.push_back(filename);

    fileData[filename];
    headers[filename];

    for (const auto& sheet : sheets) {
        fileData[filename][sheet.name] = sheet.rows;
        headers[filename][sheet.name] = sheet.headers;

        for (const auto& [rowNum, rowData] : sheet.rows) {
            for (const auto& [colIdx, cellValue] : rowData) {
                Entry entry;
                entry.filename = filename;
                entry.sheetName = sheet.name;
                entry.row = rowNum;
                entry.col = colIdx;
                entry.colName = XlsxReader::colIndexToName(colIdx);
                entry.value = cellValue;
                entries.push_back(entry);
            }
        }
    }
}

void SearchEngine::clear() {
    entries.clear();
    fileData.clear();
    headers.clear();
    loadedFiles.clear();
}

std::vector<SearchResult> SearchEngine::search(const std::string& keyword) const {
    std::vector<SearchResult> results;
    std::set<std::tuple<std::string, std::string, int>> seenRows;

    for (const auto& entry : entries) {
        if (!matchKeyword(entry.value, keyword)) continue;

        auto key = std::make_tuple(entry.filename, entry.sheetName, entry.row);
        if (seenRows.count(key)) continue;
        seenRows.insert(key);

        SearchResult sr;
        sr.filename = entry.filename;
        sr.sheetName = entry.sheetName;
        sr.row = entry.row;
        sr.col = entry.col;
        sr.colName = entry.colName;
        sr.matchedValue = entry.value;

        auto fileIt = fileData.find(entry.filename);
        if (fileIt != fileData.end()) {
            auto sheetIt = fileIt->second.find(entry.sheetName);
            if (sheetIt != fileIt->second.end()) {
                auto rowIt = sheetIt->second.find(entry.row);
                if (rowIt != sheetIt->second.end()) {
                    sr.rowCells = rowIt->second;
                }
            }
        }

        auto hdrFileIt = headers.find(entry.filename);
        if (hdrFileIt != headers.end()) {
            auto hdrSheetIt = hdrFileIt->second.find(entry.sheetName);
            if (hdrSheetIt != hdrFileIt->second.end()) {
                sr.headers = hdrSheetIt->second;
            }
        }

        results.push_back(sr);
    }

    return results;
}

std::vector<SearchResult> SearchEngine::fuzzySearch(const std::string& keyword, double minScore) const {
    // 性能优化：预计算 keyword 的字符集合与拼音首字母集合，用于粗筛
    // （单元格与 keyword 无任何公共字符时，文本相似度必 < minScore，可跳过 rapidfuzz）
    bool kwChars[256] = {};
    for (unsigned char c : keyword) kwChars[c] = true;
    std::string kwInit = pinyin::initials(keyword);
    bool kwInitChars[256] = {};
    for (unsigned char c : kwInit) kwInitChars[c] = true;

    std::vector<std::pair<double, SearchResult>> scored;
    // 按行去重：同一行多个单元格命中时只保留最高分的一条（与 search()/regexSearch() 行为一致）
    std::map<std::tuple<std::string, std::string, int>, double> rowBestScore;
    std::map<std::tuple<std::string, std::string, int>, SearchResult> rowBestResult;

    for (const auto& entry : entries) {
        const std::string& value = entry.value;
        if (value.empty()) continue;

        double best = 0;
        std::string bestValue;
        int bestCol = entry.col;
        {
            // 文本相似度：仅当与 keyword 有公共字符时才值得调用 rapidfuzz
            bool shareText = false;
            for (unsigned char c : value) {
                if (kwChars[c]) { shareText = true; break; }
            }
            if (shareText) {
                double s = rapidfuzz::fuzz::ratio(value, keyword, minScore);
                if (s > best) { best = s; bestValue = value; bestCol = entry.col; }
            }
            // 拼音首字母匹配（如 "hdgr" vs "核定工日" 的 "hdgr"）
            if (!kwInit.empty()) {
                std::string vInit = pinyin::initials(value);
                if (!vInit.empty()) {
                    bool shareInit = false;
                    for (unsigned char c : vInit) {
                        if (kwInitChars[c]) { shareInit = true; break; }
                    }
                    if (shareInit) {
                        double si = rapidfuzz::fuzz::ratio(vInit, kwInit, minScore);
                        if (si > best) { best = si; bestValue = value; bestCol = entry.col; }
                    }
                }
            }
        }
        if (best < minScore) continue;

        auto rowKey = std::make_tuple(entry.filename, entry.sheetName, entry.row);
        auto scoreIt = rowBestScore.find(rowKey);
        if (scoreIt != rowBestScore.end() && scoreIt->second >= best) continue;  // 该行已有更高分

        SearchResult sr;
        sr.filename = entry.filename;
        sr.sheetName = entry.sheetName;
        sr.row = entry.row;
        sr.col = bestCol;
        sr.colName = XlsxReader::colIndexToName(bestCol);
        sr.matchedValue = bestValue;

        auto fileIt = fileData.find(entry.filename);
        if (fileIt != fileData.end()) {
            auto sheetIt = fileIt->second.find(entry.sheetName);
            if (sheetIt != fileIt->second.end()) {
                auto rowIt = sheetIt->second.find(entry.row);
                if (rowIt != sheetIt->second.end()) sr.rowCells = rowIt->second;
            }
        }
        auto hdrFileIt = headers.find(entry.filename);
        if (hdrFileIt != headers.end()) {
            auto hdrSheetIt = hdrFileIt->second.find(entry.sheetName);
            if (hdrSheetIt != hdrFileIt->second.end()) sr.headers = hdrSheetIt->second;
        }
        rowBestScore[rowKey] = best;
        rowBestResult[rowKey] = std::move(sr);
    }

    for (auto& [key, sr] : rowBestResult) {
        scored.emplace_back(rowBestScore[key], std::move(sr));
    }

    // 按相似度降序
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<SearchResult> results;
    results.reserve(scored.size());
    for (auto& [score, sr] : scored) results.push_back(std::move(sr));
    return results;
}

std::vector<SearchResult> SearchEngine::regexSearch(const std::string& pattern) const {
    std::vector<SearchResult> results;
    std::set<std::tuple<std::string, std::string, int>> seenRows;

    std::regex re;
    try {
        re = std::regex(pattern);
    } catch (const std::regex_error&) {
        return results;   // 无效正则返回空
    }

    for (const auto& entry : entries) {
        if (!std::regex_search(entry.value, re)) continue;

        auto key = std::make_tuple(entry.filename, entry.sheetName, entry.row);
        if (seenRows.count(key)) continue;
        seenRows.insert(key);

        SearchResult sr;
        sr.filename = entry.filename;
        sr.sheetName = entry.sheetName;
        sr.row = entry.row;
        sr.col = entry.col;
        sr.colName = entry.colName;
        sr.matchedValue = entry.value;

        auto fileIt = fileData.find(entry.filename);
        if (fileIt != fileData.end()) {
            auto sheetIt = fileIt->second.find(entry.sheetName);
            if (sheetIt != fileIt->second.end()) {
                auto rowIt = sheetIt->second.find(entry.row);
                if (rowIt != sheetIt->second.end()) sr.rowCells = rowIt->second;
            }
        }
        auto hdrFileIt = headers.find(entry.filename);
        if (hdrFileIt != headers.end()) {
            auto hdrSheetIt = hdrFileIt->second.find(entry.sheetName);
            if (hdrSheetIt != hdrFileIt->second.end()) sr.headers = hdrSheetIt->second;
        }
        results.push_back(sr);
    }
    return results;
}

std::vector<SearchResult> SearchEngine::filterResults(
    const std::vector<SearchResult>& results, const std::string& keyword)
{
    std::vector<SearchResult> filtered;
    for (const auto& r : results) {
        bool found = matchKeyword(r.matchedValue, keyword);
        if (!found) {
            for (const auto& [col, val] : r.rowCells) {
                if (matchKeyword(val, keyword)) {
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            filtered.push_back(r);
        }
    }
    return filtered;
}

// V0.3.1：缓存序列化改为内存 buffer（writer/reader），最后经 core 跨平台文件 IO 一次读写
static void writeI32(std::string& o, int32_t v) { o.append(reinterpret_cast<const char*>(&v), 4); }
static void writeI64(std::string& o, int64_t v) { o.append(reinterpret_cast<const char*>(&v), 8); }
static void writeStr(std::string& o, const std::string& s) {
    writeI32(o, (int32_t)s.size());
    if (!s.empty()) o.append(s);
}

static int32_t readI32(const uint8_t*& p, const uint8_t* end) {
    if (p + 4 > end) return 0;
    int32_t v; std::memcpy(&v, p, 4); p += 4; return v;
}
static int64_t readI64(const uint8_t*& p, const uint8_t* end) {
    if (p + 8 > end) return 0;
    int64_t v; std::memcpy(&v, p, 8); p += 8; return v;
}
static std::string readStr(const uint8_t*& p, const uint8_t* end) {
    int32_t len = readI32(p, end);
    if (len <= 0 || len > 10 * 1024 * 1024 || p + len > end) return "";
    std::string s(reinterpret_cast<const char*>(p), len);
    p += len;
    return s;
}

bool SearchEngine::saveToFile(const std::string& filepath) const {
    std::string out;   // V0.3.1：内存 buffer 序列化后一次性写入
    out.append("EXSR2", 5);

    time_t now = time(nullptr);
    writeI64(out, (int64_t)now);

    writeI32(out, (int32_t)loadedFiles.size());
    for (const auto& fn : loadedFiles) {
        writeStr(out, fn);
        int64_t mtime = 0, size = 0;
        auto mtIt = fileMTimes.find(fn);
        if (mtIt != fileMTimes.end()) mtime = mtIt->second;
        auto szIt = fileSizes.find(fn);
        if (szIt != fileSizes.end()) size = szIt->second;
        writeI64(out, mtime);
        writeI64(out, size);
    }

    writeI32(out, (int32_t)entries.size());
    for (const auto& e : entries) {
        writeStr(out, e.filename);
        writeStr(out, e.sheetName);
        writeI32(out, e.row);
        writeI32(out, e.col);
        writeStr(out, e.colName);
        writeStr(out, e.value);
    }

    writeI32(out, (int32_t)fileData.size());
    for (const auto& [fname, sheets] : fileData) {
        writeStr(out, fname);
        writeI32(out, (int32_t)sheets.size());
        for (const auto& [sname, rows] : sheets) {
            writeStr(out, sname);
            writeI32(out, (int32_t)rows.size());
            for (const auto& [rowNum, cells] : rows) {
                writeI32(out, rowNum);
                writeI32(out, (int32_t)cells.size());
                for (const auto& [colIdx, val] : cells) {
                    writeI32(out, colIdx);
                    writeStr(out, val);
                }
            }
        }
    }

    writeI32(out, (int32_t)headers.size());
    for (const auto& [fname, shdrs] : headers) {
        writeStr(out, fname);
        writeI32(out, (int32_t)shdrs.size());
        for (const auto& [sname, hdrs] : shdrs) {
            writeStr(out, sname);
            writeI32(out, (int32_t)hdrs.size());
            for (const auto& h : hdrs) writeStr(out, h);
        }
    }

    return core::WriteFileBytes(filepath, out.data(), out.size());
}

bool SearchEngine::loadFromFile(const std::string& filepath) {
    // V0.3.1：经 core 跨平台文件 IO 整读，再从内存按序解析
    std::vector<uint8_t> data;
    if (!core::ReadFileBytes(filepath, data)) return false;
    const uint8_t* p = data.data();
    const uint8_t* end = data.data() + data.size();

    if (end - p < 5 || std::memcmp(p, "EXSR2", 5) != 0) return false;
    p += 5;

    readI64(p, end);   // 时间戳，跳过

    clear();

    int32_t fileCount = readI32(p, end);
    for (int32_t i = 0; i < fileCount; i++) {
        std::string fn = readStr(p, end);
        int64_t mtime = readI64(p, end);
        int64_t size = readI64(p, end);
        loadedFiles.push_back(fn);
        fileMTimes[fn] = mtime;
        fileSizes[fn] = size;
    }

    int32_t entryCount = readI32(p, end);
    entries.reserve(entryCount);
    for (int32_t i = 0; i < entryCount; i++) {
        Entry e;
        e.filename = readStr(p, end);
        e.sheetName = readStr(p, end);
        e.row = readI32(p, end);
        e.col = readI32(p, end);
        e.colName = readStr(p, end);
        e.value = readStr(p, end);
        entries.push_back(std::move(e));
    }

    int32_t fdCount = readI32(p, end);
    for (int32_t i = 0; i < fdCount; i++) {
        std::string fname = readStr(p, end);
        int32_t shCount = readI32(p, end);
        for (int32_t j = 0; j < shCount; j++) {
            std::string sname = readStr(p, end);
            int32_t rowCount = readI32(p, end);
            for (int32_t k = 0; k < rowCount; k++) {
                int rn = readI32(p, end);
                int32_t cellCount = readI32(p, end);
                for (int32_t m = 0; m < cellCount; m++) {
                    int ci = readI32(p, end);
                    fileData[fname][sname][rn][ci] = readStr(p, end);
                }
            }
        }
    }

    int32_t hdrCount = readI32(p, end);
    for (int32_t i = 0; i < hdrCount; i++) {
        std::string fname = readStr(p, end);
        int32_t shCount = readI32(p, end);
        for (int32_t j = 0; j < shCount; j++) {
            std::string sname = readStr(p, end);
            int32_t hc = readI32(p, end);
            for (int32_t k = 0; k < hc; k++) {
                headers[fname][sname].push_back(readStr(p, end));
            }
        }
    }

    return true;
}

void SearchEngine::setFileMeta(const std::map<std::string, int64_t>& mtimes,
                               const std::map<std::string, int64_t>& sizes) {
    fileMTimes = mtimes;
    fileSizes = sizes;
}
