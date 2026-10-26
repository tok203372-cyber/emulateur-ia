/* Inclus en premier dans tous les fichiers (option -include).
 *
 * Sur la 3DS (arm-none-eabi), int32_t et uint32_t sont des « long » ; sur PC, Android et dans
 * le code de Dusklight, ce sont des « int ». Les deux ont la même taille et la même convention
 * d'appel sur ARM, mais pas le même type pour le C++ (modèles, surcharges, concepts).
 * On aligne la 3DS sur les autres plateformes avant toute inclusion d'en-tête du système.
 * (libctru et citro3d sont en C : leurs fonctions ne dépendent pas de ce choix.)
 */
#ifndef ZELDA3DS_ENTIERS3DS_H
#define ZELDA3DS_ENTIERS3DS_H

#undef __INT32_TYPE__
#define __INT32_TYPE__ int
#undef __UINT32_TYPE__
#define __UINT32_TYPE__ unsigned int
#undef __INT_LEAST32_TYPE__
#define __INT_LEAST32_TYPE__ int
#undef __UINT_LEAST32_TYPE__
#define __UINT_LEAST32_TYPE__ unsigned int
#undef __INT32_C
#define __INT32_C(c) c
#undef __UINT32_C
#define __UINT32_C(c) c##U

#endif
