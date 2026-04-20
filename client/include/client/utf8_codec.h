#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>

namespace collab_client::utf8_codec {

// QString stores text as UTF-16 code units (1 QChar = 2 bytes). The collab
// protocol carries positions as UTF-8 byte offsets — src/ot.cpp applies
// operations via std::string::insert/erase, so the server's notion of
// "position" is literal byte offset. Everywhere the Qt client crosses this
// boundary it must go through the functions below.

// Returns the UTF-8 byte offset that corresponds to a given UTF-16 code-unit
// offset in `text`. O(offset).
uint32_t utf16_to_utf8_offset(const QString& text, int utf16_offset);

// Returns the UTF-16 code-unit offset that corresponds to a given UTF-8 byte
// offset in `utf8_text`. O(offset).
int utf8_to_utf16_offset(const QByteArray& utf8_text, uint32_t utf8_offset);

} // namespace collab_client::utf8_codec
