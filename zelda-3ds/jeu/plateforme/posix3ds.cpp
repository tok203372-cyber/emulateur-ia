// Fonctions POSIX absentes de la bibliothèque C de la 3DS (newlib de devkitARM), utilisées par
// borealis et Dusklight.
#include <cerrno>
#include <mutex>

#include <unistd.h>

namespace {
std::mutex sVerrouPread;
}

extern "C" {

// Lecture à une position donnée sans déplacer la position du fichier (borealis::io).
// Les autres lectures du même fichier passent par la même fonction : le verrou suffit.
ssize_t pread(int fd, void* tampon, size_t taille, off_t position) {
  std::lock_guard verrou(sVerrouPread);
  const off_t ancienne = lseek(fd, 0, SEEK_CUR);
  if (ancienne < 0 || lseek(fd, position, SEEK_SET) < 0) {
    return -1;
  }
  const ssize_t lus = read(fd, tampon, taille);
  const int erreur = errno;
  lseek(fd, ancienne, SEEK_SET);
  errno = erreur;
  return lus;
}

// Tubes (réveil du fil réseau de borealis) : pas de réseau sur 3DS
int pipe(int[2]) {
  errno = ENOSYS;
  return -1;
}

// Redémarrage du programme (après un changement de réglage sur PC) : impossible ici
int execv(const char*, char* const[]) {
  errno = ENOSYS;
  return -1;
}

} // extern "C"
