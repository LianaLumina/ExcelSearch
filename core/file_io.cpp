#include "file_io.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace core {

bool ReadFileBytes(const std::string& utf8path, std::vector<uint8_t>& out) {
    std::ifstream f(fs::u8path(utf8path), std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    std::streamoff len = f.tellg();
    if (len < 0) return false;
    f.seekg(0, std::ios::beg);
    out.assign((size_t)len, 0);
    if (len > 0 && !f.read(reinterpret_cast<char*>(out.data()), len)) return false;
    return true;
}

bool WriteFileBytes(const std::string& utf8path, const void* data, size_t len) {
    std::ofstream f(fs::u8path(utf8path), std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data), (std::streamsize)len);
    return (bool)f;
}

bool FileExists(const std::string& utf8path) {
    std::error_code ec;
    return fs::exists(fs::u8path(utf8path), ec);
}

}
