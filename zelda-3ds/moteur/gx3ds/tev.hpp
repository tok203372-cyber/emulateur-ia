#pragma once
// Traduction du combineur de couleurs GameCube (TEV, jusqu'à 16 étages) vers le combineur
// de la PICA200 (6 étages, une seule couleur constante par étage, un tampon).
//
// Ce module ne dépend pas de citro3d : il produit une description simple des étages,
// ce qui permet de le tester sur PC.

#include <dolphin/gx/GXEnum.h>

#include <cstdint>

namespace gx3ds::tev {

// Valeurs de la PICA200 (identiques à celles de libctru)
enum Source : uint8_t {
  SRC_PRIMAIRE = 0x00,
  SRC_TEXTURE0 = 0x03,
  SRC_TEXTURE1 = 0x04,
  SRC_TEXTURE2 = 0x05,
  SRC_TAMPON = 0x0D,
  SRC_CONSTANTE = 0x0E,
  SRC_PRECEDENT = 0x0F,
};
enum Fonction : uint8_t {
  F_REMPLACER = 0x00,
  F_MODULER = 0x01,
  F_AJOUTER = 0x02,
  F_AJOUTER_SIGNE = 0x03,
  F_INTERPOLER = 0x04,
  F_SOUSTRAIRE = 0x05,
  F_MULTIPLIER_AJOUTER = 0x08,
};
// Opérandes « couleur » (RVB) de la PICA200
enum OpRvb : uint8_t {
  RVB_COULEUR = 0x00,
  RVB_UN_MOINS_COULEUR = 0x01,
  RVB_ALPHA = 0x02,
  RVB_UN_MOINS_ALPHA = 0x03,
  RVB_R = 0x04,
  RVB_UN_MOINS_R = 0x05,
  RVB_V = 0x08,
  RVB_UN_MOINS_V = 0x09,
  RVB_B = 0x0C,
  RVB_UN_MOINS_B = 0x0D,
};
// Opérandes « alpha » de la PICA200
enum OpAlpha : uint8_t {
  A_ALPHA = 0x00,
  A_UN_MOINS_ALPHA = 0x01,
  A_R = 0x02,
  A_UN_MOINS_R = 0x03,
  A_V = 0x04,
  A_UN_MOINS_V = 0x05,
  A_B = 0x06,
  A_UN_MOINS_B = 0x07,
};

struct Couleur {
  float r = 0.f, v = 0.f, b = 0.f, a = 0.f;
};

struct EtageGx {
  uint8_t c[4];      // arguments couleur a, b, c, d (GXTevColorArg)
  uint8_t a[4];      // arguments alpha a, b, c, d (GXTevAlphaArg)
  uint8_t opC, biaisC, echelleC, sortieC;  // GXTevOp, GXTevBias, GXTevScale, GXTevRegID
  uint8_t opA, biaisA, echelleA, sortieA;
  uint8_t kcSel, kaSel;
  uint8_t canal;       // GXChannelID de la couleur pixellisée
  int8_t unite;        // unité de texture PICA (0..2) ou -1 si pas de texture
  uint8_t echangeRas;  // table d'échange des canaux (0..3)
  uint8_t echangeTex;
};

struct EchangeGx {
  uint8_t r, v, b, a;  // GXTevColorChan
};

struct Entree {
  uint8_t nbEtages = 1;
  EtageGx etages[16];
  EchangeGx echanges[4];
  Couleur registres[4];  // PREV, REG0, REG1, REG2 (valeurs de départ)
  Couleur konst[4];      // K0..K3
};

struct EtagePica {
  uint8_t srcRvb[3] = {SRC_PRECEDENT, SRC_PRECEDENT, SRC_PRECEDENT};
  uint8_t opRvb[3] = {RVB_COULEUR, RVB_COULEUR, RVB_COULEUR};
  uint8_t fRvb = F_REMPLACER;
  uint8_t echelleRvb = 0;  // 0 : x1, 1 : x2, 2 : x4
  uint8_t srcA[3] = {SRC_PRECEDENT, SRC_PRECEDENT, SRC_PRECEDENT};
  uint8_t opA[3] = {A_ALPHA, A_ALPHA, A_ALPHA};
  uint8_t fA = F_REMPLACER;
  uint8_t echelleA = 0;
  uint32_t constante = 0xFFFFFFFF;  // RGBA8 (R dans les bits de poids faible, comme citro3d)
  bool tamponRvb = false;           // l'étage met à jour le tampon (couleur)
  bool tamponA = false;             // l'étage met à jour le tampon (alpha)
};

struct Programme {
  EtagePica etages[6];
  uint8_t nb = 0;
  uint32_t couleurTampon = 0;
  bool approxime = false;  // une partie n'a pas pu être traduite exactement
  bool tropLong = false;   // plus de 6 étages nécessaires : la fin a été coupée
  uint8_t raison = 0;      // diagnostic : cause de la première approximation
};

// Traduit le TEV. Renvoie le programme pour la PICA200 (toujours utilisable).
void compiler(const Entree& entree, Programme& sortie) noexcept;

uint32_t empaqueter(const Couleur& c) noexcept;

} // namespace gx3ds::tev
