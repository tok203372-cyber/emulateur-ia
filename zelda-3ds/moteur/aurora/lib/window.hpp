#pragma once
// Version 3DS : pas de fenêtre ni de SDL ; l'image est affichée sur l'écran du haut.
#include <aurora/aurora.h>

#include <cstdint>

namespace aurora::window {
AuroraWindowSize get_window_size();
void set_title(const char* title);
void set_fullscreen(bool fullscreen);
bool get_fullscreen();
void set_window_size(uint32_t width, uint32_t height);
void set_window_position(uint32_t x, uint32_t y);
void center_window();
void request_frame_buffer_resize();
void set_frame_buffer_scale(float scale);
void set_frame_buffer_aspect_fit(bool fit);
void set_background_input(bool value);
bool is_paused() noexcept;
bool is_presentable() noexcept;
} // namespace aurora::window
