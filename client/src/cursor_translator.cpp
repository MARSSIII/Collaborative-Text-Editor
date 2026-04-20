#include "client/cursor_translator.h"

namespace collab_client::cursor_translator {

uint32_t translate(uint32_t my_pos, const collab::Operation& op) noexcept {
    if (op.is_noop()) return my_pos;

    if (op.type == collab::Operation::Type::Insert) {
        const auto len = static_cast<uint32_t>(op.text.size());
        if (op.position <= my_pos) return my_pos + len;
        return my_pos;
    }

    const uint32_t p = op.position;
    const uint32_t end = p + op.length;
    if (end <= my_pos) return my_pos - op.length;
    if (p >= my_pos) return my_pos;
    return p;
}

}
