#pragma once
#include "Renderer/System/Core/IRenderable.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector3Int.h"
#include "Core/Math/Vector4.h"
#include "Resource/Model/Data/VoxelizedModel.h"
#include "Core/Type/PerView.h"
#include "Renderer/Compute/IComputeTask.h"
#include <d3d12.h>
#include <memory>
#include <string>
#include <wrl.h>
#include <atomic>
#include <mutex>
#include <future>
#include <vector>
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/ConstantBuffer.h"

class IrufemiEngine;
class Camera;

// HLSL側のVoxelParticle構造体と一致させる
struct VoxelParticle {
    Irufemi::Vector3 position;
    float life;
    Irufemi::Vector3 velocity;
    float size;
    Irufemi::Vector4 color;
    Irufemi::Vector3 normal;
    uint32_t isActive;
    Irufemi::Vector3 rotation;
    float pad1;
    Irufemi::Vector3 angularVelocity;
    float pad2;
};

// HLSL側のVoxelEmitter構造体と一致させる（16バイトアライメント対応）
struct VoxelEmitter {
    Irufemi::Vector3 emitPosition = {0.0f, 0.0f, 0.0f};
    float time = 0.0f;

    float lifeTime = 0.8f;
    float gravity = 2.0f;
    uint32_t emit = 0;
    float dispersion = 8.0f;

    float convergence = 0.1f;
    Irufemi::Vector3 baseVelocity = {0.0f, 0.0f, 0.0f};

    Irufemi::Vector3 rotate = {0.0f, 0.0f, 0.0f};
    float pad1 = 0.0f;

    Irufemi::Vector3 scale = {1.0f, 1.0f, 1.0f};
    uint32_t particleType = 0;

    // 衝突判定用 (OBB近似)
    Irufemi::Vector3 collisionCenter;
    uint32_t useCollision = 0;
    Irufemi::Vector4 collisionOrientations[3];
    Irufemi::Vector3 collisionSize;
    float pad2 = 0.0f;

    // --- Material / Expression parameters (ハードコード排除用) ---
    Irufemi::Vector4 startColor = {20.0f, 15.0f, 5.0f, 1.0f};      // HDR color (ex: spark)
    Irufemi::Vector4 endColor = {8.0f, 8.0f, 8.0f, 1.0f};          // Ash/Cooling color
    Irufemi::Vector4 dissolveEdgeColor = {8.0f, 2.0f, 0.0f, 1.0f}; // Edge glow

    float spinSpeed = 15.0f;
    float noiseScale = 25.0f;
    float swayFrequency = 15.0f;
    float swayAmplitude = 10.0f;
};

class VoxelParticleSystem : public IComputeTask, public IRenderable {
public:
    enum class LoadingStatus { Pending, Loading, ReadyToCreateResources, Loaded, Failed };

    enum class ParticleType : uint32_t {
        Default = 0,
        Building = 1,
        AshDisintegration = 2,
        FineScatter = 3,
        DebrisLargeGravity = 4,
        DebrisExplosive = 5
    };

public:
    VoxelParticleSystem() = default;
    ~VoxelParticleSystem();

    /**
     * @brief Engine を設定する。
     * @param[in] engine 設定する Engine の値
     */
    static void SetEngine(IrufemiEngine* engine) {
        engine_ = engine;
    }

    /**
     * @brief Initialize を実行する。
     */
    void Initialize(const std::string& modelName, const Irufemi::Vector3Int& resolution);

    /**
     * @brief 初期化を完全に完了させ、GPUリソースおよびPSOの生成まで同期的に保証する
     */
    void EnsureInitialized();

    /**
     * @brief DispatchCompute を実行する。
     */
    void DispatchCompute() override;

    /**
     * @brief Update を実行する。
     */
    void Update(float deltaTime);
    /**
     * @brief Draw を実行する。
     */
    void Draw() override;
    /**
     * @brief SyncBeforeDraw を実行する。
     */
    void SyncBeforeDraw() override {}
    /**
     * @brief Debug を実行する。
     */
    void Debug(const char* name);

    /**
     * @brief UpdateEmitterData を実行する。
     */
    void UpdateEmitterData(uint32_t index, const VoxelEmitter& data);
    /**
     * @brief EmitterData を取得する。
     * @return 取得された EmitterData
     */
    const VoxelEmitter& GetEmitterData(uint32_t index) const {
        if (index < emittersData_.size()) {
            return emittersData_[index];
        }
        static VoxelEmitter dummy;
        return dummy;
    }

