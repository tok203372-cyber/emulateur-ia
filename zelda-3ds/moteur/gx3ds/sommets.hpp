#pragma once
// Décodage des sommets GameCube (format GX : attributs directs ou indices dans des tableaux,
// valeurs gros-boutistes, nombres à virgule fixe) vers un format simple pour la PICA200.

#include <dolphin/gx.h>

#include <cstdint>

namespace gx3ds::sommets {

constexpr int MaxTex = 3;

// Ce dont le shader a besoin pour ce dessin
struct Besoins {
  bool normale = false;
  bool couleur = false;
  uint8_t nbTex = 0;
  uint8_t sourceTex[MaxTex] = {};  // GXTexGenSrc : GX_TG_POS, GX_TG_NRM, GX_TG_TEX0..7
};

// Place de chaque attribut dans un sommet PICA (-1 : absent)
struct Disposition {
  uint8_t taille = 0;
  int8_t pos = -1;
  int8_t nrm = -1;
  int8_t clr = -1;
  int8_t mtx = -1;
  int8_t tex[MaxTex] = {-1, -1, -1};
  uint8_t compTex[MaxTex] = {0, 0, 0};  // 2 (s, t) ou 3 (x, y, z)
};

// Tableau de sommets (GXSetArray)
struct Tableau {
  const uint8_t* donnees = nullptr;
  uint32_t taille = 0;
  uint16_t pas = 0;
  bool petitBoutiste = false;
};

// Description d'un attribut GX pour le décodage
struct AttributGx {
  uint8_t mode = GX_NONE;  // GX_NONE, GX_DIRECT, GX_INDEX8, GX_INDEX16
  uint8_t type = 0;        // GXCompType
  uint8_t nb = 0;          // nombre de composantes (format GX)
  uint8_t frac = 0;
  bool nbt3 = false;       // normales GX_NRM_NBT3 : trois indices séparés
};

struct FormatGx {
  AttributGx attr[GX_VA_MAX_ATTR];
  Tableau tableaux[GX_VA_MAX_ATTR];
};

class Decodeur {
public:
  // Prépare le décodage. Renvoie false si le format n'a pas de position.
  bool preparer(const FormatGx& format, const Besoins& besoins) noexcept;
  const Disposition& disposition() const noexcept { return mDisposition; }
  uint32_t tailleSommetGx() const noexcept { return mTailleGx; }
  bool aMatriceParSommet() const noexcept { return mDisposition.mtx >= 0; }
  // Décode n sommets du flux GX (src) vers dst (n * disposition().taille octets)
  void decoder(const uint8_t* src, uint32_t n, uint8_t* dst) const noexcept;

private:
  struct Etape {
    uint8_t attribut;
    uint8_t mode;
    uint8_t type;
    uint8_t nbLu;      // composantes lues
    uint8_t nbIndices; // NBT3 : 3 indices
    uint8_t tailleComp;
    uint8_t tailleDirecte;
    float echelle;
    Tableau tableau;
  };
  Etape mEtapes[GX_VA_MAX_ATTR]{};
  int mNbEtapes = 0;
  uint32_t mTailleGx = 0;
  Disposition mDisposition;
  Besoins mBesoins;
};

// Nombre d'octets d'une composante GX
uint8_t tailleComposante(uint8_t attribut, uint8_t type) noexcept;

} // namespace gx3ds::sommets
