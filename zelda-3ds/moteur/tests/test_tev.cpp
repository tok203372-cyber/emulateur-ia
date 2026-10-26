// Test sur PC du traducteur TEV : on compare le calcul GameCube (référence)
// avec le programme produit pour la PICA200, sur des couleurs tirées au hasard.
#include "../gx3ds/tev.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

using namespace gx3ds::tev;

namespace {
struct C4 {
  float v[4];
};
float sat(float x) { return std::clamp(x, 0.f, 1.f); }

// --- Référence GX
struct Env {
  C4 tex[3];
  C4 ras;
};

C4 konstC(const Entree& e, uint8_t sel) {
  if (sel <= 7) {
    float f = (8 - sel) / 8.f;
    return {{f, f, f, f}};
  }
  if (sel >= 0x0C && sel <= 0x0F) {
    auto& k = e.konst[sel - 0x0C];
    return {{k.r, k.v, k.b, k.a}};
  }
  int idx = (sel - 0x10) & 3, comp = (sel - 0x10) >> 2;
  auto& k = e.konst[idx];
  float v = comp == 0 ? k.r : comp == 1 ? k.v : comp == 2 ? k.b : k.a;
  return {{v, v, v, v}};
}
float konstA(const Entree& e, uint8_t sel) {
  if (sel <= 7) return (8 - sel) / 8.f;
  int idx = (sel - 0x10) & 3, comp = (sel - 0x10) >> 2;
  auto& k = e.konst[idx];
  return comp == 0 ? k.r : comp == 1 ? k.v : comp == 2 ? k.b : k.a;
}

C4 reference(const Entree& e, const Env& env) {
  C4 reg[4];
  for (int i = 0; i < 4; i++) reg[i] = {{e.registres[i].r, e.registres[i].v, e.registres[i].b, e.registres[i].a}};
  for (int s = 0; s < e.nbEtages; s++) {
    const EtageGx& g = e.etages[s];
    C4 tex = g.unite >= 0 ? env.tex[g.unite] : C4{{1, 1, 1, 1}};
    C4 ras = (g.canal == GX_COLOR0A0) ? env.ras : C4{{0, 0, 0, 0}};
    auto colArg = [&](uint8_t a, int c) -> float {
      switch (a) {
      case GX_CC_CPREV: return reg[0].v[c];
      case GX_CC_APREV: return reg[0].v[3];
      case GX_CC_C0: return reg[1].v[c];
      case GX_CC_A0: return reg[1].v[3];
      case GX_CC_C1: return reg[2].v[c];
      case GX_CC_A1: return reg[2].v[3];
      case GX_CC_C2: return reg[3].v[c];
      case GX_CC_A2: return reg[3].v[3];
      case GX_CC_TEXC: return tex.v[c];
      case GX_CC_TEXA: return tex.v[3];
      case GX_CC_RASC: return ras.v[c];
      case GX_CC_RASA: return ras.v[3];
      case GX_CC_ONE: return 1.f;
      case GX_CC_HALF: return 0.5f;
      case GX_CC_KONST: return konstC(e, g.kcSel).v[c];
      default: return 0.f;
      }
    };
    auto alpArg = [&](uint8_t a) -> float {
      switch (a) {
      case GX_CA_APREV: return reg[0].v[3];
      case GX_CA_A0: return reg[1].v[3];
      case GX_CA_A1: return reg[2].v[3];
      case GX_CA_A2: return reg[3].v[3];
      case GX_CA_TEXA: return tex.v[3];
      case GX_CA_RASA: return ras.v[3];
      case GX_CA_KONST: return konstA(e, g.kaSel);
      default: return 0.f;
      }
    };
    auto calc = [](float a, float b, float c, float d, uint8_t op, uint8_t biais, uint8_t ech) {
      float l = a * (1 - c) + b * c;
      float r = op == GX_TEV_SUB ? d - l : d + l;
      if (biais == GX_TB_ADDHALF) r += 0.5f;
      if (biais == GX_TB_SUBHALF) r -= 0.5f;
      float m = ech == GX_CS_SCALE_2 ? 2.f : ech == GX_CS_SCALE_4 ? 4.f : ech == GX_CS_DIVIDE_2 ? 0.5f : 1.f;
      return sat(r * m);
    };
    C4 out;
    for (int c = 0; c < 3; c++)
      out.v[c] = calc(colArg(g.c[0], c), colArg(g.c[1], c), colArg(g.c[2], c), colArg(g.c[3], c), g.opC, g.biaisC,
                      g.echelleC);
    float oa = calc(alpArg(g.a[0]), alpArg(g.a[1]), alpArg(g.a[2]), alpArg(g.a[3]), g.opA, g.biaisA, g.echelleA);
    for (int c = 0; c < 3; c++) reg[g.sortieC].v[c] = out.v[c];
    reg[g.sortieA].v[3] = oa;
  }
  return reg[0];
}

// --- Simulation de la PICA200 (comme l'émulateur Citra)
C4 pica(const Programme& p, const Env& env) {
  C4 prec{{0, 0, 0, 0}};
  C4 tampon{{0, 0, 0, 0}};
  C4 prochain;
  float bc[4] = {(p.couleurTampon & 0xFF) / 255.f, ((p.couleurTampon >> 8) & 0xFF) / 255.f,
                 ((p.couleurTampon >> 16) & 0xFF) / 255.f, ((p.couleurTampon >> 24) & 0xFF) / 255.f};
  for (int i = 0; i < 4; i++) prochain.v[i] = bc[i];
  for (int s = 0; s < 6; s++) {
    const EtagePica& e = p.etages[s];
    C4 k{{(e.constante & 0xFF) / 255.f, ((e.constante >> 8) & 0xFF) / 255.f, ((e.constante >> 16) & 0xFF) / 255.f,
          ((e.constante >> 24) & 0xFF) / 255.f}};
    auto source = [&](uint8_t src) -> C4 {
      switch (src) {
      case SRC_PRIMAIRE: return env.ras;
      case SRC_TEXTURE0: return env.tex[0];
      case SRC_TEXTURE1: return env.tex[1];
      case SRC_TEXTURE2: return env.tex[2];
      case SRC_TAMPON: return tampon;
      case SRC_CONSTANTE: return k;
      default: return prec;
      }
    };
    auto opRvb = [](C4 v, uint8_t op, int c) -> float {
      switch (op) {
      case RVB_COULEUR: return v.v[c];
      case RVB_UN_MOINS_COULEUR: return 1 - v.v[c];
      case RVB_ALPHA: return v.v[3];
      case RVB_UN_MOINS_ALPHA: return 1 - v.v[3];
      case RVB_R: return v.v[0];
      case RVB_UN_MOINS_R: return 1 - v.v[0];
      case RVB_V: return v.v[1];
      case RVB_UN_MOINS_V: return 1 - v.v[1];
      case RVB_B: return v.v[2];
      case RVB_UN_MOINS_B: return 1 - v.v[2];
      }
      return 0;
    };
    auto opA = [](C4 v, uint8_t op) -> float {
      switch (op) {
      case A_ALPHA: return v.v[3];
      case A_UN_MOINS_ALPHA: return 1 - v.v[3];
      case A_R: return v.v[0];
      case A_UN_MOINS_R: return 1 - v.v[0];
      case A_V: return v.v[1];
      case A_UN_MOINS_V: return 1 - v.v[1];
      case A_B: return v.v[2];
      case A_UN_MOINS_B: return 1 - v.v[2];
      }
      return 0;
    };
    auto combine = [](uint8_t f, float a, float b, float c) -> float {
      switch (f) {
      case F_REMPLACER: return a;
      case F_MODULER: return a * b;
      case F_AJOUTER: return a + b;
      case F_AJOUTER_SIGNE: return a + b - 0.5f;
      case F_INTERPOLER: return a * c + b * (1 - c);
      case F_SOUSTRAIRE: return a - b;
      case F_MULTIPLIER_AJOUTER: return a * b + c;
      }
      return 0;
    };
    C4 out;
    for (int c = 0; c < 3; c++) {
      float a = opRvb(source(e.srcRvb[0]), e.opRvb[0], c);
      float b = opRvb(source(e.srcRvb[1]), e.opRvb[1], c);
      float cc = opRvb(source(e.srcRvb[2]), e.opRvb[2], c);
      float m = e.echelleRvb == 1 ? 2.f : e.echelleRvb == 2 ? 4.f : 1.f;
      out.v[c] = sat(sat(combine(e.fRvb, a, b, cc)) * m);
    }
    {
      float a = opA(source(e.srcA[0]), e.opA[0]);
      float b = opA(source(e.srcA[1]), e.opA[1]);
      float cc = opA(source(e.srcA[2]), e.opA[2]);
      float m = e.echelleA == 1 ? 2.f : e.echelleA == 2 ? 4.f : 1.f;
      out.v[3] = sat(sat(combine(e.fA, a, b, cc)) * m);
    }
    tampon = prochain;
    if (s < 4) {
      if (e.tamponRvb)
        for (int c = 0; c < 3; c++) prochain.v[c] = out.v[c];
      if (e.tamponA) prochain.v[3] = out.v[3];
    }
    prec = out;
  }
  return prec;
}
} // namespace

