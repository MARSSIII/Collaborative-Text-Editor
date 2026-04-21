#pragma once
#include <string>

namespace collab {

std::string generate_salt(size_t length = 16);
std::string sha256_hex(const std::string& input);
std::string hash_password(const std::string& password, const std::string& salt);
bool verify_password(const std::string& password, const std::string& salt,
                     const std::string& expected_hash);

}
