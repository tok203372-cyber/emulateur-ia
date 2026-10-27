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
#include <imgui.h>

#include <cstring>
#include <vector>

namespace aurora {
namespace {
constexpr Module Log{"aurora::3ds"};

std::vector<AuroraEvent> g_events;
bool g_initialFrame = false;
AuroraStats g_stats{};
uint32_t g_images = 0;
u64 g_debutMesure = 0;

// Il faut annoncer un moteur de rendu « réel » : avec BACKEND_NULL, le jeu n'afficherait que
// les menus. Le nom n'a pas d'importance (il n'apparaît que dans le journal).
constexpr AuroraBackend kBackends[] = {BACKEND_OPENGLES};

const char* copie(const char* texte, const char* defaut) { return strdup(texte != nullptr ? texte : defaut); }

// ImGui : les fenêtres de débogage de Dusklight ne sont pas affichées sur la 3DS, mais le jeu
// appelle ImGui à chaque image (console des outils, souris, caméra libre) : sans contexte ImGui
// ni image ImGui en cours, ces appels liraient l'adresse 0 (plantage sur la console). On crée
// donc un contexte avec la police intégrée, et une image ImGui par image du jeu, jamais dessinée.
void initialiserImGui() {
  if (ImGui::GetCurrentContext() != nullptr) return;
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.DisplaySize = ImVec2(400.f, 240.f);
  io.DeltaTime = 1.f / 30.f;
  io.Fonts->AddFontDefault();
  unsigned char* pixels = nullptr;
  int largeur = 0;
  int hauteur = 0;
  io.Fonts->GetTexDataAsAlpha8(&pixels, &largeur, &hauteur);
  io.Fonts->SetTexID(static_cast<ImTextureID>(1));
}
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
  g_config.userPath = copie(config->userPath, "/zelda-3ds/");
  g_config.cachePath = copie(config->cachePath, "/zelda-3ds/");
  g_config.resourcesPath = copie(config->resourcesPath, "/zelda-3ds/");
  Log.info("Aurora (3DS) : MEM1 {} Mo, ARAM {} Mo", g_config.mem1Size >> 20, g_config.mem2Size >> 20);

  window::initialize();
  gx3ds::initialiser();
  gx::fifo::init();
  initialiserImGui();

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
  ImGuiIO& io = ImGui::GetIO();
  io.DeltaTime = 1.f / 30.f;
  io.DisplaySize = ImVec2(400.f, 240.f);
  ImGui::NewFrame();
  return true;
}

void aurora_end_frame() {
  ImGui::Render();  // termine l'image ImGui (rien n'est affiché)
  gx::fifo::drain();
  gx::fifo::end_frame();
  gx3ds::fin_image();
  const auto& st = gx3ds::stats();
  g_stats.drawCallCount = st.dessins;
  g_stats.lastTextureUploadSize = st.octetsTextures;

  // Toutes les 300 images : vitesse et charge du rendu dans le journal (écran du bas et fichier)
  const u64 maintenant = svcGetSystemTick();
  if (g_debutMesure == 0) g_debutMesure = maintenant;
  if (++g_images % 300 == 0) {
    const double secondes = static_cast<double>(maintenant - g_debutMesure) / (CPU_TICKS_PER_MSEC * 1000.0);
    Log.info("{} images/s ; derniere image : {} dessins, {} sommets, textures {} Ko, commandes GPU {:.0f} %, "
             "ignores {}",
             secondes > 0 ? static_cast<int>(300.0 / secondes + 0.5) : 0, st.dessins, st.sommets,
             st.octetsTextures / 1024, st.commandes * 100.f, st.ignores);
    g_debutMesure = maintenant;
  }
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
