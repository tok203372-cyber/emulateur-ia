// Traduction du TEV GameCube vers le combineur de la PICA200.
//
// Principe :
//  - chaque étage GX calcule  sortie = (d ± lerp(a, b, c) + biais) * échelle
//    séparément pour la couleur (RVB) et pour l'alpha ;
//  - on simplifie d'abord (arguments nuls, constants, égaux...), on calcule sur le
//    processeur tout ce qui ne dépend que de constantes ;
//  - on produit 0, 1 ou plusieurs opérations PICA par domaine (RVB / alpha), en veillant
//    à n'utiliser qu'une seule constante par opération (la PICA n'en a qu'une par étage) ;
//  - les registres GX (PREV, REG0..2) sont suivis : valeur de départ (constante), alias
//    d'une source (texture, couleur pixellisée...), ou valeur calculée présente dans la
//    sortie de l'étage précédent ou dans le tampon de la PICA.
#include "tev.hpp"

#include <algorithm>
#include <cmath>

namespace gx3ds::tev {
namespace {

enum class Genre : uint8_t { Zero, Un, Constante, Texture, Ras, Registre, Resultat };

// Canal lu : en RVB : 0 = rvb, 1 = alpha répété, 2 = r, 3 = v, 4 = b
//            en alpha : 0 = a, 1 = r, 2 = v, 3 = b
struct Val {
  Genre genre = Genre::Zero;
  uint8_t unite = 0;
  uint8_t registre = 0;
  bool regAlpha = false;  // Registre : lit la valeur du domaine alpha (APREV/A0.. dans la couleur)
  uint8_t canal = 0;
  bool moins = false;
  float k[3] = {0.f, 0.f, 0.f};  // Constante : RVB (k[0..2]) ou alpha (k[0])
};

bool egal(float a, float b) { return std::fabs(a - b) < 1.f / 512.f; }

Val zero() { return {}; }
Val un() {
  Val v;
  v.genre = Genre::Un;
  return v;
}
Val constante(float r, float g, float b) {
  Val v;
  v.genre = Genre::Constante;
  v.k[0] = r;
  v.k[1] = g;
  v.k[2] = b;
  return v;
}

// Une constante peut être en réalité 0 ou 1 : on la simplifie.
Val normaliser(Val v, bool alpha) {
  if (v.genre != Genre::Constante) {
    return v;
  }
  const int n = alpha ? 1 : 3;
  float k[3];
  for (int i = 0; i < n; i++) {
    k[i] = v.moins ? 1.f - v.k[i] : v.k[i];
  }
  bool tousZero = true, tousUn = true;
  for (int i = 0; i < n; i++) {
    tousZero &= egal(k[i], 0.f);
    tousUn &= egal(k[i], 1.f);
  }
  if (tousZero) return zero();
  if (tousUn) return un();
  Val r = v;
  r.moins = false;
  for (int i = 0; i < n; i++) r.k[i] = k[i];
  return r;
}

bool estConstante(const Val& v) {
  return v.genre == Genre::Zero || v.genre == Genre::Un || v.genre == Genre::Constante;
}

float valeurK(const Val& v, int i) {
  switch (v.genre) {
  case Genre::Zero:
    return 0.f;
  case Genre::Un:
    return 1.f;
  default:
    return v.moins ? 1.f - v.k[i] : v.k[i];
  }
}

Val inverse(Val v) {
  switch (v.genre) {
  case Genre::Zero:
    return un();
  case Genre::Un:
    return zero();
  default:
    v.moins = !v.moins;
    return v;
  }
}

bool memeVal(const Val& a, const Val& b, bool alpha) {
  if (a.genre != b.genre || a.moins != b.moins) return false;
  switch (a.genre) {
  case Genre::Zero:
  case Genre::Un:
  case Genre::Resultat:
    return a.canal == b.canal;
  case Genre::Constante:
    for (int i = 0; i < (alpha ? 1 : 3); i++)
      if (!egal(a.k[i], b.k[i])) return false;
    return true;
  case Genre::Texture:
    return a.unite == b.unite && a.canal == b.canal;
  case Genre::Ras:
    return a.canal == b.canal;
  case Genre::Registre:
    return a.registre == b.registre && a.regAlpha == b.regAlpha && a.canal == b.canal;
  }
  return false;
}

// Opération d'un domaine (avant placement dans un étage)
struct OpDom {
  uint8_t f = F_REMPLACER;
  Val v[3];
  uint8_t nb = 1;
  uint8_t echelle = 0;
};

constexpr int MaxOps = 8;

struct Sequence {
  OpDom ops[MaxOps];
  int nb = 0;
  bool approx = false;
  int raison = 0;  // pour le diagnostic
  // Résultat final si aucune opération n'est nécessaire (alias)
  bool estAlias = false;
  Val alias;

