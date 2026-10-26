# Le jeu : Dusklight (Twilight Princess reconstruit à partir du jeu GameCube) pour la New 3DS.
#
# Le code du jeu est compilé tel quel (avec quelques correctifs : correctifs/dusklight.patch).
# Ce qui dépend du PC est remplacé :
#  - rendu WebGPU -> moteur gx3ds (PICA200), dans ../moteur ;
#  - fenêtre, menus RmlUi et ImGui, modifications (mods) -> versions 3DS dans plateforme/ ;
#  - lecture du disque (nod, en Rust) -> plateforme/disque (ISO, CISO, GCZ).
# SDL 3 fonctionne sur 3DS : il fournit la manette, le son, les fichiers et les fils d'exécution.

# ---------------------------------------------------------------------------------------------
# SDL 3 (bibliothèque statique, sans vidéo : l'affichage passe par gx3ds)
foreach (_sous IN ITEMS VIDEO GPU RENDER CAMERA HIDAPI DIALOG TRAY)
  set(SDL_${_sous} OFF CACHE BOOL "" FORCE)
endforeach ()
# (HAPTIC : pas de vibreur sur 3DS, mais Aurora initialise ce sous-système : pilote factice)
foreach (_sous IN ITEMS AUDIO JOYSTICK HAPTIC POWER SENSOR)
  set(SDL_${_sous} ON CACHE BOOL "" FORCE)
