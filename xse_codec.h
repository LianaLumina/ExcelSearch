#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct XseFileEntry {
    std::string filename;
    std::string srcExt;
    int64_t srcMtime = 0;
    int64_t srcSize = 0;
    std::map<std::string, std::map<int, std::map<int, std::string>>> sheets;
    std::map<std::string, std::vector<std::string>> headers;
};

namespace xse {

bool encryptData(const std::vector<uint8_t>& plaintext,
                 const std::string& additionalPassword,
                 bool passwordEnabled,
                 int64_t srcMtime, int64_t srcSize, const std::string& srcExt,
                 std::vector<uint8_t>& outContainer);

bool decryptData(const std::vector<uint8_t>& container,
                 const std::string& additionalPassword,
                 std::vector<uint8_t>& outPlaintext,
                 int64_t* srcMtime, int64_t* srcSize, std::string* srcExt,
                 bool* passwordEnabledOut);

bool readHeaderMeta(const std::vector<uint8_t>& container,
                    int64_t* srcMtime, int64_t* srcSize, std::string* srcExt,
                    bool* passwordEnabledOut);

std::vector<uint8_t> serializePayload(const std::vector<XseFileEntry>& entries);
bool deserializePayload(const std::vector<uint8_t>& payload,
                        std::vector<XseFileEntry>& entries);

}
