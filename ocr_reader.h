#pragma once

#include <string>
#include <vector>

struct SheetData;

// 图片 OCR 读取器：使用 Windows.Media.Ocr（Win10+ 系统内置，支持中文）
// 将图片（png/jpg/bmp）识别为文本，每个识别行作为一行数据
// 依赖：C++/WinRT（VS 自带头文件）+ Windows SDK，运行时使用系统 OCR 引擎（零打包）
class OcrReader {
public:
    // 检查本机是否支持 OCR（用户配置文件语言含可识别语言）
    static bool isAvailable();

    bool open(const std::string& filepath);
    const std::vector<SheetData>& getSheets() const { return sheets; }
    const std::string& getFilename() const { return filename; }

private:
    std::string filename;
    std::vector<SheetData> sheets;
};
