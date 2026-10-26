// Moteur de rendu : état GX (aurora::gx::g_gxState) -> PICA200 (citro3d).
#include "gx3ds.hpp"

#include "sommets.hpp"
#include "tev.hpp"
#include "textures.hpp"

#include "lib/gx/gx.hpp"

#include <3ds.h>
#include <citro3d.h>

#include "gx3ds_shbin.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

using aurora::gx::g_gxState;

namespace gx3ds {
namespace {

// Registres du shader (même ordre que les déclarations de gx3ds.v.pica)
constexpr int U_PROJECTION = 0;
constexpr int U_POSMTX = 4;
constexpr int U_TEXMTX = 34;
constexpr int U_MATCOL = 43;
constexpr int U_AMBCOL = 44;
constexpr int U_LUMIERES = 45;
constexpr int I_BOUCLE = 0;
enum : int {
  B_ECLAIRAGE = 0,
  B_MATVTX,
  B_AMBVTX,
  B_ALPHAVTX,
  B_ATTENUATION,
  B_PROJ0,
  B_PROJ1,
  B_PROJ2,
  B_AVECLUMIERES,
};
// Registres d'entrée du shader
constexpr int V_POS = 0, V_NRM = 1, V_CLR = 2, V_TC0 = 3, V_MTX = 6;

constexpr int MaxLumieres = 4;

struct Zone {
  float x, y, l, h;
};

Reglages sReglages;
Stats sStats;
Stats sStatsAffichees;

C3D_RenderTarget* sCible = nullptr;
DVLB_s* sDvlb = nullptr;
shaderProgram_s sProgramme;

uint8_t* sSommets = nullptr;
constexpr uint32_t TailleSommets = 6u * 1024u * 1024u;
uint32_t sPosSommets = 0;
uint16_t* sIndicesQuads = nullptr;
constexpr uint32_t MaxQuads = 16384;
uint16_t* sIndices = nullptr;
constexpr uint32_t MaxIndices = 256u * 1024u;
uint32_t sPosIndices = 0;

FonctionJournal sJournal = nullptr;
uint32_t sJournalRestant = 0;
uint32_t sNumeroDessin = 0;

bool sInitialise = false;
bool sImageEnCours = false;
uint32_t sNumeroImage = 1;
sommets::Decodeur sDecodeur;

tev::Entree sDerniereEntree;
tev::Programme sDernierProgramme;
bool sTevValide = false;

// Uniformes déjà envoyés (on n'envoie que ce qui change)
float sUnif[96][4];
bool sUnifValide[96];

Zone zone() {
  if (sReglages.format43) return {40.f, 0.f, 320.f, 240.f};
  return {0.f, 0.f, 400.f, 240.f};
}

void unif(int reg, float x, float y, float z, float w) {
  float* u = sUnif[reg];
  if (sUnifValide[reg] && u[0] == x && u[1] == y && u[2] == z && u[3] == w) return;
  u[0] = x;
  u[1] = y;
  u[2] = z;
  u[3] = w;
  sUnifValide[reg] = true;
  C3D_FVUnifSet(GPU_VERTEX_SHADER, reg, x, y, z, w);
}

void invaliderUniformes() {
  for (bool& b : sUnifValide) b = false;
}

// ----------------------------------------------------------------------------------------------
// Correspondances GX -> PICA

GPU_TESTFUNC comparaison(GXCompare c) {
  switch (c) {
  case GX_NEVER:
    return GPU_NEVER;
  case GX_LESS:
    return GPU_LESS;
  case GX_EQUAL:
    return GPU_EQUAL;
  case GX_LEQUAL:
    return GPU_LEQUAL;
  case GX_GREATER:
    return GPU_GREATER;
  case GX_NEQUAL:
    return GPU_NOTEQUAL;
  case GX_GEQUAL:
    return GPU_GEQUAL;
  default:
    return GPU_ALWAYS;
  }
}

GPU_BLENDFACTOR facteur(GXBlendFactor f, bool destination, bool alphaCible) {
  switch (f) {
  case GX_BL_ZERO:
    return GPU_ZERO;
  case GX_BL_ONE:
    return GPU_ONE;
  case GX_BL_SRCCLR:  // = GX_BL_DSTCLR
    return destination ? GPU_SRC_COLOR : GPU_DST_COLOR;
  case GX_BL_INVSRCCLR:
    return destination ? GPU_ONE_MINUS_SRC_COLOR : GPU_ONE_MINUS_DST_COLOR;
  case GX_BL_SRCALPHA:
    return GPU_SRC_ALPHA;
  case GX_BL_INVSRCALPHA:
    return GPU_ONE_MINUS_SRC_ALPHA;
  case GX_BL_DSTALPHA:
    return alphaCible ? GPU_DST_ALPHA : GPU_ONE;
  case GX_BL_INVDSTALPHA:
    return alphaCible ? GPU_ONE_MINUS_DST_ALPHA : GPU_ZERO;
  default:
    return GPU_ONE;
  }
}

GPU_LOGICOP operationLogique(GXLogicOp op) {
  static constexpr GPU_LOGICOP tab[16] = {
      GPU_LOGICOP_CLEAR, GPU_LOGICOP_AND,  GPU_LOGICOP_AND_REVERSE, GPU_LOGICOP_COPY,
      GPU_LOGICOP_AND_INVERTED, GPU_LOGICOP_NOOP, GPU_LOGICOP_XOR,  GPU_LOGICOP_OR,
      GPU_LOGICOP_NOR,   GPU_LOGICOP_EQUIV, GPU_LOGICOP_INVERT,  GPU_LOGICOP_OR_REVERSE,
      GPU_LOGICOP_COPY_INVERTED, GPU_LOGICOP_OR_INVERTED, GPU_LOGICOP_NAND, GPU_LOGICOP_SET,
  };
  return tab[op & 15];
}

GPU_TEXTURE_WRAP_PARAM enveloppe(GXTexWrapMode m) {
  switch (m) {
  case GX_REPEAT:
    return GPU_REPEAT;
  case GX_MIRROR:
    return GPU_MIRRORED_REPEAT;
  default:
    return GPU_CLAMP_TO_EDGE;
  }
}

// ----------------------------------------------------------------------------------------------
// Préparation d'un dessin

struct Unite {
  uint8_t texMap;
  uint8_t texCoord;
};

struct Preparation {
  Unite unites[3];
  int nbUnites = 0;
  int8_t uniteEtage[16];
  sommets::Besoins besoins;
};

bool etageUtiliseTexture(const aurora::gx::TevStage& s) {
  const auto& c = s.colorPass;
  const auto& a = s.alphaPass;
  auto cc = [](GXTevColorArg x) { return x == GX_CC_TEXC || x == GX_CC_TEXA; };
  auto ca = [](GXTevAlphaArg x) { return x == GX_CA_TEXA; };
  return cc(c.a) || cc(c.b) || cc(c.c) || cc(c.d) || ca(a.a) || ca(a.b) || ca(a.c) || ca(a.d);
}

void attribuerUnites(Preparation& p) {
  const int nb = std::clamp<int>(g_gxState.numTevStages, 1, 16);
  for (int s = 0; s < 16; s++) p.uniteEtage[s] = -1;
  for (int s = 0; s < nb; s++) {
    const auto& st = g_gxState.tevStages[s];
    if (st.texMapId == GX_TEXMAP_NULL || st.texMapId >= GX_MAX_TEXMAP) continue;
    if (!etageUtiliseTexture(st)) continue;
    const uint8_t tc = st.texCoordId < GX_MAX_TEXCOORD ? static_cast<uint8_t>(st.texCoordId) : 0;
    int trouve = -1;
    for (int u = 0; u < p.nbUnites; u++) {
      if (p.unites[u].texMap == st.texMapId && p.unites[u].texCoord == tc) trouve = u;
    }
    if (trouve < 0 && p.nbUnites < 3) {
      trouve = p.nbUnites++;
      p.unites[trouve] = {static_cast<uint8_t>(st.texMapId), tc};
    }
    if (trouve < 0) sStats.tevApproximes++;
    p.uniteEtage[s] = static_cast<int8_t>(trouve);
  }
}

uint8_t sourceTexgen(const aurora::gx::TcgConfig& t) {
  switch (t.src) {
  case GX_TG_POS:
  case GX_TG_NRM:
    return static_cast<uint8_t>(t.src);
  default:
    if (t.src >= GX_TG_TEX0 && t.src <= GX_TG_TEX7) return static_cast<uint8_t>(t.src);
    return GX_TG_TEX0;  // couleur, binormale... : approximation
  }
}

// ----------------------------------------------------------------------------------------------
// TEV

void preparerTev(const Preparation& p) {
  tev::Entree e{};
  e.nbEtages = static_cast<uint8_t>(std::clamp<int>(g_gxState.numTevStages, 1, 16));
  for (int s = 0; s < e.nbEtages; s++) {
    const auto& st = g_gxState.tevStages[s];
    tev::EtageGx& g = e.etages[s];
    g.c[0] = st.colorPass.a;
    g.c[1] = st.colorPass.b;
    g.c[2] = st.colorPass.c;
    g.c[3] = st.colorPass.d;
    g.a[0] = st.alphaPass.a;
    g.a[1] = st.alphaPass.b;
    g.a[2] = st.alphaPass.c;
    g.a[3] = st.alphaPass.d;
    g.opC = st.colorOp.op;
    g.biaisC = st.colorOp.bias;
    g.echelleC = st.colorOp.scale;
    g.sortieC = st.colorOp.outReg;
    g.opA = st.alphaOp.op;
    g.biaisA = st.alphaOp.bias;
    g.echelleA = st.alphaOp.scale;
    g.sortieA = st.alphaOp.outReg;
    g.kcSel = st.kcSel;
    g.kaSel = st.kaSel;
    g.canal = st.channelId == GX_COLOR_NULL ? GX_COLOR_ZERO : st.channelId;
    g.unite = p.uniteEtage[s];
    g.echangeRas = st.tevSwapRas;
    g.echangeTex = st.tevSwapTex;
  }
  for (int i = 0; i < 4; i++) {
    const auto& sw = g_gxState.tevSwapTable[i];
    e.echanges[i] = {static_cast<uint8_t>(sw.red), static_cast<uint8_t>(sw.green), static_cast<uint8_t>(sw.blue),
                     static_cast<uint8_t>(sw.alpha)};
    const auto& r = g_gxState.colorRegs[i];
    e.registres[i] = {std::clamp(r[0], 0.f, 1.f), std::clamp(r[1], 0.f, 1.f), std::clamp(r[2], 0.f, 1.f),
                      std::clamp(r[3], 0.f, 1.f)};
    const auto& k = g_gxState.kcolors[i];
    e.konst[i] = {k[0], k[1], k[2], k[3]};
  }

  if (!sTevValide || std::memcmp(&e, &sDerniereEntree, sizeof(e)) != 0) {
    sDerniereEntree = e;
    tev::compiler(e, sDernierProgramme);
    sTevValide = true;
    if (sDernierProgramme.approxime) sStats.tevApproximes++;
  }

  const tev::Programme& prog = sDernierProgramme;
  int masqueRvb = 0, masqueA = 0;
  for (int i = 0; i < 6; i++) {
    const tev::EtagePica& et = prog.etages[i];
    C3D_TexEnv* env = C3D_GetTexEnv(i);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_RGB, static_cast<GPU_TEVSRC>(et.srcRvb[0]), static_cast<GPU_TEVSRC>(et.srcRvb[1]),
                  static_cast<GPU_TEVSRC>(et.srcRvb[2]));
    C3D_TexEnvSrc(env, C3D_Alpha, static_cast<GPU_TEVSRC>(et.srcA[0]), static_cast<GPU_TEVSRC>(et.srcA[1]),
                  static_cast<GPU_TEVSRC>(et.srcA[2]));
    C3D_TexEnvOpRgb(env, static_cast<GPU_TEVOP_RGB>(et.opRvb[0]), static_cast<GPU_TEVOP_RGB>(et.opRvb[1]),
                    static_cast<GPU_TEVOP_RGB>(et.opRvb[2]));
    C3D_TexEnvOpAlpha(env, static_cast<GPU_TEVOP_A>(et.opA[0]), static_cast<GPU_TEVOP_A>(et.opA[1]),
                      static_cast<GPU_TEVOP_A>(et.opA[2]));
    C3D_TexEnvFunc(env, C3D_RGB, static_cast<GPU_COMBINEFUNC>(et.fRvb));
    C3D_TexEnvFunc(env, C3D_Alpha, static_cast<GPU_COMBINEFUNC>(et.fA));
    C3D_TexEnvScale(env, C3D_RGB, static_cast<GPU_TEVSCALE>(et.echelleRvb));
    C3D_TexEnvScale(env, C3D_Alpha, static_cast<GPU_TEVSCALE>(et.echelleA));
    C3D_TexEnvColor(env, et.constante);
    if (i < 4 && et.tamponRvb) masqueRvb |= 1 << i;
    if (i < 4 && et.tamponA) masqueA |= 1 << i;
  }
  C3D_TexEnvBufUpdate(C3D_RGB, masqueRvb);
  C3D_TexEnvBufUpdate(C3D_Alpha, masqueA);
  C3D_TexEnvBufColor(prog.couleurTampon);
}

// ----------------------------------------------------------------------------------------------
// Textures

void lierTextures(const Preparation& p) {
  for (int u = 0; u < 3; u++) {
    if (u >= p.nbUnites) {
      C3D_TexBind(u, nullptr);
      continue;
    }
    const GXTexObj_& obj = g_gxState.loadedTextures[p.unites[u].texMap];
    C3D_Tex* tex;
    if (g_gxState.copyTextures.count(obj.data) != 0) {
      tex = textures::transparente();  // copie de l'image : pas encore gérée
    } else {
      const GXTlutObj_* tlut = nullptr;
      if (obj.tlut < aurora::gx::MaxTluts) tlut = &g_gxState.loadedTluts[obj.tlut];
      tex = textures::obtenir(obj, tlut, sNumeroImage);
    }
    const auto mag = obj.mag_filter() == GX_LINEAR ? GPU_LINEAR : GPU_NEAREST;
    const auto minf = obj.min_filter() == GX_NEAR || obj.min_filter() == GX_NEAR_MIP_NEAR ||
                              obj.min_filter() == GX_NEAR_MIP_LIN
                          ? GPU_NEAREST
                          : GPU_LINEAR;
    C3D_TexSetFilter(tex, mag, minf);
    C3D_TexSetWrap(tex, enveloppe(obj.wrap_s()), enveloppe(obj.wrap_t()));
    C3D_TexBind(u, tex);
  }
}

// ----------------------------------------------------------------------------------------------
// Uniformes : projection, matrices, éclairage, coordonnées de texture

void envoyerProjection() {
  const auto& p = g_gxState.proj;
  // Rotation pour l'écran de la 3DS (tourné d'un quart de tour) : x' = y, y' = -x
  unif(U_PROJECTION + 0, p.m1[0], p.m1[1], p.m1[2], p.m1[3]);
  unif(U_PROJECTION + 1, -p.m0[0], -p.m0[1], -p.m0[2], -p.m0[3]);
  unif(U_PROJECTION + 2, p.m2[0], p.m2[1], p.m2[2], p.m2[3]);
  unif(U_PROJECTION + 3, p.m3[0], p.m3[1], p.m3[2], p.m3[3]);
}

void envoyerMatrices() {
  for (int m = 0; m < 10; m++) {
    const auto& pos = g_gxState.pnMtx[m].pos;
    unif(U_POSMTX + m * 3 + 0, pos.m0[0], pos.m0[1], pos.m0[2], pos.m0[3]);
    unif(U_POSMTX + m * 3 + 1, pos.m1[0], pos.m1[1], pos.m1[2], pos.m1[3]);
    unif(U_POSMTX + m * 3 + 2, pos.m2[0], pos.m2[1], pos.m2[2], pos.m2[3]);
  }
}

using Ligne = float[4];

void matriceTexgen(int unite, uint8_t texCoord, bool& projetee) {
  const auto& t = g_gxState.tcgs[texCoord];
  float m[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
  if (t.mtx != GX_IDENTITY && t.mtx >= GX_TEXMTX0) {
    const uint32_t idx = (t.mtx - GX_TEXMTX0) / 3;
    if (idx < aurora::gx::MaxTexMtx) {
      const auto& tm = g_gxState.texMtxs[idx];
      for (int c = 0; c < 4; c++) {
        m[0][c] = tm.m0[c];
        m[1][c] = tm.m1[c];
        m[2][c] = tm.m2[c];
      }
    }
  }
  projetee = t.type == GX_TG_MTX3x4;
  if (!projetee) {
    m[2][0] = m[2][1] = m[2][2] = 0.f;
    m[2][3] = 1.f;
  }
  // Matrice « post » (GXSetTexCoordGen2)
  if (t.postMtx != GX_PTIDENTITY && t.postMtx >= GX_PTTEXMTX0) {
    const uint32_t idx = (t.postMtx - GX_PTTEXMTX0) / 3;
    if (idx < aurora::gx::MaxPTTexMtx) {
      const auto& pm = g_gxState.ptTexMtxs[idx];
      const float post[3][4] = {{pm.m0[0], pm.m0[1], pm.m0[2], pm.m0[3]},
                                {pm.m1[0], pm.m1[1], pm.m1[2], pm.m1[3]},
                                {pm.m2[0], pm.m2[1], pm.m2[2], pm.m2[3]}};
      float r[3][4];
      for (int i = 0; i < 3; i++) {
        for (int c = 0; c < 4; c++) {
          r[i][c] = post[i][0] * m[0][c] + post[i][1] * m[1][c] + post[i][2] * m[2][c] + (c == 3 ? post[i][3] : 0.f);
        }
      }
      std::memcpy(m, r, sizeof(m));
    }
  }
  // Coordonnées (s, t) : l'entrée du shader vaut (s, t, 0, 1) alors que la GameCube utilise
  // (s, t, 1, 1) : on reporte la 3e colonne dans la 4e.
  const uint8_t src = sourceTexgen(t);
  if (src >= GX_TG_TEX0) {
    for (int i = 0; i < 3; i++) {
      m[i][3] += m[i][2];
      m[i][2] = 0.f;
    }
  }
  for (int i = 0; i < 3; i++) unif(U_TEXMTX + unite * 3 + i, m[i][0], m[i][1], m[i][2], m[i][3]);
}

void envoyerEclairage(const Preparation& p) {
  const auto& cc = g_gxState.colorChannelConfig[GX_COLOR0];
  const auto& ca = g_gxState.colorChannelConfig[GX_ALPHA0];
  const auto& sc = g_gxState.colorChannelState[GX_COLOR0];
  const auto& sa = g_gxState.colorChannelState[GX_ALPHA0];
  unif(U_MATCOL, sc.matColor[0], sc.matColor[1], sc.matColor[2], sa.matColor[3]);
  unif(U_AMBCOL, sc.ambColor[0], sc.ambColor[1], sc.ambColor[2], sa.ambColor[3]);

  const bool eclairage = cc.lightingEnabled && g_gxState.numChans > 0;
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_ECLAIRAGE, eclairage);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_MATVTX, cc.matSrc == GX_SRC_VTX);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_AMBVTX, cc.ambSrc == GX_SRC_VTX);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_ALPHAVTX, ca.matSrc == GX_SRC_VTX);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_ATTENUATION, cc.attnFn == GX_AF_SPOT);

  int nb = 0;
  if (eclairage && cc.attnFn != GX_AF_SPEC) {
    for (int l = 0; l < 8 && nb < MaxLumieres; l++) {
      if (!sc.lightMask.test(l)) continue;
      const auto& lum = g_gxState.lights[l];
      const int base = U_LUMIERES + nb * 5;
      unif(base + 0, lum.pos[0], lum.pos[1], lum.pos[2], 1.f);
      unif(base + 1, lum.dir[0], lum.dir[1], lum.dir[2], 0.f);
      unif(base + 2, lum.color[0], lum.color[1], lum.color[2], lum.color[3]);
      unif(base + 3, lum.cosAtt[0], lum.cosAtt[1], lum.cosAtt[2], 0.f);
      unif(base + 4, lum.distAtt[0], lum.distAtt[1], lum.distAtt[2], 0.f);
      nb++;
    }
  }
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_AVECLUMIERES, nb > 0);
  if (nb > 0) C3D_IVUnifSet(GPU_VERTEX_SHADER, I_BOUCLE, nb - 1, 0, 5, 0);
  (void)p;
}

