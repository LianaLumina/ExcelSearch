#include "xlsx_reader.h"
#include "miniz.h"
#include "pugixml.hpp"

#include <sstream>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include "core/file_io.h"

int XlsxReader::colNameToIndex(const std::string& name) {
    int result = -1;
    for (char c : name) {
        result = (result + 1) * 26 + (std::toupper(static_cast<unsigned char>(c)) - 'A');
    }
    return result;
}

std::string XlsxReader::colIndexToName(int idx) {
    std::string result;
    idx++;
    while (idx > 0) {
        idx--;
        result = char('A' + (idx % 26)) + result;
        idx /= 26;
    }
    return result;
}

void XlsxReader::parseCellRef(const std::string& ref, std::string& colName, int& row) {
    size_t i = 0;
    while (i < ref.size() && std::isalpha(static_cast<unsigned char>(ref[i]))) i++;
    colName = ref.substr(0, i);
    row = std::stoi(ref.substr(i));
}

void XlsxReader::parseSharedStrings(const char* data, size_t size) {
    pugi::xml_document doc;
    if (!doc.load_buffer(data, size)) return;

    auto root = doc.first_child();
    for (auto si : root.children("si")) {
        std::string text;
        auto t = si.child("t");
        if (t) {
            text = t.text().as_string();
        } else {
            for (auto r : si.children("r")) {
                auto rt = r.child("t");
                if (rt) {
                    text += rt.text().as_string();
                }
            }
        }
        sharedStrings.push_back(text);
    }
}

void XlsxReader::parseWorkbook(const char* data, size_t size) {
    pugi::xml_document doc;
    if (!doc.load_buffer(data, size)) return;

    auto root = doc.first_child();
    auto sheetsElem = root.child("sheets");
    if (!sheetsElem) return;

    sheetNamesFromWorkbook.clear();
    for (auto sheet : sheetsElem.children("sheet")) {
        const char* name = sheet.attribute("name").as_string(nullptr);
        if (name) {
            sheetNamesFromWorkbook.push_back(name);
        }
    }
}

void XlsxReader::parseSheet(const char* data, size_t size, const std::string& sheetName) {
    pugi::xml_document doc;
    if (!doc.load_buffer(data, size)) return;

    SheetData sheetData;
    sheetData.name = sheetName;

    auto root = doc.first_child();
    auto sheetDataElem = root.child("sheetData");
    if (!sheetDataElem) return;

    bool firstRow = true;

    for (auto rowElem : sheetDataElem.children("row")) {
        int rowNum = rowElem.attribute("r").as_int(0);
        if (rowNum <= 0) continue;

        for (auto c : rowElem.children("c")) {
            const char* ref = c.attribute("r").as_string(nullptr);
            if (!ref) continue;

            std::string colName;
            int row;
            parseCellRef(ref, colName, row);

            const char* type = c.attribute("t").as_string(nullptr);
            std::string value;

            auto v = c.child("v");
            if (v) {
                const char* vtext = v.text().as_string();
                if (type && std::strcmp(type, "s") == 0 && vtext) {
                    int sIdx = std::stoi(vtext);
                    if (sIdx >= 0 && sIdx < (int)sharedStrings.size()) {
                        value = sharedStrings[sIdx];
                    }
                } else if (type && std::strcmp(type, "str") == 0) {
                    value = vtext ? vtext : "";
                } else if (type && std::strcmp(type, "inlineStr") == 0) {
                    auto is_elem = c.child("is");
                    if (is_elem) {
                        auto t_elem = is_elem.child("t");
                        if (t_elem) {
                            value = t_elem.text().as_string();
                        }
                    }
                } else {
                    value = vtext ? vtext : "";
                }
            } else {
                auto is_elem = c.child("is");
                if (is_elem) {
                    auto t_elem = is_elem.child("t");
                    if (t_elem) {
                        value = t_elem.text().as_string();
                    }
                }
            }

            int colIdx = colNameToIndex(colName);
            if (!value.empty()) {
                sheetData.rows[row][colIdx] = value;
            }

            if (firstRow && colIdx >= 0) {
                while ((int)sheetData.headers.size() <= colIdx) {
                    sheetData.headers.push_back("");
                }
                if (!value.empty()) {
                    sheetData.headers[colIdx] = value;
                }
            }
        }
        firstRow = false;
    }

    sheets.push_back(std::move(sheetData));
}

bool XlsxReader::open(const std::string& filepath) {
    filename = filepath;
    sheets.clear();
    sharedStrings.clear();
    sheetNamesFromWorkbook.clear();
    sheetIndex = 0;

    // V0.3.1：统一经 core 跨平台文件 IO 读整个文件到内存，再交给 miniz 解 zip（去掉 _wfopen Win32 分支）
    std::vector<uint8_t> fileBuf;
    if (!core::ReadFileBytes(filepath, fileBuf)) return false;

    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, fileBuf.data(), fileBuf.size(), 0)) {
        return false;
    }

    int fileCount = (int)mz_zip_reader_get_num_files(&zip);

    auto readZipFile = [&](const char* name) -> std::pair<char*, size_t> {
        int idx = mz_zip_reader_locate_file(&zip, name, nullptr, 0);
        if (idx < 0) return {nullptr, 0};
        size_t sz = 0;
        void* buf = mz_zip_reader_extract_to_heap(&zip, idx, &sz, 0);
        return {(char*)buf, sz};
    };

    auto [sstData, sstSize] = readZipFile("xl/sharedStrings.xml");
    if (sstData) {
        parseSharedStrings(sstData, sstSize);
        mz_free(sstData);
    }

    auto [wbData, wbSize] = readZipFile("xl/workbook.xml");
    if (wbData) {
        parseWorkbook(wbData, wbSize);
        mz_free(wbData);
    }

    if (sheetNamesFromWorkbook.empty()) {
        for (int i = 0; i < fileCount; i++) {
            char fname[512];
            mz_zip_reader_get_filename(&zip, i, fname, sizeof(fname));
            std::string fn(fname);
            if (fn.find("xl/worksheets/sheet") != std::string::npos &&
                fn.find(".xml") != std::string::npos) {
                sheetNamesFromWorkbook.push_back("Sheet" + std::to_string(sheetNamesFromWorkbook.size() + 1));
            }
        }
    }

    for (size_t si = 0; si < sheetNamesFromWorkbook.size(); si++) {
        char sheetPath[64];
        snprintf(sheetPath, sizeof(sheetPath), "xl/worksheets/sheet%zu.xml", si + 1);
        auto [shData, shSize] = readZipFile(sheetPath);
        if (shData) {
            parseSheet(shData, shSize, sheetNamesFromWorkbook[si]);
            mz_free(shData);
        }
    }

    mz_zip_reader_end(&zip);
    return !sheets.empty();
}