int main() {
  std::mt19937 rng(1234);
  std::uniform_real_distribution<float> u(0.f, 1.f);
  auto q = [&](float x) { return std::round(x * 255.f) / 255.f; };

  int total = 0, exacts = 0, approx = 0, erreurs = 0;
  int parFamille[4] = {}, approxFamille[4] = {}, tropLongFamille[4] = {};
  int raisons[4][16] = {};
  // Plusieurs familles de configurations, des plus courantes aux plus aléatoires
  for (int essai = 0; essai < 20000; essai++) {
    Entree e{};
    int famille = essai % 4;
    e.nbEtages = 1 + (famille == 0 ? 0 : famille == 1 ? rng() % 2 : famille == 2 ? rng() % 3 : rng() % 4);
    for (int i = 0; i < 4; i++) {
      e.registres[i] = {q(u(rng)), q(u(rng)), q(u(rng)), q(u(rng))};
      e.konst[i] = {q(u(rng)), q(u(rng)), q(u(rng)), q(u(rng))};
      e.echanges[i] = {GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA};
    }
    static const uint8_t ccCourants[] = {GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC, GX_CC_CPREV, GX_CC_KONST,
                                         GX_CC_C0,   GX_CC_ONE,  GX_CC_TEXA, GX_CC_RASA,  GX_CC_HALF};
    static const uint8_t caCourants[] = {GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA, GX_CA_APREV, GX_CA_KONST, GX_CA_A0};
    for (int s = 0; s < e.nbEtages; s++) {
      EtageGx& g = e.etages[s];
      for (int i = 0; i < 4; i++) {
        g.c[i] = famille == 3 ? rng() % 16 : ccCourants[rng() % (famille == 0 ? 4 : 10)];
        g.a[i] = famille == 3 ? rng() % 8 : caCourants[rng() % (famille == 0 ? 4 : 6)];
      }
      g.opC = (famille >= 2 && rng() % 5 == 0) ? GX_TEV_SUB : GX_TEV_ADD;
      g.opA = (famille >= 2 && rng() % 5 == 0) ? GX_TEV_SUB : GX_TEV_ADD;
      g.biaisC = famille >= 2 ? rng() % 3 : 0;
      g.biaisA = famille >= 2 ? rng() % 3 : 0;
      g.echelleC = famille >= 1 ? rng() % 4 : 0;
      g.echelleA = famille >= 1 ? rng() % 4 : 0;
      g.sortieC = famille == 3 ? rng() % 4 : (s == e.nbEtages - 1 ? 0 : (rng() % 3 == 0 ? 1 : 0));
      g.sortieA = famille == 3 ? rng() % 4 : (s == e.nbEtages - 1 ? 0 : (rng() % 3 == 0 ? 1 : 0));
      if (s == e.nbEtages - 1) {
        g.sortieC = 0;
        g.sortieA = 0;
      }
      g.kcSel = rng() % 2 ? (rng() % 8) : (0x0C + rng() % 20);
      g.kaSel = rng() % 2 ? (rng() % 8) : (0x10 + rng() % 16);
      g.canal = GX_COLOR0A0;
      g.unite = static_cast<int8_t>(s % 3);
      g.echangeRas = 0;
      g.echangeTex = 0;
    }
    Programme p;
    compiler(e, p);
    total++;
    parFamille[famille]++;
    if (p.approxime) {
      approx++;
      approxFamille[famille]++;
      if (p.tropLong) tropLongFamille[famille]++;
      raisons[famille][p.raison & 15]++;
      continue;
    }
    // Plusieurs jeux de couleurs
    bool ok = true;
    for (int t = 0; t < 8 && ok; t++) {
      Env env;
      for (int i = 0; i < 3; i++) env.tex[i] = {{q(u(rng)), q(u(rng)), q(u(rng)), q(u(rng))}};
      env.ras = {{q(u(rng)), q(u(rng)), q(u(rng)), q(u(rng))}};
      C4 r = reference(e, env);
      C4 g = pica(p, env);
      // tolérance : arrondis sur 8 bits, amplifiés par l'échelle x4
      bool x4 = false;
      for (int s2 = 0; s2 < e.nbEtages; s2++) x4 |= e.etages[s2].echelleC == GX_CS_SCALE_4 || e.etages[s2].echelleA == GX_CS_SCALE_4;
      const float tol = (x4 ? 7.f : 3.f) / 255.f;
      for (int c = 0; c < 4; c++) {
        if (std::fabs(r.v[c] - g.v[c]) > tol) ok = false;
      }
      if (!ok && erreurs < 6) {
        printf("ÉCART (famille %d, %d étages GX -> %d PICA) : réf %.3f %.3f %.3f %.3f / pica %.3f %.3f %.3f %.3f\n",
               famille, e.nbEtages, p.nb, r.v[0], r.v[1], r.v[2], r.v[3], g.v[0], g.v[1], g.v[2], g.v[3]);
        for (int s = 0; s < e.nbEtages; s++) {
          auto& g2 = e.etages[s];
          printf("  GX %d : c=%d,%d,%d,%d op%d b%d e%d ->%d | a=%d,%d,%d,%d op%d b%d e%d ->%d k%x/%x\n", s, g2.c[0],
                 g2.c[1], g2.c[2], g2.c[3], g2.opC, g2.biaisC, g2.echelleC, g2.sortieC, g2.a[0], g2.a[1], g2.a[2],
                 g2.a[3], g2.opA, g2.biaisA, g2.echelleA, g2.sortieA, g2.kcSel, g2.kaSel);
        }
        for (int s = 0; s < p.nb; s++) {
          auto& x = p.etages[s];
          printf("  PICA %d : rvb f%d src %x,%x,%x op %x,%x,%x e%d | a f%d src %x,%x,%x op %x,%x,%x e%d | k %08x tampon %d%d\n",
                 s, x.fRvb, x.srcRvb[0], x.srcRvb[1], x.srcRvb[2], x.opRvb[0], x.opRvb[1], x.opRvb[2], x.echelleRvb,
                 x.fA, x.srcA[0], x.srcA[1], x.srcA[2], x.opA[0], x.opA[1], x.opA[2], x.echelleA, x.constante,
                 x.tamponRvb, x.tamponA);
        }
      }
    }
    if (ok) {
      exacts++;
    } else {
      erreurs++;
    }
  }
  for (int f = 0; f < 4; f++)
    printf("Famille %d : %d configurations, %d approximées (dont %d trop longues)\n", f, parFamille[f], approxFamille[f],
           tropLongFamille[f]);
  for (int f = 0; f < 4; f++) {
    printf("  raisons famille %d :", f);
    for (int r = 0; r < 16; r++) if (raisons[f][r]) printf(" [%d]=%d", r, raisons[f][r]);
    printf("\n");
  }
  printf("Configurations : %d, traduites exactement : %d, approximées : %d, ERREURS : %d\n", total, exacts, approx,
         erreurs);
  return erreurs == 0 ? 0 : 1;
}