// ----------------------------------------------------------------------------------------------
// État du rendu (profondeur, mélange, culling, affichage)

void appliquerEtat() {
  const bool alphaCible = g_gxState.pixelFmt == GX_PF_RGBA6_Z24;

  // Culling (l'écran de la 3DS est tourné : la rotation conserve le sens des triangles)
  switch (g_gxState.cullMode) {
  case GX_CULL_FRONT:
    C3D_CullFace(GPU_CULL_FRONT_CCW);
    break;
  case GX_CULL_BACK:
    C3D_CullFace(GPU_CULL_BACK_CCW);
    break;
  default:
    C3D_CullFace(GPU_CULL_NONE);
    break;
  }

  // Profondeur et masque d'écriture
  int masque = 0;
  if (g_gxState.colorUpdate) masque |= GPU_WRITE_RED | GPU_WRITE_GREEN | GPU_WRITE_BLUE;
  if (g_gxState.alphaUpdate) masque |= GPU_WRITE_ALPHA;
  if (g_gxState.depthCompare && g_gxState.depthUpdate) masque |= GPU_WRITE_DEPTH;
  C3D_DepthTest(true, g_gxState.depthCompare ? comparaison(g_gxState.depthFunc) : GPU_ALWAYS,
                static_cast<GPU_WRITEMASK>(masque));

  // Test alpha (la PICA n'a qu'une comparaison)
  const auto& ac = g_gxState.alphaCompare;
  if (ac.comp0 == GX_ALWAYS && ac.comp1 == GX_ALWAYS) {
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
  } else if (ac.op == GX_AOP_AND && ac.comp1 == GX_ALWAYS) {
    C3D_AlphaTest(true, comparaison(ac.comp0), static_cast<int>(ac.ref0));
  } else if (ac.op == GX_AOP_AND && ac.comp0 == GX_ALWAYS) {
    C3D_AlphaTest(true, comparaison(ac.comp1), static_cast<int>(ac.ref1));
  } else if (ac.op == GX_AOP_OR && ac.comp1 == GX_NEVER) {
    C3D_AlphaTest(true, comparaison(ac.comp0), static_cast<int>(ac.ref0));
  } else if (ac.op == GX_AOP_OR && ac.comp0 == GX_NEVER) {
    C3D_AlphaTest(true, comparaison(ac.comp1), static_cast<int>(ac.ref1));
  } else {
    C3D_AlphaTest(true, comparaison(ac.comp0), static_cast<int>(ac.ref0));
  }

  // Mélange
  switch (g_gxState.blendMode) {
  case GX_BM_BLEND:
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, facteur(g_gxState.blendFacSrc, false, alphaCible),
                   facteur(g_gxState.blendFacDst, true, alphaCible), facteur(g_gxState.blendFacSrc, false, alphaCible),
                   facteur(g_gxState.blendFacDst, true, alphaCible));
    break;
  case GX_BM_SUBTRACT:
    C3D_AlphaBlend(GPU_BLEND_REVERSE_SUBTRACT, GPU_BLEND_REVERSE_SUBTRACT, GPU_ONE, GPU_ONE, GPU_ONE, GPU_ONE);
    break;
  case GX_BM_LOGIC:
    C3D_ColorLogicOp(operationLogique(g_gxState.blendOp));
    break;
  default:
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    break;
  }

  // Affichage : coordonnées d'écran (x vers la droite, y vers le bas) -> image tournée de la 3DS
  const auto& vp = g_gxState.renderViewport;
  const int vx = static_cast<int>(std::lround(240.f - (vp.top + vp.height)));
  const int vy = static_cast<int>(std::lround(400.f - (vp.left + vp.width)));
  const int vl = static_cast<int>(std::lround(vp.height));
  const int vh = static_cast<int>(std::lround(vp.width));
  C3D_SetViewport(static_cast<u32>(vx), static_cast<u32>(vy), static_cast<u32>(std::max(vl, 1)),
                  static_cast<u32>(std::max(vh, 1)));
  C3D_DepthMap(true, vp.zfar - vp.znear, vp.zfar);

  // Ciseaux : intersection avec la zone de l'image
  const Zone z = zone();
  const auto& sc = g_gxState.renderScissor;
  const float g = std::max<float>(static_cast<float>(sc.x), z.x);
  const float h = std::max<float>(static_cast<float>(sc.y), z.y);
  const float d = std::min<float>(static_cast<float>(sc.x + sc.width), z.x + z.l);
  const float b = std::min<float>(static_cast<float>(sc.y + sc.height), z.y + z.h);
  if (d <= g || b <= h) {
    C3D_SetScissor(GPU_SCISSOR_NORMAL, 0, 0, 1, 1);
  } else {
    const u32 x0 = static_cast<u32>(std::clamp(240.f - b, 0.f, 240.f));
    const u32 x1 = static_cast<u32>(std::clamp(240.f - h, 0.f, 240.f));
    const u32 y0 = static_cast<u32>(std::clamp(400.f - d, 0.f, 400.f));
    const u32 y1 = static_cast<u32>(std::clamp(400.f - g, 0.f, 400.f));
    C3D_SetScissor(GPU_SCISSOR_NORMAL, x0, y0, x1, y1);
  }
}

