// Version 3DS des variables globales d'Aurora (lib/aurora.cpp d'origine dépend de SDL et WebGPU).
// Le point d'entrée complet (aurora_initialize, aurora_begin_frame...) pour le jeu viendra ici.
#include "internal.hpp"

namespace aurora {
AuroraConfig g_config{};
uint32_t g_sdlCustomEventsStart = 0;
char g_gameName[4]{};
} // namespace aurora
