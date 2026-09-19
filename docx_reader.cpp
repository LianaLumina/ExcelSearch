#include "docx_reader.h"
#include "xlsx_reader.h"   // 复用 SheetData 定义
#include "miniz.h"
#include "pugixml.hpp"

#include <cstring>
#include <cstdio>

#include "core/file_io.h"

bool DocxReader::open(const std::string& filepath) {
    filename = filepath;
    sheets.clear();

    std::vector<uint8_t> fileData;
    if (!core::ReadFileBytes(filepath, fileData)) return false;

    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, fileData.data(), fileData.size(), 0)) return false;

    // 定位 word/document.xml（兼容正斜杠/反斜杠分隔符）
    int idx = -1;
    {
        int fileCount = (int)mz_zip_reader_get_num_files(&zip);
        for (int i = 0; i < fileCount; i++) {
            char fname[512];
            mz_zip_reader_get_filename(&zip, i, fname, sizeof(fname));
            std::string fn(fname);
            std::string normalized;
            for (char c : fn) normalized += (c == '\\') ? '/' : c;
            if (normalized == "word/document.xml") { idx = i; break; }
        }
    }
    if (idx < 0) {
        // 回退：扫描任何 document*.xml
        int fileCount = (int)mz_zip_reader_get_num_files(&zip);
        for (int i = 0; i < fileCount; i++) {
            char fname[256];
            mz_zip_reader_get_filename(&zip, i, fname, sizeof(fname));
            std::string fn(fname);
            if (fn.find("document") != std::string::npos && fn.find(".xml") != std::string::npos) {
                idx = i;
                break;
            }
        }
        if (idx < 0) { mz_zip_reader_end(&zip); return false; }
    }

    size_t docSize = 0;
    void* docBuf = mz_zip_reader_extract_to_heap(&zip, idx, &docSize, 0);
    mz_zip_reader_end(&zip);
    if (!docBuf || docSize == 0) {
        if (docBuf) mz_free(docBuf);
        return false;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result pr = doc.load_buffer(docBuf, docSize);
    if (!pr) {
        mz_free(docBuf);
        return false;
    }
    mz_free(docBuf);

    SheetData sd;
    sd.name = "Word";
    sd.headers = { "内容" };
    int rowNum = 1;
    bool any = false;

    // 遍历 document 下的 body 中的段落 <w:p>
    // 注意：带命名空间前缀时 pugixml 的 name() 为完整名（如 "w:body"、"w:p"、"w:t"）
    auto root = doc.first_child();
    auto body = root.child("w:body");
    if (!body) body = root.child("body");   // 兼容无前缀情况
    pugi::xml_node node = body ? body.first_child() : root.first_child();

    for (; node; node = node.next_sibling()) {
        if (std::strcmp(node.name(), "w:p") != 0) continue;

        std::string text;
        // 段落内所有 <w:t> 文本拼接：w:t 可能在 <w:r><w:t> 嵌套，或 <w:t> 直接子节点
        for (auto r : node.children("w:r")) {
            for (auto t : r.children("w:t")) {
                text += t.text().as_string();
            }
        }
        for (auto t : node.children("w:t")) {
            text += t.text().as_string();
        }
        if (text.empty()) continue;   // 空段落跳过

        sd.rows[rowNum][0] = text;
        rowNum++;
        any = true;
    }

    if (!any) return false;
    sheets.push_back(std::move(sd));
    return true;
}