  void ajouter(const OpDom& op) {
    if (nb < MaxOps) {
      ops[nb++] = op;
    } else {
      approx = true;
      raison = 1;
    }
  }
};

Val resultat() {
  Val v;
  v.genre = Genre::Resultat;
  return v;
}

int nbConstantesDistinctes(const Val* v, int n, bool alpha) {
  const Val* vus[3];
  int nb = 0;
  for (int i = 0; i < n; i++) {
    if (v[i].genre != Genre::Constante && v[i].genre != Genre::Zero && v[i].genre != Genre::Un) continue;
    bool deja = false;
    for (int j = 0; j < nb; j++) {
      bool pareil = true;
      for (int c = 0; c < (alpha ? 1 : 3); c++) pareil &= egal(valeurK(*vus[j], c), valeurK(v[i], c));
      if (pareil) deja = true;
    }
    if (!deja) vus[nb++] = &v[i];
  }
  return nb;
}

OpDom op1(uint8_t f, Val a) {
  OpDom o;
  o.f = f;
  o.v[0] = a;
  o.nb = 1;
  return o;
}
OpDom op2(uint8_t f, Val a, Val b) {
  OpDom o;
  o.f = f;
  o.v[0] = a;
  o.v[1] = b;
  o.nb = 2;
  return o;
}
OpDom op3(uint8_t f, Val a, Val b, Val c) {
  OpDom o;
  o.f = f;
  o.v[0] = a;
  o.v[1] = b;
  o.v[2] = c;
  o.nb = 3;
  return o;
}

// Produit x*y : une opération, ou deux si deux constantes différentes
void emettreProduit(Sequence& s, Val x, Val y, bool alpha) {
  if (x.genre == Genre::Un) {
    s.ajouter(op1(F_REMPLACER, y));
    return;
  }
  if (y.genre == Genre::Un) {
    s.ajouter(op1(F_REMPLACER, x));
    return;
  }
  (void)alpha;
  s.ajouter(op2(F_MODULER, x, y));
}

// Somme x + y (y peut être Résultat)
void emettreSomme(Sequence& s, Val x, Val y, bool alpha, bool signe) {
  Val vals[2] = {x, y};
  if (!signe && x.genre == Genre::Zero) {
    s.ajouter(op1(F_REMPLACER, y));
    return;
  }
  if (!signe && y.genre == Genre::Zero) {
    s.ajouter(op1(F_REMPLACER, x));
    return;
  }
  if (nbConstantesDistinctes(vals, 2, alpha) > 1) {
    // les deux sont des constantes : on additionne sur le processeur
    Val c = constante(0, 0, 0);
    for (int i = 0; i < 3; i++) c.k[i] = std::clamp(valeurK(x, i) + valeurK(y, i) - (signe ? 0.5f : 0.f), 0.f, 1.f);
    s.ajouter(op1(F_REMPLACER, normaliser(c, alpha)));
    return;
  }
  s.ajouter(op2(signe ? F_AJOUTER_SIGNE : F_AJOUTER, x, y));
}

struct Calcul {
  Val a, b, c, d;
  uint8_t op;  // GXTevOp
  uint8_t biais;
  uint8_t echelle;
};

// Valeur constante exacte du calcul GX complet (tous les arguments constants)
Val replierTout(const Calcul& k, bool alpha) {
  Val r = constante(0, 0, 0);
  const float m = k.echelle == GX_CS_SCALE_2 ? 2.f : k.echelle == GX_CS_SCALE_4 ? 4.f : k.echelle == GX_CS_DIVIDE_2 ? 0.5f : 1.f;
  const float b = k.biais == GX_TB_ADDHALF ? 0.5f : k.biais == GX_TB_SUBHALF ? -0.5f : 0.f;
  for (int i = 0; i < 3; i++) {
    const float c = valeurK(k.c, i);
    const float l = valeurK(k.a, i) * (1.f - c) + valeurK(k.b, i) * c;
    const float d = valeurK(k.d, i);
    const float v = k.op == GX_TEV_SUB ? d - l : d + l;
    r.k[i] = std::clamp((v + b) * m, 0.f, 1.f);
  }
  return normaliser(r, alpha);
}

// Vérifie qu'aucune opération n'utilise deux constantes différentes
bool constantesValides(const Sequence& s, bool alpha) {
  for (int i = 0; i < s.nb; i++) {
    if (nbConstantesDistinctes(s.ops[i].v, s.ops[i].nb, alpha) > 1) return false;
  }
  return true;
}

// Produit la séquence d'opérations pour un domaine d'un étage GX.
Sequence emettre(Calcul k, bool alpha) {
  Sequence s;
  k.a = normaliser(k.a, alpha);
  k.b = normaliser(k.b, alpha);
  k.c = normaliser(k.c, alpha);
  k.d = normaliser(k.d, alpha);

  bool comparaison = k.op >= GX_TEV_COMP_R8_GT;
  if (comparaison) {
    // Les comparaisons sont rares : on garde d (approximation)
    s.approx = true;
    s.raison = 2;
    s.estAlias = true;
    s.alias = k.d;
    return s;
  }

  // Tout est constant : calcul exact sur le processeur
  if (estConstante(k.a) && estConstante(k.b) && estConstante(k.c) && estConstante(k.d)) {
    s.estAlias = true;
    s.alias = replierTout(k, alpha);
    return s;
  }

  const bool soustraire = k.op == GX_TEV_SUB;

  // Biais ajouté directement à d quand d est une constante (évite une limitation intermédiaire)
  if (k.biais != GX_TB_ZERO && estConstante(k.d)) {
    const float b = k.biais == GX_TB_ADDHALF ? 0.5f : -0.5f;
    bool possible = true;
    Val d2 = constante(0, 0, 0);
    for (int i = 0; i < 3; i++) {
      d2.k[i] = valeurK(k.d, i) + b;
      if (d2.k[i] < -0.001f || d2.k[i] > 1.001f) possible = false;
      d2.k[i] = std::clamp(d2.k[i], 0.f, 1.f);
    }
    if (possible) {
      k.d = normaliser(d2, alpha);
      k.biais = GX_TB_ZERO;
    }
  }

  // --- lerp(a, b, c) = a (1 - c) + b c
  // Forme du terme L : soit une valeur directe (aucune opération), soit un produit x*y en attente,
  // soit le résultat d'opérations déjà émises.
  enum class FormeL { Direct, Produit, Calcule } forme = FormeL::Direct;
  Val L, px, py;

  if (k.c.genre == Genre::Zero || memeVal(k.a, k.b, alpha)) {
    L = k.a;
  } else if (k.c.genre == Genre::Un) {
    L = k.b;
  } else if (k.a.genre == Genre::Zero && k.b.genre == Genre::Zero) {
    L = zero();
  } else if (estConstante(k.a) && estConstante(k.b) && estConstante(k.c)) {
    Val r = constante(0, 0, 0);
    for (int i = 0; i < 3; i++) {
      const float c = valeurK(k.c, i);
      r.k[i] = valeurK(k.a, i) * (1.f - c) + valeurK(k.b, i) * c;
    }
    L = normaliser(r, alpha);
  } else if (k.a.genre == Genre::Zero) {
    // L = b c
    if (k.b.genre == Genre::Un) {
      L = k.c;
    } else if (estConstante(k.b) && estConstante(k.c)) {
      Val r = constante(0, 0, 0);
      for (int i = 0; i < 3; i++) r.k[i] = valeurK(k.b, i) * valeurK(k.c, i);
      L = normaliser(r, alpha);
    } else {
      forme = FormeL::Produit;
      px = k.b;
      py = k.c;
    }
  } else if (k.b.genre == Genre::Zero) {
    // L = a (1 - c)
    const Val unMoinsC = inverse(k.c);
    if (k.a.genre == Genre::Un) {
      L = normaliser(unMoinsC, alpha);
    } else if (estConstante(k.a) && estConstante(k.c)) {
      Val r = constante(0, 0, 0);
      for (int i = 0; i < 3; i++) r.k[i] = valeurK(k.a, i) * (1.f - valeurK(k.c, i));
      L = normaliser(r, alpha);
    } else {
      forme = FormeL::Produit;
      px = k.a;
      py = unMoinsC;
    }
  } else {
    // cas général : interpolation
    Val args[3] = {k.b, k.a, k.c};
    const int nbK = nbConstantesDistinctes(args, 3, alpha);
    if (nbK <= 1) {
      s.ajouter(op3(F_INTERPOLER, k.b, k.a, k.c));
    } else if (estConstante(k.a) && estConstante(k.c)) {
      // b c + a (1 - c), avec a (1 - c) constant
      Val k1 = constante(0, 0, 0);
      for (int i = 0; i < 3; i++) k1.k[i] = valeurK(k.a, i) * (1.f - valeurK(k.c, i));
      emettreProduit(s, k.b, k.c, alpha);
      emettreSomme(s, resultat(), normaliser(k1, alpha), alpha, false);
    } else if (estConstante(k.b) && estConstante(k.c)) {
      // a (1 - c) + b c, avec b c constant
      Val k2 = constante(0, 0, 0);
      for (int i = 0; i < 3; i++) k2.k[i] = valeurK(k.b, i) * valeurK(k.c, i);
      emettreProduit(s, k.a, inverse(k.c), alpha);
      emettreSomme(s, resultat(), normaliser(k2, alpha), alpha, false);
    } else {
      // a et b constants (différents), c variable
      emettreProduit(s, k.b, k.c, alpha);
      s.ajouter(op3(F_MULTIPLIER_AJOUTER, k.a, inverse(k.c), resultat()));
    }
    forme = FormeL::Calcule;
    L = resultat();
  }

  // --- d ± L
  const bool biaisMoins = k.biais == GX_TB_SUBHALF;
  const bool biaisPlus = k.biais == GX_TB_ADDHALF;
  bool biaisFait = false;

  if (!soustraire) {
    if (forme == FormeL::Produit) {
      if (k.d.genre == Genre::Zero) {
        emettreProduit(s, px, py, alpha);
      } else {
        Val args[3] = {px, py, k.d};
        if (nbConstantesDistinctes(args, 3, alpha) <= 1 && !biaisMoins) {
          s.ajouter(op3(F_MULTIPLIER_AJOUTER, px, py, k.d));
        } else {
          emettreProduit(s, px, py, alpha);
          emettreSomme(s, resultat(), k.d, alpha, biaisMoins);
          biaisFait = biaisMoins;
        }
      }
    } else if (forme == FormeL::Calcule) {
      if (k.d.genre != Genre::Zero) {
        emettreSomme(s, resultat(), k.d, alpha, biaisMoins);
        biaisFait = biaisMoins;
      }
    } else {
      // L direct
      if (k.d.genre == Genre::Zero) {
        if (s.nb == 0) {
          s.estAlias = true;
          s.alias = L;
        }
      } else if (L.genre == Genre::Zero) {
        s.estAlias = true;
        s.alias = k.d;
      } else if (estConstante(L) && estConstante(k.d)) {
        // L et d constants : calcul exact (biais et échelle compris)
        s.estAlias = true;
        s.alias = replierTout(Calcul{L, L, zero(), k.d, k.op, k.biais, k.echelle}, alpha);
        return s;
      } else {
        emettreSomme(s, L, k.d, alpha, biaisMoins);
        biaisFait = biaisMoins;
      }
    }
  } else {
    // d - L
    if (forme == FormeL::Produit) {
      emettreProduit(s, px, py, alpha);
      forme = FormeL::Calcule;
      L = resultat();
    }
    if (L.genre == Genre::Zero) {
      s.estAlias = true;
      s.alias = k.d;
    } else if (estConstante(L) && estConstante(k.d)) {
      s.estAlias = true;
      s.alias = replierTout(Calcul{L, L, zero(), k.d, k.op, k.biais, k.echelle}, alpha);
      return s;
    } else if (biaisPlus) {
      // d - L + 1/2 = d + (1 - L) - 1/2 : une seule opération, sans limitation intermédiaire
      Val args[2] = {k.d, inverse(L)};
      if (nbConstantesDistinctes(args, 2, alpha) <= 1) {
        s.ajouter(op2(F_AJOUTER_SIGNE, k.d, inverse(L)));
        biaisFait = true;
      } else {
        s.ajouter(op2(F_SOUSTRAIRE, k.d, L));
      }
    } else {
      s.ajouter(op2(F_SOUSTRAIRE, k.d, L));
      if (biaisMoins) {
        s.approx = true;  // une différence négative serait limitée à 0
        s.raison = 3;
      }
    }
  }

  // Un alias constant (cas où L et d sont constants) : biais et échelle calculés directement
  auto replierAlias = [&](float ajout, float mult) {
    Val c = constante(0, 0, 0);
    for (int i = 0; i < 3; i++) c.k[i] = std::clamp((valeurK(s.alias, i) + ajout) * mult, 0.f, 1.f);
    s.alias = normaliser(c, alpha);
  };

  // Alias constant : biais et échelle calculés ensemble, sans limitation intermédiaire
  if (s.estAlias && estConstante(s.alias)) {
    const float ajout = biaisFait ? 0.f : biaisPlus ? 0.5f : biaisMoins ? -0.5f : 0.f;
    const float mult = k.echelle == GX_CS_SCALE_2   ? 2.f
                       : k.echelle == GX_CS_SCALE_4 ? 4.f
                       : k.echelle == GX_CS_DIVIDE_2 ? 0.5f
                                                     : 1.f;
    replierAlias(ajout, mult);
    return s;
  }

  // --- biais
  if (!biaisFait && (biaisMoins || biaisPlus)) {
    if (s.estAlias && estConstante(s.alias)) {
      replierAlias(biaisPlus ? 0.5f : -0.5f, 1.f);
    } else {
      Val base;
      if (s.estAlias) {
        base = s.alias;
        s.estAlias = false;
      } else {
        base = resultat();
      }
      if (biaisPlus) {
        s.ajouter(op2(F_AJOUTER, base, constante(0.5f, 0.5f, 0.5f)));
      } else {
        s.ajouter(op2(F_SOUSTRAIRE, base, constante(0.5f, 0.5f, 0.5f)));
      }
    }
  }

  // --- échelle
  if (k.echelle == GX_CS_SCALE_2 || k.echelle == GX_CS_SCALE_4) {
    if (s.estAlias && estConstante(s.alias)) {
      replierAlias(0.f, k.echelle == GX_CS_SCALE_2 ? 2.f : 4.f);
    } else {
      if (s.estAlias) {
        s.estAlias = false;
        s.ajouter(op1(F_REMPLACER, s.alias));
      }
      if (s.nb > 0) s.ops[s.nb - 1].echelle = k.echelle == GX_CS_SCALE_2 ? 1 : 2;
    }
  } else if (k.echelle == GX_CS_DIVIDE_2) {
    const Val demi = constante(0.5f, 0.5f, 0.5f);
    if (s.estAlias && estConstante(s.alias)) {
      replierAlias(0.f, 0.5f);
    } else if (s.estAlias) {
      s.estAlias = false;
      s.ajouter(op2(F_MODULER, s.alias, demi));
    } else {
      // La PICA limite chaque étage à 1 : (x + y) / 2 doit être fait en une fois.
      OpDom& der = s.ops[s.nb - 1];
      if (der.f == F_AJOUTER && !estConstante(der.v[0]) && !estConstante(der.v[1])) {
        der = op3(F_INTERPOLER, der.v[0], der.v[1], demi);
      } else {
        if (der.f == F_AJOUTER || der.f == F_MULTIPLIER_AJOUTER || der.f == F_AJOUTER_SIGNE) {
          s.approx = true;  // une somme supérieure à 1 serait déjà limitée
          s.raison = 4;
        }
        s.ajouter(op2(F_MODULER, resultat(), demi));
      }
    }
  }

  if (s.nb == 0 && !s.estAlias) {
    s.estAlias = true;
    s.alias = zero();
  }
  if (!constantesValides(s, alpha)) {
    s.approx = true;
    s.raison = 5;
  }
  return s;
}

// ---------------------------------------------------------------------------------------------
// Suivi des registres et placement dans les étages PICA

enum class EtatReg : uint8_t { Initial, Alias, Calcule };

struct Domaine {
  bool alpha = false;
  EtatReg etat[4] = {EtatReg::Initial, EtatReg::Initial, EtatReg::Initial, EtatReg::Initial};
  Val alias[4];
  // Pour une valeur calculée : intervalle des étages dont la sortie contient la valeur
  int8_t debut[4] = {-1, -1, -1, -1};
  int8_t fin[4] = {-1, -1, -1, -1};
  // Tampon de la PICA
  int8_t tamponReg = -1;
  int8_t tamponEtage = -1;           // étage dont la sortie a été copiée dans le tampon
  int8_t derniereLectureTampon = -1; // dernier étage qui lit le tampon
  // Occupant de la sortie du dernier étage émis (registre) ou -1
  int8_t occupant = -1;
};

struct Compilateur {
  const Entree& e;
  Programme& p;
  Domaine rvb;
  Domaine alp;
  int nbEtages = 0;
  bool constUtiliseeRvb[6] = {};
  bool constUtiliseeA[6] = {};
  float constRvb[6][3] = {};
  float constA[6] = {};

