// V0.3.1「铺路」重构：core::crypto 接口的 Windows(BCrypt) 实现。
// 该文件依赖 windows.h，属 Windows 平台侧，不进入跨平台 core 库。
#include "core/crypto.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <cstring>

#pragma comment(lib, "bcrypt.lib")

namespace core::crypto {

bool Random(void* buf, size_t len) {
    return BCryptGenRandom(nullptr, (PUCHAR)buf, (ULONG)len,
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}

bool Pbkdf2Sha256(const void* pw, size_t pwLen,
                  const void* salt, size_t saltLen,
                  unsigned int iters,
                  void* outKey, size_t keyLen) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                    BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) return false;
    NTSTATUS st = BCryptDeriveKeyPBKDF2(hAlg,
        (PUCHAR)pw, (ULONG)pwLen,
        (PUCHAR)salt, (ULONG)saltLen,
        (ULONG)iters, (PUCHAR)outKey, (ULONG)keyLen, 0);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return st == 0;
}

bool AesGcmEncrypt(const uint8_t* key, size_t keyLen,
                   const uint8_t* nonce, size_t nonceLen,
                   const uint8_t* plain, size_t plainLen,
                   std::vector<uint8_t>& out) {
    out.clear();
    out.resize(plainLen + 16);
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) goto cleanup;
    if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                          (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0) goto cleanup;
    if (BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key, (ULONG)keyLen, 0) != 0) goto cleanup;
    {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO ai;
        BCRYPT_INIT_AUTH_MODE_INFO(ai);
        ai.pbNonce = (PUCHAR)nonce;
        ai.cbNonce = (ULONG)nonceLen;
        ai.pbTag = out.data() + plainLen;
        ai.cbTag = 16;
        ULONG written = 0;
        if (BCryptEncrypt(hKey, (PUCHAR)plain, (ULONG)plainLen, &ai, nullptr, 0,
                          out.data(), (ULONG)out.size(), &written, 0) == 0) ok = true;
    }
cleanup:
    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!ok) out.clear();
    return ok;
}

bool AesGcmDecrypt(const uint8_t* key, size_t keyLen,
                   const uint8_t* nonce, size_t nonceLen,
                   const uint8_t* cipher, size_t cipherLen,
                   const uint8_t* tag,
                   std::vector<uint8_t>& plain) {
    plain.resize(cipherLen);
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) goto cleanup;
    if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                          (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0) goto cleanup;
    if (BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key, (ULONG)keyLen, 0) != 0) goto cleanup;
    {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO ai;
        BCRYPT_INIT_AUTH_MODE_INFO(ai);
        ai.pbNonce = (PUCHAR)nonce;
        ai.cbNonce = (ULONG)nonceLen;
        ai.pbTag = (PUCHAR)tag;
        ai.cbTag = 16;
        ULONG written = 0;
        if (BCryptDecrypt(hKey, (PUCHAR)cipher, (ULONG)cipherLen, &ai, nullptr, 0,
                          plain.data(), (ULONG)plain.size(), &written, 0) == 0) {
            plain.resize(written);
            ok = true;
        }
    }
cleanup:
    if (hKey) BCryptDestroyKey(hKey);
    if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!ok) plain.clear();
    return ok;
}

}
#endif // _WIN32
