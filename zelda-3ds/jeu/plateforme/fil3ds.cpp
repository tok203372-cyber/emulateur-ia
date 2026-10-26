// Fils d'exécution du jeu sur la 3DS (voir fil3ds.hpp).
#include "fil3ds.hpp"

#include <algorithm>
#include <atomic>

namespace zelda3ds {
namespace {

thread_local int tPriorite = 16;
thread_local uint32_t tPile = 0;
std::atomic<int> sFilsCoeur2{0};

// Priorité 3DS (0x18 = la plus haute pour une application, 0x3F = la plus basse) à partir de
// la priorité GameCube (0 = la plus haute, 31 = la plus basse ; fil principal du jeu : 16).
int prioriteNative(int gc) {
  int p = 0x30 + (gc - 16);
  return std::clamp(p, 0x19, 0x3F);
}

} // namespace

void preparer_fil(int prioriteGameCube, uint32_t taillePile) noexcept {
  tPriorite = prioriteGameCube;
  tPile = taillePile;
}

int fils_coeur2() noexcept { return sFilsCoeur2.load(); }

Thread Fil::creer(ThreadFunc fonction, void* arg) noexcept {
  // Pile : au moins 256 Ko (le code du portage PC utilise plus de pile que sur GameCube)
  const size_t pile = std::max<size_t>(256 * 1024, (static_cast<size_t>(tPile) * 2 + 0xFFF) & ~size_t{0xFFF});
  const int priorite = prioriteNative(tPriorite);

  bool new3ds = false;
  APT_CheckNew3DS(&new3ds);
  if (new3ds) {
    // Cœur 2 de la New 3DS (refusé si l'application n'y a pas accès : on revient au cœur 0)
    if (Thread t = threadCreate(fonction, arg, pile, priorite, 2, false)) {
      sFilsCoeur2++;
      return t;
    }
  }
  return threadCreate(fonction, arg, pile, priorite, -2, false);
}

} // namespace zelda3ds
