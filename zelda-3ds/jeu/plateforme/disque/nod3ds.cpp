// Lecture des images de disques GameCube pour la 3DS : remplace la bibliothèque « nod » d'Aurora.
//
// Formats pris en charge :
//  - ISO (image brute, 1,4 Go pour Twilight Princess) ;
//  - CISO (blocs vides retirés) ;
//  - GCZ (blocs compressés avec zlib, format de Dolphin).
// Les formats RVZ et WIA (Dolphin) ne sont pas lus : il faut les convertir en ISO ou GCZ avec
// Dolphin (clic droit sur le jeu > « Convertir le fichier »).
#include "nod.h"

#include "miniz.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

thread_local std::string tErreur;

NodResult erreur(NodResult r, const char* message) {
  tErreur = message;
  return r;
}

uint32_t lireBE32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 | p[3];
}
uint32_t lireLE32(const uint8_t* p) {
  return p[0] | static_cast<uint32_t>(p[1]) << 8 | static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}
uint64_t lireLE64(const uint8_t* p) { return lireLE32(p) | static_cast<uint64_t>(lireLE32(p + 4)) << 32; }

// ---------------------------------------------------------------------------------------------
// Images de disque : lecture à une position du disque (décompressé)

class Image {
public:
  explicit Image(const NodDiscStream& flux) : mFlux(flux) {}
  virtual ~Image() {
    if (mFlux.close != nullptr) mFlux.close(mFlux.user_data);
  }
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  virtual uint64_t taille() const = 0;
  // Lit len octets à la position pos du disque ; renvoie le nombre d'octets lus ou -1
  int64_t lire(uint64_t pos, void* dst, size_t len) {
    std::lock_guard verrou{mVerrou};
    if (pos >= taille()) return 0;
    len = static_cast<size_t>(std::min<uint64_t>(len, taille() - pos));
    return lireInterne(pos, static_cast<uint8_t*>(dst), len) ? static_cast<int64_t>(len) : -1;
  }

protected:
  virtual bool lireInterne(uint64_t pos, uint8_t* dst, size_t len) = 0;

  // Lecture exacte dans le fichier
  bool lireFichier(uint64_t pos, void* dst, size_t len) {
    auto* d = static_cast<uint8_t*>(dst);
    while (len > 0) {
      const int64_t n = mFlux.read_at(mFlux.user_data, pos, d, len);
      if (n <= 0) return false;
      pos += static_cast<uint64_t>(n);
      d += n;
      len -= static_cast<size_t>(n);
    }
    return true;
  }

  NodDiscStream mFlux;
  std::mutex mVerrou;
};

class ImageIso final : public Image {
public:
  ImageIso(const NodDiscStream& flux, uint64_t taille) : Image(flux), mTaille(taille) {}
  uint64_t taille() const override { return mTaille; }

protected:
  bool lireInterne(uint64_t pos, uint8_t* dst, size_t len) override { return lireFichier(pos, dst, len); }

private:
  uint64_t mTaille;
};

// CISO : en-tête de 0x8000 octets ("CISO", taille de bloc, carte des blocs présents)
class ImageCiso final : public Image {
public:
  explicit ImageCiso(const NodDiscStream& flux) : Image(flux) {}