// ----------------------------------------------------------------------------------------------
// Sommets

void remplirFormat(GXVtxFmt fmt, sommets::FormatGx& f) {
  const auto& vf = g_gxState.vtxFmts[fmt];
  for (int a = 0; a < GX_VA_MAX_ATTR; a++) {
    f.attr[a] = {};
    f.tableaux[a] = {};
  }
  for (int a = GX_VA_PNMTXIDX; a <= GX_VA_TEX7; a++) {
    auto& at = f.attr[a];
    at.mode = static_cast<uint8_t>(g_gxState.vtxDesc[a]);
    if (at.mode == GX_NONE) continue;
    const auto& af = vf.attrs[a];
    at.type = static_cast<uint8_t>(af.type);
    at.frac = af.frac;
    switch (a) {
    case GX_VA_POS:
      at.nb = af.cnt == GX_POS_XY ? 2 : 3;
      break;
    case GX_VA_NRM:
      at.nb = af.cnt == GX_NRM_XYZ ? 3 : 9;
      at.nbt3 = af.cnt == GX_NRM_NBT3;
      break;
    case GX_VA_CLR0:
    case GX_VA_CLR1:
      at.nb = 1;
      break;
    default:
      if (a >= GX_VA_TEX0) {
        at.nb = af.cnt == GX_TEX_S ? 1 : 2;
      } else {
        at.nb = 1;
      }
      break;
    }
    if (a >= GX_VA_POS && (at.mode == GX_INDEX8 || at.mode == GX_INDEX16)) {
      const auto& arr = g_gxState.arrays[a];
      f.tableaux[a] = {static_cast<const uint8_t*>(arr.data), arr.size, arr.stride, arr.le};
    }
  }
}

