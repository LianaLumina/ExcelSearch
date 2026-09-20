// config.ini 敏感值加密 / 管理密码哈希的实现（V0.3.2 加固）
// ---------------------------------------------------------------------------
// 依赖：core/crypto（BCrypt 的 RNG / PBKDF2-SHA256 / AES-256-GCM）+ Windows DPAPI（crypt32）。
// 为什么用 DPAPI 而不是"把密钥写进二进制"：写进二进制等于把钥匙和锁放在一起，谁拿到 exe 都能开；
// DPAPI 把钥匙交给 Windows 用登录凭据保管，程序不需要保管任何秘密，也不需要用户输入。
// 代价：密钥与"当前 Windows 账户"绑定 → 换账户/换机器解不开（调用方按降级路径处理）。

#include "config_crypto.h"

#include "core/crypto.h"

#include <windows.h>
#include <wincrypt.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace cfgcrypto {
namespace {

constexpr int kKeyLen = 32;      // 主密钥 32 字节（AES-256）
constexpr int kNonceLen = 12;    // GCM 推荐 nonce 长度
constexpr int kTagLen = 16;      // GCM tag
constexpr int kSaltLen = 16;
constexpr int kHashLen = 32;
constexpr unsigned int kPwdIters = 200000;   // 管理密码：一次登录只算一次，取偏大的迭代数

// 密文前缀。⚠️ 取长度一律用 kPrefix.size()，不要写死数字：曾把 7 写成 8（"enc:v1:" 是 7 个字符），
// 多切一位导致 hex 解析全部失败 —— 更糟的是"错误密钥/篡改必须失败"这两个用例会**假通过**。
const std::string kPrefix = "enc:v1:";

// DPAPI 的附加熵：把这个 blob 与本程序绑定（别的程序即使拿到 blob 也不能直接拿去用）
const char kEntropy[] = "ExcelSearch/config.ini/v1";

std::vector<uint8_t> g_key;
bool g_ready = false;

std::string ToHex(const uint8_t* p, size_t n) {
    static const char* d = "0123456789abcdef";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) { s.push_back(d[p[i] >> 4]); s.push_back(d[p[i] & 0xF]); }
    return s;
}

bool FromHex(const std::string& hex, std::vector<uint8_t>* out) {
    if (hex.size() % 2 != 0) return false;
    out->clear();
    out->reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        auto nib = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        const int hi = nib(hex[i]), lo = nib(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out->push_back((uint8_t)((hi << 4) | lo));
    }
    return true;
}

// 恒定时比较（避免按前缀逐字节比较泄露信息）
bool ConstantTimeEq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}

// DPAPI 包裹 / 解包（当前用户作用域；不需要管理员权限）
bool DpapiWrap(const std::vector<uint8_t>& plain, std::vector<uint8_t>* out) {
    DATA_BLOB in{}, ent{}, res{};
    in.pbData = const_cast<BYTE*>(plain.data());
    in.cbData = (DWORD)plain.size();
    ent.pbData = (BYTE*)kEntropy;
    ent.cbData = (DWORD)(sizeof(kEntropy) - 1);
    if (!CryptProtectData(&in, L"ExcelSearch config", &ent, nullptr, nullptr, 0, &res)) return false;
    out->assign(res.pbData, res.pbData + res.cbData);
    LocalFree(res.pbData);
    return true;
}

bool DpapiUnwrap(const std::vector<uint8_t>& blob, std::vector<uint8_t>* out) {
    DATA_BLOB in{}, ent{}, res{};
    in.pbData = const_cast<BYTE*>(blob.data());
    in.cbData = (DWORD)blob.size();
    ent.pbData = (BYTE*)kEntropy;
    ent.cbData = (DWORD)(sizeof(kEntropy) - 1);
    if (!CryptUnprotectData(&in, nullptr, &ent, nullptr, nullptr, 0, &res)) return false;
    out->assign(res.pbData, res.pbData + res.cbData);
    LocalFree(res.pbData);
    return true;
}

}   // namespace

bool LoadOrCreateKey(const std::string& keyblobHex, bool* created, std::string* newBlobHex) {
    if (created) *created = false;
    g_key.clear();
    g_ready = false;

    if (keyblobHex.empty()) {
        // 首次运行：生成主密钥并包裹（keyblob 由调用方写进 [meta] keyblob）
        std::vector<uint8_t> key(kKeyLen);
        if (!core::crypto::Random(key.data(), key.size())) return false;
        std::vector<uint8_t> blob;
        if (!DpapiWrap(key, &blob)) return false;
        g_key = key;
        g_ready = true;
        if (created) *created = true;
        if (newBlobHex) *newBlobHex = ToHex(blob.data(), blob.size());
        return true;
    }

    std::vector<uint8_t> blob;
    if (!FromHex(keyblobHex, &blob)) return false;
    std::vector<uint8_t> key;
    if (!DpapiUnwrap(blob, &key)) return false;          // 换账户/换机器：调用方走降级
    if (key.size() != (size_t)kKeyLen) return false;
    g_key = key;
    g_ready = true;
    return true;
}

