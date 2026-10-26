// Parties de Dusklight remplacées sur la 3DS.
//
// Sur PC, Dusklight ajoute au jeu des menus (RmlUi : écran de lancement, réglages, liste des
// mods, notifications), des services pour les mods qui dessinent avec WebGPU, un manifeste de
// symboles compressé (zstd) et des outils ImGui. Rien de cela n'existe sur la 3DS : le jeu démarre
// directement et ces fonctions ne font rien (ou répondent « absent »).
//
// Ce fichier est compilé comme les fichiers du jeu (avec l'en-tête précompilé de Dusklight).
#include "dusk/camera_operators.hpp"
#include "dusk/gfx.hpp"
#include "dusk/imgui/ImGuiMenuTools.hpp"
#include "dusk/mods/manifest.hpp"
#include "dusk/mods/svc/registry.hpp"
#include "dusk/mods/svc/ui_v1.hpp"
#include "dusk/ui/icon_provider.hpp"
#include "dusk/ui/mod_texture_provider.hpp"
#include "dusk/ui/prelaunch.hpp"
#include "dusk/ui/touch_controls.hpp"
#include "dusk/ui/ui.hpp"
#include "mods/svc/camera.h"
#include "mods/svc/gfx.h"
#include "mods/svc/ui.h"

#include <RmlUi/Core/Context.h>

#include <cstdio>
#include <string>
#include <vector>

// --- Interface RmlUi du PC ---
namespace dusk::ui {

bool initialize() noexcept { return false; }
void shutdown() noexcept {}
void handle_event(const SDL_Event&) noexcept {}
void update() noexcept {}
void apply_scale() noexcept {}

bool any_document_visible() noexcept { return false; }
bool is_prelaunch_open() noexcept { return false; }
Document* top_document() noexcept { return nullptr; }

std::string escape(std::string_view texte) noexcept { return std::string(texte); }

// Les notifications (succès, sauvegarde automatique, mods) s'affichent dans le journal de l'écran
// du bas
void push_toast(Toast toast) noexcept {
  std::printf("[%s] %s\n", toast.type.c_str(), toast.title.c_str());
}

// Écran de lancement (choix du disque, mise à jour) : remplacé par le lancement direct
PrelaunchState& prelaunch_state() noexcept {
  static PrelaunchState etat;
  return etat;
}
void return_to_prelaunch() noexcept {}
void Prelaunch::refresh_menu_buttons() {}

// Commandes tactiles (téléphones/tablettes) et icônes du HUD dessinées par RmlUi
bool get_equip_target(int, EquipTarget&) noexcept { return false; }
void set_control_override(Control, ControlOverride) noexcept {}
void sync_virtual_input() noexcept {}
void update_midna_icon_texture(J2DPane*) noexcept {}

std::string mod_image_source(const mods::LoadedMod&, std::string_view) { return {}; }

} // namespace dusk::ui

// Le contexte RmlUi n'existe jamais (aurora::rmlui::get_context() renvoie nullptr) : ces deux
// fonctions ne sont pas appelées, mais menu_pointer.cpp y fait référence.
namespace Rml {
Vector2i Context::GetDimensions() const { return {400, 240}; }
float Context::GetDensityIndependentPixelRatio() const { return 1.f; }
} // namespace Rml

// --- Services des mods qui dépendent de l'affichage du PC ---
namespace dusk::mods {

bool camera_run_operators(dCamera_c*) { return false; }
void gfx_run_stage(GfxStage, const view_class*, const view_port_class*) {}

namespace svc {
namespace {
bool indisponible() { return false; }
} // namespace

// Services déclarés « indisponibles » : un mod qui en a besoin est refusé avec un message clair
constinit const ServiceModule g_uiModule_v1{
    .id = UI_SERVICE_ID,
    .majorVersion = ui_v1::kMajorVersion,
    .minorVersion = ui_v1::kMinorVersion,
    .available = indisponible,
};
constinit const ServiceModule g_uiModule{
    .id = UI_SERVICE_ID,
    .majorVersion = UI_SERVICE_MAJOR,
    .minorVersion = UI_SERVICE_MINOR,
    .available = indisponible,
};
constinit const ServiceModule g_cameraModule{
    .id = CAMERA_SERVICE_ID,
    .majorVersion = CAMERA_SERVICE_MAJOR,
    .minorVersion = CAMERA_SERVICE_MINOR,
    .available = indisponible,
};
constinit const ServiceModule g_gfxModule{
    .id = GFX_SERVICE_ID,
    .majorVersion = GFX_SERVICE_MAJOR,
    .minorVersion = GFX_SERVICE_MINOR,
    .available = indisponible,
};
} // namespace svc

// Manifeste des symboles du jeu (pour accrocher des fonctions par leur nom) : absent sur 3DS
namespace manifest {
void initialize() {}
bool available() { return false; }
const std::vector<uint8_t>& image_build_id() {
  static const std::vector<uint8_t> vide;
  return vide;
}
ResolveStatus resolve(const char*, void**, HookSymbolFlags*) { return ResolveStatus::Unavailable; }
bool has_inline_sites(const void*, const char**) { return false; }
} // namespace manifest

} // namespace dusk::mods

// --- Outil ImGui de partage d'état (presse-papiers, base64) ---
void dusk::ImGuiMenuTools::ShowStateShare() {}