    /**
     * @brief IsLoaded かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsLoaded() const {
        return status_.load() == LoadingStatus::Loaded;
    }
    bool IsReady() const {
        auto s = status_.load();
        return s == LoadingStatus::Loaded || s == LoadingStatus::Failed;
    }
    /**
     * @brief Status を取得する。
     * @return 取得された Status
     */
    LoadingStatus GetStatus() const {
        return status_.load();
    }

    /**
     * @brief MaxInstances を取得する。
     * @return 取得された MaxInstances
     */
    uint32_t GetMaxInstances() const {
        return maxInstances_;
    }

private:
    /**
     * @brief CreateResources を実行する。
     */
    void CreateResources();
    /**
     * @brief CreatePSO を実行する。
     */
    void CreatePSO();
    /**
     * @brief CreateCubeMesh を実行する。
     */
    void CreateCubeMesh(float sizeX, float sizeY, float sizeZ);
    /**
     * @brief FinishInitialization を実行する。
     */
    void FinishInitialization();
    /**
     * @brief UpdateBuffers を実行する。
     */
    void UpdateBuffers();
    /**
     * @brief IsInFrustum かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsInFrustum(uint32_t index) const;

private:
    std::shared_ptr<VoxelizedModel> voxelModel_;

    // GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> voxelBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> cubeVertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> cubeIndexBuffer_;

    // デスクリプタハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE voxelSrvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE voxelSrvHandleGPU_{};
    D3D12_CPU_DESCRIPTOR_HANDLE particleSrvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE particleSrvHandleGPU_{};
    D3D12_CPU_DESCRIPTOR_HANDLE particleUavHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE particleUavHandleGPU_{};

    // デスクリプタインデックスの保持
    uint32_t voxelSrvIndex_ = 0xFFFFFFFF;
    uint32_t particleUavIndex_ = 0xFFFFFFFF;
    uint32_t particleSrvIndex_ = 0xFFFFFFFF;

    // インスタンシング用のエミッターバッファ（StructuredBuffer, トリプルバッファリング）
    uint32_t maxInstances_ = 1;
    std::vector<VoxelEmitter> emittersData_;
    Microsoft::WRL::ComPtr<ID3D12Resource> emittersBuffer_[3];
    VoxelEmitter* emittersMappedData_[3] = {nullptr, nullptr, nullptr};
    uint32_t emittersSrvIndex_[3] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    D3D12_CPU_DESCRIPTOR_HANDLE emittersSrvHandleCPU_[3]{};
    D3D12_GPU_DESCRIPTOR_HANDLE emittersSrvHandleGPU_[3]{};

    // メッシュビュー
    D3D12_VERTEX_BUFFER_VIEW cubeVertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW cubeIndexBufferView_{};
    uint32_t cubeIndexCount_ = 0;

    // PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> initializePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> emitPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> drawPSO_;

    struct VoxelPerFrame {
        float time = 0.0f;
        float deltaTime = 0.0f;
        float pad[2] = {};
    };
    static_assert(sizeof(VoxelPerFrame) == 16, "VoxelPerFrame size must be 16 bytes for ConstantBuffer alignment.");
    ConstantBuffer<VoxelPerFrame> perFrameBuffer_;
    VoxelPerFrame perFrameData_{};

    struct VoxelSystemCb {
        uint32_t voxelCount;
        uint32_t pad[3];
    };
    ConstantBuffer<VoxelSystemCb> voxelSystemCbBuffer_;
    VoxelSystemCb voxelSystemCbData_{};

    uint32_t voxelCount_ = 0;
    bool needsInitialize_ = true;

    struct AsyncLoadData {
        std::string modelName;
        std::shared_ptr<VoxelizedModel> voxelModel;
        uint32_t voxelCount = 0;
        std::atomic<LoadingStatus> status{LoadingStatus::Loading};
    };
    std::shared_ptr<AsyncLoadData> asyncData_;

    std::atomic<LoadingStatus> status_ = LoadingStatus::Pending;
    std::future<void> initializeFuture_;

    static IrufemiEngine* engine_;
};