  Compilateur(const Entree& entree, Programme& sortie) : e(entree), p(sortie) { alp.alpha = true; }

  Domaine& dom(bool alpha) { return alpha ? alp : rvb; }

  // --- Lecture des arguments GX

  uint8_t canalEchange(uint8_t table, bool lireAlpha) const {
    const EchangeGx& ech = e.echanges[table & 3];
    if (lireAlpha) {
      switch (ech.a) {
      case GX_CH_RED:
        return 2;
      case GX_CH_GREEN:
        return 3;
      case GX_CH_BLUE:
        return 4;
      default:
        return 1;
      }
    }
    if (ech.r == GX_CH_RED && ech.v == GX_CH_GREEN && ech.b == GX_CH_BLUE) return 0;
    if (ech.r == ech.v && ech.v == ech.b) {
      switch (ech.r) {
      case GX_CH_RED:
        return 2;
      case GX_CH_GREEN:
        return 3;
      case GX_CH_BLUE:
        return 4;
      default:
        return 1;
      }
    }
    return 0;  // permutation non gérée : approximation
  }

  // Convertit un canal « vu en RVB » (0..4) en canal du domaine alpha (0..3)
  static uint8_t canalPourAlpha(uint8_t canalRvb) {
    switch (canalRvb) {
    case 2:
      return 1;
    case 3:
      return 2;
    case 4:
      return 3;
    default:
      return 0;
    }
  }

