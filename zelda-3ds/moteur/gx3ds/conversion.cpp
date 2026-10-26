// Conversion des textures GX vers la PICA200 (voir conversion.hpp).
#include "conversion.hpp"

#include <dolphin/gx/GXEnum.h>

#include <algorithm>
#include <cstring>

namespace gx3ds::conversion {
namespace {

inline uint32_t rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a) { return r | g << 8 | b << 16 | a << 24; }
inline uint32_t rouge(uint32_t c) { return c & 0xFF; }
inline uint32_t vert(uint32_t c) { return c >> 8 & 0xFF; }
inline uint32_t bleu(uint32_t c) { return c >> 16 & 0xFF; }
inline uint32_t alpha(uint32_t c) { return c >> 24; }

inline uint32_t be16(const uint8_t* p) { return static_cast<uint32_t>(p[0]) << 8 | p[1]; }

inline uint32_t e5(uint32_t v) { return v << 3 | v >> 2; }
inline uint32_t e6(uint32_t v) { return v << 2 | v >> 4; }
inline uint32_t e4(uint32_t v) { return v << 4 | v; }
inline uint32_t e3(uint32_t v) { return v << 5 | v << 2 | v >> 1; }

uint32_t depuis565(uint32_t v) { return rgba(e5(v >> 11 & 31), e6(v >> 5 & 63), e5(v & 31), 255); }

uint32_t depuis5A3(uint32_t v) {
  if (v & 0x8000) return rgba(e5(v >> 10 & 31), e5(v >> 5 & 31), e5(v & 31), 255);
  return rgba(e4(v >> 8 & 15), e4(v >> 4 & 15), e4(v & 15), e3(v >> 12 & 7));
}

uint32_t depuisIA8(uint32_t v) {
  const uint32_t i = v & 0xFF;
  return rgba(i, i, i, v >> 8);
}

uint32_t couleurTlut(const uint8_t* tlut, uint32_t formatTlut, uint32_t nbTlut, uint32_t indice) {
  if (tlut == nullptr || indice >= nbTlut) return rgba(0, 0, 0, 0);
  const uint32_t v = be16(tlut + indice * 2);
  switch (formatTlut) {
  case GX_TL_IA8:
    return depuisIA8(v);
  case GX_TL_RGB565:
    return depuis565(v);
  default:
    return depuis5A3(v);
  }
}

// Parcourt les blocs GX : appelle f(bx, by, bloc) pour chaque bloc de bw x bh texels
template <typename F>
void parBlocs(const uint8_t* src, uint32_t largeur, uint32_t hauteur, uint32_t bw, uint32_t bh, uint32_t octetsBloc,
              F&& f) {
  const uint32_t nbx = (largeur + bw - 1) / bw;
  const uint32_t nby = (hauteur + bh - 1) / bh;
  for (uint32_t by = 0; by < nby; by++) {
    for (uint32_t bx = 0; bx < nbx; bx++) {
      f(bx * bw, by * bh, src);
      src += octetsBloc;
    }
  }
}

inline void poser(Image& im, uint32_t x, uint32_t y, uint32_t c) {
  if (x < im.largeur && y < im.hauteur) im.rgba[y * im.largeur + x] = c;
}

// DXT1 version GameCube : couleurs gros-boutistes, mélanges 3/8 - 5/8
void decoderCmprSousBloc(const uint8_t* p, Image& im, uint32_t x0, uint32_t y0) {
  const uint32_t c0 = be16(p);
  const uint32_t c1 = be16(p + 2);
  uint32_t pal[4];
  pal[0] = depuis565(c0);
  pal[1] = depuis565(c1);
  auto melange = [](uint32_t a, uint32_t b, uint32_t pa, uint32_t pb, uint32_t div) {
    return rgba((rouge(a) * pa + rouge(b) * pb) / div, (vert(a) * pa + vert(b) * pb) / div,
                (bleu(a) * pa + bleu(b) * pb) / div, 255);
  };
  if (c0 > c1) {
    pal[2] = melange(pal[0], pal[1], 5, 3, 8);
    pal[3] = melange(pal[0], pal[1], 3, 5, 8);
  } else {
    pal[2] = melange(pal[0], pal[1], 1, 1, 2);
    pal[3] = pal[2] & 0x00FFFFFF;  // transparent
  }
  for (uint32_t ty = 0; ty < 4; ty++) {
    const uint8_t ligne = p[4 + ty];
    for (uint32_t tx = 0; tx < 4; tx++) {
      const uint32_t idx = ligne >> (6 - tx * 2) & 3;
      poser(im, x0 + tx, y0 + ty, pal[idx]);
    }
  }
}

inline uint32_t morton8(uint32_t x, uint32_t y) {
  return (x & 1) | (y & 1) << 1 | (x & 2) << 1 | (y & 2) << 2 | (x & 4) << 2 | (y & 4) << 3;
}

bool puissanceDe2(uint32_t v) { return v != 0 && (v & (v - 1)) == 0; }

uint32_t puissanceSuperieure(uint32_t v) {
  uint32_t p = 8;
  while (p < v) p <<= 1;
  return p;
}

} // namespace

