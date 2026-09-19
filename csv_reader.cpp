#include "csv_reader.h"
#include "xlsx_reader.h"   // 复用 SheetData 定义

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

#include "core/file_io.h"

// 逐行解析 CSV（处理引号、转义），返回单元格列表
static bool ParseCsvLine(const char* p, const char* end, std::vector<std::string>& cells) {
    cells.clear();
    std::string cur;
    bool inQuotes = false;
    while (p < end) {
        char c = *p;
        if (inQuotes) {
            if (c == '"') {
                if (p + 1 < end && p[1] == '"') {   // "" -> 转义引号
                    cur += '"';
                    p += 2;
                    continue;
                }
                inQuotes = false;
                p++;
            } else {
                cur += c;
                p++;
            }
        } else {
            if (c == '"') {
                inQuotes = true;
                p++;
            } else if (c == ',') {
                cells.push_back(cur);
                cur.clear();
                p++;
            } else if (c == '\r' || c == '\n') {
                break;   // 行结束（\r\n 由外层统一处理）
            } else {
                cur += c;
                p++;
            }
        }
    }
    if (inQuotes) return false;   // 引号未闭合，视为损坏行
    cells.push_back(cur);
    return true;
}

bool CsvReader::open(const std::string& filepath) {
    filename = filepath;
    sheets.clear();

    std::vector<uint8_t> data;
    if (!core::ReadFileBytes(filepath, data)) return false;

    // 去除 UTF-8 BOM
    size_t start = 0;
    if (data.size() >= 3 &&
        (unsigned char)data[0] == 0xEF &&
        (unsigned char)data[1] == 0xBB &&
        (unsigned char)data[2] == 0xBF) {
        start = 3;
    }

    SheetData sd;
    sd.name = "CSV";
    int rowNum = 1;
    bool firstRow = true;

    const char* p = reinterpret_cast<const char*>(data.data()) + start;
    const char* end = reinterpret_cast<const char*>(data.data()) + data.size();

    while (p < end) {
        // 定位行结束（支持 \r\n 与 \n）
        const char* lineEnd = p;
        while (lineEnd < end && *lineEnd != '\r' && *lineEnd != '\n') lineEnd++;
        // 注意：引号内可能包含 \n（多行字段），此处简化处理——CSV 规范字段可跨行，
        // 但本工具面向表格导出场景，单行字段为主；若需完整支持可在此扩展。

        std::vector<std::string> cells;
        if (ParseCsvLine(p, lineEnd, cells)) {
            for (int c = 0; c < (int)cells.size(); c++) {
                if (cells[c].empty()) continue;
                sd.rows[rowNum][c] = cells[c];
                if (firstRow) {
                    while ((int)sd.headers.size() <= c) sd.headers.push_back("");
                    sd.headers[c] = cells[c];
                }
            }
            rowNum++;
            firstRow = false;
        }

        // 跳过行结束符
        if (lineEnd < end) {
            if (*lineEnd == '\r' && lineEnd + 1 < end && lineEnd[1] == '\n') p = lineEnd + 2;
            else p = lineEnd + 1;
        } else {
            break;
        }
    }

    if (sd.rows.empty()) return false;
    sheets.push_back(std::move(sd));
    return true;
}
