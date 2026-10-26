// Version 3DS des fonctions de fenêtre d'Aurora : l'écran du haut fait 400x240.
#include "window.hpp"

#include <gx3ds/gx3ds.hpp>

namespace aurora::window {
AuroraWindowSize get_window_size() {
  float x, y, l, h;
  gx3ds::zone_ecran(x, y, l, h);
  AuroraWindowSize s{};
  s.width = 400;
  s.height = 240;
  s.fb_width = static_cast<uint32_t>(l);
  s.fb_height = static_cast<uint32_t>(h);
  s.native_fb_width = 400;
  s.native_fb_height = 240;
  s.scale = 1.f;
  return s;
}
void set_title(const char*) {}
void set_fullscreen(bool) {}
bool get_fullscreen() { return true; }
void set_window_size(uint32_t, uint32_t) {}
void set_window_position(uint32_t, uint32_t) {}
void center_window() {}
void request_frame_buffer_resize() {}
void set_frame_buffer_scale(float) {}
void set_frame_buffer_aspect_fit(bool) {}
void set_background_input(bool) {}
bool is_paused() noexcept { return false; }
bool is_presentable() noexcept { return true; }
} // namespace aurora::window
