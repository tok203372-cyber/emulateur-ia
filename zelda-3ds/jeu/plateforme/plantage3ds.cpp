// Rapport de plantage.
//
// Si le processeur s'arrête sur une erreur (accès à une adresse invalide, instruction inconnue...)
// ou si le programme s'interrompt (abort, exception C++ non rattrapée), on écrit
// sdmc:/zelda-3ds/plantage.txt et on l'affiche sur l'écran du bas : version, registres, et les
// adresses du code trouvées sur la pile (les fonctions appelantes probables). Avec le fichier
// zelda3ds.map de la même version (branche fabrication-3ds), on retrouve les fonctions en cause.
#include "plateforme3ds.hpp"

#include <3ds.h>

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <typeinfo>

extern "C" char __exidx_start[];  // fin du code et des données en lecture seule (3dsx.ld)

namespace zelda3ds {
namespace {

alignas(8) uint8_t sPileGestionnaire[0x4000];
char sTexte[3072];
size_t sLongueur = 0;
std::atomic<int> sEnCours{0};

void ajouter(const char* format, ...) {
  if (sLongueur + 1 >= sizeof(sTexte)) return;
  va_list args;
  va_start(args, format);
  const int n = std::vsnprintf(sTexte + sLongueur, sizeof(sTexte) - sLongueur, format, args);
  va_end(args);
  if (n > 0) sLongueur = std::min(sizeof(sTexte) - 1, sLongueur + static_cast<size_t>(n));
}

// Adresse dans le code du programme (instructions ARM : multiples de 4)
bool estDansLeCode(u32 v) {
  return v >= 0x100000 && v < reinterpret_cast<u32>(__exidx_start) && (v & 3) == 0;
}

// Adresses du code trouvées sur la pile, en partant de sp
void ajouterPile(u32 sp) {
  ajouter("Pile (appelants probables) :");
  MemInfo info{};
  PageInfo page{};
  if (R_SUCCEEDED(svcQueryMemory(&info, &page, sp)) && (info.perm & MEMPERM_READ)) {
    const u32 fin = std::min<u32>(info.base_addr + info.size, sp + 0x6000);
    int n = 0;
    for (u32 a = sp & ~3u; a + 4 <= fin && n < 36; a += 4) {
      const u32 v = *reinterpret_cast<const volatile u32*>(a);
      if (estDansLeCode(v)) {
        ajouter("%s%08X", n % 6 == 0 ? "\n " : " ", v);
        n++;
      }
    }
  }
  ajouter("\n");
}

// Écriture directe par le service FS (sans malloc : le tas est peut-être abîmé)
void ecrireRapport() {
  Handle fichier = 0;
  const Result r = FSUSER_OpenFileDirectly(&fichier, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                                           fsMakePath(PATH_ASCII, "/zelda-3ds/plantage.txt"),
                                           FS_OPEN_WRITE | FS_OPEN_CREATE, 0);
  if (R_FAILED(r)) return;
  FSFILE_SetSize(fichier, 0);
  u32 ecrits = 0;
  FSFILE_Write(fichier, &ecrits, 0, sTexte, sLongueur, FS_WRITE_FLUSH);
  FSFILE_Close(fichier);
}

[[noreturn]] void attendreEtQuitter() {
  std::printf("Rapport : sdmc:/zelda-3ds/plantage.txt\nAppuyez sur START pour quitter.\n");
  std::fflush(stdout);
  for (;;) {
    hidScanInput();
    if (hidKeysDown() & KEY_START) break;
    svcSleepThread(16'666'667);
  }
  std::exit(1);
}

[[noreturn]] void bloquer() {
  for (;;) svcSleepThread(1'000'000'000);
}

void entete(const char* quoi) {
  u32 fil = 0;
  svcGetThreadId(&fil, CUR_THREAD_HANDLE);
  ajouter("Crepuscule %s - %s\n", version(), quoi);
  ajouter("Fil %u, temps %llu ms\n", fil, osGetTime());
}

// Erreur du processeur. libctru est compilée avec des énumérations d'un octet : on lit les mots
// bruts de ERRF_ExceptionInfo (type, fsr, far).
[[noreturn]] void gestionnaire(ERRF_ExceptionInfo* infoBrute, CpuRegisters* regs) {
  if (sEnCours.exchange(1) != 0) bloquer();  // plantage pendant le rapport, ou dans un autre fil
  const u32* mots = reinterpret_cast<const u32*>(infoBrute);
  const u32 type = mots[0] & 0xFF;
  const u32 fsr = mots[1];
  const u32 far = mots[2];
  static const char* const kTypes[] = {"execution d'une adresse invalide (prefetch abort)",
                                       "acces memoire invalide (data abort)", "instruction inconnue",
                                       "erreur de calcul flottant (VFP)"};
  entete(type < 4 ? kTypes[type] : "erreur inconnue");
  ajouter("PC %08X  LR %08X\nSP %08X  CPSR %08X\n", regs->pc, regs->lr, regs->sp, regs->cpsr);
  if (type <= 1) ajouter("Adresse en faute %08X (FSR %08X)\n", far, fsr);
  for (int i = 0; i < 13; i++) ajouter("R%-2d %08X%s", i, regs->r[i], i % 3 == 2 || i == 12 ? "\n" : "  ");
  ajouterPile(regs->sp);
  ecrireRapport();
  std::printf("\n\x1b[31;1m*** Plantage ***\x1b[0m\n%s", sTexte);
  attendreEtQuitter();
}

[[noreturn]] void interruption(const char* quoi) {
  if (sEnCours.exchange(1) != 0) bloquer();
  entete(quoi);
  u32 sp;
  asm volatile("mov %0, sp" : "=r"(sp));
  ajouterPile(sp);
  ecrireRapport();
  std::printf("\n\x1b[31;1m*** Arret du programme ***\x1b[0m\n%s", sTexte);
  attendreEtQuitter();
}

void surAbort(int) { interruption("abort() : arret demande par le programme"); }

void surTerminate() {
  // Message de l'exception C++ en cours, s'il y en a une
  static char message[256];
  std::snprintf(message, sizeof(message), "exception C++ non rattrapee");
  if (auto e = std::current_exception()) {
    try {
      std::rethrow_exception(e);
    } catch (const std::exception& x) {
      std::snprintf(message, sizeof(message), "exception C++ non rattrapee : %s", x.what());
    } catch (...) {
    }
  }
  interruption(message);
}

} // namespace

void installer_gestionnaire_plantage() noexcept {
  threadOnException(gestionnaire, sPileGestionnaire + sizeof(sPileGestionnaire), WRITE_DATA_TO_HANDLER_STACK);
}

void installer_gestionnaires_programme() noexcept {
  installer_gestionnaire_plantage();
  std::signal(SIGABRT, surAbort);
  std::set_terminate(surTerminate);
}

} // namespace zelda3ds
