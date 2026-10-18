#pragma once

#include "Renderer/System/Core/IRenderable.h"
#include <vector>
#include <memory>
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include <array>
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "Renderer/Data/Material.h"
#include "Core/Math/Math.h"
#include "Core/Math/Transform.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "Renderer/Camera/Camera.h"
#include "Core/System/ResourceHandle.h"

class Camera;
class TextureManager;
class DrawManager;
class DescriptorPool;

class BaseBatch : public IRenderable {
public:
    BaseBatch();
    virtual ~BaseBatch();

    /**
     * @brief バッチ描画システム全体で共有する DirectXCommon インスタンスを設定する
     * @param[in] dx 設定する DirectXCommon のポインタ
     */
    static void SetDirectXCommon(DirectXCommon* dx) {
        dx_ = dx;
    }

    /**
     * @brief バッチ描画で使用するテクスチャマネージャーを設定する
     * @param[in] tm 設定する TextureManager のポインタ
     */
    static void SetTextureManager(TextureManager* tm) {
        textureManager_ = tm;
    }

    /**
     * @brief バッチ描画の描画マネージャーを設定する
     * @param[in] dm 設定する DrawManager のポインタ
     */
    static void SetDrawManager(DrawManager* dm) {
        drawManager_ = dm;
    }

    /**
     * @brief バッチ描画用の SRV ディスクリプタプールを設定する
     * @param[in] alloc 設定する DescriptorPool のポインタ
     */
    static void SetSrvAllocator(DescriptorPool* alloc) {
        srvPool_ = alloc;
    }

    /**
     * @brief オブジェクトカリングの有効/無効を設定する
     * @param[in] enabled カリングを有効にする場合は true
     */
    void SetCullingEnabled(bool enabled) {
        isCullingEnabled_ = enabled;
    }
    /**
     * @brief IsCullingEnabled かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsCullingEnabled() const {
        return isCullingEnabled_;
    }

    /**
     * @brief UseGPUCulling を設定する。
     * @param[in] use 設定する UseGPUCulling の値
     */
    void SetUseGPUCulling(bool use) {
        useGPUCulling_ = use;
    }
    /**
     * @brief UseGPUCulling を取得する。
     * @return 取得された UseGPUCulling
     */
    bool GetUseGPUCulling() const {
        return useGPUCulling_;
    }

    /**
     * @brief BlendMode を設定する。
     * @param[in] blendMode 設定する BlendMode の値
     */
    void SetBlendMode(Irufemi::BlendMode blendMode) {
        blendMode_ = blendMode;
    }
    /**
     * @brief BlendMode を取得する。
     * @return 取得された BlendMode
     */
    Irufemi::BlendMode GetBlendMode() const {
        return blendMode_;
    }

    /**
     * @brief DepthWrite を設定する。
     * @param[in] depthWrite 設定する DepthWrite の値
     */
    void SetDepthWrite(PSOManager::DepthWrite depthWrite) {
        depthWrite_ = depthWrite;
    }
    /**
     * @brief DepthWrite を取得する。
     * @return 取得された DepthWrite
     */
    PSOManager::DepthWrite GetDepthWrite() const {
        return depthWrite_;
    }

    /**
     * @brief CullMode を設定する。
     * @param[in] cullMode 設定する CullMode の値
     */
    void SetCullMode(PSOManager::CullMode cullMode) {
        cullMode_ = cullMode;
    }
    /**
     * @brief CullMode を取得する。
     * @return 取得された CullMode
     */
    PSOManager::CullMode GetCullMode() const {
        return cullMode_;
    }

    /**
     * @brief CastShadows を設定する。
     * @param[in] cast 設定する CastShadows の値
     */
    void SetCastShadows(bool cast) {
        castShadows_ = cast;
    }
    /**
     * @brief CastShadows を取得する。
     * @return 取得された CastShadows
     */
    bool GetCastShadows() const {
        return castShadows_;
    }

