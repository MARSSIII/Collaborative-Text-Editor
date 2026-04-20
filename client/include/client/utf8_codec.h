#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>

namespace collab_client::utf8_codec {

uint32_t utf16_to_utf8_offset(const QString& text, int utf16_offset);
int utf8_to_utf16_offset(const QByteArray& utf8_text, uint32_t utf8_offset);

} // namespace collab_client::utf8_codec