  Val konstCouleur(uint8_t sel) const {
    if (sel <= GX_TEV_KCSEL_1_8) {
      const float f = static_cast<float>(8 - sel) / 8.f;
      return constante(f, f, f);
    }
    if (sel >= GX_TEV_KCSEL_K0 && sel <= GX_TEV_KCSEL_K3) {
      const Couleur& k = e.konst[sel - GX_TEV_KCSEL_K0];
      return constante(k.r, k.v, k.b);
    }
    if (sel >= GX_TEV_KCSEL_K0_R && sel <= GX_TEV_KCSEL_K3_A) {
      const int idx = (sel - GX_TEV_KCSEL_K0_R) & 3;
      const int comp = (sel - GX_TEV_KCSEL_K0_R) >> 2;
      const Couleur& k = e.konst[idx];
      const float v = comp == 0 ? k.r : comp == 1 ? k.v : comp == 2 ? k.b : k.a;
      return constante(v, v, v);
    }
    return un();
  }

  Val konstAlpha(uint8_t sel) const {
    if (sel <= GX_TEV_KASEL_1_8) {
      const float f = static_cast<float>(8 - sel) / 8.f;
      return constante(f, f, f);
    }
    if (sel >= GX_TEV_KASEL_K0_R && sel <= GX_TEV_KASEL_K3_A) {
      const int idx = (sel - GX_TEV_KASEL_K0_R) & 3;
      const int comp = (sel - GX_TEV_KASEL_K0_R) >> 2;
      const Couleur& k = e.konst[idx];
      const float v = comp == 0 ? k.r : comp == 1 ? k.v : comp == 2 ? k.b : k.a;
      return constante(v, v, v);
    }
    return un();
  }

