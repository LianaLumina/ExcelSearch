#include "xse_codec.h"
#include "core/crypto.h"

#include <cstring>

// ============ Embedded master key (split into fragments, XOR assembled) ============
static const uint8_t kKeyFragA[32] = {
    0x3C, 0x91, 0xE7, 0x4A, 0xB2, 0x5F, 0x08, 0xD6,
    0x71, 0xCE, 0x2B, 0x88, 0xF4, 0x19, 0xA3, 0x6D,
    0x0E, 0xC5, 0x97, 0x52, 0xBF, 0x30, 0x84, 0xEA,
    0x1D, 0x77, 0xAC, 0x43, 0xD9, 0x62, 0xF1, 0x25,
};

static const uint8_t kKeyFragB[32] = {
    0x5F, 0xC2, 0x94, 0x31, 0xD7, 0x6A, 0x39, 0xA5,
    0x08, 0xFB, 0x4E, 0xE3, 0x87, 0x26, 0xD0, 0x1C,
    0x69, 0xB8, 0xF2, 0x27, 0xDE, 0x41, 0xE9, 0x93,
    0x74, 0x05, 0xCB, 0x30, 0xA8, 0x17, 0x86, 0x52,
};

static void getMasterKey(uint8_t out[32]) {
    for (int i = 0; i < 32; i++) out[i] = kKeyFragA[i] ^ kKeyFragB[i];
}

// 加密原语统一走 core::crypto 平台后端（Windows=BCrypt；Android/未来=OpenSSL 对等实现，保证 xse 两端一致）
static bool deriveKey(const uint8_t* masterKey, const std::string& additionalPassword,
                      const uint8_t salt[16], uint8_t outKey[32]) {
    std::vector<uint8_t> material(masterKey, masterKey + 32);
    if (!additionalPassword.empty()) {
        material.insert(material.end(), additionalPassword.begin(), additionalPassword.end());
    }
    return core::crypto::Pbkdf2Sha256(material.data(), material.size(), salt, 16, 100000, outKey, 32);
}

// ============ Header layout ============
// "XSE1"(4) ver(2) flags(1) salt(16) nonce(12) srcMtime(8) srcSize(8) srcExtLen(2) srcExt(N)
// followed by ciphertext and 16-byte GCM tag at end.

static const char kMagic[4] = { 'X', 'S', 'E', '1' };
static const uint16_t kVersion = 1;
static const uint8_t kFlagPassword = 0x01;