endforeach ()
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory(${EXTERNE}/sdl3 ${CMAKE_BINARY_DIR}/sdl3 EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------------------------
# Petites bibliothèques
add_library(imgui3ds STATIC
    ${EXTERNE}/imgui/imgui.cpp
    ${EXTERNE}/imgui/imgui_draw.cpp
    ${EXTERNE}/imgui/imgui_tables.cpp
    ${EXTERNE}/imgui/imgui_widgets.cpp
    ${EXTERNE}/imgui/imgui_demo.cpp
)
target_include_directories(imgui3ds PUBLIC ${EXTERNE}/imgui)

add_library(miniz3ds STATIC ${EXTERNE}/miniz/miniz.c)
target_include_directories(miniz3ds PUBLIC ${EXTERNE}/miniz)

add_library(xxhash3ds STATIC ${EXTERNE}/xxhash/xxhash.c)
target_include_directories(xxhash3ds PUBLIC ${EXTERNE}/xxhash)

add_library(freeverb3ds STATIC
    ${DUSK_DIR}/libs/freeverb/comb.cpp
    ${DUSK_DIR}/libs/freeverb/allpass.cpp
    ${DUSK_DIR}/libs/freeverb/revmodel.cpp
)
target_include_directories(freeverb3ds PRIVATE ${DUSK_DIR}/libs/freeverb/include/freeverb)
target_include_directories(freeverb3ds PUBLIC ${DUSK_DIR}/libs/freeverb/include)

# En-tête de version de borealis (généré par CMake sur PC)
set(GENERE ${CMAKE_CURRENT_BINARY_DIR}/genere)
file(WRITE ${GENERE}/borealis/version.h.tmp
"#ifndef BOREALIS_VERSION_H
#define BOREALIS_VERSION_H
#define BOREALIS_APP_DESCRIBE \"3ds\"
#define BOREALIS_APP_VERSION \"0.0.0.0\"
#define BOREALIS_APP_SHORT_VERSION \"0.0.0\"
#define BOREALIS_APP_VERSION_CODE 0
#define BOREALIS_APP_BRANCH \"3ds\"
#define BOREALIS_APP_REVISION \"0\"
#define BOREALIS_APP_DATE \"2026\"
#define BOREALIS_PLATFORM_NAME \"3DS\"
#define BOREALIS_LIB_DESCRIBE \"3ds\"
#define BOREALIS_ARCH \"arm\"
#endif
")
configure_file(${GENERE}/borealis/version.h.tmp ${GENERE}/borealis/version.h COPYONLY)

# ---------------------------------------------------------------------------------------------
# En-têtes et définitions communs au jeu, à Aurora et à borealis (comme GameABIConfig.cmake)
add_library(jeu_commun INTERFACE)
target_include_directories(jeu_commun INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/plateforme
    ${CMAKE_CURRENT_SOURCE_DIR}/plateforme/disque
    ${DUSK_DIR}/include
    ${DUSK_DIR}/assets/GZ2E01
    ${DUSK_DIR}/libs/JSystem/include
    ${AURORA3DS}/include/dolphin
    ${AURORA3DS}/include
    ${DUSK_DIR}/sdk/include
    ${DUSK_DIR}/src
    ${EXTERN3DS}
    ${DUSK_DIR}/extern
    ${BOREALIS_DIR}/include
    ${GENERE}
    ${EXTERNE}/json/single_include
    ${EXTERNE}/cxxopts/include
    ${EXTERNE}/picosha2
    ${EXTERNE}/rmlui/Include
    ${EXTERNE}/tracy/public
)
target_compile_definitions(jeu_commun INTERFACE
    TARGET_PC=1 WIDESCREEN_SUPPORT=1 AVOID_UB=1 VERSION=0 MTX_USE_PS=1 PARTIAL_DEBUG=1
    AURORA=1 AURORA_ENABLE_GX=1 AURORA_ENABLE_RMLUI=1
    DUSK_HAS_FUNCHOOK=0 DUSK_HAS_PREPATCH=0 DUSK_3DS=1
    BOREALIS_HAS_DISCORD=0 BOREALIS_HAS_SENTRY=0 BOREALIS_BUILD_TYPE="RelWithDebInfo"
)
target_link_libraries(jeu_commun INTERFACE SDL3::SDL3-static fmt3ds imgui3ds miniz3ds xxhash3ds)

# ---------------------------------------------------------------------------------------------
# Aurora : modules restants (le GX est dans moteur3ds)
set(A ${AURORA3DS}/lib)
file(GLOB AURORA_GD ${A}/dolphin/gd/*.cpp)
file(GLOB AURORA_OS ${A}/dolphin/os/*.cpp)
file(GLOB AURORA_CARTE ${A}/card/*.cpp)
add_library(aurora3ds_systeme STATIC
    ${AURORA_GD}
    ${AURORA_OS}
    ${AURORA_CARTE}
    ${A}/dolphin/AR.cpp
    ${A}/dolphin/card.cpp
    ${A}/dolphin/pad/pad.cpp
    ${A}/dolphin/si/si.cpp
    ${A}/dolphin/ms/mouse.cpp
    ${A}/dolphin/dvd/dvd.cpp
    ${A}/dolphin/dvd/fst.cpp
    ${A}/dolphin/thp/THPAudio.cpp
    ${A}/dolphin/thp/THPDec.cpp
    ${A}/gx/dl.cpp
    ${A}/device.cpp
    ${A}/input.cpp
    ${A}/io.cpp
    ${A}/time.cpp
    plateforme/aurora_jeu.cpp
    plateforme/disque/nod3ds.cpp
)
target_link_libraries(aurora3ds_systeme PUBLIC moteur3ds jeu_commun)

# ---------------------------------------------------------------------------------------------
# borealis (outils de Dusklight) : seulement ce qui fonctionne sans réseau
set(B ${BOREALIS_DIR}/src)
add_library(borealis3ds STATIC
    ${B}/log.cpp
    ${B}/cli.cpp
    ${B}/task.cpp
    ${B}/url.cpp
    ${B}/io.cpp
    ${B}/data.cpp
    ${B}/disc.cpp
    ${B}/sentry.cpp
    ${B}/presentation.cpp
    ${B}/discord/stub.cpp
    ${B}/http.cpp
    ${B}/http_download.cpp
    ${B}/http/no_backend.cpp
    ${B}/ws.cpp
    ${B}/ws/no_backend.cpp
    ${B}/net.cpp
    ${B}/net/platform_posix.cpp
    ${B}/update/github.cpp
    ${B}/update/version.cpp
    ${B}/file_select/file_select.cpp
    plateforme/borealis3ds.cpp
)
target_link_libraries(borealis3ds PUBLIC jeu_commun)

# ---------------------------------------------------------------------------------------------
# Listes de fichiers de Dusklight (files.cmake), avec des chemins absolus
function(dusk3ds_add_library nom type)
  list(TRANSFORM ARGN PREPEND "${DUSK_DIR}/")
  add_library(${nom} ${type} ${ARGN})
endfunction()
file(READ ${DUSK_DIR}/files.cmake FICHIERS_DUSK)
string(REPLACE "add_library(" "dusk3ds_add_library(" FICHIERS_DUSK "${FICHIERS_DUSK}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/fichiers_dusk.cmake "${FICHIERS_DUSK}")
include(${CMAKE_CURRENT_BINARY_DIR}/fichiers_dusk.cmake)

set(DEFS_JEU DUSK_BUILDING_GAME=1)
foreach (lib IN LISTS JSYSTEM_LIBRARIES)
  target_compile_definitions(${lib} PRIVATE ${DEFS_JEU})
  target_link_libraries(${lib} PRIVATE jeu_commun)
endforeach ()

# Fichiers propres à Dusklight : on retire ce qui ne fonctionne pas sur 3DS (menus RmlUi,
# services de mods liés aux menus et à WebGPU, manifeste zstd) ; des remplaçants sont dans
# plateforme/remplacants_dusk.cpp. (Les modules natifs sont compilés avec compat/dlfcn.h : leur
# chargement échoue proprement.)
set(DUSK_3DS ${DUSK_FILES})
list(FILTER DUSK_3DS EXCLUDE REGEX "^src/dusk/ui/")
list(REMOVE_ITEM DUSK_3DS
    src/dusk/mods/manifest.cpp
    src/dusk/mods/svc/ui.cpp
    src/dusk/mods/svc/camera.cpp
    src/dusk/mods/svc/gfx.cpp
    src/dusk/imgui/ImGuiStateShare.cpp
)
list(FILTER DUSK_3DS INCLUDE REGEX "\\.(c|cpp)$")

set(FICHIERS_JEU
    ${DOLZEL_FILES}
    ${Z2AUDIOLIB_FILES}
    ${SSYSTEM_FILES}
    ${REL_FILES}
    ${DOLPHIN_FILES}
    ${DUSK_3DS}
    src/dusk/main.cpp
    src/dusk/audio/DuskAudioSystem.cpp
    src/dusk/audio/JASCriticalSection.cpp
    src/dusk/audio/DuskDsp.cpp
    src/dusk/audio/Adpcm.cpp
    src/dusk/audio/DspStub.cpp
    src/dusk/imgui/ImGuiAudio.cpp
)
list(REMOVE_DUPLICATES FICHIERS_JEU)
list(TRANSFORM FICHIERS_JEU PREPEND "${DUSK_DIR}/")
# Remplaçants de parties de Dusklight : compilés comme les fichiers du jeu
list(APPEND FICHIERS_JEU ${CMAKE_CURRENT_SOURCE_DIR}/plateforme/remplacants_dusk.cpp)
set_source_files_properties(${FICHIERS_JEU} PROPERTIES COMPILE_DEFINITIONS "NDEBUG=1;NDEBUG_DEFINED=1;DEBUG_DEFINED=0")

file(GLOB PLATEFORME plateforme/*.cpp)
list(REMOVE_ITEM PLATEFORME ${CMAKE_CURRENT_SOURCE_DIR}/plateforme/aurora_jeu.cpp
                            ${CMAKE_CURRENT_SOURCE_DIR}/plateforme/borealis3ds.cpp
                            ${CMAKE_CURRENT_SOURCE_DIR}/plateforme/remplacants_dusk.cpp)

# Les bibliothèques JSystem dépendent les unes des autres : ld doit les parcourir en groupe
set(CMAKE_LINK_GROUP_USING_RESCAN "LINKER:--start-group" "LINKER:--end-group")
set(CMAKE_LINK_GROUP_USING_RESCAN_SUPPORTED TRUE)

add_executable(zelda3ds ${FICHIERS_JEU} ${PLATEFORME})
target_compile_definitions(zelda3ds PRIVATE ${DEFS_JEU})
target_link_libraries(zelda3ds PRIVATE
    "$<LINK_GROUP:RESCAN,${JSYSTEM_LIBRARIES}>"
    aurora3ds_systeme borealis3ds freeverb3ds jeu_commun)
target_precompile_headers(zelda3ds PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${DUSK_DIR}/include/dusk_pch.hpp>")
# Les fichiers de la plateforme 3DS n'utilisent pas l'en-tête précompilé du jeu
set_source_files_properties(${PLATEFORME} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
# Le code inutilisé (et ses dépendances) est retiré à l'édition de liens (voir aussi
# -ffunction-sections dans le CMakeLists.txt principal)
target_link_options(zelda3ds PRIVATE -Wl,--gc-sections -Wl,-Map,zelda3ds.map)

ctr_generate_smdh(zelda3ds.smdh
    NAME "Crepuscule"
    DESCRIPTION "Twilight Princess (Dusklight) pour New 3DS"
    AUTHOR "emulateur-ia"
    ICON ${CMAKE_CURRENT_SOURCE_DIR}/../cia/icone.png
)
ctr_create_3dsx(zelda3ds SMDH zelda3ds.smdh)
