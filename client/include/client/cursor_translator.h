#pragma once

#include "collab/operation.h"

#include <cstdint>

namespace collab_client::cursor_translator {

uint32_t translate(uint32_t my_pos, const collab::Operation& op) noexcept;

} // namespace collab_client::cursor_translator
