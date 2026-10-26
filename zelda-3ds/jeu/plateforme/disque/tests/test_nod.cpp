// Test sur PC de la lecture des disques (nod3ds.cpp) : un petit disque GameCube synthétique est
// écrit aux formats ISO, CISO et GCZ, puis relu par l'interface « nod » comme le fait Aurora.
//
//   g++ -std=c++20 -I.. -I<miniz> tests/test_nod.cpp ../nod3ds.cpp <miniz>/miniz.c -o test_nod
#include "nod.h"

#include "miniz.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int sErreurs = 0;
#define VERIFIER(cond)                                                                                                 \
  do {                                                                                                                 \
    if (!(cond)) {                                                                                                     \
      std::printf("ÉCHEC ligne %d : %s\n", __LINE__, #cond);                                                          \
      sErreurs++;                                                                                                      \
    }                                                                                                                  \
  } while (0)

void ecrireBE32(std::vector<uint8_t>& d, size_t pos, uint32_t v) {
  d[pos] = static_cast<uint8_t>(v >> 24);
  d[pos + 1] = static_cast<uint8_t>(v >> 16);
  d[pos + 2] = static_cast<uint8_t>(v >> 8);
  d[pos + 3] = static_cast<uint8_t>(v);
}
void ecrireLE32(std::vector<uint8_t>& d, size_t pos, uint32_t v) {
  for (int i = 0; i < 4; i++) d[pos + i] = static_cast<uint8_t>(v >> (8 * i));
}
void ecrireLE64(std::vector<uint8_t>& d, size_t pos, uint64_t v) {
  for (int i = 0; i < 8; i++) d[pos + i] = static_cast<uint8_t>(v >> (8 * i));
}

struct Fichier {
  std::string nom;
  bool dossier;
  uint32_t decalage;  // fichier : position sur le disque ; dossier : parent
  uint32_t taille;    // fichier : taille ; dossier : indice suivant
};

// Arborescence : / ; /res (1) ; /res/a.bin (2) ; /res/Sous (3) ; /res/Sous/c.arc (4) ; /b.bin (5)
std::vector<Fichier> sFichiers = {
    {"", true, 0, 6},
    {"res", true, 0, 5},
    {"a.bin", false, 0x30000, 5000},
    {"Sous", true, 1, 5},
    {"c.arc", false, 0x38000, 70000},
    {"b.bin", false, 0x60000, 12345},
};

uint8_t motif(uint32_t pos) { return static_cast<uint8_t>((pos * 2654435761u) >> 13); }

std::vector<uint8_t> fabriquerDisque() {
  std::vector<uint8_t> d(0x80000 + 0x10000, 0);
  std::memcpy(&d[0], "GZ2P01", 6);
  ecrireBE32(d, 0x1C, 0xC2339F3D);
  std::memcpy(&d[0x20], "DISQUE DE TEST", 14);
  // Apploader : en-tête de 0x20 octets, 0x100 octets de code, pas de fin
  ecrireBE32(d, 0x2440 + 0x14, 0x100);
  ecrireBE32(d, 0x2440 + 0x18, 0);
  // DOL à 0x10000 : une section de code (0x100, 0x200 octets) et une de données (0x300, 0x100)
  ecrireBE32(d, 0x420, 0x10000);
  ecrireBE32(d, 0x10000 + 0x00, 0x100);
  ecrireBE32(d, 0x10000 + 0x90, 0x200);
  ecrireBE32(d, 0x10000 + 0x1C, 0x300);
  ecrireBE32(d, 0x10000 + 0xAC, 0x100);
  for (uint32_t i = 0x100; i < 0x400; i++) d[0x10000 + i] = motif(i + 77);
  // FST à 0x20000
  std::vector<uint8_t> fst(sFichiers.size() * 12, 0);
  std::string noms;
  for (size_t i = 0; i < sFichiers.size(); i++) {
    const auto& f = sFichiers[i];
    const uint32_t decNom = static_cast<uint32_t>(noms.size());
    noms += f.nom;
    noms += '\0';
    ecrireBE32(fst, i * 12, (f.dossier ? 0x01000000u : 0u) | (i == 0 ? 0 : decNom));
    ecrireBE32(fst, i * 12 + 4, f.decalage);
    ecrireBE32(fst, i * 12 + 8, f.taille);
  }
  fst.insert(fst.end(), noms.begin(), noms.end());
  ecrireBE32(d, 0x424, 0x20000);
  ecrireBE32(d, 0x428, static_cast<uint32_t>(fst.size()));
  std::memcpy(&d[0x20000], fst.data(), fst.size());
  // Contenu des fichiers
  for (const auto& f : sFichiers)
    if (!f.dossier)
      for (uint32_t i = 0; i < f.taille; i++) d[f.decalage + i] = motif(f.decalage + i);
  return d;
}

void ecrireFichier(const std::string& chemin, const std::vector<uint8_t>& d) {
  FILE* f = std::fopen(chemin.c_str(), "wb");
  std::fwrite(d.data(), 1, d.size(), f);
  std::fclose(f);
}

std::vector<uint8_t> enCiso(const std::vector<uint8_t>& d, uint32_t bloc) {
  std::vector<uint8_t> sortie(0x8000, 0);
  std::memcpy(&sortie[0], "CISO", 4);
  ecrireLE32(sortie, 4, bloc);
  for (size_t b = 0; b * bloc < d.size(); b++) {
    bool vide = true;
    for (uint32_t i = 0; i < bloc && b * bloc + i < d.size(); i++) vide = vide && d[b * bloc + i] == 0;
    if (vide) continue;
    sortie[8 + b] = 1;
    std::vector<uint8_t> contenu(bloc, 0);
    std::memcpy(contenu.data(), &d[b * bloc], std::min<size_t>(bloc, d.size() - b * bloc));
    sortie.insert(sortie.end(), contenu.begin(), contenu.end());
  }
  return sortie;
}

std::vector<uint8_t> enGcz(const std::vector<uint8_t>& d, uint32_t bloc) {
  const uint32_t nb = static_cast<uint32_t>((d.size() + bloc - 1) / bloc);
  std::vector<uint8_t> donnees;
  std::vector<uint64_t> pointeurs;
  for (uint32_t b = 0; b < nb; b++) {
    std::vector<uint8_t> contenu(bloc, 0);
    std::memcpy(contenu.data(), &d[b * bloc], std::min<size_t>(bloc, d.size() - static_cast<size_t>(b) * bloc));
    mz_ulong t = mz_compressBound(bloc);
    std::vector<uint8_t> comp(t);
    mz_compress2(comp.data(), &t, contenu.data(), bloc, 9);
    if (t >= bloc || (b % 7) == 3) {  // quelques blocs non compressés, comme Dolphin
      pointeurs.push_back(donnees.size() | (1ull << 63));
      donnees.insert(donnees.end(), contenu.begin(), contenu.end());
    } else {
      pointeurs.push_back(donnees.size());
      donnees.insert(donnees.end(), comp.begin(), comp.begin() + t);
    }
  }
  std::vector<uint8_t> sortie(32 + nb * 12, 0);
  ecrireLE32(sortie, 0, 0xB10BC001);
  ecrireLE64(sortie, 8, donnees.size());
  ecrireLE64(sortie, 16, d.size());
  ecrireLE32(sortie, 24, bloc);
  ecrireLE32(sortie, 28, nb);
  for (uint32_t b = 0; b < nb; b++) ecrireLE64(sortie, 32 + b * 8, pointeurs[b]);
  sortie.insert(sortie.end(), donnees.begin(), donnees.end());
  return sortie;
}

int64_t lireA(void* u, uint64_t pos, void* out, size_t len) {
  FILE* f = static_cast<FILE*>(u);
  if (std::fseek(f, static_cast<long>(pos), SEEK_SET) != 0) return -1;
  return static_cast<int64_t>(std::fread(out, 1, len, f));
}
int64_t longueur(void* u) {
  FILE* f = static_cast<FILE*>(u);
  std::fseek(f, 0, SEEK_END);
  return std::ftell(f);
}
void fermer(void* u) { std::fclose(static_cast<FILE*>(u)); }

struct Vu {
  uint32_t indice;
  NodNodeKind genre;
  std::string nom;
  uint32_t taille;
};

uint32_t rappelFst(uint32_t i, NodNodeKind genre, const char* nom, uint32_t taille, void* u) {
  static_cast<std::vector<Vu>*>(u)->push_back({i, genre, nom, taille});
  return i + 1;
}

void tester(const char* nomFormat, const std::string& chemin, const std::vector<uint8_t>& disque) {
  std::printf("== %s\n", nomFormat);
  FILE* f = std::fopen(chemin.c_str(), "rb");
  NodDiscStream flux{f, lireA, longueur, fermer};
  NodDiscOptions opts{.preloader_threads = 1};
  NodHandle* h = nullptr;
  VERIFIER(nod_disc_open_stream(&flux, &opts, &h) == NOD_RESULT_OK && h != nullptr);
  if (h == nullptr) {
    std::printf("  %s\n", nod_error_message());
    return;
  }
  NodDiscHeader e{};
  VERIFIER(nod_disc_header(h, &e) == NOD_RESULT_OK);
  VERIFIER(std::memcmp(e.game_id, "GZ2P01", 6) == 0);
  VERIFIER(std::strcmp(e.game_title, "DISQUE DE TEST") == 0);

  NodHandle* p = nullptr;
  VERIFIER(nod_disc_open_partition_kind(h, NOD_PARTITION_KIND_DATA, nullptr, &p) == NOD_RESULT_OK && p != nullptr);
  NodPartitionMeta m{};
  VERIFIER(nod_partition_meta(p, &m) == NOD_RESULT_OK);
  VERIFIER(m.raw_dol.size == 0x400);
  VERIFIER(m.raw_dol.data != nullptr && std::memcmp(m.raw_dol.data, &disque[0x10000], 0x400) == 0);
  VERIFIER(m.raw_apploader.size == 0x120);
  VERIFIER(m.raw_boot.size == 0x440 && std::memcmp(m.raw_boot.data, disque.data(), 0x440) == 0);

  std::vector<Vu> vus;
  nod_partition_iterate_fst(p, rappelFst, &vus);
  VERIFIER(vus.size() == sFichiers.size() - 1);
  for (const auto& v : vus) {
    const auto& attendu = sFichiers[v.indice];
    VERIFIER(v.nom == attendu.nom);
    VERIFIER((v.genre == NOD_NODE_KIND_DIRECTORY) == attendu.dossier);
    VERIFIER(v.taille == attendu.taille);
  }

  for (uint32_t i = 1; i < sFichiers.size(); i++) {
    const auto& fi = sFichiers[i];
    NodHandle* hf = nullptr;
    const NodResult r = nod_partition_open_file(p, i, &hf);
    if (fi.dossier) {
      VERIFIER(r != NOD_RESULT_OK);
      continue;
    }
    VERIFIER(r == NOD_RESULT_OK && hf != nullptr);
    std::vector<uint8_t> lu;
    uint8_t tampon[777];
    int64_t n;
    while ((n = nod_read(hf, tampon, sizeof tampon)) > 0) lu.insert(lu.end(), tampon, tampon + n);
    VERIFIER(n == 0);
    VERIFIER(lu.size() == fi.taille && std::memcmp(lu.data(), &disque[fi.decalage], fi.taille) == 0);
    // Positionnement puis lecture
    VERIFIER(nod_seek(hf, fi.taille / 2, 0) == fi.taille / 2);
    VERIFIER(nod_read(hf, tampon, 100) == 100 && std::memcmp(tampon, &disque[fi.decalage + fi.taille / 2], 100) == 0);
    VERIFIER(nod_seek(hf, -10, 2) == fi.taille - 10);
    VERIFIER(nod_read(hf, tampon, 100) == 10);
    nod_free(hf);
  }

  // Lecture absolue sur le disque (DVDReadAbs)
  uint8_t t2[4000];
  VERIFIER(nod_seek(h, 0x37F00, 0) == 0x37F00);
  VERIFIER(nod_read(h, t2, sizeof t2) == sizeof t2 && std::memcmp(t2, &disque[0x37F00], sizeof t2) == 0);

  nod_free(p);
  nod_free(h);
}

} // namespace

