#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <array>
#include <memory>
#include "Renderer/Data/VertexData.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include <mutex>

// 前方宣言
class DrawManager;

enum class DebugCategory : uint32_t {
    None = 0,
    Collision = 1 << 0, ///< 物理コライダー（OBB, Sphere, AABB）
    Combat = 1 << 1,    ///< 弾幕、攻撃判定、ヒットボックス
    Particle = 1 << 2,  ///< パーティクル・エミッター領域
    Level = 1 << 3,     ///< スポーン範囲、トリガー領域
    Path = 1 << 4,      ///< スプラインレール、移動ノード
    General = 1 << 5,   ///< その他汎用
    All = 0xFFFFFFFF
};

inline constexpr DebugCategory operator|(DebugCategory a, DebugCategory b) {
    return static_cast<DebugCategory>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline constexpr DebugCategory operator&(DebugCategory a, DebugCategory b) {
    return static_cast<DebugCategory>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline constexpr DebugCategory operator~(DebugCategory a) {
    return static_cast<DebugCategory>(~static_cast<uint32_t>(a));
}

/**
 * @class DebugPrimitiveRenderer
 * @brief GPUインスタンシングを用いた高速なデバッグ用プリミティブ描画クラス
 * @details 以前はシングルトンでしたが、破棄順序の安全性を確保するため IrufemiEngine 管理に変更されました。
 */
class DebugPrimitiveRenderer {
public:
    /**
     * @brief コンストラクタ
     */
    DebugPrimitiveRenderer() = default;

    ~DebugPrimitiveRenderer();

    /**
     * @brief Initialize を実行する。
     */
    void Initialize(Irufemi::DirectXCommon* dx, DrawManager* drawM, Irufemi::DescriptorPool* srvAlloc);

    /**
     * @brief Update を実行する。
     */
    void Update();

    /**
     * @brief 全デバッグプリミティブ描画の一括有効/無効を設定する
     */
    void SetEnabled(bool enabled) {
        isEnabled_ = enabled;
    }

    /**
     * @brief 全デバッグプリミティブ描画の一括有効状態を取得する
     */
    bool IsEnabled() const {
        return isEnabled_;
    }

    /**
     * @brief 表示対象カテゴリのビットマスクを設定する
     */
    void SetCategoryMask(uint32_t mask) {
        categoryMask_ = mask;
    }

    /**
     * @brief 表示対象カテゴリのビットマスクを取得する
     */
    uint32_t GetCategoryMask() const {
        return categoryMask_;
    }

    /**
     * @brief 指定したカテゴリの表示/非表示を設定する
     */
    void SetCategoryEnabled(DebugCategory category, bool enabled) {
        if (enabled) {
            categoryMask_ |= static_cast<uint32_t>(category);
        } else {
            categoryMask_ &= ~static_cast<uint32_t>(category);
        }
    }

    /**
     * @brief 指定したカテゴリが表示対象かどうかを取得する
     */
    bool IsCategoryEnabled(DebugCategory category) const {
        return (categoryMask_ & static_cast<uint32_t>(category)) != 0;
    }

    /**
     * @brief AddSphere を実行する。
     */
    void AddSphere(const Irufemi::Vector3& center, float radius, const Irufemi::Vector4& color,
                   DebugCategory category = DebugCategory::General);
    /**
     * @brief AddCube を実行する。
     */
    void AddCube(const Irufemi::Matrix4x4& transform, const Irufemi::Vector4& color,
                 DebugCategory category = DebugCategory::General);

    /**
     * @brief ClearInstances を実行する。（シミュレーション用・描画用双方をクリア）
     */
    void ClearInstances();

    /**
     * @brief 毎フレーム末尾に描画フェーズ用のプリミティブのみをクリアする
     */
    void ClearDrawInstances();

    /**
     * @brief シミュレーション（Update）用のプリミティブのみをクリアする
     */
    void ClearSimulationInstances();

    /**
     * @brief シミュレーション（Update）フェーズを開始する
     * @details
     * Updateフェーズ中に呼ばれたAddSphere/AddCubeはシミュレーションバッファに格納され、ポーズ中もクリアされずにフリーズ保持されます。
     */
    void BeginSimulationFrame();

    /**
     * @brief シミュレーション（Update）フェーズを終了する
     */
    void EndSimulationFrame();

    /**
     * @brief BuildInstanceBuffer を実行する。
     */
    void BuildInstanceBuffer();
    /**
     * @brief Draw を実行する。
     */
    void Draw();

private:
    struct GPUInstanceData {
        Irufemi::Matrix4x4 world;
        Irufemi::Vector4 color;
    };

    struct CPUInstanceData {
        Irufemi::Matrix4x4 world;
        Irufemi::Vector4 color;
        DebugCategory category = DebugCategory::General;
    };

    /**
     * @brief CreateSphereResource を実行する。
     */
    void CreateSphereResource();
    /**
     * @brief CreateCubeResource を実行する。
     */
    void CreateCubeResource();
    /**
     * @brief EnsureInstancingSRVs を実行する。
     */
    void EnsureInstancingSRVs();

    Irufemi::DirectXCommon* dx_ = nullptr;
    DrawManager* drawManager_ = nullptr;
    Irufemi::DescriptorPool* srvAllocator_ = nullptr;

    // --- Irufemi::Sphere Data ---
    Microsoft::WRL::ComPtr<ID3D12Resource> sphereVertexResource_;
    D3D12_VERTEX_BUFFER_VIEW sphereVBV_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> sphereIndexResource_;
    D3D12_INDEX_BUFFER_VIEW sphereIBV_{};
    uint32_t sphereIndexCount_ = 0;

    // 描画フェーズ用（毎フレームクリア）
    std::vector<CPUInstanceData> drawSphereInstances_;
    size_t activeDrawSphereCount_ = 0;

    // シミュレーションフェーズ用（Update単位でクリア、ポーズ中は保持）
    std::vector<CPUInstanceData> simSphereInstances_;
    size_t activeSimSphereCount_ = 0;

    size_t maxSphereInstances_ = 65535;

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, Irufemi::kMaxFramesInFlight> sphereInstanceBuffer_;
    std::array<GPUInstanceData*, Irufemi::kMaxFramesInFlight> sphereInstanceDataMap_{};
    std::array<uint32_t, Irufemi::kMaxFramesInFlight> sphereInstanceCapacity_{};
    std::array<uint32_t, Irufemi::kMaxFramesInFlight> sphereSrvIndex_{};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, Irufemi::kMaxFramesInFlight> sphereSrvGPU_{};
    std::array<size_t, Irufemi::kMaxFramesInFlight> visibleSphereCount_{};

    // --- Cube Data ---
    Microsoft::WRL::ComPtr<ID3D12Resource> cubeVertexResource_;
    D3D12_VERTEX_BUFFER_VIEW cubeVBV_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> cubeIndexResource_;
    D3D12_INDEX_BUFFER_VIEW cubeIBV_{};
    uint32_t cubeIndexCount_ = 0;

    // 描画フェーズ用（毎フレームクリア）
    std::vector<CPUInstanceData> drawCubeInstances_;
    size_t activeDrawCubeCount_ = 0;

    // シミュレーションフェーズ用（Update単位でクリア、ポーズ中は保持）
    std::vector<CPUInstanceData> simCubeInstances_;
    size_t activeSimCubeCount_ = 0;

    size_t maxCubeInstances_ = 65535;

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, Irufemi::kMaxFramesInFlight> cubeInstanceBuffer_;
    std::array<GPUInstanceData*, Irufemi::kMaxFramesInFlight> cubeInstanceDataMap_{};
    std::array<uint32_t, Irufemi::kMaxFramesInFlight> cubeInstanceCapacity_{};
    std::array<uint32_t, Irufemi::kMaxFramesInFlight> cubeSrvIndex_{};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, Irufemi::kMaxFramesInFlight> cubeSrvGPU_{};
    std::array<size_t, Irufemi::kMaxFramesInFlight> visibleCubeCount_{};

    uint32_t lastUpdateFrameIndex_ = 0;

    bool isEnabled_ = true;
    uint32_t categoryMask_ = static_cast<uint32_t>(DebugCategory::All);
    bool isSimulating_ = false;

    std::mutex mutex_;
};