void configurerAttributs(const sommets::Disposition& d, void* donnees) {
  C3D_AttrInfo* ai = C3D_GetAttrInfo();
  AttrInfo_Init(ai);
  int nbLoaders = 0;
  // Attributs présents dans le tampon, dans l'ordre de la mémoire
  struct Elem {
    int8_t off;
    int reg;
    GPU_FORMATS fmt;
    int nb;
  };
  Elem elems[8];
  int ne = 0;
  elems[ne++] = {d.pos, V_POS, GPU_FLOAT, 3};
  if (d.nrm >= 0) elems[ne++] = {d.nrm, V_NRM, GPU_FLOAT, 3};
  for (int i = 0; i < sommets::MaxTex; i++)
    if (d.tex[i] >= 0) elems[ne++] = {d.tex[i], V_TC0 + i, GPU_FLOAT, d.compTex[i]};
  if (d.clr >= 0) elems[ne++] = {d.clr, V_CLR, GPU_UNSIGNED_BYTE, 4};
  if (d.mtx >= 0) elems[ne++] = {d.mtx, V_MTX, GPU_UNSIGNED_BYTE, 4};
  std::sort(elems, elems + ne, [](const Elem& a, const Elem& b) { return a.off < b.off; });
  u64 permutation = 0;
  for (int i = 0; i < ne; i++) {
    const int id = AttrInfo_AddLoader(ai, elems[i].reg, elems[i].fmt, elems[i].nb);
    permutation |= static_cast<u64>(id) << (nbLoaders * 4);
    nbLoaders++;
  }
  // Attributs absents : valeurs fixes
  if (d.nrm < 0) {
    const int id = AttrInfo_AddFixed(ai, V_NRM);
    C3D_FixedAttribSet(id, 0.f, 0.f, 1.f, 0.f);
  }
  if (d.clr < 0) {
    const int id = AttrInfo_AddFixed(ai, V_CLR);
    C3D_FixedAttribSet(id, 255.f, 255.f, 255.f, 255.f);
  }
  for (int i = 0; i < sommets::MaxTex; i++) {
    if (d.tex[i] < 0) {
      const int id = AttrInfo_AddFixed(ai, V_TC0 + i);
      C3D_FixedAttribSet(id, 0.f, 0.f, 0.f, 1.f);
    }
  }
  if (d.mtx < 0) {
    const int id = AttrInfo_AddFixed(ai, V_MTX);
    C3D_FixedAttribSet(id, static_cast<float>(g_gxState.currentPnMtx * 3), 0.f, 0.f, 0.f);
  }

  C3D_BufInfo* bi = C3D_GetBufInfo();
  BufInfo_Init(bi);
  BufInfo_Add(bi, donnees, d.taille, nbLoaders, permutation);
}

