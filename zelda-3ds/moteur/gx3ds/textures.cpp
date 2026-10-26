// Cache des textures GX converties pour la PICA200.
#include "textures.hpp"

#include "conversion.hpp"
#include "gx3ds.hpp"

#include <dolphin/gx/GXEnum.h>

#include <cstring>
#include <unordered_map>

namespace gx3ds::textures {
namespace {

struct Cle {
  const void* donnees;
  const void* tlut;
  uint16_t largeur;
  uint16_t hauteur;
  uint8_t format;
  uint8_t formatTlut;
  uint16_t nbTlut;

  bool operator==(const Cle& o) const {
    return donnees == o.donnees && tlut == o.tlut && largeur == o.largeur && hauteur == o.hauteur &&
           format == o.format && formatTlut == o.formatTlut && nbTlut == o.nbTlut;
  }
};

struct HachageCle {
  size_t operator()(const Cle& c) const noexcept {
    size_t h = reinterpret_cast<uintptr_t>(c.donnees);
    h ^= reinterpret_cast<uintptr_t>(c.tlut) * 0x9E3779B1u;
    h = h * 31 + (static_cast<size_t>(c.largeur) << 16 | c.hauteur);
    h = h * 31 + (static_cast<size_t>(c.format) << 8 | c.formatTlut);
    return h;
  }
};

struct Entree {
  C3D_Tex tex{};
  bool valide = false;
  uint32_t octets = 0;
  uint32_t derniereImage = 0;
  uint32_t texObjId = 0;
  uint32_t version = 0;
  uint32_t tlutObjId = 0;
  uint32_t tlutVersion = 0;
  uint32_t empreinte = 0;
};

std::unordered_map<Cle, Entree, HachageCle> sCache;
uint32_t sOctets = 0;
uint32_t sConversionsImage = 0;
constexpr uint32_t BudgetOctets = 18u * 1024u * 1024u;
constexpr uint32_t ConversionsMaxParImage = 48;  // évite les longues pauses

C3D_Tex sBlanche;
C3D_Tex sTransparente;
bool sInit = false;
conversion::Image sImage;

uint32_t empreinte(const uint8_t* p, size_t taille, const uint8_t* tlut, uint32_t nbTlut) {
  uint32_t h = 2166136261u;
  if (p != nullptr && taille >= 4) {
    const size_t pas = taille / 32 > 4 ? taille / 32 : 4;
    for (size_t o = 0; o + 4 <= taille; o += pas) {
      uint32_t v;
      std::memcpy(&v, p + o, 4);
      h = (h ^ v) * 16777619u;
    }
    uint32_t fin;
    std::memcpy(&fin, p + taille - 4, 4);
    h = (h ^ fin) * 16777619u;
  }
  if (tlut != nullptr) {
    for (uint32_t i = 0; i < nbTlut; i += (nbTlut > 16 ? nbTlut / 16 : 1)) {
      uint16_t v;
      std::memcpy(&v, tlut + i * 2, 2);
      h = (h ^ v) * 16777619u;
    }
  }
  return h;
}

void remplirUni(C3D_Tex& tex, uint32_t rgba) {
  C3D_TexInit(&tex, 8, 8, GPU_RGBA8);
  auto* p = static_cast<uint32_t*>(tex.data);
  // RGBA8 PICA : mot de 32 bits = R<<24 | G<<16 | B<<8 | A
  const uint32_t r = rgba & 0xFF, g = rgba >> 8 & 0xFF, b = rgba >> 16 & 0xFF, a = rgba >> 24;
  const uint32_t v = r << 24 | g << 16 | b << 8 | a;
  for (int i = 0; i < 64; i++) p[i] = v;
  C3D_TexFlush(&tex);
  C3D_TexSetFilter(&tex, GPU_NEAREST, GPU_NEAREST);
}

void liberer(Entree& e) {
  if (e.valide) {
    C3D_TexDelete(&e.tex);
    sOctets -= e.octets;
    e.valide = false;
    e.octets = 0;
  }
}

// Libère les textures inutilisées depuis le plus longtemps
void faireDeLaPlace(uint32_t image, uint32_t besoin) {
  while (sOctets + besoin > BudgetOctets) {
    auto plusVieille = sCache.end();
    for (auto it = sCache.begin(); it != sCache.end(); ++it) {
      if (!it->second.valide || it->second.derniereImage >= image) continue;
      if (plusVieille == sCache.end() || it->second.derniereImage < plusVieille->second.derniereImage) {
        plusVieille = it;
      }
    }
    if (plusVieille == sCache.end()) return;
    liberer(plusVieille->second);
    sCache.erase(plusVieille);
  }
}

bool convertir(Entree& e, const GXTexObj_& obj, const GXTlutObj_* tlut, uint32_t image) {
  const uint32_t fmt = obj.format();
  const uint8_t* tlutDonnees = tlut ? static_cast<const uint8_t*>(tlut->data) : nullptr;
  if (!conversion::decoder(static_cast<const uint8_t*>(obj.data), fmt, obj.width(), obj.height(), tlutDonnees,
                           tlut ? tlut->format : 0, tlut ? tlut->numEntries : 0, sImage)) {
    return false;
  }
  const auto formatPica = conversion::choisirFormat(fmt, tlut ? tlut->format : 0, sImage);
  conversion::adapterTaille(sImage, reglages().textureMax);
  const uint32_t octets = sImage.largeur * sImage.hauteur * conversion::octetsParTexel(formatPica);
  faireDeLaPlace(image, octets);
  liberer(e);
  if (!C3D_TexInit(&e.tex, static_cast<u16>(sImage.largeur), static_cast<u16>(sImage.hauteur),
                   static_cast<GPU_TEXCOLOR>(formatPica))) {
    return false;
  }
  conversion::encoder(sImage, formatPica, static_cast<uint8_t*>(e.tex.data));
  C3D_TexFlush(&e.tex);
  e.valide = true;
  e.octets = octets;
  sOctets += octets;
  sConversionsImage++;
  return true;
}

} // namespace

void initialiser() noexcept {
  if (sInit) return;
  remplirUni(sBlanche, 0xFFFFFFFF);
  remplirUni(sTransparente, 0x00000000);
  sInit = true;
}

void arreter() noexcept {
  vider();
  if (sInit) {
    C3D_TexDelete(&sBlanche);
    C3D_TexDelete(&sTransparente);
    sInit = false;
  }
}

C3D_Tex* blanche() noexcept { return &sBlanche; }
C3D_Tex* transparente() noexcept { return &sTransparente; }

C3D_Tex* obtenir(const GXTexObj_& obj, const GXTlutObj_* tlut, uint32_t image) noexcept {
  if (!obj.has_data()) return &sTransparente;
  const uint32_t fmt = obj.format();
  const bool palette = fmt == GX_TF_C4 || fmt == GX_TF_C8 || fmt == GX_TF_C14X2;
  if (!palette) tlut = nullptr;

  Cle cle{obj.data, tlut ? tlut->data : nullptr, static_cast<uint16_t>(obj.width()),
          static_cast<uint16_t>(obj.height()), static_cast<uint8_t>(fmt),
          static_cast<uint8_t>(tlut ? tlut->format : 0), static_cast<uint16_t>(tlut ? tlut->numEntries : 0)};
  auto it = sCache.find(cle);
  if (it != sCache.end() && it->second.valide) {
    Entree& e = it->second;
    e.derniereImage = image;  // avant toute conversion : l'entrée ne doit pas être libérée
    const bool memesIds = e.texObjId == obj.texObjId && e.version == obj.texDataVersion &&
                          (tlut == nullptr || (e.tlutObjId == tlut->tlutObjId && e.tlutVersion == tlut->tlutDataVersion));
    if (!memesIds) {
      // Objet réinitialisé : on vérifie si le contenu a changé
      const uint32_t emp = empreinte(static_cast<const uint8_t*>(obj.data), conversion::tailleGx(fmt, obj.width(), obj.height()),
                                     tlut ? static_cast<const uint8_t*>(tlut->data) : nullptr, tlut ? tlut->numEntries : 0);
      if (emp != e.empreinte) {
        if (sConversionsImage < ConversionsMaxParImage) {
          convertir(e, obj, tlut, image);
          e.empreinte = emp;
        }
      }
      e.texObjId = obj.texObjId;
      e.version = obj.texDataVersion;
      e.tlutObjId = tlut ? tlut->tlutObjId : 0;
      e.tlutVersion = tlut ? tlut->tlutDataVersion : 0;
    }
    e.derniereImage = image;
    return e.valide ? &e.tex : &sBlanche;
  }

  if (sConversionsImage >= ConversionsMaxParImage) {
    return &sBlanche;  // la conversion se fera à l'image suivante
  }
  Entree nouvelle;
  if (!convertir(nouvelle, obj, tlut, image)) {
    return &sBlanche;
  }
  nouvelle.derniereImage = image;
  nouvelle.texObjId = obj.texObjId;
  nouvelle.version = obj.texDataVersion;
  nouvelle.tlutObjId = tlut ? tlut->tlutObjId : 0;
  nouvelle.tlutVersion = tlut ? tlut->tlutDataVersion : 0;
  nouvelle.empreinte = empreinte(static_cast<const uint8_t*>(obj.data), conversion::tailleGx(fmt, obj.width(), obj.height()),
                                 tlut ? static_cast<const uint8_t*>(tlut->data) : nullptr, tlut ? tlut->numEntries : 0);
  auto [pos, ok] = sCache.insert_or_assign(cle, nouvelle);
  (void)ok;
  return &pos->second.tex;
}

void fin_image(uint32_t image) noexcept {
  (void)image;
  sConversionsImage = 0;
}

void oublier_objet(uint32_t texObjId) noexcept {
  for (auto it = sCache.begin(); it != sCache.end();) {
    if (it->second.texObjId == texObjId) {
      // la texture a peut-être été utilisée par l'image en cours : on la garde jusqu'au
      // prochain besoin de place (elle n'est plus référencée par le jeu)
      it->second.texObjId = 0;
    }
    ++it;
  }
}

void oublier_tlut(uint32_t tlutObjId) noexcept {
  for (auto& [cle, e] : sCache) {
    if (e.tlutObjId == tlutObjId) e.tlutObjId = 0;
  }
}

void vider() noexcept {
  for (auto& [cle, e] : sCache) liberer(e);
  sCache.clear();
  sOctets = 0;
}

uint32_t octets_utilises() noexcept { return sOctets; }
uint32_t conversions_image() noexcept { return sConversionsImage; }

} // namespace gx3ds::textures
