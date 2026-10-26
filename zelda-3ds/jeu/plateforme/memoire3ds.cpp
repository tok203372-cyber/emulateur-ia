// Répartition de la mémoire de l'application entre :
//  - le tas (malloc, new) : mémoire « GameCube » du jeu (MEM1, ARAM), piles des fils, données
//    d'Aurora et de Dusklight ;
//  - la mémoire linéaire (lue par le processeur graphique et le processeur audio) : textures,
//    sommets et commandes du GPU (gx3ds), tampons du son, images des écrans.
//
// Remplace __system_allocateHeaps de libctru (fonction « faible »), qui donnerait au plus 32 Mo
// à la mémoire linéaire et s'arrêterait (svcBreak) si une allocation échouait. Ici :
//  - 36 Mo de mémoire linéaire au moins (gx3ds : 18 Mo de textures, 6 Mo de sommets, 2 Mo de
//    commandes...) et le reste pour le tas, au plus 96 Mo (taille de la zone du tas) : en mode
//    178 Mo de la New 3DS, environ 96 Mo de tas et 66 Mo de mémoire linéaire ;
//  - si une allocation échoue (un émulateur peut mal compter la mémoire libre), on réessaie plus
//    petit au lieu de s'arrêter.
#include <3ds.h>
#include <3ds/allocator/mappable.h>

extern "C" {

extern char* fake_heap_start;
extern char* fake_heap_end;
extern u32 __ctru_heap;
extern u32 __ctru_heap_size;
extern u32 __ctru_linear_heap;
extern u32 __ctru_linear_heap_size;

void __system_allocateHeaps(void) {
  constexpr u32 Mo = 1u << 20;
  constexpr u32 ZoneTas = OS_HEAP_AREA_END - OS_HEAP_AREA_BEGIN;  // 96 Mo
  constexpr MemPerm LectureEcriture = static_cast<MemPerm>(MEMPERM_READ | MEMPERM_WRITE);

  // Mémoire encore libre pour le processus
  Handle limite = 0;
  if (R_FAILED(svcGetResourceLimit(&limite, CUR_PROCESS_HANDLE))) svcBreak(USERBREAK_PANIC);
  s64 maximum = 0;
  s64 utilise = 0;
  ResourceLimitType type = RESLIMIT_COMMIT;
  svcGetResourceLimitLimitValues(&maximum, limite, &type, 1);
  svcGetResourceLimitCurrentValues(&utilise, limite, &type, 1);
  svcCloseHandle(limite);
  const u32 reste = static_cast<u32>(maximum - utilise) & ~0xFFFu;

  u32 lineaire = 36 * Mo;
  if (lineaire > reste / 2) lineaire = (reste / 2) & ~0xFFFu;
  u32 tas = reste - lineaire;
  if (tas > ZoneTas) {
    lineaire += tas - ZoneTas;
    tas = ZoneTas;
  }

  // Tas : plus petit en cas d'échec
  u32 adresseTas = 0;
  while (R_FAILED(svcControlMemory(&adresseTas, OS_HEAP_AREA_BEGIN, 0, tas, MEMOP_ALLOC, LectureEcriture))) {
    if (tas <= 8 * Mo) svcBreak(USERBREAK_PANIC);
    tas -= 2 * Mo;
  }

  // Mémoire linéaire : si elle ne tient pas, on rend d'abord de la mémoire du tas (jusqu'à 32 Mo
  // de tas), puis on la réduit
  u32 adresseLineaire = 0;
  while (R_FAILED(svcControlMemory(&adresseLineaire, 0, 0, lineaire, MEMOP_ALLOC_LINEAR, LectureEcriture))) {
    if (tas > 32 * Mo) {
      u32 inutile = 0;
      svcControlMemory(&inutile, adresseTas + tas - 4 * Mo, 0, 4 * Mo, MEMOP_FREE, static_cast<MemPerm>(0));
      tas -= 4 * Mo;
    } else if (lineaire > 8 * Mo) {
      lineaire -= 2 * Mo;
    } else {
      svcBreak(USERBREAK_PANIC);
    }
  }

  __ctru_heap = adresseTas;
  __ctru_heap_size = tas;
  __ctru_linear_heap = adresseLineaire;
  __ctru_linear_heap_size = lineaire;

  mappableInit(OS_MAP_AREA_BEGIN, OS_MAP_AREA_END);

  // Tas de la bibliothèque C (malloc)
  fake_heap_start = reinterpret_cast<char*>(__ctru_heap);
  fake_heap_end = fake_heap_start + __ctru_heap_size;
}

} // extern "C"
