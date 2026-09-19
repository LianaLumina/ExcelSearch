#pragma once

#include <string>
#include <vector>
#include <map>

struct SheetData;

// .docx 读取器：解析 Word 文档中的段落文本
// 每个段落作为一行（单列），表头为"内容"——用于关键字搜索
// 依赖：miniz（zip 解压）+ pugixml（XML 解析），与 xlsx 共用
class DocxReader {
public:
    bool open(const std::string& filepath);
    const std::vector<SheetData>& getSheets() const { return sheets; }
    const std::string& getFilename() const { return filename; }

private:
    std::string filename;
    std::vector<SheetData> sheets;
};
