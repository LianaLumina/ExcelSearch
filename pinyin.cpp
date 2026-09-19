#include "pinyin.h"

#include <cstring>
#include <cstdint>

namespace pinyin {

// ============ 拼音首字母映射表（自动生成，GB2312 一级字库 3755 字） ============
#include "pinyin_table.inc"

static char initialOf(uint32_t cp) {
    // 二分查找（表按码点升序，每个条目 lo==hi 单字）
    size_t lo = 0, hi = kUniCount;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (kUniTable[mid].lo > cp) hi = mid;
        else if (kUniTable[mid].hi < cp) lo = mid + 1;
        else return kUniTable[mid].ch;
    }
    return 0;   // 表中未收录（如二级字库/生僻字），调用方保留原字符
}

std::string initials(const std::string& utf8str) {
    std::string out;
    const unsigned char* p = (const unsigned char*)utf8str.data();
    const unsigned char* end = p + utf8str.size();
    while (p < end) {
        unsigned char c = *p;
        if (c < 0x80) {
            // ASCII：转小写保留
            char ch = (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : (char)c;
            out += ch;
            p++;
        } else if ((c & 0xE0) == 0xC0) {
            p += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3 字节 UTF-8
            if (p + 2 < end) {
                uint32_t cp = ((uint32_t)(c & 0x0F) << 12) |
                              (((uint32_t)p[1] & 0x3F) << 6) |
                              ((uint32_t)p[2] & 0x3F);
                char ini = initialOf(cp);
                if (ini) out += ini;
            }
            p += 3;
        } else {
            p += 4;
        }
    }
    return out;
}

}