void* allouerSommets(uint32_t octets) {
  octets = (octets + 15) & ~15u;
  if (sPosSommets + octets > TailleSommets) return nullptr;
  void* p = sSommets + sPosSommets;
  sPosSommets += octets;
  return p;
}

bool preparerDessin(GXVtxFmt fmt, uint16_t n, const uint8_t* donnees, Preparation& p, void*& sortie) {
  attribuerUnites(p);
  preparerTev(p);

  // Besoins du shader
  const auto& cc = g_gxState.colorChannelConfig[GX_COLOR0];
  const auto& ca = g_gxState.colorChannelConfig[GX_ALPHA0];
  const bool eclairage = cc.lightingEnabled && g_gxState.numChans > 0;
  p.besoins.normale = eclairage && g_gxState.vtxDesc[GX_VA_NRM] != GX_NONE;
  p.besoins.couleur = g_gxState.vtxDesc[GX_VA_CLR0] != GX_NONE &&
                      (cc.matSrc == GX_SRC_VTX || ca.matSrc == GX_SRC_VTX || (eclairage && cc.ambSrc == GX_SRC_VTX));
  p.besoins.nbTex = static_cast<uint8_t>(p.nbUnites);
  for (int u = 0; u < p.nbUnites; u++) {
    const uint8_t s = sourceTexgen(g_gxState.tcgs[p.unites[u].texCoord]);
    p.besoins.sourceTex[u] = s;
    if (s == GX_TG_NRM && g_gxState.vtxDesc[GX_VA_NRM] == GX_NONE) p.besoins.sourceTex[u] = GX_TG_TEX0;
  }

  sommets::FormatGx f;
  remplirFormat(fmt, f);
  if (!sDecodeur.preparer(f, p.besoins)) return false;
  const auto& d = sDecodeur.disposition();
  sortie = allouerSommets(static_cast<uint32_t>(n) * d.taille);
  if (sortie == nullptr) {
    sStats.ignores++;
    return false;
  }
  sDecodeur.decoder(donnees, n, static_cast<uint8_t*>(sortie));
  GSPGPU_FlushDataCache(sortie, static_cast<uint32_t>(n) * d.taille);

  configurerAttributs(d, sortie);
  lierTextures(p);
  envoyerProjection();
  envoyerMatrices();
  bool proj[3] = {false, false, false};
  for (int u = 0; u < p.nbUnites; u++) matriceTexgen(u, p.unites[u].texCoord, proj[u]);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_PROJ0, proj[0]);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_PROJ1, proj[1]);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_PROJ2, proj[2]);
  envoyerEclairage(p);
  appliquerEtat();
  return true;
}