    /**
     * @brief CustomPSO を設定する（生ポインタ指定・下位互換用）。
     * @param[in] pso 設定する CustomPSO の値
     */
    void SetCustomPSO(ID3D12PipelineState* pso) {
        customPSOName_.clear();
        customPSO_ = pso;
    }
    /**
     * @brief CustomPSO を設定する（名前指定・推奨）。
     * @param[in] psoName 設定する PSO 名
     */
    void SetCustomPSO(const std::string& psoName) {
        customPSOName_ = psoName;
    }
    /**
     * @brief CustomPSO を詳細設定付きで設定する（名前指定・推奨）。
     * @param[in] psoName 設定する PSO 名
     * @param[in] blend ブレンドモード
     * @param[in] depth 深度書き込み設定
     * @param[in] cull カリングモード
     */
    void SetCustomPSO(const std::string& psoName, Irufemi::BlendMode blend, PSOManager::DepthWrite depth,
                      PSOManager::CullMode cull) {
        customPSOName_ = psoName;
        blendMode_ = blend;
        depthWrite_ = depth;
        cullMode_ = cull;
    }
    /**
     * @brief CustomPSO を取得する。名前指定がある場合は PSOManager から動的解決する。
     * @return 取得された CustomPSO
     */
    ID3D12PipelineState* GetCustomPSO() const {
        if (!customPSOName_.empty() && dx_) {
            if (auto* pm = dx_->GetPSOManager()) {
                return pm->GetPSO(customPSOName_, blendMode_, depthWrite_, cullMode_);
            }
        }
        return customPSO_;
    }

    /**
     * @brief CustomCBVAddress を設定する。
     * @param[in] addr 設定する CustomCBVAddress の値
     */
    void SetCustomCBVAddress(D3D12_GPU_VIRTUAL_ADDRESS addr) {
        customCBVAddress_ = addr;
    }
    /**
     * @brief CustomCBVAddress を取得する。
     * @return 取得された CustomCBVAddress
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetCustomCBVAddress() const {
        return customCBVAddress_;
    }

    // Color controls
    /**
     * @brief Color を設定する。
     * @param[in] color 設定する Color の値
     */
    void SetColor(const Irufemi::Vector4& color);
    /**
     * @brief EnvironmentCoefficient を設定する。
     * @param[in] coefficient 設定する EnvironmentCoefficient の値
     */
    void SetEnvironmentCoefficient(float coefficient);
    /**
     * @brief InstanceColor を設定する。
     * @param[in] index 設定する InstanceColor の値
     * @param[in] color 設定する InstanceColor の値
     */
    void SetInstanceColor(uint32_t index, const Irufemi::Vector4& color);
    /**
     * @brief AllInstanceColor を設定する。
     * @param[in] color 設定する AllInstanceColor の値
     */
    void SetAllInstanceColor(const Irufemi::Vector4& color);

    // Instances
    /**
     * @brief AddInstance を実行する。
     */
    void AddInstance(const Irufemi::Transform& t, int32_t effectType = 0, float effectParam = 0.0f,
                     bool enableMask = false);
    /**
     * @brief AddInstance を実行する。
     */
    void AddInstance(const Irufemi::Transform& t, const Irufemi::Vector4& color, int32_t effectType = 0,
                     float effectParam = 0.0f, bool enableMask = false);
    /**
     * @brief AddInstance を実行する。
     */
    void AddInstance(const Irufemi::Vector3& center, float scale = 1.0f, const Irufemi::Vector3& rotate = {0, 0, 0},
                     int32_t effectType = 0, float effectParam = 0.0f, bool enableMask = false);
    /**
     * @brief AddInstance を実行する。
     */
    void AddInstance(const Irufemi::Vector3& center, float scale, const Irufemi::Vector3& rotate,
                     const Irufemi::Vector4& color, int32_t effectType = 0, float effectParam = 0.0f,
                     bool enableMask = false);

