#pragma once
// Fonctions propres à la 3DS appelées par le code du jeu (correctifs de m_Do_main.cpp et
// m_Do_machine.cpp) et par les fichiers de plateforme/.

#include <cstdint>
#include <string>

namespace zelda3ds {

// Dossier du jeu sur la carte SD (disque, sauvegardes, réglages, journal). Sans le préfixe
// « sdmc: » (la carte SD est le périphérique par défaut de libctru) : std::filesystem ne
// considère comme absolus que les chemins commençant par « / » ; « sdmc:/... » serait collé au
// dossier courant (« sdmc:/3ds/sdmc:/... »).
inline constexpr const char* kDossier = "/zelda-3ds/";

// Version du programme (commit de emulateur-ia) : affichée au démarrage et dans les rapports
const char* version() noexcept;

// Image du disque GameCube trouvée dans le dossier du jeu (.iso, .gcm, .ciso ou .gcz) ;
// chaîne vide si aucune.
std::string chemin_disque();

// Mémoire « GameCube » réservée au jeu (MEM1 et ARAM), en octets
uint32_t taille_mem1();
uint32_t taille_aram();

// Affiche un message d'erreur sur l'écran du bas, attend START, puis quitte le programme
[[noreturn]] void erreur_fatale(const std::string& message);

// Rapport de plantage (plantage3ds.cpp) : pour le fil en cours (à appeler au début de chaque
// fil), et pour tout le programme (fil principal, abort, exceptions C++ non rattrapées)
void installer_gestionnaire_plantage() noexcept;
void installer_gestionnaires_programme() noexcept;

} // namespace zelda3ds