size_t tailleGx(uint32_t format, uint32_t largeur, uint32_t hauteur) noexcept {
  auto arrondir = [](uint32_t v, uint32_t a) { return (v + a - 1) / a * a; };
  switch (format) {
  case GX_TF_I4:
  case GX_TF_C4:
  case GX_TF_CMPR:
    return static_cast<size_t>(arrondir(largeur, 8)) * arrondir(hauteur, 8) / 2;
  case GX_TF_I8:
  case GX_TF_IA4:
  case GX_TF_C8:
    return static_cast<size_t>(arrondir(largeur, 8)) * arrondir(hauteur, 4);
  case GX_TF_IA8:
  case GX_TF_RGB565:
  case GX_TF_RGB5A3:
  case GX_TF_C14X2:
    return static_cast<size_t>(arrondir(largeur, 4)) * arrondir(hauteur, 4) * 2;
  case GX_TF_RGBA8:
    return static_cast<size_t>(arrondir(largeur, 4)) * arrondir(hauteur, 4) * 4;
  default:
    return 0;
  }
}

bool decoder(const uint8_t* src, uint32_t format, uint32_t largeur, uint32_t hauteur, const uint8_t* tlut,
             uint32_t formatTlut, uint32_t nbTlut, Image& im) noexcept {
  if (src == nullptr || largeur == 0 || hauteur == 0 || largeur > 1024 || hauteur > 1024) return false;
  im.largeur = largeur;
  im.hauteur = hauteur;
  im.rgba.assign(static_cast<size_t>(largeur) * hauteur, 0);

  switch (format) {
  case GX_TF_I4:
    parBlocs(src, largeur, hauteur, 8, 8, 32, [&](uint32_t x0, uint32_t y0, const uint8_t* b) {
      for (uint32_t ty = 0; ty < 8; ty++)
        for (uint32_t tx = 0; tx < 8; tx++) {
          const uint8_t o = b[ty * 4 + tx / 2];
          const uint32_t i = e4((tx & 1) ? (o & 15) : (o >> 4));
          poser(im, x0 + tx, y0 + ty, rgba(i, i, i, i));
        }
    });
    break;
  case GX_TF_I8:
    parBlocs(src, largeur, hauteur, 8, 4, 32, [&](uint32_t x0, uint32_t y0, const uint8_t* b) {
      for (uint32_t ty = 0; ty < 4; ty++)
        for (uint32_t tx = 0; tx < 8; tx++) {
          const uint32_t i = b[ty * 8 + tx];
          poser(im, x0 + tx, y0 + ty, rgba(i, i, i, i));
        }
    });
    break;
  case GX_TF_IA4:
    parBlocs(src, largeur, hauteur, 8, 4, 32, [&](uint32_t x0, uint32_t y0, const uint8_t* b) {
      for (uint32_t ty = 0; ty < 4; ty++)
        for (uint32_t tx = 0; tx < 8; tx++) {
          const uint8_t o = b[ty * 8 + tx];
          const uint32_t i = e4(o & 15);
          poser(im, x0 + tx, y0 + ty, rgba(i, i, i, e4(o >> 4)));
        }
    });
    break;
  case GX_TF_IA8:
    parBlocs(src, largeur, hauteur, 4, 4, 32, [&](uint32_t x0, uint32_t y0, const uint8_t* b) {
      for (uint32_t t = 0; t < 16; t++) poser(im, x0 + t % 4, y0 + t / 4, depuisIA8(be16(b + t * 2)));
    });
    break;
  case GX_TF_RGB565:
    parBlocs(src, largeur, hauteur, 4, 4, 32, [&](uint32_t x0, uint32_t y0, const uint8_t* b) {
      for (uint32_t t = 0; t < 16; t++) poser(im, x0 + t % 4, y0 + t / 4, depuis565(be16(b + t * 2)));
    });
    break;
  case GX_TF_RGB5A3:
    parBlocs(src, largeur, hauteur, 4, 4, 32, [&](uint32_t x0, uint32_t y0, const uint8_t* b) {
      for (uint32_t t = 0; t < 16; t++) poser(im, x0 + t % 4, y0 + t / 4, depuis5A3(be16(b + t * 2)));
    });
    break;
  case GX_TF_RGBA8:
    parBlocs(src, largeur, hauteur, 4, 4, 64, [&](uint32_t x0, uint32_t y0, const uint8_t* b) {
      for (uint32_t t = 0; t < 16; t++) {
        const uint32_t a = b[t * 2], r = b[t * 2 + 1];
        const uint32_t g = b[32 + t * 2], bl = b[32 + t * 2 + 1];
        poser(im, x0 + t % 4, y0 + t / 4, rgba(r, g, bl, a));
      }
    });
    break;
  case GX_TF_C4:
    parBlocs(src, largeur, hauteur, 8, 8, 32, [&](uint32_t x0, uint32_t y0, const uint8_t* b) {
      for (uint32_t ty = 0; ty < 8; ty++)
        for (uint32_t tx = 0; tx < 8; tx++) {
          const uint8_t o = b[ty * 4 + tx / 2];
          const uint32_t idx = (tx & 1) ? (o & 15) : (o >> 4);
          poser(im, x0 + tx, y0 + ty, couleurTlut(tlut, formatTlut, nbTlut, idx));
        }
    });
    break;
  case GX_TF_C8:
    parBlocs(src, largeur, hauteur, 8, 4, 32, [&](uint32_t x0, uint32_t y0, const uint8_t* b) {
      for (uint32_t ty = 0; ty < 4; ty++)
        for (uint32_t tx = 0; tx < 8; tx++)
          poser(im, x0 + tx, y0 + ty, couleurTlut(tlut, formatTlut, nbTlut, b[ty * 8 + tx]));
    });
    break;
  case GX_TF_C14X2:
    parBlocs(src, largeur, hauteur, 4, 4, 32, [&](uint32_t x0, uint32_t y0, const uint8_t* b) {
      for (uint32_t t = 0; t < 16; t++)
        poser(im, x0 + t % 4, y0 + t / 4, couleurTlut(tlut, formatTlut, nbTlut, be16(b + t * 2) & 0x3FFF));
    });
    break;
  case GX_TF_CMPR:
    parBlocs(src, largeur, hauteur, 8, 8, 32, [&](uint32_t x0, uint32_t y0, const uint8_t* b) {
      decoderCmprSousBloc(b, im, x0, y0);
      decoderCmprSousBloc(b + 8, im, x0 + 4, y0);
      decoderCmprSousBloc(b + 16, im, x0, y0 + 4);
      decoderCmprSousBloc(b + 24, im, x0 + 4, y0 + 4);
    });
    break;
  default:
    return false;
  }

  im.aAlpha = false;
  im.alphaBinaire = true;
  for (uint32_t c : im.rgba) {
    const uint32_t a = alpha(c);
    if (a != 255) {
      im.aAlpha = true;
      if (a != 0) im.alphaBinaire = false;
    }
  }
  return true;
}