namespace xse {

bool encryptData(const std::vector<uint8_t>& plaintext,
                 const std::string& additionalPassword,
                 bool passwordEnabled,
                 int64_t srcMtime, int64_t srcSize, const std::string& srcExt,
                 std::vector<uint8_t>& outContainer)
{
    uint8_t masterKey[32];
    getMasterKey(masterKey);

    uint8_t salt[16], nonce[12], key[32];
    core::crypto::Random(salt, sizeof(salt));
    core::crypto::Random(nonce, sizeof(nonce));

    std::string pass = passwordEnabled ? additionalPassword : "";
    if (!deriveKey(masterKey, pass, salt, key)) return false;

    // Build header
    outContainer.clear();
    outContainer.insert(outContainer.end(), kMagic, kMagic + 4);
    outContainer.push_back((uint8_t)(kVersion & 0xFF));
    outContainer.push_back((uint8_t)(kVersion >> 8));
    outContainer.push_back(passwordEnabled ? kFlagPassword : 0);
    outContainer.insert(outContainer.end(), salt, salt + 16);
    outContainer.insert(outContainer.end(), nonce, nonce + 12);

    uint8_t tmp[8];
    memcpy(tmp, &srcMtime, 8);
    outContainer.insert(outContainer.end(), tmp, tmp + 8);
    memcpy(tmp, &srcSize, 8);
    outContainer.insert(outContainer.end(), tmp, tmp + 8);

    uint16_t extLen = (uint16_t)(srcExt.size() > 255 ? 255 : srcExt.size());
    outContainer.push_back((uint8_t)(extLen & 0xFF));
    outContainer.push_back((uint8_t)(extLen >> 8));
    outContainer.insert(outContainer.end(), srcExt.begin(), srcExt.begin() + extLen);

    // AES-256-GCM 加密（经 core::crypto 平台后端）
    std::vector<uint8_t> cipher;
    if (!core::crypto::AesGcmEncrypt(key, 32, nonce, sizeof(nonce),
                                     plaintext.data(), plaintext.size(), cipher)) return false;
    outContainer.insert(outContainer.end(), cipher.begin(), cipher.end());
    return true;
}

bool decryptData(const std::vector<uint8_t>& container,
                 const std::string& additionalPassword,
                 std::vector<uint8_t>& outPlaintext,
                 int64_t* srcMtime, int64_t* srcSize, std::string* srcExt,
                 bool* passwordEnabledOut)
{
    if (container.size() < 4 + 2 + 1 + 16 + 12 + 8 + 8 + 2 + 16) return false;
    if (memcmp(container.data(), kMagic, 4) != 0) return false;

    size_t pos = 4;
    uint16_t ver = (uint16_t)(container[pos] | (container[pos + 1] << 8));
    (void)ver;
    pos += 2;
    uint8_t flags = container[pos]; pos += 1;
    bool passwordEnabled = (flags & kFlagPassword) != 0;
    if (passwordEnabledOut) *passwordEnabledOut = passwordEnabled;

    const uint8_t* salt = container.data() + pos; pos += 16;
    const uint8_t* nonce = container.data() + pos; pos += 12;

    int64_t mtime = 0, size = 0;
    memcpy(&mtime, container.data() + pos, 8); pos += 8;
    memcpy(&size, container.data() + pos, 8); pos += 8;

    uint16_t extLen = (uint16_t)(container[pos] | (container[pos + 1] << 8)); pos += 2;
    if (pos + extLen > container.size()) return false;
    std::string ext((const char*)container.data() + pos, extLen); pos += extLen;

    if (srcMtime) *srcMtime = mtime;
    if (srcSize) *srcSize = size;
    if (srcExt) *srcExt = ext;

    size_t tagStart = container.size() - 16;
    size_t cipherLen = tagStart - pos;
    if (cipherLen <= 0 || cipherLen > container.size()) return false;
    const uint8_t* cipher = container.data() + pos;
    const uint8_t* tag = container.data() + tagStart;

    uint8_t masterKey[32];
    getMasterKey(masterKey);

    uint8_t key[32];
    std::string pass = passwordEnabled ? additionalPassword : "";
    if (!deriveKey(masterKey, pass, salt, key)) return false;

    // AES-256-GCM 解密（经 core::crypto 平台后端）
    std::vector<uint8_t> plain;
    if (!core::crypto::AesGcmDecrypt(key, 32, nonce, 12, cipher, cipherLen, tag, plain)) return false;
    outPlaintext = std::move(plain);
    return true;
}

bool readHeaderMeta(const std::vector<uint8_t>& container,
                    int64_t* srcMtime, int64_t* srcSize, std::string* srcExt,
                    bool* passwordEnabledOut)
{
    if (container.size() < 4 + 2 + 1 + 16 + 12 + 8 + 8 + 2) return false;
    if (memcmp(container.data(), kMagic, 4) != 0) return false;

    size_t pos = 6;
    uint8_t flags = container[pos]; pos += 1;
    if (passwordEnabledOut) *passwordEnabledOut = (flags & kFlagPassword) != 0;
    pos += 16 + 12;

    int64_t mtime = 0, size = 0;
    memcpy(&mtime, container.data() + pos, 8); pos += 8;
    memcpy(&size, container.data() + pos, 8); pos += 8;

    uint16_t extLen = (uint16_t)(container[pos] | (container[pos + 1] << 8)); pos += 2;
    if (pos + extLen > container.size()) return false;
    std::string ext((const char*)container.data() + pos, extLen);

    if (srcMtime) *srcMtime = mtime;
    if (srcSize) *srcSize = size;
    if (srcExt) *srcExt = ext;
    return true;
}

// ============ Unified payload serialization ============
static void wI32(std::vector<uint8_t>& b, int32_t v) {
    for (int i = 0; i < 4; i++) b.push_back((uint8_t)(((uint32_t)v >> (i * 8)) & 0xFF));
}
static void wI64(std::vector<uint8_t>& b, int64_t v) {
    for (int i = 0; i < 8; i++) b.push_back((uint8_t)(((uint64_t)v >> (i * 8)) & 0xFF));
}
static void wStr(std::vector<uint8_t>& b, const std::string& s) {
    wI32(b, (int32_t)s.size());
    b.insert(b.end(), s.begin(), s.end());
}

static bool rI32(const std::vector<uint8_t>& b, size_t& pos, int32_t& v) {
    if (pos + 4 > b.size()) return false;
    v = (int32_t)((uint32_t)b[pos] | ((uint32_t)b[pos + 1] << 8) |
                  ((uint32_t)b[pos + 2] << 16) | ((uint32_t)b[pos + 3] << 24));
    pos += 4;
    return true;
}
static bool rI64(const std::vector<uint8_t>& b, size_t& pos, int64_t& v) {
    if (pos + 8 > b.size()) return false;
    uint64_t u = 0;
    for (int i = 0; i < 8; i++) u |= ((uint64_t)b[pos + i]) << (i * 8);
    v = (int64_t)u;
    pos += 8;
    return true;
}
static bool rStr(const std::vector<uint8_t>& b, size_t& pos, std::string& s) {
    int32_t len = 0;
    if (!rI32(b, pos, len)) return false;
    if (len < 0 || len > 10 * 1024 * 1024 || pos + (size_t)len > b.size()) return false;
    s.assign((const char*)b.data() + pos, len);
    pos += len;
    return true;
}

std::vector<uint8_t> serializePayload(const std::vector<XseFileEntry>& entries) {
    std::vector<uint8_t> b;
    b.insert(b.end(), { 'X', 'S', 'P', 'D' });
    wI32(b, (int32_t)entries.size());
    for (const auto& e : entries) {
        wStr(b, e.filename);
        wStr(b, e.srcExt);
        wI64(b, e.srcMtime);
        wI64(b, e.srcSize);
        wI32(b, (int32_t)e.sheets.size());
        for (const auto& sh : e.sheets) {
            wStr(b, sh.first);
            wI32(b, (int32_t)sh.second.size());
            for (const auto& row : sh.second) {
                wI32(b, row.first);
                wI32(b, (int32_t)row.second.size());
                for (const auto& cell : row.second) {
                    wI32(b, cell.first);
                    wStr(b, cell.second);
                }
            }
            auto hIt = e.headers.find(sh.first);
            if (hIt != e.headers.end()) {
                wI32(b, (int32_t)hIt->second.size());
                for (const auto& h : hIt->second) wStr(b, h);
            } else {
                wI32(b, 0);
            }
        }
    }
    return b;
}

bool deserializePayload(const std::vector<uint8_t>& b, std::vector<XseFileEntry>& entries) {
    if (b.size() < 4 || memcmp(b.data(), "XSPD", 4) != 0) return false;
    size_t pos = 4;
    int32_t fileCount = 0;
    if (!rI32(b, pos, fileCount)) return false;
    entries.clear();
    for (int32_t fi = 0; fi < fileCount; fi++) {
        XseFileEntry e;
        if (!rStr(b, pos, e.filename)) return false;
        if (!rStr(b, pos, e.srcExt)) return false;
        if (!rI64(b, pos, e.srcMtime)) return false;
        if (!rI64(b, pos, e.srcSize)) return false;
        int32_t sheetCount = 0;
        if (!rI32(b, pos, sheetCount)) return false;
        for (int32_t si = 0; si < sheetCount; si++) {
            std::string sheetName;
            if (!rStr(b, pos, sheetName)) return false;
            int32_t rowCount = 0;
            if (!rI32(b, pos, rowCount)) return false;
            for (int32_t ri = 0; ri < rowCount; ri++) {
                int32_t rowNum = 0;
                if (!rI32(b, pos, rowNum)) return false;
                int32_t cellCount = 0;
                if (!rI32(b, pos, cellCount)) return false;
                for (int32_t ci = 0; ci < cellCount; ci++) {
                    int32_t col = 0;
                    std::string val;
                    if (!rI32(b, pos, col)) return false;
                    if (!rStr(b, pos, val)) return false;
                    e.sheets[sheetName][rowNum][col] = val;
                }
            }
            int32_t hdrCount = 0;
            if (!rI32(b, pos, hdrCount)) return false;
            for (int32_t hi = 0; hi < hdrCount; hi++) {
                std::string h;
                if (!rStr(b, pos, h)) return false;
                e.headers[sheetName].push_back(h);
            }
        }
        entries.push_back(std::move(e));
    }
    return true;
}

}
