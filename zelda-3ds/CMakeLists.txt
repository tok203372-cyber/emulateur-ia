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

set(EXTERNE ${CMAKE_CURRENT_SOURCE_DIR}/externe)
set(DUSK_DIR ${EXTERNE}/dusklight)
set(AURORA_DIR ${DUSK_DIR}/extern/aurora)
if (NOT EXISTS ${AURORA_DIR}/lib/gx/fifo.cpp)
  message(FATAL_ERROR "Sources absentes : lancez d'abord outils/recuperer-sources.sh")
endif ()

# Options communes : le code de Dusklight est écrit pour un PC (TARGET_PC) ; caractères signés
# comme sur GameCube ; pas d'avertissements pour le code d'origine.
add_compile_options(-fsigned-char -Wno-multichar $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-declarations>)
add_compile_definitions(TARGET_PC=1)

# fmt (mise en forme du texte)
add_library(fmt3ds STATIC ${EXTERNE}/fmt/src/format.cc)
target_include_directories(fmt3ds PUBLIC ${EXTERNE}/fmt/include)

add_subdirectory(moteur)
add_subdirectory(demo-gx)
