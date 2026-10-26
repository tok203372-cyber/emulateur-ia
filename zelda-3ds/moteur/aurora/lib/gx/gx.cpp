// Version 3DS de l'état GX d'Aurora : passage des coordonnées logiques (image GameCube de
// 640x480) aux coordonnées de l'écran du haut de la 3DS, et liens avec le moteur gx3ds.
#include "gx.hpp"

#include "../dolphin/vi/vi_internal.hpp"
#include "../internal.hpp"

#include <gx3ds/gx3ds.hpp>

#include <algorithm>
#include <cmath>

namespace aurora::gx {

GXState g_gxState{};

void initialize() noexcept {}

void shutdown() noexcept {
  g_gxState.loadedTextures.fill({});
  g_gxState.loadedTluts.fill({});
  clear_copy_texture_cache();
}

void update() noexcept {}

void set_viewport_policy(AuroraViewportPolicy policy) noexcept { g_gxState.viewportPolicy = policy; }

Vec2<uint32_t> logical_fb_size() noexcept { return vi::configured_fb_size(); }

gfx::Viewport map_logical_viewport(const gfx::Viewport& v) noexcept {
  const auto [lw, lh] = logical_fb_size();
  float zx, zy, zl, zh;
  gx3ds::zone_ecran(zx, zy, zl, zh);
  const float sx = zl / static_cast<float>(std::max<uint32_t>(lw, 1));
  const float sy = zh / static_cast<float>(std::max<uint32_t>(lh, 1));
  return {
      .left = zx + v.left * sx,
      .top = zy + v.top * sy,
      .width = v.width * sx,
      .height = v.height * sy,
      .znear = v.znear,
      .zfar = v.zfar,
  };
}

gfx::ClipRect map_logical_scissor(const gfx::ClipRect& s) noexcept {
  const auto [lw, lh] = logical_fb_size();
  float zx, zy, zl, zh;
  gx3ds::zone_ecran(zx, zy, zl, zh);
  const float sx = zl / static_cast<float>(std::max<uint32_t>(lw, 1));
  const float sy = zh / static_cast<float>(std::max<uint32_t>(lh, 1));
  const auto g = static_cast<int32_t>(std::floor(zx + static_cast<float>(s.x) * sx));
  const auto h = static_cast<int32_t>(std::floor(zy + static_cast<float>(s.y) * sy));
  const auto d = static_cast<int32_t>(std::ceil(zx + static_cast<float>(s.x + s.width) * sx));
  const auto b = static_cast<int32_t>(std::ceil(zy + static_cast<float>(s.y + s.height) * sy));
  return {g, h, std::max<int32_t>(d - g, 0), std::max<int32_t>(b - h, 0)};
}

void set_logical_viewport(const gfx::Viewport& viewport) noexcept {
  g_gxState.logicalViewport = viewport;
  set_render_viewport(map_logical_viewport(viewport));
}

void set_render_viewport(const gfx::Viewport& viewport) noexcept {
  g_gxState.renderViewport = viewport;
  g_gxState.dirty |= DirtyUniform;
}

void set_logical_scissor(const gfx::ClipRect& scissor) noexcept {
  g_gxState.logicalScissor = scissor;
  set_render_scissor(map_logical_scissor(scissor));
}

void set_render_scissor(const gfx::ClipRect& scissor) noexcept { g_gxState.renderScissor = scissor; }

void clear_static_texture_cache() noexcept { gx3ds::vider_cache_textures(); }

void clear_copy_texture_cache() noexcept {
  g_gxState.copyTextures.clear();
  g_gxState.copyTextureCache.clear();
}

void evict_copy_texture(const void* dest) noexcept {
  g_gxState.copyTextures.erase(dest);
  gx3ds::oublier_copie(dest);
}

void evict_texture_object(u32 texObjId) noexcept { gx3ds::oublier_objet_texture(texObjId); }

void evict_tlut_object(u32 tlutObjId) noexcept { gx3ds::oublier_tlut(tlutObjId); }

} // namespace aurora::gx