FormatPica choisirFormat(uint32_t formatGx, uint32_t formatTlut, const Image& im) noexcept {
  switch (formatGx) {
  case GX_TF_I4:
  case GX_TF_IA4:
    return PICA_LA4;
  case GX_TF_I8:
  case GX_TF_IA8:
    return PICA_LA8;
  case GX_TF_RGB565:
    return PICA_RGB565;
  case GX_TF_RGBA8:
    return PICA_RGBA8;
  case GX_TF_CMPR:
    return im.aAlpha ? PICA_RGBA5551 : PICA_RGB565;
  case GX_TF_C4:
  case GX_TF_C8:
  case GX_TF_C14X2:
    if (formatTlut == GX_TL_IA8) return PICA_LA8;
    if (formatTlut == GX_TL_RGB565) return PICA_RGB565;
    [[fallthrough]];
  default:  // RGB5A3
    if (!im.aAlpha) return PICA_RGB565;
    return im.alphaBinaire ? PICA_RGBA5551 : PICA_RGBA4;
  }
}

void adapterTaille(Image& im, uint32_t maxDim) noexcept {
  maxDim = std::clamp<uint32_t>(maxDim, 8, 1024);
  // Dimensions non puissances de 2 : rééchantillonnage
  if (!puissanceDe2(im.largeur) || !puissanceDe2(im.hauteur) || im.largeur < 8 || im.hauteur < 8) {
    const uint32_t l = std::min<uint32_t>(puissanceSuperieure(im.largeur), 1024u);
    const uint32_t h = std::min<uint32_t>(puissanceSuperieure(im.hauteur), 1024u);
    std::vector<uint32_t> n(static_cast<size_t>(l) * h);
    for (uint32_t y = 0; y < h; y++) {
      const uint32_t sy = std::min(y * im.hauteur / h, im.hauteur - 1);
      for (uint32_t x = 0; x < l; x++) {
        const uint32_t sx = std::min(x * im.largeur / l, im.largeur - 1);
        n[y * l + x] = im.rgba[sy * im.largeur + sx];
      }
    }
    im.rgba.swap(n);
    im.largeur = l;
    im.hauteur = h;
  }
  // Réduction de moitié (moyenne de 2x2 texels) tant que c'est trop grand
  while ((im.largeur > maxDim || im.hauteur > maxDim) && im.largeur >= 16 && im.hauteur >= 16) {
    const uint32_t l = im.largeur / 2, h = im.hauteur / 2;
    std::vector<uint32_t> n(static_cast<size_t>(l) * h);
    for (uint32_t y = 0; y < h; y++) {
      for (uint32_t x = 0; x < l; x++) {
        const uint32_t a = im.rgba[(2 * y) * im.largeur + 2 * x];
        const uint32_t b = im.rgba[(2 * y) * im.largeur + 2 * x + 1];
        const uint32_t c = im.rgba[(2 * y + 1) * im.largeur + 2 * x];
        const uint32_t d = im.rgba[(2 * y + 1) * im.largeur + 2 * x + 1];
        n[y * l + x] = rgba((rouge(a) + rouge(b) + rouge(c) + rouge(d) + 2) / 4,
                            (vert(a) + vert(b) + vert(c) + vert(d) + 2) / 4,
                            (bleu(a) + bleu(b) + bleu(c) + bleu(d) + 2) / 4,
                            (alpha(a) + alpha(b) + alpha(c) + alpha(d) + 2) / 4);
      }
    }
    im.rgba.swap(n);
    im.largeur = l;
    im.hauteur = h;
  }
}

