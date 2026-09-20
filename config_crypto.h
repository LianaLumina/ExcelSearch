#pragma once
// config.ini 敏感值加密 / 管理密码哈希（V0.3.2 加固；设计与决策见 CONFIG-HARDENING-PLAN.md）
// ---------------------------------------------------------------------------
// 改这里之前先读这五条：
//   1) **密钥**：随机 32 字节主密钥，用 Windows DPAPI（当前用户作用域）包裹后以 hex 存进
//      config.ini 的 [meta] keyblob。密钥由 Windows 用登录凭据派生，**不落进二进制**、
//      也不需要用户输入 → 满足"静默独立运行"。绿色版分发不受影响（无需安装器/管理员/注册表预置）。
//   2) **数据**：AES-256-GCM（复用 core/crypto 的 BCrypt 实现），值格式 `enc:v1:<hex(nonce|tag|cipher)>`。
//      GCM 是认证加密：密文被改一个字节就解不开 → 篡改会被拒绝，不会静默出错值。
//   3) **管理密码**：程序只需要"校验"、从不需要读回，所以存 PBKDF2-SHA256 哈希
//      （20 万次迭代 + 16 字节随机盐），格式 `pbkdf2$<iters>$<saltHex>$<hashHex>`。
//      旧版明文配置能自动识别并迁移。
//   4) **安全上限（必须如实告知，别给虚假安全感）**：程序要静默解密，就必须自己拿得到密钥 →
//      任何能在本机运行本程序、或读到这些文件的人，都能把配置解回明文。
//      本功能防的是"顺手翻看 / 配置被拷走 / 备份与云盘同步外泄 / 误传文件"，
//      **不防**有决心的本地攻击者，也不防账户已被控制的情形。
//   5) **失败路径**：换 Windows 账户、换机器、管理员重置账户密码之后，DPAPI 会解不开。
//      调用方必须"保留原值不覆盖 + 明确提示 + 该项退回默认"，**绝不静默清空用户数据**。

#include <string>

namespace cfgcrypto {

// 主密钥装载：调用方从 config.ini 读到 keyblob（hex）后传进来。
//   keyblobHex 为空         → 生成新主密钥，并把包裹后的 keyblob 通过 newBlobHex 回填（created=true）
//   keyblobHex 非空且能解开 → 装载成功（created=false）
//   返回 false              → DPAPI 解包失败（换账户/换机器/profile 重置），调用方走降级路径
bool LoadOrCreateKey(const std::string& keyblobHex, bool* created, std::string* newBlobHex);

// 当前会话是否已装载主密钥
bool KeyReady();

// 值是否已加密（形如 enc:v1:...）
bool IsProtected(const std::string& stored);

// 加密一个值；失败返回空串（调用方此时应放弃写该键，而不是写明文）
std::string Protect(const std::string& plain);

// 解密：stored 是明文时原样返回（兼容旧配置）。
//   wasProtected 输出"它原本是不是密文"，供调用方统计解密失败数
//   返回值 false 表示"它本是密文但解不开"——调用方必须保留原值、提示、退回默认
bool Unprotect(const std::string& stored, std::string* out, bool* wasProtected);

// 管理密码：哈希与校验（legacyPlainOut=true 表示 stored 是旧版明文，校验策略相同）
std::string HashPassword(const std::string& pw);
bool VerifyPassword(const std::string& pw, const std::string& stored, bool* legacyPlainOut);

// 仅供 --cfgselftest：清掉当前会话密钥，便于测"错误密钥必须解不开"
void ResetKeyForTest();

}   // namespace cfgcrypto
