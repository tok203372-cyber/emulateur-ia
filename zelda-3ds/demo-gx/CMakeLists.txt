# Démonstration du moteur GX -> PICA200
add_executable(demo-gx source/main.cpp)
target_link_libraries(demo-gx PRIVATE moteur3ds)
ctr_generate_smdh(demo-gx.smdh
    NAME "Crepuscule - moteur"
    DESCRIPTION "Demonstration du moteur GX vers PICA200"
    AUTHOR "emulateur-ia"
    ICON ${CMAKE_CURRENT_SOURCE_DIR}/../cia/icone.png
)
ctr_create_3dsx(demo-gx SMDH demo-gx.smdh)