    // World matrix based instances
    /**
     * @brief AddInstanceWorld を実行する。
     */
    void AddInstanceWorld(const Irufemi::Matrix4x4& world, const Irufemi::Vector4& color = {1, 1, 1, 1},
                          int32_t effectType = 0, float effectParam = 0.0f, bool enableMask = false);

    /**
     * @brief UpdateInstance を実行する。
     */
    void UpdateInstance(uint32_t index, const Irufemi::Transform& t);
    /**
     * @brief ClearInstances を実行する。
     */
    void ClearInstances();

    /**
     * @brief BuildInstanceBuffer を実行する。
     */
    virtual void BuildInstanceBuffer(bool force = false);
    /**
     * @brief SyncBeforeDraw を実行する。
     */
    void SyncBeforeDraw() override;

    /**
     * @brief MaterialVAddress を取得する。
     * @return 取得された MaterialVAddress
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetMaterialVAddress() const;
    /**
     * @brief InstancingSrvHandleGPU を取得する。
     * @return 取得された InstancingSrvHandleGPU
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetInstancingSrvHandleGPU() const {
        return instancingSrvGPU_[lastUpdateFrameIndex_];
    }
    /**
     * @brief InstanceCount を取得する。
     * @return 取得された InstanceCount
     */
    UINT GetInstanceCount() const {
        return visibleInstanceCount_;
    }

    // --- GPU Culling Getters ---
    /**
     * @brief OutputInstancesSrvHandleGPU を取得する。
     * @return 取得された OutputInstancesSrvHandleGPU
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetOutputInstancesSrvHandleGPU() const {
        return outputInstanceSrvGPU_[lastUpdateFrameIndex_];
    }
    /**
     * @brief OutputInstancesUavHandleGPU を取得する。
     * @return 取得された OutputInstancesUavHandleGPU
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetOutputInstancesUavHandleGPU() const {
        return outputInstanceUavGPU_[lastUpdateFrameIndex_];
    }
    /**
     * @brief OutputInstanceBuffer を取得する。
     * @return 取得された OutputInstanceBuffer
     */
    ID3D12Resource* GetOutputInstanceBuffer() const {
        return outputInstanceBuffer_[lastUpdateFrameIndex_].Get();
    }
    /**
     * @brief IndirectCommandBuffer を取得する。
     * @return 取得された IndirectCommandBuffer
     */
    ID3D12Resource* GetIndirectCommandBuffer() const {
        return indirectCommandBuffer_[lastUpdateFrameIndex_].Get();
    }
    /**
     * @brief IndirectCommandUploadBuffer を取得する。
     * @return 取得された IndirectCommandUploadBuffer
     */
    ID3D12Resource* GetIndirectCommandUploadBuffer() const {
        return indirectCommandUploadBuffer_[lastUpdateFrameIndex_].Get();
    }
    /**
     * @brief IndirectCommandUavHandleGPU を取得する。
     * @return 取得された IndirectCommandUavHandleGPU
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetIndirectCommandUavHandleGPU() const {
        return indirectCommandUavGPU_[lastUpdateFrameIndex_];
    }
    /**
     * @brief CullingDataAddress を取得する。
     * @return 取得された CullingDataAddress
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetCullingDataAddress() const {
        return cullingDataBuffer_[lastUpdateFrameIndex_]
                   ? cullingDataBuffer_[lastUpdateFrameIndex_]->GetGPUVirtualAddress()
                   : 0;
    }
    /**
     * @brief MaxInstanceCount を取得する。
     * @return 取得された MaxInstanceCount
     */
    UINT GetMaxInstanceCount() const {
        return static_cast<UINT>(instances_.size() + instanceWorlds_.size());
    }

protected:
    struct InstanceData {
        Irufemi::Matrix4x4 WVP;
        Irufemi::Matrix4x4 World;
        Irufemi::Matrix4x4 WorldInverseTranspose;
        Irufemi::Vector4 color;
        Irufemi::Vector4 customEffect; // x: type, y: param, z: enableMask, w: padding
    };