  Val texture(const EtageGx& g, bool lireAlpha, bool domaineAlpha) {
    if (g.unite < 0) {
      // pas de texture disponible : blanc opaque
      p.approxime = true;
      return un();
    }
    Val v;
    v.genre = Genre::Texture;
    v.unite = static_cast<uint8_t>(g.unite);
    const uint8_t c = canalEchange(g.echangeTex, lireAlpha);
    v.canal = domaineAlpha ? canalPourAlpha(c) : c;
    return v;
  }

  Val ras(const EtageGx& g, bool lireAlpha, bool domaineAlpha) {
    if (g.canal == GX_COLOR_ZERO || g.canal == GX_COLOR_NULL || g.canal == GX_ALPHA_BUMP ||
        g.canal == GX_ALPHA_BUMPN) {
      return zero();
    }
    if (g.canal != GX_COLOR0A0 && g.canal != GX_COLOR0 && g.canal != GX_ALPHA0) {
      p.approxime = true;  // couleur 1 : on prend la couleur 0
    }
    Val v;
    v.genre = Genre::Ras;
    const uint8_t c = canalEchange(g.echangeRas, lireAlpha);
    v.canal = domaineAlpha ? canalPourAlpha(c) : c;
    return v;
  }

  // Valeur d'un registre GX, telle que connue maintenant
  Val registre(int reg, bool valeurAlpha, bool domaineAlpha) {
    Domaine& d = dom(valeurAlpha);
    switch (d.etat[reg]) {
    case EtatReg::Initial: {
      const Couleur& c = e.registres[reg];
      if (valeurAlpha) {
        Val v = constante(c.a, c.a, c.a);
        return v;
      }
      return constante(c.r, c.v, c.b);
    }
    case EtatReg::Alias: {
      Val v = d.alias[reg];
      if (valeurAlpha && !domaineAlpha) {
        // alpha d'un alias lu dans le domaine RVB : alpha répété
        if (v.genre == Genre::Texture || v.genre == Genre::Ras || v.genre == Genre::Resultat ||
            v.genre == Genre::Registre) {
          v.canal = v.canal == 0 ? 1 : static_cast<uint8_t>(v.canal + 1);
          v.regAlpha = true;
        }
      }
      return v;
    }
    case EtatReg::Calcule: {
      Val v;
      v.genre = Genre::Registre;
      v.registre = static_cast<uint8_t>(reg);
      v.regAlpha = valeurAlpha;
      v.canal = (valeurAlpha && !domaineAlpha) ? 1 : 0;
      return v;
    }
    }
    return zero();
  }

