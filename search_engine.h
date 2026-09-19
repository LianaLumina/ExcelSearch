#pragma once

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdint>
#include "xlsx_reader.h"
#include "pinyin.h"

struct SearchResult {
    std::string filename;
    std::string sheetName;
    int row;
    int col;
    std::string colName;
    std::string matchedValue;
    std::map<int, std::string> rowCells;
    std::vector<std::string> headers;
};

class SearchEngine {
public:
    void addFile(const std::string& filename, const std::vector<SheetData>& sheets);
    void clear();

    std::vector<SearchResult> search(const std::string& keyword) const;
    // 模糊搜索：对全部条目做 rapidfuzz 相似度匹配，返回按相似度降序的结果
    std::vector<SearchResult> fuzzySearch(const std::string& keyword, double minScore) const;
    // 正则搜索：keyword 为正则表达式，匹配任意单元格
    std::vector<SearchResult> regexSearch(const std::string& pattern) const;
    static std::vector<SearchResult> filterResults(
        const std::vector<SearchResult>& results, const std::string& keyword);

    const std::vector<std::string>& getFiles() const { return loadedFiles; }
    size_t getEntryCount() const { return entries.size(); }

    const auto& getHeaders() const { return headers; }
    const auto& getFileData() const { return fileData; }

    void setFileMeta(const std::map<std::string, int64_t>& mtimes,
                     const std::map<std::string, int64_t>& sizes);
    const std::map<std::string, int64_t>& getFileMTimes() const { return fileMTimes; }
    const std::map<std::string, int64_t>& getFileSizes() const { return fileSizes; }

    bool saveToFile(const std::string& filepath) const;
    bool loadFromFile(const std::string& filepath);

private:
    struct Entry {
        std::string filename;
        std::string sheetName;
        int row;
        int col;
        std::string colName;
        std::string value;
    };

    std::vector<Entry> entries;
    std::map<std::string,
        std::map<std::string,
            std::map<int,
                std::map<int, std::string>>>> fileData;
    std::map<std::string,
        std::map<std::string,
            std::vector<std::string>>> headers;
    std::vector<std::string> loadedFiles;
    std::map<std::string, int64_t> fileMTimes;
    std::map<std::string, int64_t> fileSizes;

    static bool matchKeyword(const std::string& text, const std::string& keyword);
    static std::string toLower(const std::string& str);
};
