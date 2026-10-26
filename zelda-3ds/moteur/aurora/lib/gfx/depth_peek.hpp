#pragma once
// Version 3DS : lecture de la profondeur (GXPeekZ) non disponible.
#include <cstdint>

namespace aurora::gfx::depth_peek {
inline bool read_latest(uint16_t, uint16_t, uint32_t& value) noexcept {
  value = 0;
  return false;
}
inline void request_snapshot() noexcept {}
} // namespace aurora::gfx::depth_peek