// ----------------------------------------------------------------------------------------------
// Effacement d'une zone (début d'image, copies avec effacement)

void effacerZone(float x, float y, float l, float h, uint32_t couleurRgba, float profondeur, bool couleur,
                 bool alpha, bool z) {
  struct Sommet {
    float x, y, z;
  };
  auto* v = static_cast<Sommet*>(allouerSommets(4 * sizeof(Sommet)));
  if (v == nullptr) return;
  v[0] = {x, y, 0.f};
  v[1] = {x, y + h, 0.f};
  v[2] = {x + l, y + h, 0.f};
  v[3] = {x + l, y, 0.f};
  GSPGPU_FlushDataCache(v, 4 * sizeof(Sommet));

  C3D_AttrInfo* ai = C3D_GetAttrInfo();
  AttrInfo_Init(ai);
  AttrInfo_AddLoader(ai, V_POS, GPU_FLOAT, 3);
  C3D_FixedAttribSet(AttrInfo_AddFixed(ai, V_NRM), 0.f, 0.f, 1.f, 0.f);
  C3D_FixedAttribSet(AttrInfo_AddFixed(ai, V_CLR), 255.f, 255.f, 255.f, 255.f);
  for (int i = 0; i < 3; i++) C3D_FixedAttribSet(AttrInfo_AddFixed(ai, V_TC0 + i), 0.f, 0.f, 0.f, 1.f);
  C3D_FixedAttribSet(AttrInfo_AddFixed(ai, V_MTX), 0.f, 0.f, 0.f, 0.f);
  C3D_BufInfo* bi = C3D_GetBufInfo();
  BufInfo_Init(bi);
  BufInfo_Add(bi, v, sizeof(Sommet), 1, 0x0);

  // Projection : pixels de l'écran du haut -> espace de découpage (avec la rotation)
  unif(U_PROJECTION + 0, 0.f, -2.f / 240.f, 0.f, 1.f);
  unif(U_PROJECTION + 1, -2.f / 400.f, 0.f, 0.f, 1.f);
  unif(U_PROJECTION + 2, 0.f, 0.f, 0.f, -0.5f);
  unif(U_PROJECTION + 3, 0.f, 0.f, 0.f, 1.f);
  unif(U_POSMTX + 0, 1.f, 0.f, 0.f, 0.f);
  unif(U_POSMTX + 1, 0.f, 1.f, 0.f, 0.f);
  unif(U_POSMTX + 2, 0.f, 0.f, 1.f, 0.f);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_ECLAIRAGE, false);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_MATVTX, false);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_ALPHAVTX, false);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_PROJ0, false);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_PROJ1, false);
  C3D_BoolUnifSet(GPU_VERTEX_SHADER, B_PROJ2, false);

  for (int i = 0; i < 6; i++) {
    C3D_TexEnv* env = C3D_GetTexEnv(i);
    C3D_TexEnvInit(env);
    if (i == 0) {
      C3D_TexEnvSrc(env, C3D_Both, GPU_CONSTANT, GPU_CONSTANT, GPU_CONSTANT);
      C3D_TexEnvColor(env, couleurRgba);
    }
  }
  C3D_TexEnvBufUpdate(C3D_Both, 0);
  sTevValide = false;
  for (int u = 0; u < 3; u++) C3D_TexBind(u, nullptr);

  int masque = 0;
  if (couleur) masque |= GPU_WRITE_RED | GPU_WRITE_GREEN | GPU_WRITE_BLUE;
  if (alpha) masque |= GPU_WRITE_ALPHA;
  if (z) masque |= GPU_WRITE_DEPTH;
  C3D_CullFace(GPU_CULL_NONE);
  C3D_AlphaTest(false, GPU_ALWAYS, 0);
  C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
  C3D_DepthTest(true, GPU_ALWAYS, static_cast<GPU_WRITEMASK>(masque));
  C3D_DepthMap(true, 0.f, profondeur);
  C3D_SetViewport(0, 0, 240, 400);
  C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
  C3D_DrawArrays(GPU_TRIANGLE_FAN, 0, 4);
}

// ----------------------------------------------------------------------------------------------
// Diagnostic

