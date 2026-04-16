#include "collab/hash_utils.h"

#include <CommonCrypto/CommonDigest.h>
#include <cstdlib>
#include <format>

namespace collab {

std::string generate_salt(size_t length) {
    std::vector<uint8_t> buf(length);
    arc4random_buf(buf.data(), buf.size());

    std::string hex;
    hex.reserve(length * 2);
    for (uint8_t byte : buf) {
        hex += std::format("{:02x}", byte);
    }
    return hex;
}

std::string sha256_hex(const std::string& input) {
    unsigned char hash[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(input.data(), static_cast<CC_LONG>(input.size()), hash);

    std::string hex;
    hex.reserve(CC_SHA256_DIGEST_LENGTH * 2);
    for (unsigned char byte : hash) {
        hex += std::format("{:02x}", byte);
    }
    return hex;
}

std::string hash_password(const std::string& password, const std::string& salt) {
    return sha256_hex(salt + password);
}

bool verify_password(const std::string& password, const std::string& salt,
                     const std::string& expected_hash) {
    return hash_password(password, salt) == expected_hash;
}

}
