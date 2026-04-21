#pragma once
#include <cstdint>
#include <string>
#include <format>

namespace collab {

struct Operation {
    enum class Type { Insert, Delete };

    Type type;
    uint32_t position;
    std::string text;
    uint32_t length;
    uint32_t userId;
    uint32_t revision;

    bool is_noop() const noexcept;
    bool operator==(const Operation&) const = default;
    std::string to_string() const;
};

Operation make_insert(uint32_t pos, const std::string& text, uint32_t userId, uint32_t rev);
Operation make_delete(uint32_t pos, uint32_t length, const std::string& text, uint32_t userId, uint32_t rev);

}