bool KeyReady() { return g_ready; }

bool IsProtected(const std::string& stored) {
    return stored.compare(0, kPrefix.size(), kPrefix) == 0;
}

std::string Protect(const std::string& plain) {
    if (!g_ready) return std::string();
    std::vector<uint8_t> nonce(kNonceLen);
    if (!core::crypto::Random(nonce.data(), nonce.size())) return std::string();
    std::vector<uint8_t> body;   // AesGcmEncrypt 的输出 = 密文 || tag(16)
    if (!core::crypto::AesGcmEncrypt(g_key.data(), g_key.size(),
                                     nonce.data(), nonce.size(),
                                     (const uint8_t*)plain.data(), plain.size(), body)) {
        return std::string();
    }
    std::vector<uint8_t> all;
    all.reserve(nonce.size() + body.size());
    all.insert(all.end(), nonce.begin(), nonce.end());
    all.insert(all.end(), body.begin(), body.end());
    return "enc:v1:" + ToHex(all.data(), all.size());
}

bool Unprotect(const std::string& stored, std::string* out, bool* wasProtected) {
    if (wasProtected) *wasProtected = false;
    if (!IsProtected(stored)) {                 // 旧版明文：原样返回（兼容）
        if (out) *out = stored;
        return true;
    }
    if (wasProtected) *wasProtected = true;
    if (out) out->clear();
    if (!g_ready) return false;                 // 密钥没装载（换账户等）→ 调用方保留原值

    std::vector<uint8_t> all;
    if (!FromHex(stored.substr(kPrefix.size()), &all)) return false;
    if (all.size() < (size_t)(kNonceLen + kTagLen)) return false;
    const size_t bodyLen = all.size() - kNonceLen;
    const uint8_t* nonce = all.data();
    const uint8_t* cipher = all.data() + kNonceLen;
    const size_t cipherLen = bodyLen - kTagLen;
    const uint8_t* tag = all.data() + kNonceLen + cipherLen;
    std::vector<uint8_t> plain;
    if (!core::crypto::AesGcmDecrypt(g_key.data(), g_key.size(), nonce, kNonceLen,
                                     cipher, cipherLen, tag, plain)) {
        return false;                           // GCM 认证失败：密文被改过，或密钥不对
    }
    if (out) out->assign(plain.begin(), plain.end());
    return true;
}

std::string HashPassword(const std::string& pw) {
    std::vector<uint8_t> salt(kSaltLen);
    if (!core::crypto::Random(salt.data(), salt.size())) return std::string();
    std::vector<uint8_t> hash(kHashLen);
    if (!core::crypto::Pbkdf2Sha256(pw.data(), pw.size(), salt.data(), salt.size(),
                                    kPwdIters, hash.data(), hash.size())) {
        return std::string();
    }
    return "pbkdf2$" + std::to_string(kPwdIters) + "$" + ToHex(salt.data(), salt.size())
           + "$" + ToHex(hash.data(), hash.size());
}

bool VerifyPassword(const std::string& pw, const std::string& stored, bool* legacyPlainOut) {
    if (legacyPlainOut) *legacyPlainOut = false;
    static const std::string kPre = "pbkdf2$";
    if (stored.compare(0, kPre.size(), kPre) != 0) {
        // 旧版明文（或空）：按明文比较，由调用方在下次保存时迁移成哈希
        if (legacyPlainOut) *legacyPlainOut = true;
        return ConstantTimeEq(pw, stored);
    }
    const size_t p1 = stored.find('$', kPre.size());
    if (p1 == std::string::npos) return false;
    const size_t p2 = stored.find('$', p1 + 1);
    if (p2 == std::string::npos) return false;
    unsigned int iters = 0;
    try { iters = (unsigned int)std::stoul(stored.substr(kPre.size(), p1 - kPre.size())); }
    catch (...) { return false; }
    if (iters == 0) return false;
    std::vector<uint8_t> salt, want;
    if (!FromHex(stored.substr(p1 + 1, p2 - p1 - 1), &salt)) return false;
    if (!FromHex(stored.substr(p2 + 1), &want)) return false;
    if (salt.empty() || want.empty()) return false;
    std::vector<uint8_t> got(want.size());
    if (!core::crypto::Pbkdf2Sha256(pw.data(), pw.size(), salt.data(), salt.size(),
                                    iters, got.data(), got.size())) {
        return false;
    }
    return ConstantTimeEq(std::string((const char*)got.data(), got.size()),
                          std::string((const char*)want.data(), want.size()));
}

void ResetKeyForTest() {
    g_key.clear();
    g_ready = false;
}

}   // namespace cfgcrypto
