#pragma once
// Conversion des textures GameCube (formats GX en blocs, gros-boutistes) vers les formats
// de la PICA200 (tuiles de 8x8 en ordre de Morton, retournées verticalement).
// Ne dépend pas de citro3d : testable sur PC.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gx3ds::conversion {

// Mêmes valeurs que GPU_TEXCOLOR de libctru
enum FormatPica : uint8_t {
  PICA_RGBA8 = 0x0,
  PICA_RGB8 = 0x1,
  PICA_RGBA5551 = 0x2,
  PICA_RGB565 = 0x3,
  PICA_RGBA4 = 0x4,
  PICA_LA8 = 0x5,
  PICA_L8 = 0x7,
  PICA_A8 = 0x8,
  PICA_LA4 = 0x9,
};

struct Image {
  uint32_t largeur = 0;
  uint32_t hauteur = 0;
  std::vector<uint32_t> rgba;  // octets R, V, B, A ; ligne 0 en haut
  bool aAlpha = false;         // au moins un texel non opaque
  bool alphaBinaire = true;    // alpha uniquement 0 ou 255
};

// Taille en octets d'une texture GX (niveau de base)
size_t tailleGx(uint32_t format, uint32_t largeur, uint32_t hauteur) noexcept;

// Décode une texture GX en RGBA. tlut : palette (GX_TF_C4/C8/C14X2), gros-boutiste.
bool decoder(const uint8_t* donnees, uint32_t format, uint32_t largeur, uint32_t hauteur, const uint8_t* tlut,
             uint32_t formatTlut, uint32_t nbTlut, Image& sortie) noexcept;

// Choisit le format PICA adapté au format GX et au contenu
FormatPica choisirFormat(uint32_t formatGx, uint32_t formatTlut, const Image& image) noexcept;

// Ramène l'image à des dimensions puissances de 2 comprises entre 8 et maxDim
void adapterTaille(Image& image, uint32_t maxDim) noexcept;

// Octets par texel du format PICA
uint32_t octetsParTexel(FormatPica f) noexcept;

// Écrit l'image dans le format PICA (tuiles 8x8, ordre de Morton, retournée verticalement)
void encoder(const Image& image, FormatPica format, uint8_t* sortie) noexcept;

} // namespace gx3ds::conversion
