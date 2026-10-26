// Fils d'exécution du jeu sur la 3DS (voir fil3ds.hpp).
#include "fil3ds.hpp"

#include "plateforme3ds.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <new>

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

// Début de chaque fil : gestionnaire de plantage, puis la fonction du fil
struct Depart {
  ThreadFunc fonction;
  void* arg;
};

void demarrer(void* arg) {
  const Depart depart = *static_cast<Depart*>(arg);
  delete static_cast<Depart*>(arg);
  installer_gestionnaire_plantage();
  depart.fonction(depart.arg);
}

} // namespace

void preparer_fil(int prioriteGameCube, uint32_t taillePile) noexcept {
  tPriorite = prioriteGameCube;
  tPile = taillePile;
}

int fils_coeur2() noexcept { return sFilsCoeur2.load(); }

Thread creer_fil(ThreadFunc fonction, void* arg, size_t pile, int priorite) noexcept {
  auto* depart = new (std::nothrow) Depart{fonction, arg};
  if (depart == nullptr) return nullptr;

  bool new3ds = false;
  APT_CheckNew3DS(&new3ds);
  if (new3ds) {
    // Cœur 2 de la New 3DS (refusé si l'application n'y a pas accès : on revient au cœur du
    // programme)
    if (Thread t = threadCreate(demarrer, depart, pile, priorite, 2, false)) {
      sFilsCoeur2++;
      return t;
    }
  }
  Thread t = threadCreate(demarrer, depart, pile, priorite, -2, false);
  if (t == nullptr) delete depart;
  return t;
}

void Fil::entree(void* arg) {
  std::unique_ptr<std::function<void()>> travail(static_cast<std::function<void()>*>(arg));
  (*travail)();
}

Thread Fil::creer(ThreadFunc fonction, void* arg) noexcept {
  // Pile : au moins 256 Ko (le code du portage PC utilise plus de pile que sur GameCube)
  const size_t pile = std::max<size_t>(256 * 1024, (static_cast<size_t>(tPile) * 2 + 0xFFF) & ~size_t{0xFFF});
  return creer_fil(fonction, arg, pile, prioriteNative(tPriorite));
}

} // namespace zelda3ds

// --- std::thread (pthread_create de newlib -> __syscall_thread_create de libctru) ---
// libctru crée ces fils avec 32 Ko de pile, la priorité la plus basse (0x3F) et sur le cœur 0 :
// un fil du jeu de priorité plus haute qui attendrait en boucle l'empêcherait de tourner. On les
// crée comme les autres fils, juste en dessous du fil principal (0x31), avec 128 Ko de pile.
struct __pthread_t;

namespace {
struct DepartPosix {
  void* (*fonction)(void*);
  void* arg;
};

void demarrerPosix(void* arg) {
  const DepartPosix depart = *static_cast<DepartPosix*>(arg);
  delete static_cast<DepartPosix*>(arg);
  void* resultat = depart.fonction(depart.arg);
  threadExit(static_cast<int>(reinterpret_cast<intptr_t>(resultat)));
}
} // namespace

extern "C" int __wrap___syscall_thread_create(__pthread_t** fil, void* (*fonction)(void*), void* arg, void* pile,
                                              size_t taillePile) {
  if (pile != nullptr) return EINVAL;
  auto* depart = new (std::nothrow) DepartPosix{fonction, arg};
  if (depart == nullptr) return ENOMEM;
  Thread t = zelda3ds::creer_fil(demarrerPosix, depart, std::max<size_t>(taillePile, 128 * 1024), 0x31);
  if (t == nullptr) {
    delete depart;
    return ENOMEM;
  }
  *fil = reinterpret_cast<__pthread_t*>(t);
  return 0;
}