uint32_t octetsParTexel(FormatPica f) noexcept {
  switch (f) {
  case PICA_RGBA8:
    return 4;
  case PICA_RGB8:
    return 3;
  case PICA_RGBA5551:
  case PICA_RGB565:
  case PICA_RGBA4:
  case PICA_LA8:
    return 2;
  default:
    return 1;
  }
}

void encoder(const Image& im, FormatPica format, uint8_t* sortie) noexcept {
  const uint32_t l = im.largeur, h = im.hauteur;
  const uint32_t opt = octetsParTexel(format);
  for (uint32_t r = 0; r < h; r++) {
    // La PICA lit la ligne 0 de la mémoire pour la coordonnée v = 1 : on retourne l'image
    const uint32_t y = h - 1 - r;
    for (uint32_t x = 0; x < l; x++) {
      const uint32_t c = im.rgba[r * l + x];
      const uint32_t indice = ((y >> 3) * (l >> 3) + (x >> 3)) * 64 + morton8(x & 7, y & 7);
      uint8_t* p = sortie + indice * opt;
      const uint32_t R = rouge(c), G = vert(c), B = bleu(c), A = alpha(c);
      switch (format) {
      case PICA_RGBA8:
        p[0] = static_cast<uint8_t>(A);
        p[1] = static_cast<uint8_t>(B);
        p[2] = static_cast<uint8_t>(G);
        p[3] = static_cast<uint8_t>(R);
        break;
      case PICA_RGB8:
        p[0] = static_cast<uint8_t>(B);
        p[1] = static_cast<uint8_t>(G);
        p[2] = static_cast<uint8_t>(R);
        break;
      case PICA_RGBA5551: {
        const uint32_t v = (R >> 3) << 11 | (G >> 3) << 6 | (B >> 3) << 1 | (A >= 128 ? 1 : 0);
        p[0] = static_cast<uint8_t>(v);
        p[1] = static_cast<uint8_t>(v >> 8);
        break;
      }
      case PICA_RGB565: {
        const uint32_t v = (R >> 3) << 11 | (G >> 2) << 5 | (B >> 3);
        p[0] = static_cast<uint8_t>(v);
        p[1] = static_cast<uint8_t>(v >> 8);
        break;
      }
      case PICA_RGBA4: {
        const uint32_t v = (R >> 4) << 12 | (G >> 4) << 8 | (B >> 4) << 4 | (A >> 4);
        p[0] = static_cast<uint8_t>(v);
        p[1] = static_cast<uint8_t>(v >> 8);
        break;
      }
      case PICA_LA8:
        p[0] = static_cast<uint8_t>(A);
        p[1] = static_cast<uint8_t>(R);
        break;
      case PICA_LA4:
        p[0] = static_cast<uint8_t>((R >> 4) << 4 | (A >> 4));
        break;
      case PICA_L8:
        p[0] = static_cast<uint8_t>(R);
        break;
      case PICA_A8:
        p[0] = static_cast<uint8_t>(A);
        break;
      }
    }
  }
}

} // namespace gx3ds::conversion
