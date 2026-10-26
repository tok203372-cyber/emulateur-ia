// Point d'entrée du programme sur la 3DS.
//
// Le main() de Dusklight (src/dusk/main.cpp) est renommé aurora_main par <aurora/main.h> ;
// on prépare ici la console (écran du bas : journal), puis on l'appelle.
#include "plateforme3ds.hpp"

#include <3ds.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

extern "C" int aurora_main(int argc, char* argv[]);

namespace zelda3ds {

namespace {
bool finitPar(const std::string& nom, const char* fin) {
  const std::string f = fin;
  if (nom.size() < f.size()) return false;
  for (size_t i = 0; i < f.size(); i++) {
    if (std::tolower(static_cast<unsigned char>(nom[nom.size() - f.size() + i])) != f[i]) return false;
  }
  return true;
}
} // namespace

std::string chemin_disque() {
  std::vector<std::string> trouves;
  if (DIR* d = opendir(kDossier)) {
    while (dirent* e = readdir(d)) {
      const std::string nom = e->d_name;
      if (finitPar(nom, ".iso") || finitPar(nom, ".gcm") || finitPar(nom, ".ciso") || finitPar(nom, ".gcz")) {
        trouves.push_back(nom);
      }
    }
    closedir(d);
  }
  if (trouves.empty()) return {};
  std::sort(trouves.begin(), trouves.end());
  return std::string(kDossier) + trouves.front();
}

// Tailles choisies pour tenir dans les 124 Mo de la New 3DS (mode CIA) avec le code du jeu, la
// mémoire graphique (32 Mo) et le reste : 40 Mo de MEM1 (24 Mo sur GameCube) et 16 Mo d'ARAM.
uint32_t taille_mem1() { return 40u * 1024 * 1024; }
uint32_t taille_aram() { return 16u * 1024 * 1024; }

void erreur_fatale(const std::string& message) {
  std::printf("\n\x1b[31;1mErreur :\x1b[0m %s\n\nAppuyez sur START pour quitter.\n", message.c_str());
  std::fflush(stdout);
  if (FILE* f = std::fopen("sdmc:/zelda-3ds/erreur.txt", "w")) {
    std::fprintf(f, "%s\n", message.c_str());
    std::fclose(f);
  }
  while (aptMainLoop()) {
    hidScanInput();
    if (hidKeysDown() & KEY_START) break;
    gspWaitForVBlank();
  }
  std::exit(1);
}

} // namespace zelda3ds

int main(int argc, char* argv[]) {
  // New 3DS : processeur à 804 MHz et cache L2
  osSetSpeedupEnable(true);
  gfxInitDefault();
  consoleInit(GFX_BOTTOM, nullptr);
  std::printf("\x1b[32;1mCrepuscule\x1b[0m - Twilight Princess (Dusklight) pour New 3DS\n\n");
  mkdir("sdmc:/zelda-3ds", 0777);

  bool new3ds = false;
  APT_CheckNew3DS(&new3ds);
  if (!new3ds) {
    std::printf("Attention : cette version est prévue pour la New 3DS.\n");
  }
  // Le son de SDL tourne sur le cœur 1 (partagé avec le système) : 30 % de ce cœur pour le jeu
  APT_SetAppCpuTimeLimit(30);

  const int resultat = aurora_main(argc, argv);
  gfxExit();
  return resultat;
}
