// Démonstration du moteur GX -> PICA200 : quatre petites scènes écrites avec l'API graphique
// de la GameCube (celle qu'utilise le jeu), affichées chacune dans un quart de l'image.
//
//  en haut à gauche  : triangle coloré (sommets directs, projection orthographique)
//  en haut à droite  : carré texturé RGB5A3 (repères : rouge en haut, vert à gauche)
//  en bas à gauche   : cube éclairé qui tourne (sommets indexés, normales, perspective)
//  en bas à droite   : deux textures (CMPR et I8) mélangées par le TEV, et transparence
#include <3ds.h>

#include <dolphin/gx.h>
#include <dolphin/mtx.h>
#include <dolphin/vi.h>

#include <aurora/aurora.h>
#include <gx3ds/gx3ds.hpp>

#include "lib/gx/fifo.hpp"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace aurora {
extern AuroraConfig g_config;
}

namespace {
FILE* sJournal = nullptr;

void journal(const char* fmt, ...) {
  char texte[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(texte, sizeof texte, fmt, ap);
  va_end(ap);
  printf("%s\n", texte);
  if (sJournal) {
    fprintf(sJournal, "%s\n", texte);
    fflush(sJournal);
  }
}

void journalAurora(AuroraLogLevel niveau, const char* module, const char* message, unsigned int len) {
  if (niveau < LOG_WARNING) return;
  journal("[%s] %.*s", module ? module : "", static_cast<int>(len), message);
}

// ---------------------------------------------------------------------------------------------
// Fabrication de textures au format GX (en blocs, gros-boutiste)

alignas(32) u8 sTexRgb5a3[64 * 64 * 2];
alignas(32) u8 sTexCmpr[64 * 64 / 2];
alignas(32) u8 sTexI8[32 * 32];

u16 rgb5a3(u8 r, u8 g, u8 b, u8 a) {
  if (a >= 224) return static_cast<u16>(0x8000 | (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3));
  return static_cast<u16>((a >> 5) << 12 | (r >> 4) << 8 | (g >> 4) << 4 | (b >> 4));
}

void fabriquerRgb5a3() {
  // 64x64, blocs de 4x4 texels
  u8* p = sTexRgb5a3;
  for (int by = 0; by < 64; by += 4)
    for (int bx = 0; bx < 64; bx += 4)
      for (int t = 0; t < 16; t++) {
        const int x = bx + t % 4, y = by + t / 4;
        u16 c;
        if (y < 16) c = rgb5a3(255, 0, 0, 255);
        else if (x < 16) c = rgb5a3(0, 255, 0, 255);
        else c = (((x >> 3) ^ (y >> 3)) & 1) ? rgb5a3(255, 255, 255, 255) : rgb5a3(0, 0, 255, 255);
        *p++ = static_cast<u8>(c >> 8);
        *p++ = static_cast<u8>(c);
      }
}

u16 rgb565(u8 r, u8 g, u8 b) { return static_cast<u16>((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)); }

void fabriquerCmpr() {
  // 64x64 : blocs de 8x8 = 4 sous-blocs DXT1 ; dégradé rouge -> jaune horizontal
  u8* p = sTexCmpr;
  for (int by = 0; by < 64; by += 8)
    for (int bx = 0; bx < 64; bx += 8)
      for (int s = 0; s < 4; s++) {
        const int x0 = bx + (s & 1) * 4;
        const u16 c0 = rgb565(255, static_cast<u8>(x0 * 4), 0);
        const u16 c1 = rgb565(255, static_cast<u8>(std::min(255, (x0 + 4) * 4)), 32);
        const u16 a = c0 > c1 ? c0 : c1;
        const u16 b = c0 > c1 ? c1 : c0;
        *p++ = static_cast<u8>(a >> 8);
        *p++ = static_cast<u8>(a);
        *p++ = static_cast<u8>(b >> 8);
        *p++ = static_cast<u8>(b);
        for (int r = 0; r < 4; r++) *p++ = 0x1B;  // indices 0, 1, 2, 3 sur chaque ligne
      }
}

void fabriquerI8() {
  // 32x32, blocs de 8x4 : cercle clair au centre
  u8* p = sTexI8;
  for (int by = 0; by < 32; by += 4)
    for (int bx = 0; bx < 32; bx += 8)
      for (int t = 0; t < 32; t++) {
        const int x = bx + t % 8, y = by + t / 8;
        const float d = std::sqrt(static_cast<float>((x - 16) * (x - 16) + (y - 16) * (y - 16)));
        *p++ = static_cast<u8>(d < 12.f ? 255 - static_cast<int>(d * 10.f) : 40);
      }
}

// ---------------------------------------------------------------------------------------------
// Cube : positions s16 (virgule fixe, 8 bits fractionnaires), normales s8, couleurs RGBA8

alignas(32) s16 sCubePos[8][3];
alignas(32) s8 sCubeNrm[6][3] = {{0, 0, 64}, {0, 0, -64}, {64, 0, 0}, {-64, 0, 0}, {0, 64, 0}, {0, -64, 0}};
alignas(32) u8 sCubeClr[6][4] = {{230, 60, 60, 255},  {60, 200, 60, 255},  {60, 90, 230, 255},
                                 {230, 200, 50, 255}, {200, 70, 200, 255}, {60, 210, 210, 255}};

void preparerCube() {
  for (int i = 0; i < 8; i++) {
    sCubePos[i][0] = static_cast<s16>((i & 1) ? 256 : -256);
    sCubePos[i][1] = static_cast<s16>((i & 2) ? 256 : -256);
    sCubePos[i][2] = static_cast<s16>((i & 4) ? 256 : -256);
  }
}

void quadrant(int q) {
  const f32 x = (q & 1) ? 320.f : 0.f;
  const f32 y = (q & 2) ? 240.f : 0.f;
  GXSetViewport(x, y, 320.f, 240.f, 0.f, 1.f);
  GXSetScissor(static_cast<u32>(x), static_cast<u32>(y), 320, 240);
}

void projectionOrtho() {
  Mtx44 p;
  C_MTXOrtho(p, 0.f, 240.f, 0.f, 320.f, 0.f, 10.f);
  GXSetProjection(p, GX_ORTHOGRAPHIC);
  Mtx m;
  C_MTXIdentity(m);
  C_MTXTrans(m, 0.f, 0.f, -1.f);
  GXLoadPosMtxImm(m, GX_PNMTX0);
  GXSetCurrentMtx(GX_PNMTX0);
}

void tevCouleurSommet() {
  GXSetNumChans(1);
  GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
  GXSetNumTexGens(0);
  GXSetNumTevStages(1);
  GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
  GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
}

void sceneTriangle() {
  quadrant(0);
  projectionOrtho();
  tevCouleurSommet();
  GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
  GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
  GXSetCullMode(GX_CULL_NONE);
  GXClearVtxDesc();
  GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
  GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GXBegin(GX_TRIANGLES, GX_VTXFMT0, 3);
  GXPosition3f32(160.f, 30.f, 0.f);
  GXColor4u8(255, 0, 0, 255);
  GXPosition3f32(40.f, 210.f, 0.f);
  GXColor4u8(0, 255, 0, 255);
  GXPosition3f32(280.f, 210.f, 0.f);
  GXColor4u8(0, 0, 255, 255);
  GXEnd();
}

void sceneTexture(GXTexObj& tex) {
  quadrant(1);
  projectionOrtho();
  GXSetNumChans(1);
  GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
  GXSetChanMatColor(GX_COLOR0A0, GXColor{255, 255, 255, 255});
  GXSetNumTexGens(1);
  GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  GXSetNumTevStages(1);
  GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
  GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
  GXLoadTexObj(&tex, GX_TEXMAP0);
  GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
  GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
  GXSetCullMode(GX_CULL_NONE);
  GXClearVtxDesc();
  GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
  GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
  GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
  GXBegin(GX_QUADS, GX_VTXFMT1, 4);
  GXPosition3f32(80.f, 40.f, 0.f);
  GXTexCoord2f32(0.f, 0.f);
  GXPosition3f32(240.f, 40.f, 0.f);
  GXTexCoord2f32(1.f, 0.f);
  GXPosition3f32(240.f, 200.f, 0.f);
  GXTexCoord2f32(1.f, 1.f);
  GXPosition3f32(80.f, 200.f, 0.f);
  GXTexCoord2f32(0.f, 1.f);
  GXEnd();
}

void sceneCube(float angle) {
  quadrant(2);
  Mtx44 p;
  C_MTXPerspective(p, 50.f, 320.f / 240.f, 1.f, 100.f);
  GXSetProjection(p, GX_PERSPECTIVE);

  Mtx rot, rot2, mv;
  C_MTXRotRad(rot, 'y', angle);
  C_MTXRotRad(rot2, 'x', 0.5f);
  C_MTXConcat(rot2, rot, mv);
  mv[2][3] = -4.f;
  GXLoadPosMtxImm(mv, GX_PNMTX0);
  GXLoadNrmMtxImm(mv, GX_PNMTX0);
  GXSetCurrentMtx(GX_PNMTX0);

  // Lumière en haut à gauche, devant le cube (coordonnées de la caméra)
  GXLightObj lum;
  GXInitLightPos(&lum, -20.f, 20.f, 10.f);
  GXInitLightColor(&lum, GXColor{255, 255, 255, 255});
  GXInitLightAttn(&lum, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f);
  GXLoadLightObjImm(&lum, GX_LIGHT0);
  GXSetNumChans(1);
  GXSetChanCtrl(GX_COLOR0A0, GX_TRUE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT0, GX_DF_CLAMP, GX_AF_NONE);
  GXSetChanAmbColor(GX_COLOR0A0, GXColor{50, 50, 60, 255});
  GXSetNumTexGens(0);
  GXSetNumTevStages(1);
  GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
  GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);

  GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
  GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
  GXSetCullMode(GX_CULL_BACK);

  GXClearVtxDesc();
  GXSetVtxDesc(GX_VA_POS, GX_INDEX8);
  GXSetVtxDesc(GX_VA_NRM, GX_INDEX8);
  GXSetVtxDesc(GX_VA_CLR0, GX_INDEX8);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_POS, GX_POS_XYZ, GX_S16, 8);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_NRM, GX_NRM_XYZ, GX_S8, 6);
  GXSetVtxAttrFmt(GX_VTXFMT2, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  // Tableaux gros-boutistes, comme sur GameCube : on retourne les octets des positions
  static s16 posBE[8][3];
  for (int i = 0; i < 8; i++)
    for (int c = 0; c < 3; c++) {
      const u16 v = static_cast<u16>(sCubePos[i][c]);
      posBE[i][c] = static_cast<s16>((v >> 8) | (v << 8));
    }
  GXSetArray(GX_VA_POS, posBE, sizeof posBE, 6, false);
  GXSetArray(GX_VA_NRM, sCubeNrm, sizeof sCubeNrm, 3, false);
  GXSetArray(GX_VA_CLR0, sCubeClr, sizeof sCubeClr, 4, false);

  // Faces : (indices des coins, normale) ; sens des aiguilles d'une montre vu de l'extérieur
  static const u8 faces[6][4] = {{4, 5, 7, 6}, {1, 0, 2, 3}, {5, 1, 3, 7}, {0, 4, 6, 2}, {6, 7, 3, 2}, {0, 1, 5, 4}};
  GXBegin(GX_QUADS, GX_VTXFMT2, 24);
  for (int f = 0; f < 6; f++) {
    for (int c = 0; c < 4; c++) {
      GXPosition1x8(faces[f][c]);
      GXNormal1x8(static_cast<u8>(f));
      GXColor1x8(static_cast<u8>(f));
    }
  }
  GXEnd();
}

