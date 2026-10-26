#pragma once
// Version 3DS : le cache des textures est géré par gx3ds.
#include "gx.hpp"

#include <cstdint>

namespace aurora::gx::texture {
inline void invalidate_bindings() noexcept {}
inline uint64_t current_bind_generation() noexcept { return 0; }
inline void end_frame() noexcept {}
inline void shutdown() noexcept {}
} // namespace aurora::gx::texture