  bool ouvrir() {
    std::vector<uint8_t> entete(0x8000);
    if (!lireFichier(0, entete.data(), entete.size())) return false;
    mTailleBloc = lireLE32(&entete[4]);
    if (mTailleBloc == 0 || (mTailleBloc & (mTailleBloc - 1)) != 0) return false;
    const size_t nbBlocs = 0x8000 - 8;
    size_t dernier = 0;
    mIndices.assign(nbBlocs, UINT32_MAX);
    uint32_t n = 0;
    for (size_t b = 0; b < nbBlocs; b++) {
      if (entete[8 + b] != 0) {
        mIndices[b] = n++;
        dernier = b + 1;
      }
    }
    mIndices.resize(dernier);
    // Taille du disque GameCube : 1 459 978 240 octets (au moins tous les blocs présents)
    mTaille = std::max<uint64_t>(static_cast<uint64_t>(dernier) * mTailleBloc, 1459978240ull);
    return true;
  }
  uint64_t taille() const override { return mTaille; }

protected:
  bool lireInterne(uint64_t pos, uint8_t* dst, size_t len) override {
    while (len > 0) {
      const uint64_t bloc = pos / mTailleBloc;
      const uint32_t dans = static_cast<uint32_t>(pos % mTailleBloc);
      const size_t n = std::min<size_t>(len, mTailleBloc - dans);
      if (bloc >= mIndices.size() || mIndices[bloc] == UINT32_MAX) {
        std::memset(dst, 0, n);
      } else if (!lireFichier(0x8000 + static_cast<uint64_t>(mIndices[bloc]) * mTailleBloc + dans, dst, n)) {
        return false;
      }
      pos += n;
      dst += n;
      len -= n;
    }
    return true;
  }

private:
  uint32_t mTailleBloc = 0;
  uint64_t mTaille = 0;
  std::vector<uint32_t> mIndices;
};

// GCZ : blocs compressés par zlib (format « CompressedBlob » de Dolphin)
class ImageGcz final : public Image {
public:
  explicit ImageGcz(const NodDiscStream& flux) : Image(flux) {}

  bool ouvrir() {
    uint8_t e[32];
    if (!lireFichier(0, e, sizeof e)) return false;
    mTailleCompressee = lireLE64(e + 8);
    mTaille = lireLE64(e + 16);
    mTailleBloc = lireLE32(e + 24);
    const uint32_t nbBlocs = lireLE32(e + 28);
    if (mTailleBloc == 0 || mTailleBloc > 16u * 1024 * 1024 || nbBlocs == 0) return false;
    std::vector<uint8_t> tab(static_cast<size_t>(nbBlocs) * 8);
    if (!lireFichier(32, tab.data(), tab.size())) return false;
    mPointeurs.resize(nbBlocs);
    for (uint32_t i = 0; i < nbBlocs; i++) mPointeurs[i] = lireLE64(&tab[i * 8]);
    mDebutDonnees = 32 + static_cast<uint64_t>(nbBlocs) * 12;  // pointeurs (8) + sommes de contrôle (4)
    mBloc.resize(mTailleBloc);
    mCompresse.resize(mTailleBloc + 1024);
    return true;
  }
  uint64_t taille() const override { return mTaille; }

protected:
  bool lireInterne(uint64_t pos, uint8_t* dst, size_t len) override {
    while (len > 0) {
      const uint64_t bloc = pos / mTailleBloc;
      const uint32_t dans = static_cast<uint32_t>(pos % mTailleBloc);
      const size_t n = std::min<size_t>(len, mTailleBloc - dans);
      if (!chargerBloc(bloc)) return false;
      std::memcpy(dst, mBloc.data() + dans, n);
      pos += n;
      dst += n;
      len -= n;
    }
    return true;
  }

private:
  bool chargerBloc(uint64_t bloc) {
    if (bloc == mBlocCharge) return true;
    if (bloc >= mPointeurs.size()) {
      std::memset(mBloc.data(), 0, mBloc.size());
      mBlocCharge = bloc;
      return true;
    }
    constexpr uint64_t NonCompresse = 1ull << 63;
    const uint64_t p = mPointeurs[bloc];
    const uint64_t debut = p & ~NonCompresse;
    const uint64_t fin =
        bloc + 1 < mPointeurs.size() ? (mPointeurs[bloc + 1] & ~NonCompresse) : mTailleCompressee;
    if (fin < debut) return false;
    const uint64_t tailleComp = fin - debut;
    if (p & NonCompresse) {
      if (!lireFichier(mDebutDonnees + debut, mBloc.data(), std::min<uint64_t>(tailleComp, mTailleBloc))) return false;
    } else {
      if (tailleComp > mCompresse.size()) mCompresse.resize(static_cast<size_t>(tailleComp));
      if (!lireFichier(mDebutDonnees + debut, mCompresse.data(), static_cast<size_t>(tailleComp))) return false;
      mz_ulong tailleSortie = mTailleBloc;
      if (mz_uncompress(mBloc.data(), &tailleSortie, mCompresse.data(), static_cast<mz_ulong>(tailleComp)) != MZ_OK)
        return false;
    }
    mBlocCharge = bloc;
    return true;
  }