    struct TransformData {
        Irufemi::Vector4 position; // w is padding
        Irufemi::Vector4 rotation; // w is padding
        Irufemi::Vector4 scale;    // w is padding
        Irufemi::Vector4 color;
        Irufemi::Vector4 customEffect; // x: type, y: param, z: enableMask, w: padding
    };

    struct CullingData {
        Irufemi::Vector4 planes[6];
        uint32_t maxInstanceCount;
        float localRadius;
        float time;
        float padding;
    };

    /**
     * @brief CreateOrResizeInstanceBuffer を実行する。
     */
    void CreateOrResizeInstanceBuffer(uint32_t instanceCount);
    /**
     * @brief CreateGPUCullingBuffers を実行する。
     */
    void CreateGPUCullingBuffers(uint32_t instanceCount);

    /**
     * @brief BoundingSphereRadius を取得する。
     * @return 取得された BoundingSphereRadius
     */
    virtual float GetBoundingSphereRadius() const = 0; // for culling

protected:
    static DirectXCommon* dx_;
    static TextureManager* textureManager_;
    static DrawManager* drawManager_;
    static DescriptorPool* srvPool_;

    uint32_t materialCbIndex_ = static_cast<uint32_t>(-1);
    Material cpuMaterialData_{};
    bool hasCustomColor_ = false;
    ResourceHandle textureHandle_{};

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> instanceBuffer_{};
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kMaxFramesInFlight> instancingSrvCPU_{};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, kMaxFramesInFlight> instancingSrvGPU_{};
    std::array<uint32_t, kMaxFramesInFlight> instancingSrvIndex_{};

    std::vector<Irufemi::Transform> instances_;
    std::vector<Irufemi::Vector4> instanceColors_;
    std::vector<Irufemi::Vector4> instanceEffects_;
    std::vector<Irufemi::Matrix4x4> instanceWorlds_;
    std::vector<Irufemi::Vector4> instanceWorldColors_;
    std::vector<Irufemi::Vector4> instanceWorldEffects_;

    bool instanceDirty_ = false;
    bool isCullingEnabled_ = true;
    bool useGPUCulling_ = false;
    uint32_t visibleInstanceCount_ = 0;

    // --- GPU Culling Resources ---
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> outputInstanceBuffer_{};
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kMaxFramesInFlight> outputInstanceSrvCPU_{};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, kMaxFramesInFlight> outputInstanceSrvGPU_{};
    std::array<uint32_t, kMaxFramesInFlight> outputInstanceSrvIndex_{};

    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kMaxFramesInFlight> outputInstanceUavCPU_{};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, kMaxFramesInFlight> outputInstanceUavGPU_{};
    std::array<uint32_t, kMaxFramesInFlight> outputInstanceUavIndex_{};

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> indirectCommandBuffer_{};
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> indirectCommandUploadBuffer_{};

    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kMaxFramesInFlight> indirectCommandUavCPU_{};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, kMaxFramesInFlight> indirectCommandUavGPU_{};
    std::array<uint32_t, kMaxFramesInFlight> indirectCommandUavIndex_{};

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> cullingDataBuffer_{};

    uint32_t lastUpdateFrameIndex_ = 0;
    bool isDirty_ = true;

    Irufemi::BlendMode blendMode_ = Irufemi::BlendMode::kBlendModeNormal;
    PSOManager::DepthWrite depthWrite_ = PSOManager::DepthWrite::Enable;
    PSOManager::CullMode cullMode_ = PSOManager::CullMode::Back;
    bool castShadows_ = true;
    ID3D12PipelineState* customPSO_ = nullptr;
    std::string customPSOName_ = "";
    D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress_ = 0;
};