void sceneMelange(GXTexObj& texCmpr, GXTexObj& texI8, float t) {
  quadrant(3);
  projectionOrtho();
  GXSetNumChans(1);
  GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
  GXSetNumTexGens(2);
  GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
  // Deuxième coordonnée : la texture I8 défile grâce à une matrice de texture
  Mtx tm;
  C_MTXIdentity(tm);
  tm[0][3] = t * 0.25f;
  GXLoadTexMtxImm(tm, GX_TEXMTX0, GX_MTX2x4);
  GXSetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0);
  GXLoadTexObj(&texCmpr, GX_TEXMAP0);
  GXLoadTexObj(&texI8, GX_TEXMAP1);
  GXSetTevKColor(GX_KCOLOR0, GXColor{255, 255, 255, 160});
  GXSetNumTevStages(2);
  // étage 0 : texture CMPR x couleur du sommet
  GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
  GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC, GX_CC_ZERO);
  GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
  GXSetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);
  GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
  GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
  // étage 1 : mélange avec la texture I8 selon son intensité
  GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR0A0);
  GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_CPREV, GX_CC_ONE, GX_CC_TEXC, GX_CC_ZERO);
  GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
  GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
  GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

  GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
  GXSetCullMode(GX_CULL_NONE);
  GXClearVtxDesc();
  GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
  GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
  GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
  GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
  GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
  GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

  // Fond opaque gris foncé (sans texture), puis carré semi-transparent par-dessus
  GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
  GXBegin(GX_QUADS, GX_VTXFMT3, 4);
  GXPosition3f32(60.f, 30.f, 0.f);
  GXColor4u8(255, 255, 255, 255);
  GXTexCoord2f32(0.f, 0.f);
  GXPosition3f32(260.f, 30.f, 0.f);
  GXColor4u8(255, 255, 255, 255);
  GXTexCoord2f32(1.f, 0.f);
  GXPosition3f32(260.f, 210.f, 0.f);
  GXColor4u8(100, 100, 255, 255);
  GXTexCoord2f32(1.f, 1.f);
  GXPosition3f32(60.f, 210.f, 0.f);
  GXColor4u8(100, 100, 255, 255);
  GXTexCoord2f32(0.f, 1.f);
  GXEnd();
}

} // namespace

