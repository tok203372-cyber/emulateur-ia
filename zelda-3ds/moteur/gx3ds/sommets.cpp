// Décodage des sommets GX vers le format PICA (voir sommets.hpp).
#include "sommets.hpp"

#include <cstring>

namespace gx3ds::sommets {
namespace {

inline uint16_t lire16(const uint8_t* p, bool petitBoutiste) {
  return petitBoutiste ? static_cast<uint16_t>(p[0] | p[1] << 8) : static_cast<uint16_t>(p[0] << 8 | p[1]);
}

inline uint32_t lire32(const uint8_t* p, bool petitBoutiste) {
  if (petitBoutiste) return p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24;
  return static_cast<uint32_t>(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

inline float lireComposante(const uint8_t* p, uint8_t type, bool petitBoutiste, float echelle) {
  switch (type) {
  case GX_U8:
    return static_cast<float>(p[0]) * echelle;
  case GX_S8:
    return static_cast<float>(static_cast<int8_t>(p[0])) * echelle;
  case GX_U16:
    return static_cast<float>(lire16(p, petitBoutiste)) * echelle;
  case GX_S16:
    return static_cast<float>(static_cast<int16_t>(lire16(p, petitBoutiste))) * echelle;
  default: {
    const uint32_t bits = lire32(p, petitBoutiste);
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
  }
  }
}

inline uint8_t etendre5(uint32_t v) { return static_cast<uint8_t>(v << 3 | v >> 2); }
inline uint8_t etendre6(uint32_t v) { return static_cast<uint8_t>(v << 2 | v >> 4); }
inline uint8_t etendre4(uint32_t v) { return static_cast<uint8_t>(v << 4 | v); }

void lireCouleur(const uint8_t* p, uint8_t type, bool petitBoutiste, uint8_t out[4]) {
  switch (type) {
  case GX_RGB565: {
    const uint32_t v = lire16(p, petitBoutiste);
    out[0] = etendre5(v >> 11 & 31);
    out[1] = etendre6(v >> 5 & 63);
    out[2] = etendre5(v & 31);
    out[3] = 255;
    break;
  }
  case GX_RGB8:
  case GX_RGBX8:
    out[0] = p[0];
    out[1] = p[1];
    out[2] = p[2];
    out[3] = 255;
    break;
  case GX_RGBA4: {
    const uint32_t v = lire16(p, petitBoutiste);
    out[0] = etendre4(v >> 12 & 15);
    out[1] = etendre4(v >> 8 & 15);
    out[2] = etendre4(v >> 4 & 15);
    out[3] = etendre4(v & 15);
    break;
  }
  case GX_RGBA6: {
    const uint32_t v = static_cast<uint32_t>(p[0]) << 16 | p[1] << 8 | p[2];
    out[0] = etendre6(v >> 18 & 63);
    out[1] = etendre6(v >> 12 & 63);
    out[2] = etendre6(v >> 6 & 63);
    out[3] = etendre6(v & 63);
    break;
  }
  default:  // GX_RGBA8
    out[0] = p[0];
    out[1] = p[1];
    out[2] = p[2];
    out[3] = p[3];
    break;
  }
}

uint8_t tailleCouleur(uint8_t type) {
  switch (type) {
  case GX_RGB565:
  case GX_RGBA4:
    return 2;
  case GX_RGB8:
  case GX_RGBA6:
    return 3;
  default:
    return 4;
  }
}

struct Temp {
  float pos[3];
  float nrm[3];
  float tex[8][2];
  uint8_t clr0[4];
  uint8_t mtx;
};

const uint8_t kZeros[64] = {};

} // namespace

uint8_t tailleComposante(uint8_t attribut, uint8_t type) noexcept {
  if (attribut == GX_VA_CLR0 || attribut == GX_VA_CLR1) return tailleCouleur(type);
  switch (type) {
  case GX_U8:
  case GX_S8:
    return 1;
  case GX_U16:
  case GX_S16:
    return 2;
  default:
    return 4;
  }
}

bool Decodeur::preparer(const FormatGx& format, const Besoins& besoins) noexcept {
  mBesoins = besoins;
  mNbEtapes = 0;
  mTailleGx = 0;
  bool aPosition = false;
  bool aMatrice = false;

  for (int a = GX_VA_PNMTXIDX; a <= GX_VA_TEX7; a++) {
    const AttributGx& at = format.attr[a];
    if (at.mode == GX_NONE) continue;
    Etape& et = mEtapes[mNbEtapes++];
    et.attribut = static_cast<uint8_t>(a);
    et.mode = at.mode;
    et.type = at.type;
    et.nbIndices = 1;
    et.tableau = format.tableaux[a];

    if (a <= GX_VA_TEX7MTXIDX) {
      // indices de matrices : toujours un octet direct
      et.mode = GX_DIRECT;
      et.tailleComp = 1;
      et.nbLu = 1;
      et.tailleDirecte = 1;
      et.echelle = 1.f;
      mTailleGx += 1;
      if (a == GX_VA_PNMTXIDX) aMatrice = true;
      continue;
    }

    et.tailleComp = tailleComposante(static_cast<uint8_t>(a), at.type);
    et.echelle = 1.f / static_cast<float>(1u << (at.frac & 31));
    uint32_t nbDirect = at.nb;
    switch (a) {
    case GX_VA_POS:
      et.nbLu = at.nb;
      aPosition = true;
      break;
    case GX_VA_NRM:
      et.nbLu = 3;
      if (at.nbt3 && at.mode != GX_DIRECT) et.nbIndices = 3;
      break;
    case GX_VA_CLR0:
    case GX_VA_CLR1:
      et.nbLu = 1;
      nbDirect = 1;
      break;
    default:
      et.nbLu = at.nb;
      break;
    }
    et.tailleDirecte = static_cast<uint8_t>(nbDirect * et.tailleComp);
    switch (et.mode) {
    case GX_DIRECT:
      mTailleGx += et.tailleDirecte;
      break;
    case GX_INDEX8:
      mTailleGx += et.nbIndices;
      break;
    default:
      mTailleGx += 2u * et.nbIndices;
      break;
    }
  }

  // Disposition du sommet PICA
  Disposition d;
  uint8_t off = 0;
  d.pos = static_cast<int8_t>(off);
  off += 12;
  if (besoins.normale) {
    d.nrm = static_cast<int8_t>(off);
    off += 12;
  }
  for (int i = 0; i < besoins.nbTex && i < MaxTex; i++) {
    const uint8_t s = besoins.sourceTex[i];
    d.tex[i] = static_cast<int8_t>(off);
    d.compTex[i] = (s == GX_TG_POS || s == GX_TG_NRM) ? 3 : 2;
    off += d.compTex[i] * 4;
  }
  if (besoins.couleur) {
    d.clr = static_cast<int8_t>(off);
    off += 4;
  }
  if (aMatrice) {
    d.mtx = static_cast<int8_t>(off);
    off += 4;  // un octet utile, aligné sur 4
  }
  d.taille = off;
  mDisposition = d;
  return aPosition;
}

void Decodeur::decoder(const uint8_t* src, uint32_t n, uint8_t* dst) const noexcept {
  const Disposition& d = mDisposition;
  for (uint32_t v = 0; v < n; v++) {
    Temp t;
    t.pos[0] = t.pos[1] = t.pos[2] = 0.f;
    t.nrm[0] = 0.f;
    t.nrm[1] = 0.f;
    t.nrm[2] = 1.f;
    t.clr0[0] = t.clr0[1] = t.clr0[2] = t.clr0[3] = 255;
    t.mtx = 0;
    for (auto& tc : t.tex) tc[0] = tc[1] = 0.f;

    for (int e = 0; e < mNbEtapes; e++) {
      const Etape& et = mEtapes[e];
      const uint8_t* p;
      bool petit;
      if (et.mode == GX_DIRECT) {
        p = src;
        petit = false;
        src += et.tailleDirecte;
      } else {
        uint32_t idx;
        if (et.mode == GX_INDEX8) {
          idx = src[0];
          src += et.nbIndices;
        } else {
          idx = static_cast<uint32_t>(src[0]) << 8 | src[1];
          src += 2u * et.nbIndices;
        }
        const uint32_t decalage = idx * et.tableau.pas;
        const uint32_t besoin = et.nbLu * et.tailleComp;
        if (et.tableau.donnees == nullptr || (et.tableau.taille != 0 && decalage + besoin > et.tableau.taille)) {
          p = kZeros;
          petit = false;
        } else {
          p = et.tableau.donnees + decalage;
          petit = et.tableau.petitBoutiste;
        }
      }

      switch (et.attribut) {
      case GX_VA_PNMTXIDX:
        t.mtx = p[0];
        break;
      case GX_VA_POS:
        for (int c = 0; c < et.nbLu && c < 3; c++)
          t.pos[c] = lireComposante(p + c * et.tailleComp, et.type, petit, et.echelle);
        break;
      case GX_VA_NRM:
        for (int c = 0; c < 3; c++) t.nrm[c] = lireComposante(p + c * et.tailleComp, et.type, petit, et.echelle);
        break;
      case GX_VA_CLR0:
        lireCouleur(p, et.type, petit, t.clr0);
        break;
      default:
        if (et.attribut >= GX_VA_TEX0 && et.attribut <= GX_VA_TEX7) {
          float* tc = t.tex[et.attribut - GX_VA_TEX0];
          for (int c = 0; c < et.nbLu && c < 2; c++)
            tc[c] = lireComposante(p + c * et.tailleComp, et.type, petit, et.echelle);
        }
        break;
      }
    }

    // Assemblage du sommet PICA
    std::memcpy(dst + d.pos, t.pos, 12);
    if (d.nrm >= 0) std::memcpy(dst + d.nrm, t.nrm, 12);
    for (int i = 0; i < MaxTex; i++) {
      if (d.tex[i] < 0) continue;
      const uint8_t s = mBesoins.sourceTex[i];
      if (s == GX_TG_POS) {
        std::memcpy(dst + d.tex[i], t.pos, 12);
      } else if (s == GX_TG_NRM) {
        std::memcpy(dst + d.tex[i], t.nrm, 12);
      } else if (s >= GX_TG_TEX0 && s <= GX_TG_TEX7) {
        std::memcpy(dst + d.tex[i], t.tex[s - GX_TG_TEX0], 8);
      } else {
        std::memset(dst + d.tex[i], 0, d.compTex[i] * 4u);
      }
    }
    if (d.clr >= 0) std::memcpy(dst + d.clr, t.clr0, 4);
    if (d.mtx >= 0) {
      dst[d.mtx] = t.mtx;
      dst[d.mtx + 1] = dst[d.mtx + 2] = dst[d.mtx + 3] = 0;
    }
    dst += d.taille;
  }
}

} // namespace gx3ds::sommets
