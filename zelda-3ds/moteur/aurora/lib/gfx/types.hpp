#pragma once
// Version 3DS : types graphiques d'Aurora sans WebGPU.

#include "../internal.hpp"

#include <aurora/math.hpp>
#include <dolphin/gx/GXEnum.h>

#include <cstdint>
#include <cstring>
#include <memory>

namespace aurora::gfx {
struct Range {
  uint32_t offset = 0;
  uint32_t size = 0;

  bool operator==(const Range& rhs) const { return offset == rhs.offset && size == rhs.size; }
};

struct ClipRect {
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;

  bool operator==(const ClipRect& rhs) const { return memcmp(this, &rhs, sizeof(*this)) == 0; }
  bool operator!=(const ClipRect& rhs) const { return !(*this == rhs); }
};

struct Viewport {
  float left;
  float top;
  float width;
  float height;
  float znear;
  float zfar;

  bool operator==(const Viewport& rhs) const {
    return left == rhs.left && top == rhs.top && width == rhs.width && height == rhs.height && znear == rhs.znear &&
           zfar == rhs.zfar;
  }
  bool operator!=(const Viewport& rhs) const { return !(*this == rhs); }
};

// Texture côté 3DS (définie par le moteur gx3ds)
struct TextureRef;
using TextureHandle = std::shared_ptr<TextureRef>;
} // namespace aurora::gfx
