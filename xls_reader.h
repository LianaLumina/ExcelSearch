#pragma once

#include <string>
#include <vector>
#include <map>

struct SheetData;

class XlsReader {
public:
    bool open(const std::string& filepath);
    const std::vector<SheetData>& getSheets() const { return sheets; }
    const std::string& getFilename() const { return filename; }

private:
    std::string filename;
    std::vector<SheetData> sheets;
};
