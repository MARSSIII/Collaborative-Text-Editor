#include "collab/operation.h"

namespace collab {

bool Operation::is_noop() const noexcept {
    if (type == Type::Insert && text.empty()) {
        return true;
    }
    if (type == Type::Delete && length == 0) {
        return true;
    }
    return false;
}

std::string Operation::to_string() const {
    if (type == Type::Insert) {
        return std::format("Insert(pos={}, text=\"{}\", uid={}, rev={})",
                           position, text, userId, revision);
    }
    return std::format("Delete(pos={}, len={}, text=\"{}\", uid={}, rev={})",
                       position, length, text, userId, revision);
}

Operation make_insert(uint32_t pos, const std::string& text, uint32_t userId, uint32_t rev) {
    return Operation{
        .type     = Operation::Type::Insert,
        .position = pos,
        .text     = text,
        .length   = static_cast<uint32_t>(text.size()),
        .userId   = userId,
        .revision = rev
    };
}

Operation make_delete(uint32_t pos, uint32_t length, const std::string& text, uint32_t userId, uint32_t rev) {
    return Operation{
        .type     = Operation::Type::Delete,
        .position = pos,
        .text     = text,
        .length   = length,
        .userId   = userId,
        .revision = rev
    };
}

}
