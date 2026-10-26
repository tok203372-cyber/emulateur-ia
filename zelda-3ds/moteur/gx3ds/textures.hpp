#pragma once
// Cache des textures converties pour la PICA200 (mémoire linéaire).

#include "lib/gfx/texture.hpp"

#include <3ds.h>
#include <citro3d.h>

#include <cstdint>

namespace gx3ds::textures {

void initialiser() noexcept;
void arreter() noexcept;

// Texture PICA pour un objet texture GX (et sa palette) ; jamais nul (texture de secours)
C3D_Tex* obtenir(const GXTexObj_& obj, const GXTlutObj_* tlut, uint32_t image) noexcept;

// Texture de secours (blanche) et texture transparente (copies de l'image non gérées)
C3D_Tex* blanche() noexcept;
C3D_Tex* transparente() noexcept;

void fin_image(uint32_t image) noexcept;
void oublier_objet(uint32_t texObjId) noexcept;
void oublier_tlut(uint32_t tlutObjId) noexcept;
void vider() noexcept;

// Statistiques
uint32_t octets_utilises() noexcept;
uint32_t conversions_image() noexcept;

} // namespace gx3ds::textures
