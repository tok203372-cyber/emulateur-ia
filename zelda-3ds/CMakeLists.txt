# Fabrication pour la New 3DS (devkitARM) :
#   cmake -B construction -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/3DS.cmake
# Les sources externes sont récupérées par outils/recuperer-sources.sh
cmake_minimum_required(VERSION 3.20)
project(zelda3ds C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
  # optimisé, avec les informations de débogage (pour retrouver l'origine d'un plantage)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

option(ZELDA3DS_JEU "Fabriquer aussi le jeu (Dusklight), pas seulement le moteur et sa démonstration" ON)

set(EXTERNE ${CMAKE_CURRENT_SOURCE_DIR}/externe)
set(DUSK_DIR ${EXTERNE}/dusklight)
set(AURORA_DIR ${DUSK_DIR}/extern/aurora)
set(BOREALIS_DIR ${DUSK_DIR}/extern/borealis)
if (NOT EXISTS ${AURORA_DIR}/lib/gx/fifo.cpp)
  message(FATAL_ERROR "Sources absentes : lancez d'abord outils/recuperer-sources.sh")
endif ()

# Options communes : le code de Dusklight est écrit pour un PC (TARGET_PC) ; caractères signés
# comme sur GameCube ; pas d'avertissements pour le code d'origine.
add_compile_options(-fsigned-char -Wno-multichar $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-declarations>)
add_compile_definitions(TARGET_PC=1)
# Comme sur PC : énumérations de la taille d'un int (arm-none-eabi les réduit par défaut) et
# int32_t = int (au lieu de long). Voir commun/entiers3ds.h.
# Attention : avec -fno-short-enums, ne jamais lire les champs de C3D_FrameBuf, C3D_RenderTarget
# ni DVLE_s (citro3d et libctru sont compilés avec des énumérations réduites).
add_compile_options(-fno-short-enums "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/commun/entiers3ds.h")
add_link_options(-Wl,--no-enum-size-warning)
# Une section par fonction : le code inutilisé est retiré à l'édition de liens (--gc-sections)
add_compile_options(-ffunction-sections -fdata-sections)

# Arbre fusionné d'Aurora : fichiers d'origine, puis nos versions 3DS (moteur/aurora) par-dessus.
# Rangé dans extern3ds/aurora pour que les inclusions <aurora/lib/...> du jeu les trouvent aussi.
set(EXTERN3DS ${CMAKE_BINARY_DIR}/extern3ds)
set(AURORA3DS ${EXTERN3DS}/aurora)
file(REMOVE_RECURSE ${AURORA3DS})
file(COPY ${AURORA_DIR}/lib ${AURORA_DIR}/include DESTINATION ${AURORA3DS})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/moteur/aurora/lib DESTINATION ${AURORA3DS})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/moteur/aurora/lib)
# (les petites corrections des fichiers d'origine sont dans correctifs/aurora.patch,
#  appliqué par outils/recuperer-sources.sh)

# fmt (mise en forme du texte)
add_library(fmt3ds STATIC ${EXTERNE}/fmt/src/format.cc)
target_include_directories(fmt3ds PUBLIC ${EXTERNE}/fmt/include)

add_subdirectory(moteur)
add_subdirectory(demo-gx)
if (ZELDA3DS_JEU)
  add_subdirectory(jeu)
endif ()