void journal(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void journal(const char* fmt, ...) {
  if (sJournal == nullptr) return;
  char texte[320];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(texte, sizeof texte, fmt, ap);
  va_end(ap);
  sJournal(texte);
}

const char* nomSource(GXColorSrc s) { return s == GX_SRC_VTX ? "sommet" : "registre"; }

void decrireDessin(int prim, uint16_t n, GXVtxFmt fmt, const Preparation& p, const void* sortie) {
  const auto& d = sDecodeur.disposition();
  journal("dessin %lu : prim 0x%02X, %u sommets, format %d, matrice %lu", static_cast<unsigned long>(sNumeroDessin),
          prim, n, static_cast<int>(fmt), static_cast<unsigned long>(g_gxState.currentPnMtx));
  journal("  besoins : normale %d couleur %d textures %d (sources %d %d %d)", p.besoins.normale, p.besoins.couleur,
          p.besoins.nbTex, p.besoins.sourceTex[0], p.besoins.sourceTex[1], p.besoins.sourceTex[2]);
  journal("  sommet PICA : %u octets, pos %d nrm %d clr %d tex %d/%d/%d mtx %d", d.taille, d.pos, d.nrm, d.clr,
          d.tex[0], d.tex[1], d.tex[2], d.mtx);
  const auto& cc = g_gxState.colorChannelConfig[GX_COLOR0];
  const auto& ca = g_gxState.colorChannelConfig[GX_ALPHA0];
  const auto& sc = g_gxState.colorChannelState[GX_COLOR0];
  journal("  canal 0 : %lu canaux, éclairage %d, matériau %s/%s, ambiante %s, atténuation %d, lumières %02lX",
          static_cast<unsigned long>(g_gxState.numChans), cc.lightingEnabled, nomSource(cc.matSrc),
          nomSource(ca.matSrc), nomSource(cc.ambSrc), static_cast<int>(cc.attnFn),
          static_cast<unsigned long>(sc.lightMask.to_ulong()));
  journal("  matériau (%.2f %.2f %.2f %.2f) ambiante (%.2f %.2f %.2f) ; booléens du shader %04X",
          static_cast<double>(sc.matColor[0]), static_cast<double>(sc.matColor[1]),
          static_cast<double>(sc.matColor[2]), static_cast<double>(sc.matColor[3]),
          static_cast<double>(sc.ambColor[0]), static_cast<double>(sc.ambColor[1]),
          static_cast<double>(sc.ambColor[2]), C3D_BoolUnifs[GPU_VERTEX_SHADER]);
  const tev::Programme& prog = sDernierProgramme;
  journal("  TEV : %lu étages GX -> %u étages PICA, approximé %d (raison %u), unités %d", 
          static_cast<unsigned long>(g_gxState.numTevStages), prog.nb, prog.approxime, prog.raison, p.nbUnites);
  for (int i = 0; i < prog.nb && i < 6; i++) {
    const auto& e = prog.etages[i];
    journal("    %d : rvb f%u src %X %X %X op %u %u %u x%u | a f%u src %X %X %X op %u %u %u x%u | k %08lX t%d%d", i,
            e.fRvb, e.srcRvb[0], e.srcRvb[1], e.srcRvb[2], e.opRvb[0], e.opRvb[1], e.opRvb[2], 1u << e.echelleRvb,
            e.fA, e.srcA[0], e.srcA[1], e.srcA[2], e.opA[0], e.opA[1], e.opA[2], 1u << e.echelleA,
            static_cast<unsigned long>(e.constante), e.tamponRvb, e.tamponA);
  }
  const auto& vp = g_gxState.renderViewport;
  const auto& sci = g_gxState.renderScissor;
  journal("  affichage (%.0f %.0f %.0f %.0f, z %.2f..%.2f) ciseaux (%ld %ld %ld %ld) cull %d z %d/%d/%d mélange %d",
          static_cast<double>(vp.left), static_cast<double>(vp.top), static_cast<double>(vp.width),
          static_cast<double>(vp.height), static_cast<double>(vp.znear), static_cast<double>(vp.zfar),
          static_cast<long>(sci.x), static_cast<long>(sci.y), static_cast<long>(sci.width),
          static_cast<long>(sci.height), static_cast<int>(g_gxState.cullMode), g_gxState.depthCompare,
          static_cast<int>(g_gxState.depthFunc), g_gxState.depthUpdate, static_cast<int>(g_gxState.blendMode));
  if (sortie != nullptr && d.taille > 0) {
    char hex[200];
    int k = 0;
    const auto* o = static_cast<const uint8_t*>(sortie);
    for (int i = 0; i < d.taille && k < 190; i++) k += snprintf(hex + k, sizeof hex - k, "%02X", o[i]);
    journal("  premier sommet : %s", hex);
    const float* f = static_cast<const float*>(sortie);
    journal("  position : %.3f %.3f %.3f", static_cast<double>(f[0]), static_cast<double>(f[1]),
            static_cast<double>(f[2]));
  }
}

uint32_t couleurEffacement() {
  const auto& c = g_gxState.clearColor;
  auto o = [](float f) { return static_cast<uint32_t>(std::lround(std::clamp(f, 0.f, 1.f) * 255.f)); };
  // Constante du combineur : R dans les bits de poids faible
  return o(c[0]) | o(c[1]) << 8 | o(c[2]) << 16 | o(c[3]) << 24;
}

float profondeurEffacement() { return static_cast<float>(g_gxState.clearDepth & 0xFFFFFF) / 16777215.f; }

} // namespace

Reglages& reglages() noexcept { return sReglages; }

void zone_ecran(float& x, float& y, float& largeur, float& hauteur) noexcept {
  const Zone z = zone();
  x = z.x;
  y = z.y;
  largeur = z.l;
  hauteur = z.h;
}
const Stats& stats() noexcept { return sStatsAffichees; }

void definir_journal(FonctionJournal fonction) noexcept { sJournal = fonction; }
void journaliser_dessins(uint32_t nombre) noexcept { sJournalRestant = nombre; }

