#pragma once
// V0.3.1「铺路」重构：加密后端接口（core 层只声明，平台层提供实现）。
// Windows 实现 = core 外的 crypto_win.cpp（BCrypt）；Android/未来 = OpenSSL 等对等实现。
// 只要两端按同一参数调用，xse 加密文件即可跨平台加解密（保持一致 = 特供版分发的基础）。
#include <cstdint>
#include <cstddef>
#include <vector>

namespace core::crypto {

// 密码学安全随机数填充
bool Random(void* buf, size_t len);

// PBKDF2-HMAC-SHA256 密钥派生
bool Pbkdf2Sha256(const void* pw, size_t pwLen,
                  const void* salt, size_t saltLen,
                  unsigned int iters,
                  void* outKey, size_t keyLen);

// AES-256-GCM 加密：out = 密文，末尾自动附加 16 字节 GCM tag
bool AesGcmEncrypt(const uint8_t* key, size_t keyLen,
                   const uint8_t* nonce, size_t nonceLen,
                   const uint8_t* plain, size_t plainLen,
                   std::vector<uint8_t>& out);

// AES-256-GCM 解密：cipher 为密文（不含 tag），tag 单独传入（16 字节）
bool AesGcmDecrypt(const uint8_t* key, size_t keyLen,
                   const uint8_t* nonce, size_t nonceLen,
                   const uint8_t* cipher, size_t cipherLen,
                   const uint8_t* tag,
                   std::vector<uint8_t>& plain);

}
