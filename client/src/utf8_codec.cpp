#include "client/utf8_codec.h"

namespace collab_client::utf8_codec {

uint32_t utf16_to_utf8_offset(const QString& text, int utf16_offset) {
    if (utf16_offset <= 0) return 0;
    if (utf16_offset >= text.size()) return static_cast<uint32_t>(text.toUtf8().size());
    return static_cast<uint32_t>(text.left(utf16_offset).toUtf8().size());
}

int utf8_to_utf16_offset(const QByteArray& utf8_text, uint32_t utf8_offset) {
    if (utf8_offset == 0) return 0;
    if (utf8_offset >= static_cast<uint32_t>(utf8_text.size())) {
        return QString::fromUtf8(utf8_text).size();
    }
    return QString::fromUtf8(utf8_text.left(static_cast<int>(utf8_offset))).size();
}

}