void initialiser() noexcept {
  if (sInitialise) return;
  C3D_Init(0x200000);
  sCible = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
  C3D_RenderTargetSetOutput(sCible, GFX_TOP, GFX_LEFT,
                            GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
                                GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                                GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

  sDvlb = DVLB_ParseFile(reinterpret_cast<u32*>(const_cast<u8*>(gx3ds_shbin)), gx3ds_shbin_size);
  shaderProgramInit(&sProgramme);
  shaderProgramSetVsh(&sProgramme, &sDvlb->DVLE[0]);
  C3D_BindProgram(&sProgramme);

  // Vérification des registres du shader
  const int p = shaderInstanceGetUniformLocation(sProgramme.vertexShader, "projection");
  const int l = shaderInstanceGetUniformLocation(sProgramme.vertexShader, "lumieres");
  if (p != U_PROJECTION || l != U_LUMIERES) {
    printf("gx3ds : registres du shader inattendus (%d, %d)\n", p, l);
  }

  sSommets = static_cast<uint8_t*>(linearAlloc(TailleSommets));
  sIndices = static_cast<uint16_t*>(linearAlloc(MaxIndices * sizeof(uint16_t)));
  sIndicesQuads = static_cast<uint16_t*>(linearAlloc(MaxQuads * 6 * sizeof(uint16_t)));
  for (uint32_t q = 0; q < MaxQuads; q++) {
    const uint16_t b = static_cast<uint16_t>(q * 4);
    uint16_t* i = sIndicesQuads + q * 6;
    i[0] = b;
    i[1] = static_cast<uint16_t>(b + 1);
    i[2] = static_cast<uint16_t>(b + 2);
    i[3] = static_cast<uint16_t>(b + 2);
    i[4] = static_cast<uint16_t>(b + 3);
    i[5] = b;
  }
  GSPGPU_FlushDataCache(sIndicesQuads, MaxQuads * 6 * sizeof(uint16_t));

  textures::initialiser();
  invaliderUniformes();
  sInitialise = true;
}

void arreter() noexcept {
  if (!sInitialise) return;
  textures::arreter();
  linearFree(sSommets);
  linearFree(sIndices);
  linearFree(sIndicesQuads);
  shaderProgramFree(&sProgramme);
  DVLB_Free(sDvlb);
  C3D_Fini();
  sInitialise = false;
}

void debut_image() noexcept {
  if (!sInitialise || sImageEnCours) return;
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
  sPosSommets = 0;
  sPosIndices = 0;
  sStats = {};
  C3D_RenderTargetClear(sCible, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
  C3D_FrameDrawOn(sCible);
  invaliderUniformes();
  sTevValide = false;
  sImageEnCours = true;
  // Fond de la zone de jeu : couleur d'effacement de la GameCube
  const Zone z = zone();
  effacerZone(z.x, z.y, z.l, z.h, couleurEffacement(), profondeurEffacement(), true, true, true);
}

void fin_image() noexcept {
  if (!sImageEnCours) return;
  C3D_FrameEnd(0);
  sImageEnCours = false;
  sStats.texturesConverties = textures::conversions_image();
  sStats.octetsTextures = textures::octets_utilises();
  sStatsAffichees = sStats;
  textures::fin_image(sNumeroImage);
  sNumeroImage++;
}

void dessiner(GXPrimitive prim, GXVtxFmt fmt, uint16_t n, const uint8_t* donnees) noexcept {
  if (!sImageEnCours || n == 0) return;
  GPU_Primitive_t primPica;
  bool quads = false;
  switch (prim) {
  case GX_TRIANGLES:
    primPica = GPU_TRIANGLES;
    break;
  case GX_TRIANGLESTRIP:
    primPica = GPU_TRIANGLE_STRIP;
    break;
  case GX_TRIANGLEFAN:
    primPica = GPU_TRIANGLE_FAN;
    break;
  case GX_QUADS:
  case static_cast<GXPrimitive>(0x88):  // GX_QUADS2
    primPica = GPU_TRIANGLES;
    quads = true;
    break;
  default:
    sStats.ignores++;  // lignes et points : pas encore gérés
    return;
  }
  Preparation p;
  void* sortie = nullptr;
  sNumeroDessin++;
  if (!preparerDessin(fmt, n, donnees, p, sortie)) return;
  if (sJournalRestant > 0) {
    sJournalRestant--;
    decrireDessin(prim, n, fmt, p, sortie);
  }
  if (quads) {
    const uint32_t nbQuads = std::min<uint32_t>(n / 4, MaxQuads);
    if (nbQuads > 0) C3D_DrawElements(GPU_TRIANGLES, static_cast<int>(nbQuads * 6), C3D_UNSIGNED_SHORT, sIndicesQuads);
  } else {
    C3D_DrawArrays(primPica, 0, n);
  }
  sStats.dessins++;
  sStats.sommets += n;
}

void dessiner_indexe(GXVtxFmt fmt, uint16_t nbSommets, const uint8_t* donnees, const uint16_t* indices,
                     uint32_t nbIndices) noexcept {
  if (!sImageEnCours || nbSommets == 0 || nbIndices == 0) return;
  if (sPosIndices + nbIndices > MaxIndices) {
    sStats.ignores++;
    return;
  }
  Preparation p;
  void* sortie = nullptr;
  sNumeroDessin++;
  if (!preparerDessin(fmt, nbSommets, donnees, p, sortie)) return;
  if (sJournalRestant > 0) {
    sJournalRestant--;
    decrireDessin(-1, nbSommets, fmt, p, sortie);
  }
  uint16_t* dst = sIndices + sPosIndices;
  std::memcpy(dst, indices, nbIndices * sizeof(uint16_t));
  GSPGPU_FlushDataCache(dst, nbIndices * sizeof(uint16_t));
  sPosIndices += (nbIndices + 7) & ~7u;
  C3D_DrawElements(GPU_TRIANGLES, static_cast<int>(nbIndices), C3D_UNSIGNED_SHORT, dst);
  sStats.dessins++;
  sStats.sommets += nbSommets;
}

void copier_vers_texture(const void* dest, bool effacer) noexcept {
  (void)dest;
  if (!sImageEnCours || !effacer) return;
  // La copie elle-même n'est pas encore faite ; on applique l'effacement de la zone source.
  const auto rect = aurora::gx::map_logical_scissor(g_gxState.texCopySrc);
  effacerZone(static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width),
              static_cast<float>(rect.height), couleurEffacement(), profondeurEffacement(), g_gxState.colorUpdate,
              g_gxState.alphaUpdate, g_gxState.depthUpdate);
}

void copier_vers_ecran(bool effacer) noexcept { (void)effacer; }

void oublier_objet_texture(uint32_t texObjId) noexcept { textures::oublier_objet(texObjId); }
void oublier_tlut(uint32_t tlutObjId) noexcept { textures::oublier_tlut(tlutObjId); }
void oublier_copie(const void* dest) noexcept { (void)dest; }
void vider_cache_textures() noexcept { textures::vider(); }

} // namespace gx3ds