int main() {
  gfxInitDefault();
  consoleInit(GFX_BOTTOM, nullptr);
  osSetSpeedupEnable(true);
  mkdir("sdmc:/zelda-3ds", 0777);
  sJournal = fopen("sdmc:/zelda-3ds/demo-journal.txt", "w");
  journal("=== Démonstration du moteur GX -> PICA200 ===");

  aurora::g_config.logLevel = LOG_WARNING;
  aurora::g_config.logCallback = journalAurora;

  gx3ds::initialiser();
  aurora::gx::fifo::init();
  alignas(32) static u8 memoireFifo[256 * 1024];
  GXInit(memoireFifo, sizeof memoireFifo);
  VIConfigure(&GXNtsc480IntDf);
  GXSetCopyClear(GXColor{40, 40, 70, 255}, 0xFFFFFF);

  fabriquerRgb5a3();
  fabriquerCmpr();
  fabriquerI8();
  preparerCube();
  GXTexObj texRgb5a3, texCmpr, texI8;
  GXInitTexObj(&texRgb5a3, sTexRgb5a3, 64, 64, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
  GXInitTexObjLOD(&texRgb5a3, GX_NEAR, GX_NEAR, 0.f, 0.f, 0.f, GX_FALSE, GX_FALSE, GX_ANISO_1);
  GXInitTexObj(&texCmpr, sTexCmpr, 64, 64, GX_TF_CMPR, GX_REPEAT, GX_REPEAT, GX_FALSE);
  GXInitTexObj(&texI8, sTexI8, 32, 32, GX_TF_I8, GX_REPEAT, GX_REPEAT, GX_FALSE);
  journal("Prêt. START pour quitter.");

  u32 image = 0;
  float angle = 0.f;
  u64 debut = osGetTime();
  while (aptMainLoop()) {
    hidScanInput();
    if (hidKeysDown() & KEY_START) break;

    gx3ds::debut_image();
    aurora::gx::fifo::begin_frame();
    sceneTriangle();
    sceneTexture(texRgb5a3);
    sceneCube(angle);
    sceneMelange(texCmpr, texI8, angle);
    aurora::gx::fifo::drain();
    aurora::gx::fifo::end_frame();
    gx3ds::fin_image();

    angle += 0.02f;
    image++;
    if (image == 120 || image == 600) {
      const auto& st = gx3ds::stats();
      journal("Image %lu : %lu dessins, %lu sommets, %lu Ko de textures, TEV approchés %lu, ignorés %lu",
              static_cast<unsigned long>(image), static_cast<unsigned long>(st.dessins),
              static_cast<unsigned long>(st.sommets), static_cast<unsigned long>(st.octetsTextures / 1024),
              static_cast<unsigned long>(st.tevApproximes), static_cast<unsigned long>(st.ignores));
      journal("%lu images en %llu ms", static_cast<unsigned long>(image),
              static_cast<unsigned long long>(osGetTime() - debut));
    }
  }

  journal("Fin.");
  if (sJournal) fclose(sJournal);
  gx3ds::arreter();
  gfxExit();
  return 0;
}
