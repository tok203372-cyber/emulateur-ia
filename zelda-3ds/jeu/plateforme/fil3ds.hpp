#pragma once
// Fils d'exécution du jeu sur la 3DS (remplace std::thread dans OSThread.cpp).
//
// std::thread passe par pthread, que libctru crée toujours avec une petite pile (32 Ko), la
// priorité la plus basse et sur le cœur 0. Ici :
//  - pile de 256 Ko (comme les piles GameCube les plus grandes, avec de la marge) ;
//  - priorité déduite de celle du fil GameCube (fil principal du jeu : 16 -> 0x30 sur 3DS) ;
//  - sur New 3DS, le cœur 2 (libre pour les jeux) quand l'application y a accès : les fils
//    du jeu (son, chargement, décompression) tournent alors en parallèle du fil principal.
// Les autres std::thread (lecture du disque d'Aurora...) sont placés de la même façon : voir
// __wrap___syscall_thread_create dans fil3ds.cpp (option -Wl,--wrap de l'édition de liens).
#include <3ds.h>

#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <utility>

namespace zelda3ds {

// À appeler juste avant de construire un Fil : priorité GameCube (0 = la plus haute) et taille
// de pile demandée par le jeu (0 : valeur par défaut)
void preparer_fil(int prioriteGameCube, uint32_t taillePile) noexcept;

// Nombre de fils placés sur le cœur 2 (pour le journal)
int fils_coeur2() noexcept;

// Crée un fil natif : cœur 2 de la New 3DS si possible, sinon le cœur du programme. Le
// gestionnaire de plantage est installé au début du fil.
Thread creer_fil(ThreadFunc fonction, void* arg, size_t pile, int priorite) noexcept;

class Fil {
public:
  Fil() noexcept = default;

  template <class F, class... Args>
  explicit Fil(F&& f, Args&&... args) {
    auto* travail = new std::function<void()>(
        [f = std::forward<F>(f), ... a = std::forward<Args>(args)]() mutable { f(a...); });
    mFil = creer(&Fil::entree, travail);
    if (mFil == nullptr) {
      delete travail;
      std::terminate();  // comme std::thread quand la création échoue (sans exceptions)
    }
  }

  Fil(Fil&& autre) noexcept : mFil(std::exchange(autre.mFil, nullptr)) {}
  Fil& operator=(Fil&& autre) noexcept {
    if (joinable()) std::terminate();
    mFil = std::exchange(autre.mFil, nullptr);
    return *this;
  }
  Fil(const Fil&) = delete;
  Fil& operator=(const Fil&) = delete;
  ~Fil() {
    if (joinable()) std::terminate();
  }

  bool joinable() const noexcept { return mFil != nullptr; }
  void join() {
    threadJoin(mFil, U64_MAX);
    threadFree(mFil);
    mFil = nullptr;
  }
  void detach() {
    threadDetach(mFil);
    mFil = nullptr;
  }

private:
  static void entree(void* arg);
  static Thread creer(ThreadFunc fonction, void* arg) noexcept;

  Thread mFil = nullptr;
};

} // namespace zelda3ds
