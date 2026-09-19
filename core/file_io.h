#pragma once
// V0.3.1「铺路」重构：跨平台纯文件 IO（core 层，零 Win32 依赖）。
// UTF-8 路径内部经 std::filesystem 处理 —— Windows 中文路径 / Linux / Android 均正确。
#include <string>
#include <vector>
#include <cstdint>

namespace core {

// 读整个文件为字节；失败返回 false
bool ReadFileBytes(const std::string& utf8path, std::vector<uint8_t>& out);

// 写字节到文件；失败返回 false
bool WriteFileBytes(const std::string& utf8path, const void* data, size_t len);

// 文件是否存在（异常安全）
bool FileExists(const std::string& utf8path);

}