  Val argCouleur(const EtageGx& g, uint8_t arg) {
    switch (arg) {
    case GX_CC_CPREV:
      return registre(0, false, false);
    case GX_CC_APREV:
      return registre(0, true, false);
    case GX_CC_C0:
      return registre(1, false, false);
    case GX_CC_A0:
      return registre(1, true, false);
    case GX_CC_C1:
      return registre(2, false, false);
    case GX_CC_A1:
      return registre(2, true, false);
    case GX_CC_C2:
      return registre(3, false, false);
    case GX_CC_A2:
      return registre(3, true, false);
    case GX_CC_TEXC:
      return texture(g, false, false);
    case GX_CC_TEXA:
      return texture(g, true, false);
    case GX_CC_RASC:
      return ras(g, false, false);
    case GX_CC_RASA:
      return ras(g, true, false);
    case GX_CC_ONE:
      return un();
    case GX_CC_HALF:
      return constante(0.5f, 0.5f, 0.5f);
    case GX_CC_KONST:
      return konstCouleur(g.kcSel);
    default:
      return zero();
    }
  }

  Val argAlpha(const EtageGx& g, uint8_t arg) {
    switch (arg) {
    case GX_CA_APREV:
      return registre(0, true, true);
    case GX_CA_A0:
      return registre(1, true, true);
    case GX_CA_A1:
      return registre(2, true, true);
    case GX_CA_A2:
      return registre(3, true, true);
    case GX_CA_TEXA:
      return texture(g, true, true);
    case GX_CA_RASA:
      return ras(g, true, true);
    case GX_CA_KONST:
      return konstAlpha(g.kaSel);
    default:
      return zero();
    }
  }

  // --- Placement

  // Rend la valeur d'un registre calculé lisible à l'étage « ici » : (source, est-ce possible)
  bool localiserRegistre(Domaine& d, int reg, int ici, uint8_t& source) {
    // Toujours présent dans la sortie de l'étage précédent ?
    if (d.fin[reg] == ici - 1 && d.occupant == reg) {
      source = SRC_PRECEDENT;
      return true;
    }
    // Déjà dans le tampon ? (une valeur copiée après l'étage t est lisible à partir de t + 2)
    if (d.tamponReg == reg && d.tamponEtage >= d.debut[reg] && d.tamponEtage <= d.fin[reg] &&
        d.tamponEtage + 2 <= ici) {
      source = SRC_TAMPON;
      d.derniereLectureTampon = static_cast<int8_t>(std::max<int>(d.derniereLectureTampon, ici));
      return true;
    }
    // Copier après coup dans le tampon : il faut un étage t (0..3) qui contenait la valeur,
    // sans écraser une valeur du tampon encore lue après t + 2.
    const int t = std::min<int>(d.fin[reg], 3);
    if (t >= d.debut[reg] && t >= 0 && t + 2 <= ici) {
      bool ok = d.derniereLectureTampon < t + 2;
      // aucun autre étage après t ne doit déjà écrire le tampon
      for (int i = t + 1; i < nbEtages && i <= 3; i++) {
        if (d.alpha ? p.etages[i].tamponA : p.etages[i].tamponRvb) ok = false;
      }
      if (ok) {
        if (d.alpha) {
          p.etages[t].tamponA = true;
        } else {
          p.etages[t].tamponRvb = true;
        }
        d.tamponReg = static_cast<int8_t>(reg);
        d.tamponEtage = static_cast<int8_t>(t);
        d.derniereLectureTampon = static_cast<int8_t>(ici);
        source = SRC_TAMPON;
        return true;
      }
    }
    return false;
  }