  uint64_t mTaille = 0;
  uint64_t mTailleCompressee = 0;
  uint32_t mTailleBloc = 0;
  uint64_t mDebutDonnees = 0;
  std::vector<uint64_t> mPointeurs;
  std::vector<uint8_t> mBloc;
  std::vector<uint8_t> mCompresse;
  uint64_t mBlocCharge = UINT64_MAX;
};

// ---------------------------------------------------------------------------------------------
// Partition de données (sur GameCube : le disque entier)

struct Partition {
  std::shared_ptr<Image> image;
  std::vector<uint8_t> boot;
  std::vector<uint8_t> bi2;
  std::vector<uint8_t> apploader;
  std::vector<uint8_t> dol;
  std::vector<uint8_t> fst;
  uint32_t nbEntrees = 0;
  uint32_t debutNoms = 0;

  bool lire(uint64_t pos, std::vector<uint8_t>& dst, size_t taille) {
    dst.resize(taille);
    return taille == 0 || image->lire(pos, dst.data(), taille) == static_cast<int64_t>(taille);
  }
};

} // namespace

// Poignée : disque, partition ou fichier
struct NodHandle {
  enum class Genre { Disque, Partition, Fichier } genre;
  std::shared_ptr<Image> image;
  std::shared_ptr<Partition> partition;
  uint64_t debut = 0;
  uint64_t taille = 0;
  uint64_t position = 0;
};

