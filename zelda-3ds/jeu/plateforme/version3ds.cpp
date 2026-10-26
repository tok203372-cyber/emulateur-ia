// Version du programme : commit de emulateur-ia, donné par CMake (jeu/CMakeLists.txt).
#include "plateforme3ds.hpp"

#ifndef ZELDA3DS_VERSION
#define ZELDA3DS_VERSION "locale"
#endif

const char* zelda3ds::version() noexcept { return ZELDA3DS_VERSION; }
