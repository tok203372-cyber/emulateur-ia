# Moteur graphique : API GX d'Aurora (fichiers d'origine + versions 3DS) et rendu gx3ds (citro3d)

# (arbre fusionné AURORA3DS préparé par le CMakeLists.txt principal)

file(GLOB AURORA_GX_SOURCES ${AURORA3DS}/lib/dolphin/gx/*.cpp)
file(GLOB AURORA_MTX_SOURCES ${AURORA3DS}/lib/dolphin/mtx/*.c)
set(AURORA_SOURCES
    ${AURORA_GX_SOURCES}
    ${AURORA_MTX_SOURCES}
    ${AURORA3DS}/lib/dolphin/vi/vi.cpp
    ${AURORA3DS}/lib/gx/fifo.cpp
    ${AURORA3DS}/lib/gx/regs.cpp
    ${AURORA3DS}/lib/gx/attr_fmt.cpp
    ${AURORA3DS}/lib/gx/command_processor.cpp
    ${AURORA3DS}/lib/gx/gx.cpp
    ${AURORA3DS}/lib/logging.cpp
    ${AURORA3DS}/lib/thread.cpp
    ${AURORA3DS}/lib/window3ds.cpp
    ${AURORA3DS}/lib/aurora3ds.cpp
)

ctr_add_shader_library(gx3ds_shader OUTPUT gx3ds.shbin gx3ds/gx3ds.v.pica)
dkp_add_embedded_binary_library(gx3ds_shbin gx3ds_shader)

add_library(moteur3ds STATIC
    ${AURORA_SOURCES}
    gx3ds/rendu.cpp
    gx3ds/tev.cpp
    gx3ds/sommets.cpp
    gx3ds/conversion.cpp
    gx3ds/textures.cpp
)
target_include_directories(moteur3ds PUBLIC
    ${AURORA3DS}/include
    ${AURORA3DS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${EXTERNE}/tracy/public
)
target_compile_definitions(moteur3ds PUBLIC AURORA_ENABLE_GX=1 NDEBUG=1)
target_link_libraries(moteur3ds PUBLIC fmt3ds citro3d gx3ds_shbin)
