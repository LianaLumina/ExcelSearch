#include "xls_reader.h"
#include "xlsx_reader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/file_io.h"

#include "xls.h"

static std::string cleanNumberStr(const std::string& s) {
    if (s.empty()) return s;
    if (s.find('.') == std::string::npos) return s;
    char* endp = nullptr;
    double v = strtod(s.c_str(), &endp);
    if (endp && *endp == '\0') {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.10g", v);
        return buf;
    }
    return s;
}

bool XlsReader::open(const std::string& filepath) {
    filename = filepath;
    sheets.clear();

    // V0.3.1：统一经 core 跨平台文件 IO 读整个文件，交给 libxls 从内存解析
    std::vector<unsigned char> buffer;
    if (!core::ReadFileBytes(filepath, buffer) || buffer.empty()) return false;

    xls::xls_error_t err = xls::LIBXLS_OK;
    xls::xlsWorkBook* pWB = xls::xls_open_buffer(buffer.data(), buffer.size(), "UTF-8", &err);
    if (!pWB) return false;

    for (xls::DWORD si = 0; si < pWB->sheets.count; si++) {
        xls::xlsWorkSheet* pWS = xls::xls_getWorkSheet(pWB, si);
        if (!pWS) continue;
        if (xls::xls_parseWorkSheet(pWS) != xls::LIBXLS_OK) {
            xls::xls_close_WS(pWS);
            continue;
        }

        SheetData sd;
        const char* sheetName = pWB->sheets.sheet[si].name;
        sd.name = sheetName ? sheetName : ("Sheet" + std::to_string(si + 1));

        bool firstDataRow = true;

        for (xls::DWORD r = 0; r <= pWS->rows.lastrow; r++) {
            xls::xlsRow* row = xls::xls_row(pWS, (xls::WORD)r);
            if (!row || row->cells.count == 0) continue;

            for (xls::DWORD c = 0; c < row->cells.count; c++) {
                xls::xlsCell* cell = &row->cells.cell[c];
                std::string value;

                if (cell->str) {
                    value = cell->str;
                    if (value == "bool") {
                        value = (cell->d != 0.0) ? "TRUE" : "FALSE";
                    } else if (value == "error") {
                        value = "";
                    } else {
                        value = cleanNumberStr(value);
                    }
                } else if (cell->d != 0.0) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%.10g", cell->d);
                    value = buf;
                }

                if (!value.empty()) {
                    int rowNum = (int)r + 1;
                    int colIdx = (int)cell->col;
                    sd.rows[rowNum][colIdx] = value;

                    if (firstDataRow) {
                        while ((int)sd.headers.size() <= colIdx) {
                            sd.headers.push_back("");
                        }
                        sd.headers[colIdx] = value;
                    }
                }
            }
            firstDataRow = false;
        }

        xls::xls_close_WS(pWS);
        sheets.push_back(std::move(sd));
    }

    xls::xls_close_WB(pWB);
    return !sheets.empty();
}