int main() {
  const auto disque = fabriquerDisque();
  ecrireFichier("/tmp/test_nod.iso", disque);
  ecrireFichier("/tmp/test_nod.ciso", enCiso(disque, 0x8000));
  ecrireFichier("/tmp/test_nod.gcz", enGcz(disque, 0x4000));
  tester("ISO", "/tmp/test_nod.iso", disque);
  tester("CISO", "/tmp/test_nod.ciso", disque);
  tester("GCZ", "/tmp/test_nod.gcz", disque);

  // Format refusé
  std::vector<uint8_t> rvz(64, 0);
  std::memcpy(rvz.data(), "RVZ\x01", 4);
  ecrireFichier("/tmp/test_nod.rvz", rvz);
  FILE* f = std::fopen("/tmp/test_nod.rvz", "rb");
  NodDiscStream flux{f, lireA, longueur, fermer};
  NodHandle* h = nullptr;
  VERIFIER(nod_disc_open_stream(&flux, nullptr, &h) == NOD_RESULT_ERR_FORMAT && h == nullptr);
  std::printf("RVZ : %s\n", nod_error_message());

  std::printf(sErreurs == 0 ? "Tout est correct.\n" : "%d erreur(s)\n", sErreurs);
  return sErreurs == 0 ? 0 : 1;
}
