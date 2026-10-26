// Parties de borealis remplacées sur la 3DS.
//
// crash.cpp (rapport de plantage) utilise les signaux POSIX, dlfcn et l'ELF du système, absents
// de la 3DS. En cas de plantage, Luma3DS affiche déjà l'adresse et les registres à l'écran.
#include <borealis/crash.hpp>

namespace borealis::crash {
void install() {}
} // namespace borealis::crash