extern "C" {

const char* nod_error_message(void) { return tErreur.c_str(); }

NodResult nod_disc_open_stream(const NodDiscStream* flux, const NodDiscOptions* options, NodHandle** out) {
  (void)options;
  if (flux == nullptr || out == nullptr || flux->read_at == nullptr) return erreur(NOD_RESULT_ERR_OTHER, "paramètres");
  *out = nullptr;
  uint8_t e[0x40] = {};
  if (flux->read_at(flux->user_data, 0, e, sizeof e) != static_cast<int64_t>(sizeof e)) {
    if (flux->close) flux->close(flux->user_data);
    return erreur(NOD_RESULT_ERR_IO, "lecture impossible");
  }
  std::shared_ptr<Image> image;
  if (std::memcmp(e, "CISO", 4) == 0) {
    auto ciso = std::make_shared<ImageCiso>(*flux);
    if (!ciso->ouvrir()) return erreur(NOD_RESULT_ERR_FORMAT, "CISO invalide");
    image = ciso;
  } else if (lireLE32(e) == 0xB10BC001u) {
    auto gcz = std::make_shared<ImageGcz>(*flux);
    if (!gcz->ouvrir()) return erreur(NOD_RESULT_ERR_FORMAT, "GCZ invalide");
    image = gcz;
  } else if (std::memcmp(e, "RVZ\x01", 4) == 0 || std::memcmp(e, "WIA\x01", 4) == 0) {
    if (flux->close) flux->close(flux->user_data);
    return erreur(NOD_RESULT_ERR_FORMAT, "format RVZ/WIA : convertir en ISO ou GCZ avec Dolphin");
  } else {
    const int64_t t = flux->stream_len != nullptr ? flux->stream_len(flux->user_data) : -1;
    if (t <= 0) {
      if (flux->close) flux->close(flux->user_data);
      return erreur(NOD_RESULT_ERR_IO, "taille inconnue");
    }
    image = std::make_shared<ImageIso>(*flux, static_cast<uint64_t>(t));
  }
  // Vérification : disque GameCube (code magique à 0x1C)
  uint8_t entete[0x20];
  if (image->lire(0, entete, sizeof entete) != sizeof entete || lireBE32(entete + 0x1C) != 0xC2339F3Du) {
    if (lireBE32(entete + 0x18) == 0x5D1C9EA3u) return erreur(NOD_RESULT_ERR_FORMAT, "disque Wii non pris en charge");
    return erreur(NOD_RESULT_ERR_FORMAT, "ce n'est pas un disque GameCube");
  }
  auto* h = new NodHandle{NodHandle::Genre::Disque, image, nullptr, 0, image->taille(), 0};
  *out = h;
  return NOD_RESULT_OK;
}

NodResult nod_disc_open_partition_kind(NodHandle* disque, uint32_t genre, const NodPartitionOptions* options,
                                       NodHandle** out) {
  (void)options;
  if (disque == nullptr || out == nullptr || disque->genre != NodHandle::Genre::Disque)
    return erreur(NOD_RESULT_ERR_INVALID_HANDLE, "poignée");
  if (genre != NOD_PARTITION_KIND_DATA) return erreur(NOD_RESULT_ERR_NOT_FOUND, "partition absente");
  auto p = std::make_shared<Partition>();
  p->image = disque->image;
  if (!p->lire(0, p->boot, 0x440) || !p->lire(0x440, p->bi2, 0x2000)) return erreur(NOD_RESULT_ERR_IO, "en-tête");

  // Chargeur d'application (apploader)
  uint8_t al[0x20];
  if (p->image->lire(0x2440, al, sizeof al) != sizeof al) return erreur(NOD_RESULT_ERR_IO, "apploader");
  const uint32_t tailleAl = 0x20 + lireBE32(al + 0x14) + lireBE32(al + 0x18);
  if (tailleAl < 16u * 1024 * 1024) p->lire(0x2440, p->apploader, tailleAl);

  // Programme principal (DOL) : taille calculée à partir de ses sections
  const uint32_t debutDol = lireBE32(&p->boot[0x420]);
  uint8_t ed[0x100];
  if (p->image->lire(debutDol, ed, sizeof ed) != sizeof ed) return erreur(NOD_RESULT_ERR_IO, "DOL");
  uint32_t tailleDol = 0x100;
  for (int i = 0; i < 18; i++) {
    const uint32_t off = lireBE32(ed + i * 4);
    const uint32_t taille = lireBE32(ed + 0x90 + i * 4);
    if (taille != 0) tailleDol = std::max(tailleDol, off + taille);
  }
  if (tailleDol > 32u * 1024 * 1024 || !p->lire(debutDol, p->dol, tailleDol)) return erreur(NOD_RESULT_ERR_IO, "DOL");

  // Table des fichiers (FST)
  const uint32_t debutFst = lireBE32(&p->boot[0x424]);
  const uint32_t tailleFst = lireBE32(&p->boot[0x428]);
  if (tailleFst < 12 || tailleFst > 16u * 1024 * 1024 || !p->lire(debutFst, p->fst, tailleFst))
    return erreur(NOD_RESULT_ERR_FORMAT, "FST");
  p->nbEntrees = lireBE32(&p->fst[8]);
  if (static_cast<uint64_t>(p->nbEntrees) * 12 > tailleFst) return erreur(NOD_RESULT_ERR_FORMAT, "FST");
  p->debutNoms = p->nbEntrees * 12;

  auto* h = new NodHandle{NodHandle::Genre::Partition, disque->image, p, 0, disque->taille, 0};
  *out = h;
  return NOD_RESULT_OK;
}

NodResult nod_partition_open_file(NodHandle* partition, uint32_t indice, NodHandle** out) {
  if (partition == nullptr || out == nullptr || partition->genre != NodHandle::Genre::Partition)
    return erreur(NOD_RESULT_ERR_INVALID_HANDLE, "poignée");
  const Partition& p = *partition->partition;
  if (indice == 0 || indice >= p.nbEntrees) return erreur(NOD_RESULT_ERR_NOT_FOUND, "entrée");
  const uint8_t* e = &p.fst[indice * 12];
  if (e[0] != 0) return erreur(NOD_RESULT_ERR_OTHER, "c'est un dossier");
  auto* h = new NodHandle{NodHandle::Genre::Fichier, partition->image, partition->partition, lireBE32(e + 4),
                          lireBE32(e + 8), 0};
  *out = h;
  return NOD_RESULT_OK;
}

void nod_free(NodHandle* poignee) { delete poignee; }

int64_t nod_read(NodHandle* h, uint8_t* buf, size_t len) {
  if (h == nullptr || buf == nullptr || h->genre == NodHandle::Genre::Partition) return -1;
  if (h->position >= h->taille) return 0;
  const size_t n = static_cast<size_t>(std::min<uint64_t>(len, h->taille - h->position));
  const int64_t lus = h->image->lire(h->debut + h->position, buf, n);
  if (lus > 0) h->position += static_cast<uint64_t>(lus);
  return lus;
}

int64_t nod_seek(NodHandle* h, int64_t decalage, int32_t origine) {
  if (h == nullptr) return -1;
  int64_t base = 0;
  if (origine == 1) base = static_cast<int64_t>(h->position);
  else if (origine == 2) base = static_cast<int64_t>(h->taille);
  const int64_t pos = base + decalage;
  if (pos < 0) return -1;
  h->position = static_cast<uint64_t>(pos);
  return pos;
}

NodResult nod_disc_header(const NodHandle* disque, NodDiscHeader* out) {
  if (disque == nullptr || out == nullptr) return erreur(NOD_RESULT_ERR_INVALID_HANDLE, "poignée");
  static_assert(sizeof(NodDiscHeader) == 0x400);
  if (disque->image->lire(0, out, sizeof *out) != static_cast<int64_t>(sizeof *out)) return erreur(NOD_RESULT_ERR_IO, "en-tête");
  return NOD_RESULT_OK;
}

uint64_t nod_disc_size(const NodHandle* disque) { return disque != nullptr ? disque->image->taille() : 0; }

NodResult nod_partition_meta(const NodHandle* partition, NodPartitionMeta* out) {
  if (partition == nullptr || out == nullptr || partition->genre != NodHandle::Genre::Partition)
    return erreur(NOD_RESULT_ERR_INVALID_HANDLE, "poignée");
  const Partition& p = *partition->partition;
  *out = {};
  out->raw_boot = {p.boot.data(), p.boot.size()};
  out->raw_bi2 = {p.bi2.data(), p.bi2.size()};
  out->raw_apploader = {p.apploader.data(), p.apploader.size()};
  out->raw_dol = {p.dol.data(), p.dol.size()};
  out->raw_fst = {p.fst.data(), p.fst.size()};
  return NOD_RESULT_OK;
}

void nod_partition_iterate_fst(const NodHandle* partition, NodFstCallback rappel, void* donnees) {
  if (partition == nullptr || rappel == nullptr || partition->genre != NodHandle::Genre::Partition) return;
  const Partition& p = *partition->partition;
  uint32_t i = 1;  // l'entrée 0 est la racine
  while (i < p.nbEntrees) {
    const uint8_t* e = &p.fst[i * 12];
    const uint32_t decalageNom = lireBE32(e) & 0x00FFFFFFu;
    const char* nom = "";
    if (p.debutNoms + decalageNom < p.fst.size()) nom = reinterpret_cast<const char*>(&p.fst[p.debutNoms + decalageNom]);
    const NodNodeKind genre = e[0] != 0 ? NOD_NODE_KIND_DIRECTORY : NOD_NODE_KIND_FILE;
    i = rappel(i, genre, nom, lireBE32(e + 8), donnees);
  }
}

uint32_t nod_partition_find_file(const NodHandle* partition, const char* chemin, NodNodeKind* genre, uint32_t* taille) {
  (void)partition;
  (void)chemin;
  (void)genre;
  (void)taille;
  return NOD_FST_STOP;  // non utilisé par Aurora
}

} // extern "C"
