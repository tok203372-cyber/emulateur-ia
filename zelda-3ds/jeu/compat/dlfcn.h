#pragma once
// La 3DS n'a pas de bibliothèques dynamiques : les mods « natifs » de Dusklight (.dll/.so) ne
// peuvent pas être chargés. Ces versions échouent toujours, et le chargeur de mods l'indique.
#ifdef __cplusplus
extern "C" {
#endif

#define RTLD_LAZY 0x1
#define RTLD_NOW 0x2
#define RTLD_LOCAL 0x0
#define RTLD_GLOBAL 0x100

static inline void* dlopen(const char* chemin, int mode) {
  (void)chemin;
  (void)mode;
  return 0;
}
static inline void* dlsym(void* module, const char* nom) {
  (void)module;
  (void)nom;
  return 0;
}
static inline int dlclose(void* module) {
  (void)module;
  return 0;
}
static inline const char* dlerror(void) {
  return "les modules natifs ne sont pas pris en charge sur 3DS";
}

#ifdef __cplusplus
}
#endif
