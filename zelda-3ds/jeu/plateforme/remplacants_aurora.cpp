// Fonctions d'Aurora qui n'existent pas sur la 3DS (pas de WebGPU, pas de menus RmlUi/ImGui
// affichés) : versions vides, utilisées par le code du jeu qui en vérifie le résultat.
#include <dolphin/gx.h>

#include <aurora/imgui.h>
#include <aurora/rmlui.hpp>
#include <aurora/texture.hpp>

// --- Remplacement de textures (packs de textures HD du PC) : non pris en charge ---
namespace aurora::texture {

std::optional<TextureSourceKey> parse_replacement_filename(std::string_view) noexcept { return std::nullopt; }
ReplacementRegistration register_virtual_replacement(std::string_view, VirtualFileSource, ReplacementOptions) {
  return {};
}
ReplacementRegistration register_replacement(ReplacementKey key, RawTextureReplacement, ReplacementOptions) {
  return {0, std::move(key)};
}
void unregister_replacement(const ReplacementRegistration&) {}
void unregister_replacements(std::span<const ReplacementRegistration>) {}
void unregister_replacements(const ReplacementGroup&) {}
void unregister_replacements(const ReplacementKey&) {}
void clear_replacements() {}
ReplacementGroup load_replacement_directory(const std::filesystem::path&, ReplacementOptions) { return {}; }
void reload_replacement_directory(const std::filesystem::path&, ReplacementGroup&, ReplacementOptions) {}
bool has_replacement(const GXTexObj*, const GXTlutObj*) { return false; }

} // namespace aurora::texture

// --- Menus RmlUi : jamais initialisés sur la 3DS (le jeu teste get_context() == nullptr) ---
namespace aurora::rmlui {

Rml::Context* get_context() noexcept { return nullptr; }
bool is_initialized() noexcept { return false; }
void set_input_type(InputType) noexcept {}
void set_ui_scale(float) noexcept {}
float get_ui_scale() noexcept { return 1.f; }
void set_glass_light_dir(float, float) noexcept {}
void register_texture_provider(std::string, TextureProvider) {}
void unregister_texture_provider(std::string_view) noexcept {}

} // namespace aurora::rmlui

// --- ImGui : pas d'affichage des fenêtres de débogage ---
extern "C" ImTextureID aurora_imgui_add_texture(uint32_t, uint32_t, const void*) { return ImTextureID{}; }
