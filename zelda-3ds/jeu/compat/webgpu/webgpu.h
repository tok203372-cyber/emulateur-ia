/*
 * Version 3DS minimale de webgpu.h : seulement les déclarations utilisées par l'interface des
 * modifications (mods/svc/gfx.h), incluse par le rendu du jeu. Il n'y a pas de WebGPU sur la 3DS
 * et les modifications (mods) ne sont pas prises en charge : ces types ne servent jamais.
 */
#ifndef WEBGPU_H_
#define WEBGPU_H_

#include <stddef.h>
#include <stdint.h>

typedef struct WGPUAdapterImpl* WGPUAdapter;
typedef struct WGPUBufferImpl* WGPUBuffer;
typedef struct WGPUCommandEncoderImpl* WGPUCommandEncoder;
typedef struct WGPUDeviceImpl* WGPUDevice;
typedef struct WGPUInstanceImpl* WGPUInstance;
typedef struct WGPUQueueImpl* WGPUQueue;
typedef struct WGPURenderPassEncoderImpl* WGPURenderPassEncoder;
typedef struct WGPUSurfaceImpl* WGPUSurface;
typedef struct WGPUTextureImpl* WGPUTexture;
typedef struct WGPUTextureViewImpl* WGPUTextureView;

typedef enum WGPUTextureFormat {
  WGPUTextureFormat_Undefined = 0,
  WGPUTextureFormat_Force32 = 0x7FFFFFFF
} WGPUTextureFormat;

typedef enum WGPUCompositeAlphaMode {
  WGPUCompositeAlphaMode_Auto = 0,
  WGPUCompositeAlphaMode_Force32 = 0x7FFFFFFF
} WGPUCompositeAlphaMode;

typedef uint64_t WGPUFlags;
typedef WGPUFlags WGPUColorWriteMask;
static const WGPUColorWriteMask WGPUColorWriteMask_None = 0;
static const WGPUColorWriteMask WGPUColorWriteMask_All = 0xF;
typedef WGPUFlags WGPUTextureUsage;
static const WGPUTextureUsage WGPUTextureUsage_None = 0;

typedef struct WGPUChainedStruct {
  struct WGPUChainedStruct const* next;
  uint32_t sType;
} WGPUChainedStruct;

typedef struct WGPUBlendComponent {
  uint32_t operation;
  uint32_t srcFactor;
  uint32_t dstFactor;
} WGPUBlendComponent;

typedef struct WGPUBlendState {
  WGPUBlendComponent color;
  WGPUBlendComponent alpha;
} WGPUBlendState;

typedef struct WGPUColorTargetState {
  WGPUChainedStruct const* nextInChain;
  WGPUTextureFormat format;
  WGPUBlendState const* blend;
  WGPUColorWriteMask writeMask;
} WGPUColorTargetState;

#define WGPU_COLOR_TARGET_STATE_INIT {NULL, WGPUTextureFormat_Undefined, NULL, WGPUColorWriteMask_All}

#endif /* WEBGPU_H_ */
