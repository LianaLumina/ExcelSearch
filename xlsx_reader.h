#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct CellData {
    int row;
    int col;
    std::string colName;
    std::string value;
};

struct SheetData {
    std::string name;
    std::map<int, std::map<int, std::string>> rows;
    std::vector<std::string> headers;
};

class XlsxReader {
public:
    bool open(const std::string& filepath);
    const std::vector<SheetData>& getSheets() const { return sheets; }
    const std::string& getFilename() const { return filename; }

    static std::string colIndexToName(int idx);
    static int colNameToIndex(const std::string& name);
    static void parseCellRef(const std::string& ref, std::string& colName, int& row);

private:
    std::string filename;
    std::vector<SheetData> sheets;
    std::vector<std::string> sharedStrings;

    void parseSharedStrings(const char* data, size_t size);
    void parseSheet(const char* data, size_t size, const std::string& sheetName);
    void parseWorkbook(const char* data, size_t size);

    std::vector<std::string> sheetNamesFromWorkbook;
    int sheetIndex;
};
