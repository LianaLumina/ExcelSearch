#pragma once

#include <string>
#include <vector>
#include <map>

struct SheetData;

// CSV 读取器：将 .csv 文件解析为 SheetData（单工作表，第一行为表头）
// 支持 UTF-8（含 BOM）、逗号分隔、引号包裹（"" 转义）、CRLF/LF 行尾
class CsvReader {
public:
    bool open(const std::string& filepath);
    const std::vector<SheetData>& getSheets() const { return sheets; }
    const std::string& getFilename() const { return filename; }

private:
    std::string filename;
    std::vector<SheetData> sheets;
};
