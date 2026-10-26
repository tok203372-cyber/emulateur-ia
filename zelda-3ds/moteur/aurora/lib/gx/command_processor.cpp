// Version 3DS du processeur de commandes d'Aurora : décode le flux GX (registres BP/CP/XF,
// commandes Aurora) et transmet les dessins au moteur gx3ds (PICA200).
#include "command_processor.hpp"

#include "../internal.hpp"
#include "dolphin/gd/GDGeometry.h"
#include "dolphin/gx/GXAurora.h"
#include "gx.hpp"
#include "regs.hpp"

#include <gx3ds/gx3ds.hpp>

#include <algorithm>
#include <cstdint>
#include <span>

namespace aurora::gx::fifo {
namespace {
constexpr Module Log{"aurora::gx::fifo"};

constexpr u8 CP_CMD_NOP = GX_NOP;
constexpr u8 CP_CMD_LOAD_CP_REG = GX_LOAD_CP_REG;
constexpr u8 CP_CMD_LOAD_XF_REG = GX_LOAD_XF_REG;
constexpr u8 CP_CMD_LOAD_INDX_A = GX_LOAD_INDX_A;
constexpr u8 CP_CMD_LOAD_INDX_B = GX_LOAD_INDX_B;
constexpr u8 CP_CMD_LOAD_INDX_C = GX_LOAD_INDX_C;
constexpr u8 CP_CMD_LOAD_INDX_D = GX_LOAD_INDX_D;
constexpr u8 CP_CMD_CALL_DL = GX_CMD_CALL_DL;
constexpr u8 CP_CMD_INVAL_VTX = GX_CMD_INVL_VC;
constexpr u8 CP_CMD_LOAD_BP_REG = GX_LOAD_BP_REG & GX_OPCODE_MASK;
constexpr u8 CP_OPCODE_MASK = GX_OPCODE_MASK;
constexpr u8 CP_VAT_MASK = GX_VAT_MASK;

u32 calc_vtx_size(GXVtxFmt fmt) noexcept {
  u32 vtxSize = 0;
  const auto& vtxFmt = g_gxState.vtxFmts[fmt];
  for (int i = GX_VA_PNMTXIDX; i <= GX_VA_TEX7; ++i) {
    const auto& attrFmt = vtxFmt.attrs[i];
    switch (g_gxState.vtxDesc[i]) {
    case GX_NONE:
      break;
    case GX_DIRECT: {
      const auto attr = static_cast<GXAttr>(i);
      vtxSize += comp_type_size(attr, attrFmt.type) * comp_cnt_count(attr, attrFmt.cnt);
      break;
    }
    case GX_INDEX8:
      vtxSize += i == GX_VA_NRM && attrFmt.cnt == GX_NRM_NBT3 ? 3 : 1;
      break;
    case GX_INDEX16:
      vtxSize += i == GX_VA_NRM && attrFmt.cnt == GX_NRM_NBT3 ? 6 : 2;
      break;
    }
  }
  g_gxState.lastVtxFmt = fmt;
  g_gxState.lastVtxSize = static_cast<u16>(vtxSize);
  return vtxSize;
}

u32 vtx_size(GXVtxFmt fmt) noexcept {
  if (g_gxState.lastVtxFmt == fmt) return g_gxState.lastVtxSize;
  return calc_vtx_size(fmt);
}

void draw_prim(GXPrimitive prim, GXVtxFmt fmt, u16 vtxCount, ByteReader& reader) noexcept {
  const u32 vtxSize = vtx_size(fmt);
  const u32 total = vtxCount * vtxSize;
  if (total > reader.remaining()) {
    Log.error("dessin : données de sommets incomplètes ({} octets, {} restants)", total, reader.remaining());
    reader.skip(reader.remaining());
    return;
  }
  const auto data = reader.take(total);
  gx3ds::dessiner(prim, fmt, vtxCount, data.data());
}

void handle_draw(u8 cmd, ByteReader& reader) noexcept {
  const auto fmt = static_cast<GXVtxFmt>(cmd & CP_VAT_MASK);
  const auto prim = static_cast<GXPrimitive>(cmd & CP_OPCODE_MASK);
  draw_prim(prim, fmt, reader.read<u16>(), reader);
}

void handle_aurora(ByteReader& reader) noexcept {
  const u16 subCmd = reader.read<u16>();

  if (subCmd == GX_AURORA_LOAD_VIEWPORT_RENDER) {
    const f32 left = reader.read<f32>();
    const f32 top = reader.read<f32>();
    const f32 width = reader.read<f32>();
    const f32 height = reader.read<f32>();
    const f32 nearZ = reader.read<f32>();
    const f32 farZ = reader.read<f32>();
    set_render_viewport({left, top, width, height, nearZ, farZ});
  } else if (subCmd == GX_AURORA_LOAD_SCISSOR_RENDER) {
    const s32 left = reader.read<s32>();
    const s32 top = reader.read<s32>();
    const s32 width = reader.read<s32>();
    const s32 height = reader.read<s32>();
    set_render_scissor({left, top, width, height});
  } else if (subCmd == GX_AURORA_LOAD_PROJECTION_FULL) {
    auto& proj = g_gxState.proj;
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        proj[r][c] = reader.read<f32>();
      }
    }
    for (u32 reg = 0x20; reg <= 0x26; ++reg) {
      g_gxState.xfRegValid.reset(reg);
    }
    g_gxState.dirty |= DirtyUniform;
  } else if (subCmd >= GX_AURORA_LOAD_ARRAYBASE && subCmd <= (GX_AURORA_LOAD_ARRAYBASE | 0x0f)) {
    const u32 attrIdx = subCmd - GX_AURORA_LOAD_ARRAYBASE + GX_VA_POS;
    const u64 arrayAddr = reader.read<u64>();
    const u32 arraySize = reader.read<u32>();
    const bool le = reader.read<u8>() == 1;
    if (attrIdx < MaxVtxAttr) {
      auto& array = g_gxState.arrays[attrIdx];
      array.data = reinterpret_cast<const void*>(static_cast<uintptr_t>(arrayAddr));
      array.size = arraySize;
      array.le = le;
      g_gxState.dirty |= DirtyImmediates;
    }
  } else if (subCmd == GX_AURORA_LOAD_TEXOBJ) {
    const auto texMapId = reader.read<u8>();
    const auto newData = reinterpret_cast<const void*>(static_cast<uintptr_t>(reader.read<u64>()));
    const u32 newWidth = reader.read<u32>();
    const u32 newHeight = reader.read<u32>();
    const auto newFormat = static_cast<GXTexFmt>(reader.read<u32>());
    const auto newTlut = static_cast<GXTlut>(reader.read<u32>());
    const bool mips = reader.read<u8>() != 0;
    const u32 newTexObjId = reader.read<u32>();
    const u32 newTexDataVersion = reader.read<u32>();
    if (texMapId < MaxTextures) {
      auto& slot = g_gxState.loadedTextures[texMapId];
      slot.data = newData;
      slot.mWidth = newWidth;
      slot.mHeight = newHeight;
      slot.mFormat = newFormat;
      slot.tlut = newTlut;
      slot.flags = static_cast<u8>((slot.flags & ~0x81u) | (mips ? 1u : 0u));
      slot.texObjId = newTexObjId;
      slot.texDataVersion = newTexDataVersion;
      g_gxState.dirty |= DirtyTextures;
    }
  } else if (subCmd == GX_AURORA_LOAD_TLUT) {
    const auto idx = reader.read<u8>();
    const auto newData = reinterpret_cast<const void*>(static_cast<uintptr_t>(reader.read<u64>()));
    const auto newFormat = static_cast<GXTlutFmt>(reader.read<u32>());
    const u16 newNumEntries = reader.read<u16>();
    const u32 newTlutObjId = reader.read<u32>();
    const u32 newTlutDataVersion = reader.read<u32>();
    if (idx < MaxTluts) {
      auto& slot = g_gxState.loadedTluts[idx];
      slot.data = newData;
      slot.format = newFormat;
      slot.numEntries = newNumEntries;
      slot.tlutObjId = newTlutObjId;
      slot.tlutDataVersion = newTlutDataVersion;
      g_gxState.dirty |= DirtyTextures;
    }
  } else if (subCmd == GX2_SET_POLYGON_OFFSET) {
    g_gxState.frontOffset = reader.read<f32>();
    g_gxState.frontScale = reader.read<f32>();
    g_gxState.backOffset = reader.read<f32>();
    g_gxState.backScale = reader.read<f32>();
    g_gxState.clamp = reader.read<f32>();
  } else if (subCmd == GX_AURORA_LOAD_COPY_SRC) {
    const s32 left = reader.read<s32>();
    const s32 top = reader.read<s32>();
    const s32 width = reader.read<s32>();
    const s32 height = reader.read<s32>();
    g_gxState.texCopySrc = {left, top, width, height};
  } else if (subCmd == GX_AURORA_LOAD_COPY_DST) {
    g_gxState.texCopyDstWidth = reader.read<u32>();
    g_gxState.texCopyDstHeight = reader.read<u32>();
    g_gxState.texCopyFmt = static_cast<GXTexFmt>(reader.read<u32>());
    reader.skip(1);
    g_gxState.texCopyDstWide = true;
  } else if (subCmd == GX_AURORA_LOAD_COPY_DEST) {
    g_gxState.texCopyDest = reinterpret_cast<const void*>(static_cast<uintptr_t>(reader.read<u64>()));
  } else if (subCmd == GX_AURORA_REQUEST_DEPTH_SNAPSHOT) {
    // non géré
  } else if (subCmd == GX_AURORA_BEGIN_OFFSCREEN) {
    reader.skip(8);  // largeur, hauteur : rendu hors écran non géré
  } else if (subCmd == GX_AURORA_END_OFFSCREEN) {
  } else if (subCmd == GX_AURORA_DESTROY_TEXOBJ) {
    evict_texture_object(reader.read<u32>());
  } else if (subCmd == GX_AURORA_DESTROY_TLUT) {
    evict_tlut_object(reader.read<u32>());
  } else if (subCmd == GX_AURORA_DESTROY_COPY_TEX) {
    evict_copy_texture(reinterpret_cast<const void*>(static_cast<uintptr_t>(reader.read<u64>())));
  } else if (subCmd == GX_AURORA_DRAW_SIZED) {
    const u8 cmd = reader.read<u8>();
    const u32 byteLen = reader.read<u32>();
    const GXVtxFmt fmt = static_cast<GXVtxFmt>(cmd & CP_VAT_MASK);
    const GXPrimitive prim = static_cast<GXPrimitive>(cmd & CP_OPCODE_MASK);
    if (byteLen != 0) {
      const u32 vtxSize = vtx_size(fmt);
      if (vtxSize == 0 || byteLen % vtxSize != 0) {
        Log.error("GX_AURORA_DRAW_SIZED : {} octets pour des sommets de {} octets", byteLen, vtxSize);
        reader.skip(byteLen);
      } else {
        draw_prim(prim, fmt, static_cast<u16>(std::min<u32>(byteLen / vtxSize, 0xFFFF)), reader);
      }
    }
  } else if (subCmd == GX_AURORA_DRAW_INDEXED) {
    const u8 cmd = reader.read<u8>();
    const u16 vtxCount = reader.read<u16>();
    const u32 indexCount = reader.read<u32>();
    const GXVtxFmt fmt = static_cast<GXVtxFmt>(cmd & CP_VAT_MASK);
    const auto indexData = reader.take(static_cast<size_t>(indexCount) * sizeof(u16));
    const u32 vtxSize = vtx_size(fmt);
    const auto vertexData = reader.take(static_cast<size_t>(vtxCount) * vtxSize);
    if (indexCount != 0) {
      gx3ds::dessiner_indexe(fmt, vtxCount, vertexData.data(), reinterpret_cast<const u16*>(indexData.data()),
                             indexCount);
    }
  } else if (subCmd == GX_AURORA_DEBUG_GROUP_PUSH || subCmd == GX_AURORA_DEBUG_MARKER_INSERT) {
    const u16 len = reader.read<u16>();
    reader.skip(len);
  } else if (subCmd == GX_AURORA_DEBUG_GROUP_POP) {
  } else {
    Log.error("Sous-commande Aurora inconnue : {:04X}", subCmd);
  }
}
} // namespace

