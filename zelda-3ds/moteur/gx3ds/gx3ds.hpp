#pragma once
// gx3ds : rendu des commandes GameCube (GX) avec la puce graphique PICA200 de la 3DS (citro3d).
//
// Le processeur de commandes d'Aurora décode le flux GX et met à jour aurora::gx::g_gxState ;
// gx3ds lit cet état au moment de chaque dessin et le traduit pour la PICA200.

#include <dolphin/gx.h>

#include <cstdint>

namespace gx3ds {

struct Reglages {
  // Zone de l'écran du haut où l'image du jeu est affichée (l'écran fait 400x240).
  // 4/3 : 320x240 au centre (image non déformée) ; sinon 400x240 (étirée).
  bool format43 = true;
  // Taille maximale des textures (les plus grandes sont réduites de moitié).
  uint32_t textureMax = 256;
};

Reglages& reglages() noexcept;

// Zone de l'écran du haut (400x240, y vers le bas) où le jeu est dessiné
void zone_ecran(float& x, float& y, float& largeur, float& hauteur) noexcept;

void initialiser() noexcept;
void arreter() noexcept;

// Début et fin d'une image (appelés par la boucle principale)
void debut_image() noexcept;
void fin_image() noexcept;

// Dessin : données de sommets brutes GX (gros-boutiste, attributs directs ou indices)
void dessiner(GXPrimitive prim, GXVtxFmt fmt, uint16_t nbSommets, const uint8_t* donnees) noexcept;
// Dessin de triangles indexés (indices 16 bits natifs) préparé par l'optimiseur de listes d'Aurora
void dessiner_indexe(GXVtxFmt fmt, uint16_t nbSommets, const uint8_t* donnees, const uint16_t* indices,
                     uint32_t nbIndices) noexcept;

// Copie de l'image en cours vers une texture (GXCopyTex) et effacement éventuel
void copier_vers_texture(const void* dest, bool effacer) noexcept;
// Copie vers l'écran (GXCopyDisp) : marque l'image comme terminée, avec effacement éventuel
void copier_vers_ecran(bool effacer) noexcept;

// Oubli d'objets du cache
void oublier_objet_texture(uint32_t texObjId) noexcept;
void oublier_tlut(uint32_t tlutObjId) noexcept;
void oublier_copie(const void* dest) noexcept;
void vider_cache_textures() noexcept;

// Statistiques de la dernière image (pour le journal)
struct Stats {
  uint32_t dessins = 0;
  uint32_t sommets = 0;
  uint32_t texturesConverties = 0;
  uint32_t octetsTextures = 0;
  uint32_t tevApproximes = 0;
  uint32_t ignores = 0;
};
const Stats& stats() noexcept;

} // namespace gx3ds
