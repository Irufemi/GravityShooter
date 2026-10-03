#pragma once

#include <d3d12.h>
#include <cstdint>
#include <functional>
#include "Core/Type/BlendMode.h"
#include "Renderer/Pipeline/PSOManager.h"

/**
 * @file RenderPackets.h
 * @brief 描画命令（パケット）の構造体定義群
 * @details 各パケットが保持するリソース生ポインタ（Object3DResource*, ID3D12Resource* 等）は、
 *          当該フレームの RenderGraph 実行中のみ有効な一時参照です。
 *          フレームを跨いでキャッシュまたは保持することは厳禁です。
 */

// 前方宣言
class Object3DResource;
class Object2DResource;
class LineResource;
struct GpuMesh;

/**
 * @namespace RenderPackets
 * @brief レンダリングパイプラインにおいて描画パスへ投入されるコマンドパケット構造体群の名前空間
 */
namespace RenderPackets {

struct Standard3DPacket {
    const class Object3DResource* resource;
    const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride;
    Irufemi::BlendMode blendMode;
    PSOManager::DepthWrite depthWrite;
    PSOManager::CullMode cullMode;
    bool castShadows;
    ID3D12PipelineState* customPSO = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
    ID3D12Resource* vertexBufferResourceOverride = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS overrideMaterialCBV = 0;
    float distanceToCamera = 0.0f; // 半透明描画のZソート用
};

struct SpritePacket {
    const class Object2DResource* resource;
    Irufemi::BlendMode blendMode;
    PSOManager::DepthWrite depthWrite;
    PSOManager::CullMode cullMode;
    ID3D12PipelineState* customPSO = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
};

struct LinePacket {
    const class LineResource* resource;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;
    UINT instanceCount;
    Irufemi::BlendMode blendMode;
    PSOManager::DepthWrite depthWrite;
    PSOManager::CullMode cullMode;
    ID3D12PipelineState* customPSO = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
};

struct GPUParticlePacket {
    D3D12_VERTEX_BUFFER_VIEW vbv;
    D3D12_INDEX_BUFFER_VIEW ibv;
    uint32_t indexCount;
    D3D12_GPU_VIRTUAL_ADDRESS materialAddress;
    D3D12_GPU_VIRTUAL_ADDRESS perViewAddress;
    D3D12_GPU_VIRTUAL_ADDRESS emitterAddress;
    D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE sortListSrvHandle;
    uint32_t instanceCount;
    ID3D12Resource* particleResource;
    Irufemi::BlendMode blendMode;
    PSOManager::DepthWrite depthWrite;
    PSOManager::CullMode cullMode;
    ID3D12PipelineState* customPSO = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
};

struct VoxelParticlePacket {
    uint32_t instanceCount;
    D3D12_VERTEX_BUFFER_VIEW vbv;
    D3D12_INDEX_BUFFER_VIEW ibv;
    uint32_t indexCount;
    D3D12_GPU_VIRTUAL_ADDRESS systemCbAddress;
    D3D12_GPU_DESCRIPTOR_HANDLE emitterHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE particleDataHandle;
    ID3D12Resource* particleResource;
    ID3D12PipelineState* drawPSO;
};

struct SkyboxPacket {
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    D3D12_GPU_VIRTUAL_ADDRESS materialAddress;
    D3D12_GPU_VIRTUAL_ADDRESS transformationAddress;
    UINT indexCount;
};

struct SkydomePacket {
    D3D12_GPU_VIRTUAL_ADDRESS materialAddress = 0;
};

struct PrimitiveBatchPacket {
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    D3D12_GPU_VIRTUAL_ADDRESS materialAddress;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;
    UINT indexCount;
    UINT instanceCount;
    Irufemi::BlendMode blendMode;
    PSOManager::DepthWrite depthWrite;
    PSOManager::CullMode cullMode;
    bool castShadows;
    ID3D12PipelineState* customPSO = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
};

struct ModelBatchPacket {
    const struct GpuMesh* gpuMesh;
    D3D12_GPU_VIRTUAL_ADDRESS materialAddress;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;
    UINT instanceCount;
    Irufemi::BlendMode blendMode;
    PSOManager::DepthWrite depthWrite;
    PSOManager::CullMode cullMode;
    bool castShadows;
    ID3D12PipelineState* customPSO = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;

    // GPU Culling 拡張用
    bool useGPUCulling = false;
    ID3D12Resource* indirectCommandBuffer = nullptr;
    ID3D12Resource* indirectCommandUploadBuffer = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE indirectCommandUav{};
    D3D12_GPU_VIRTUAL_ADDRESS cullingDataAddress = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE inputInstancesSrv{};
    D3D12_GPU_DESCRIPTOR_HANDLE outputInstancesUav{};
    ID3D12Resource* outputInstancesBuffer = nullptr;
    UINT maxInstanceCount = 0;
};

struct Primitive2DBatchPacket {
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    D3D12_GPU_VIRTUAL_ADDRESS materialAddress;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;
    UINT indexCount;
    UINT instanceCount;
    Irufemi::BlendMode blendMode;
    PSOManager::DepthWrite depthWrite;
    PSOManager::CullMode cullMode;
    ID3D12PipelineState* customPSO = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
};

struct SpriteBatchPacket {
    const class Object2DResource* resource;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;
    UINT instanceCount;
    Irufemi::BlendMode blendMode;
    PSOManager::DepthWrite depthWrite;
    PSOManager::CullMode cullMode;
    ID3D12PipelineState* customPSO = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
};

struct DebugPrimitivePacket {
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    UINT indexCount;
    UINT instanceCount;
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU;
    Irufemi::BlendMode blendMode;
    PSOManager::DepthWrite depthWrite;
    PSOManager::CullMode cullMode;
    ID3D12PipelineState* customPSO = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress = 0;
};

} // namespace RenderPackets