  // Source et opérande PICA pour une valeur, à l'étage « ici », dans un domaine.
  // Renvoie false si la valeur demande une constante différente de celle déjà utilisée.
  void resoudre(const Val& v, bool domaineAlpha, int ici, uint8_t& src, uint8_t& op) {
    uint8_t canal = v.canal;
    switch (v.genre) {
    case Genre::Zero:
    case Genre::Un:
    case Genre::Constante: {
      src = SRC_CONSTANTE;
      float k[3];
      for (int i = 0; i < 3; i++) k[i] = valeurK(v, i);
      if (domaineAlpha) {
        constA[ici] = k[0];
        constUtiliseeA[ici] = true;
      } else {
        for (int i = 0; i < 3; i++) constRvb[ici][i] = k[i];
        constUtiliseeRvb[ici] = true;
      }
      op = domaineAlpha ? static_cast<uint8_t>(A_ALPHA) : static_cast<uint8_t>(RVB_COULEUR);
      return;
    }
    case Genre::Texture:
      src = static_cast<uint8_t>(SRC_TEXTURE0 + v.unite);
      break;
    case Genre::Ras:
      src = SRC_PRIMAIRE;
      break;
    case Genre::Resultat:
      src = SRC_PRECEDENT;
      break;
    case Genre::Registre: {
      Domaine& d = dom(v.regAlpha);
      if (!localiserRegistre(d, v.registre, ici, src)) {
        p.approxime = true;
        if (!p.raison) p.raison = 10;
        src = SRC_PRECEDENT;
      }
      break;
    }
    }
    if (domaineAlpha) {
      static constexpr uint8_t tab[4] = {A_ALPHA, A_R, A_V, A_B};
      op = tab[canal & 3];
      if (v.moins) op = static_cast<uint8_t>(op + 1);
    } else {
      static constexpr uint8_t tab[5] = {RVB_COULEUR, RVB_ALPHA, RVB_R, RVB_V, RVB_B};
      op = tab[canal < 5 ? canal : 0];
      if (v.moins) op = static_cast<uint8_t>(op + 1);
    }
  }

  void placerOp(const OpDom& o, bool domaineAlpha, int ici) {
    EtagePica& et = p.etages[ici];
    uint8_t* src = domaineAlpha ? et.srcA : et.srcRvb;
    uint8_t* ops = domaineAlpha ? et.opA : et.opRvb;
    for (int i = 0; i < 3; i++) {
      if (i < o.nb) {
        resoudre(o.v[i], domaineAlpha, ici, src[i], ops[i]);
      } else {
        src[i] = SRC_PRECEDENT;
        ops[i] = domaineAlpha ? static_cast<uint8_t>(A_ALPHA) : static_cast<uint8_t>(RVB_COULEUR);
      }
    }
    if (domaineAlpha) {
      et.fA = o.f;
      et.echelleA = o.echelle;
    } else {
      et.fRvb = o.f;
      et.echelleRvb = o.echelle;
    }
  }

  void placerPassage(bool domaineAlpha, int ici) {
    EtagePica& et = p.etages[ici];
    if (domaineAlpha) {
      et.srcA[0] = SRC_PRECEDENT;
      et.opA[0] = A_ALPHA;
      et.fA = F_REMPLACER;
      et.echelleA = 0;
    } else {
      et.srcRvb[0] = SRC_PRECEDENT;
      et.opRvb[0] = RVB_COULEUR;
      et.fRvb = F_REMPLACER;
      et.echelleRvb = 0;
    }
  }

  // Après placement : mise à jour du suivi d'un domaine
  void finirDomaine(Domaine& d, const Sequence& s, int sortie, int base, int nb) {
    if (s.estAlias) {
      // Pas d'opération : le registre devient un alias. Les étages ajoutés (passage)
      // prolongent la valeur qui occupait la sortie.
      if (nb > 0 && d.occupant >= 0 && d.fin[d.occupant] == base - 1) {
        d.fin[d.occupant] = static_cast<int8_t>(base + nb - 1);
      }
      Val a = s.alias;
      // un alias d'un registre calculé n'est utilisable que si on le résout maintenant
      if (a.genre == Genre::Registre) {
        const int r = a.registre;
        Domaine& src = dom(a.regAlpha);
        if (&src == &d && src.etat[r] == EtatReg::Calcule) {
          d.etat[sortie] = EtatReg::Calcule;
          d.debut[sortie] = d.debut[r];
          d.fin[sortie] = d.fin[r];
          if (d.occupant == r) {
            // les deux registres partagent la même valeur ; on garde l'occupant
          }
          return;
        }
      }
      if (a.genre == Genre::Resultat) {
        // ne devrait pas arriver
        p.approxime = true;
      }
      d.etat[sortie] = EtatReg::Alias;
      d.alias[sortie] = a;
      return;
    }
    // Valeur calculée : présente à la sortie du dernier étage du groupe
    const int dernier = base + nb - 1;
    const int premierOk = base + s.nb - 1;
    d.etat[sortie] = EtatReg::Calcule;
    d.debut[sortie] = static_cast<int8_t>(premierOk);
    d.fin[sortie] = static_cast<int8_t>(dernier);
    d.occupant = static_cast<int8_t>(sortie);
    // les autres registres ne sont plus dans la sortie
    for (int r = 0; r < 4; r++) {
      if (r != sortie && d.etat[r] == EtatReg::Calcule && d.fin[r] >= premierOk) {
        d.fin[r] = static_cast<int8_t>(premierOk - 1);
      }
    }
  }

