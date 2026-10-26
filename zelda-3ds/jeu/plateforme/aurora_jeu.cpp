// Version 3DS du point d'entrée d'Aurora pour le jeu (lib/aurora.cpp et lib/window.cpp d'origine).
//
// SDL 3 sert aux événements (manette, son), pas à l'affichage : l'image est dessinée par gx3ds sur
// l'écran du haut. Le menu HOME est géré par aptMainLoop() (libctru).
#include "lib/input.hpp"
#include "lib/internal.hpp"
#include "lib/window.hpp"
#include "lib/gx/fifo.hpp"
#include "lib/gx/gx.hpp"

#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/gfx.h>
#include <aurora/time.hpp>
#include <gx3ds/gx3ds.hpp>

#include <SDL3/SDL.h>
#include <3ds.h>

#include <cstring>
#include <vector>

namespace aurora {
namespace {
constexpr Module Log{"aurora::3ds"};

std::vector<AuroraEvent> g_events;
bool g_initialFrame = false;
AuroraStats g_stats{};

// Il faut annoncer un moteur de rendu « réel » : avec BACKEND_NULL, le jeu n'afficherait que
// les menus. Le nom n'a pas d'importance (il n'apparaît que dans le journal).
constexpr AuroraBackend kBackends[] = {BACKEND_OPENGLES};

const char* copie(const char* texte, const char* defaut) { return strdup(texte != nullptr ? texte : defaut); }
} // namespace

namespace window {

bool initialize() {
  if (!SDL_InitSubSystem(SDL_INIT_EVENTS)) {
    Log.error("SDL_InitSubSystem(EVENTS) : {}", SDL_GetError());
    return false;
  }
  return true;
}

bool initialize_event_watch() { return true; }
bool push_custom_event(CustomEvent) { return true; }
void shutdown() { SDL_QuitSubSystem(SDL_INIT_EVENTS); }
bool is_paused() noexcept { return false; }

const AuroraEvent* poll_events() {
  g_events.clear();
  // Menu HOME : aptMainLoop() met le jeu en pause tant que le menu est ouvert, et renvoie false
  // quand il faut quitter (bouton « Fermer » du menu HOME, extinction).
  if (!aptMainLoop()) {
    g_events.push_back(AuroraEvent{.type = AURORA_EXIT});
  }
  SDL_Event event;
  input::set_mouse_scroll(0, 0);
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_EVENT_GAMEPAD_ADDED: {
      const auto instance = input::add_controller(event.gdevice.which);
      g_events.push_back(AuroraEvent{.type = AURORA_CONTROLLER_ADDED, .controller = instance});
      break;
    }
    case SDL_EVENT_GAMEPAD_REMOVED:
      input::remove_controller(event.gdevice.which);
      g_events.push_back(AuroraEvent{.type = AURORA_CONTROLLER_REMOVED, .controller = event.gdevice.which});
      break;
    case SDL_EVENT_QUIT:
      g_events.push_back(AuroraEvent{.type = AURORA_EXIT});
      break;
    default:
      break;
    }
    AuroraEvent e{};
    e.type = AURORA_SDL_EVENT;
    e.sdl = event;
    g_events.push_back(e);
  }
  g_events.push_back(AuroraEvent{.type = AURORA_NONE});
  return g_events.data();
}

} // namespace window
} // namespace aurora

using namespace aurora;

extern "C" {

AuroraInfo aurora_initialize(int argc, char* argv[], const AuroraConfig* config) {
  (void)argc;
  (void)argv;
  g_config = *config;
  g_config.appName = copie(config->appName, "Aurora");
  g_config.userPath = copie(config->userPath, "sdmc:/zelda-3ds/");
  g_config.cachePath = copie(config->cachePath, "sdmc:/zelda-3ds/");
  g_config.resourcesPath = copie(config->resourcesPath, "sdmc:/zelda-3ds/");
  Log.info("Aurora (3DS) : MEM1 {} Mo, ARAM {} Mo", g_config.mem1Size >> 20, g_config.mem2Size >> 20);

  window::initialize();
  gx3ds::initialiser();
  gx::fifo::init();

  g_initialFrame = true;
  g_config.desiredBackend = kBackends[0];
  return AuroraInfo{
      .backend = g_config.desiredBackend,
      .userPath = g_config.userPath,
      .cachePath = g_config.cachePath,
      .window = nullptr,
      .windowSize = window::get_window_size(),
  };
}

void aurora_shutdown() {
  gx::fifo::shutdown();
  input::shutdown();
  gx3ds::arreter();
  window::shutdown();
}

const AuroraEvent* aurora_update() {
  if (g_initialFrame) {
    g_initialFrame = false;
    input::initialize();
  }
  gx::update();
  return window::poll_events();
}

bool aurora_begin_frame() {
  gx3ds::debut_image();
  gx::fifo::begin_frame();
  return true;
}

void aurora_end_frame() {
  gx::fifo::drain();
  gx::fifo::end_frame();
  gx3ds::fin_image();
  const auto& st = gx3ds::stats();
  g_stats.drawCallCount = st.dessins;
  g_stats.lastTextureUploadSize = st.octetsTextures;
}

AuroraBackend aurora_get_backend() { return g_config.desiredBackend; }

const AuroraBackend* aurora_get_available_backends(size_t* count) {
  if (count != nullptr) *count = 1;
  return kBackends;
}

void aurora_set_log_level(AuroraLogLevel level) { g_config.logLevel = level; }
void aurora_set_pause_on_focus_lost(bool value) { g_config.pauseOnFocusLost = value; }
void aurora_set_background_input(bool value) { g_config.allowJoystickBackgroundEvents = value; }
void aurora_set_resampler(AuroraSampler) {}
void aurora_set_timescale(float scale) { time::set_scale(scale); }
float aurora_get_timescale() { return time::scale(); }

const AuroraStats* aurora_get_stats() { return &g_stats; }
float aurora_get_fps() { return 60.f; }
void aurora_enable_vsync(bool) {}
void push_debug_group(const char*) {}
void pop_debug_group() {}

} // extern "C"
