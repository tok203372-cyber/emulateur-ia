#pragma once
// Fonctions propres à la 3DS appelées par le code du jeu (correctifs de m_Do_main.cpp et
// m_Do_machine.cpp). Implémentées dans main3ds.cpp.

#include <cstdint>
#include <string>

namespace zelda3ds {

// Dossier du jeu sur la carte SD (disque, sauvegardes, réglages, journal)
inline constexpr const char* kDossier = "sdmc:/zelda-3ds/";

// Image du disque GameCube trouvée dans le dossier du jeu (.iso, .gcm, .ciso ou .gcz) ;
// chaîne vide si aucune.
std::string chemin_disque();

// Mémoire « GameCube » réservée au jeu (MEM1 et ARAM), en octets
uint32_t taille_mem1();
uint32_t taille_aram();

// Affiche un message d'erreur sur l'écran du bas, attend START, puis quitte le programme
[[noreturn]] void erreur_fatale(const std::string& message);

} // namespace zelda3ds