  void compiler() {
    p = Programme{};
    const int nbGx = std::clamp<int>(e.nbEtages, 1, 16);

    for (int s = 0; s < nbGx; s++) {
      const EtageGx& g = e.etages[s];

      Calcul cc{argCouleur(g, g.c[0]), argCouleur(g, g.c[1]), argCouleur(g, g.c[2]), argCouleur(g, g.c[3]),
                g.opC, g.biaisC, g.echelleC};
      Calcul ca{argAlpha(g, g.a[0]), argAlpha(g, g.a[1]), argAlpha(g, g.a[2]), argAlpha(g, g.a[3]),
                g.opA, g.biaisA, g.echelleA};
      Sequence sc = emettre(cc, false);
      Sequence sa = emettre(ca, true);
      if (sc.approx || sa.approx) {
        p.approxime = true;
        if (!p.raison) p.raison = static_cast<uint8_t>(sc.approx ? sc.raison : sa.raison);
      }

      int nb = std::max(sc.nb, sa.nb);
      if (nbEtages + nb > 6) {
        // plus de place : on arrête ici (approximation)
        p.tropLong = true;
        p.approxime = true;
        if (!p.raison) p.raison = 11;
        break;
      }
      const int base = nbEtages;
      for (int i = 0; i < nb; i++) {
        const int ici = base + i;
        p.etages[ici] = EtagePica{};
        if (i < sc.nb) {
          placerOp(sc.ops[i], false, ici);
        } else {
          placerPassage(false, ici);
        }
        if (i < sa.nb) {
          placerOp(sa.ops[i], true, ici);
        } else {
          placerPassage(true, ici);
        }
      }
      nbEtages += nb;
      // occupant de la sortie pendant les étages intermédiaires
      if (sc.nb > 0) rvb.occupant = -1;
      if (sa.nb > 0) alp.occupant = -1;
      finirDomaine(rvb, sc, g.sortieC & 3, base, nb);
      finirDomaine(alp, sa, g.sortieA & 3, base, nb);
    }

    // Le résultat final est la valeur de PREV : s'il n'est pas déjà la sortie du dernier
    // étage, on ajoute un étage qui le recopie.
    bool rvbOk = rvb.etat[0] == EtatReg::Calcule && rvb.occupant == 0 && rvb.fin[0] == nbEtages - 1;
    bool aOk = alp.etat[0] == EtatReg::Calcule && alp.occupant == 0 && alp.fin[0] == nbEtages - 1;
    if (!rvbOk || !aOk || nbEtages == 0) {
      if (nbEtages < 6) {
        const int ici = nbEtages;
        p.etages[ici] = EtagePica{};
        if (rvbOk) {
          placerPassage(false, ici);
        } else {
          OpDom o = op1(F_REMPLACER, registre(0, false, false));
          placerOp(o, false, ici);
        }
        if (aOk) {
          placerPassage(true, ici);
        } else {
          OpDom o = op1(F_REMPLACER, registre(0, true, true));
          placerOp(o, true, ici);
        }
        nbEtages++;
      } else {
        p.approxime = true;
        if (!p.raison) p.raison = 12;
      }
    }

    // Constantes des étages
    for (int i = 0; i < nbEtages; i++) {
      Couleur c{1.f, 1.f, 1.f, 1.f};
      if (constUtiliseeRvb[i]) {
        c.r = constRvb[i][0];
        c.v = constRvb[i][1];
        c.b = constRvb[i][2];
      }
      if (constUtiliseeA[i]) c.a = constA[i];
      p.etages[i].constante = empaqueter(c);
    }
    p.nb = static_cast<uint8_t>(nbEtages);
    // étages restants : passage
    for (int i = nbEtages; i < 6; i++) p.etages[i] = EtagePica{};
  }
};

} // namespace

uint32_t empaqueter(const Couleur& c) noexcept {
  auto o = [](float f) -> uint32_t { return static_cast<uint32_t>(std::lround(std::clamp(f, 0.f, 1.f) * 255.f)); };
  return o(c.r) | o(c.v) << 8 | o(c.b) << 16 | o(c.a) << 24;
}

void compiler(const Entree& entree, Programme& sortie) noexcept {
  Compilateur comp(entree, sortie);
  comp.compiler();
}

} // namespace gx3ds::tev