ProcessResult process(const u8* data, u32 size) noexcept {
  ByteReader reader{{data, size}};

  while (!reader.empty()) {
    const u8 cmd = reader.read<u8>();
    const u8 opcode = cmd & CP_OPCODE_MASK;

    switch (opcode) {
    case CP_CMD_NOP:
      continue;

    case CP_CMD_LOAD_BP_REG: {
      const u32 value = reader.read<u32>();
      handle_bp(value);
      if (reg_get(value, 8, 24) == GX_BP_REG_DRAWDONE) {
        return {static_cast<u32>(reader.offset()), true};
      }
      break;
    }

    case CP_CMD_LOAD_CP_REG: {
      const u8 addr = reader.read<u8>();
      handle_cp(addr, reader.read<u32>());
      break;
    }

    case CP_CMD_LOAD_XF_REG: {
      const u32 header = reader.read<u32>();
      const u32 count = ((header >> 16) & 0xFFFF) + 1;
      const u16 addr = header & 0xFFFF;
      handle_xf(addr, reader.take(count * sizeof(u32)));
      break;
    }

    case CP_CMD_LOAD_INDX_A:
    case CP_CMD_LOAD_INDX_B:
    case CP_CMD_LOAD_INDX_C:
    case CP_CMD_LOAD_INDX_D: {
      const u32 arrayType = GX_POS_MTX_ARRAY + (opcode - CP_CMD_LOAD_INDX_A) / 0x08;
      const u16 srcArrayIdx = reader.read<u16>();
      const u16 addrLen = reader.read<u16>();
      const u16 len = (addrLen >> 12) + 1;
      const u16 dstAddr = addrLen & 0x0FFF;
      const auto& array = g_gxState.arrays[arrayType];
      const u32 srcOffset = static_cast<u32>(srcArrayIdx) * array.stride;
      const u32 srcSize = static_cast<u32>(len) * sizeof(u32);
      if (array.data != nullptr && (array.size == 0 || srcOffset + srcSize <= array.size)) {
        const auto* srcData = static_cast<const u8*>(array.data) + srcOffset;
        copy_xf_data(dstAddr, srcData, len, array.le ? std::endian::little : std::endian::big);
      }
      break;
    }

    case CP_CMD_CALL_DL:
      reader.skip(8);
      break;

    case CP_CMD_INVAL_VTX:
      g_gxState.dirty |= DirtyImmediates;
      break;

    case GX_AURORA:
      handle_aurora(reader);
      break;

    default:
      if (cmd >= 0x80) {
        handle_draw(cmd, reader);
      } else {
        Log.error("processeur de commandes : code inconnu 0x{:02X} à la position {}", cmd, reader.offset() - 1);
        return {size, false};
      }
      break;
    }
  }
  return {size, false};
}

void clear_draw_cache() noexcept {}

} // namespace aurora::gx::fifo
