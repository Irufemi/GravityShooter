#pragma once

#include "Renderer/System/Core/IRenderable.h"
#include <d3d12.h>
#include <memory>
#include <cstdint>
#include <random>
#include <array>
#include "RHI/DirectX12/DirectXCommon.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Transform.h"
#include "Renderer/Data/TransformationMatrix.h"
#include "Renderer/System/Core/LineResource.h"

// 前方宣言
class Camera;
class DrawManager;
class DirectXCommon;
class DescriptorPool;

// 3Dラインのインスタンス描画用クラス
class Line3DBatch : public IRenderable {
public:
    Line3DBatch() {
        instancingSrvIndex_.fill(UINT32_MAX);
    }
    ~Line3DBatch();

    /**
     * @brief 3Dラインバッチレンダラーの初期化（ベースメッシュおよびインスタンシングリソースの構築）
     */
    void Initialize();

    /**
     * @brief 描画中ラインの寿命更新処理
     */
    void Update();

    /**
     * @brief 新しい3Dラインインスタンスを追加する
     * @param[in] start ラインの始点ワールド座標
     * @param[in] end ラインの終点ワールド座標
     * @param[in] color ラインのカラー（RGBA）
     * @param[in] life ラインの表示持続時間（秒、デフォルト 1.0f）
     */
    void AddInstance(const Irufemi::Vector3& start, const Irufemi::Vector3& end, const Irufemi::Vector4& color,
                     float life = 1.0f);

    /**
     * @brief 登録されているすべてのラインインスタンスをクリアする
     */
    void ClearInstances();

    /**
     * @brief インスタンスデータをGPUバッファへ転送・構築する
     * @param[in] force 変更有無に関わらず強制転送するかどうか
     */
    void BuildInstanceBuffer(bool force = false);

    /**
     * @brief 描画前フレーム同期（DrawManagerへのパケット登録）
     */
    void SyncBeforeDraw() override;

    /**
     * @brief 3Dラインバッチの直接描画処理
     */
    void Draw() override;

    // --- DrawManager から参照する Getter 群 ---
    /**
     * @brief ベースとなるラインリソースを取得する
     * @return ラインリソースの生ポインタ
     */
    LineResource* GetBaseResource() const {
        return baseLineResource_.get();
    }

    /**
     * @brief 現在フレームのインスタンシングSRVのGPUハンドルを取得する
     * @return GPUデスクリプタハンドル
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetInstancingSrvHandleGPU() const {
        return instancingSrvGPU_[lastUpdateFrameIndex_];
    }

    /**
     * @brief アクティブなラインインスタンス数を取得する
     * @return 描画対象のインスタンス総数
     */
    UINT GetInstanceCountU32() const {
        return static_cast<UINT>(activeCount_);
    }

    /**
     * @brief 深度書き込み（DepthWrite）の有効/無効を設定する
     * @param[in] depthWrite 設定する DepthWrite モード
     */
    void SetDepthWrite(PSOManager::DepthWrite depthWrite) {
        depthWrite_ = depthWrite;
    }

    /**
     * @brief 現在の深度書き込みモードを取得する
     * @return DepthWrite モード
     */
    PSOManager::DepthWrite GetDepthWrite() const {
        return depthWrite_;
    }

    // 依存注入
    /**
     * @brief DirectXCommon インスタンスを設定する
     * @param[in] dx DirectXCommon ポインタ
     */
    static void SetDirectXCommon(DirectXCommon* dx) {
        dx_ = dx;
    }

    /**
     * @brief SRV用デスクリプタプールを設定する
     * @param[in] alloc DescriptorPool ポインタ
     */
    static void SetSrvAllocator(DescriptorPool* alloc) {
        srvAllocator_ = alloc;
    }

    /**
     * @brief DrawManager インスタンスを設定する
     * @param[in] drawM DrawManager ポインタ
     */
    static void SetDrawManager(DrawManager* drawM) {
        drawManager_ = drawM;
    }

    /**
     * @brief IrufemiEngine インスタンスを設定する
     * @param[in] engine エンジンポインタ
     */
    static void SetEngine(class IrufemiEngine* engine) {
        engine_ = engine;
    }

private:
    struct LineInstance {
        Irufemi::Vector3 start;
        Irufemi::Vector3 end;
        Irufemi::Vector4 color;
        float life = 1.0f;
        float age = 0.0f;
        bool active = false;
    };

    // VS 側が読む1インスタンス分のデータ
    struct InstanceData {
        Irufemi::Vector4 start;
        Irufemi::Vector4 end;
        Irufemi::Vector4 color;
    };

    /**
     * @brief インスタンシング描画用の頂点/インスタンスバッファを生成またはリサイズする
     * @param[in] instanceCount 確保する最大インスタンス数
     */
    void CreateOrResizeInstanceBuffer(uint32_t instanceCount);
    /**
     * @brief StructuredBuffer に対する SRV デスクリプタの確保と更新を保証する
     */
    void EnsureInstancingSRV();

private:
    // 静的依存
    static DirectXCommon* dx_;
    static DrawManager* drawManager_;
    static DescriptorPool* srvAllocator_;
    static class IrufemiEngine* engine_;

    std::unique_ptr<LineResource> baseLineResource_ = nullptr;

    std::vector<LineInstance> instances_;
    size_t activeCount_ = 0;
    size_t maxInstances_ = 65535;

    // インスタンシング用 StructuredBuffer と SRV
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxFramesInFlight> instanceBuffer_;
    std::array<InstanceData*, kMaxFramesInFlight> instanceData_{};
    std::array<uint32_t, kMaxFramesInFlight> instanceCapacity_{};

    uint32_t lastUpdateFrameIndex_ = 0;
    bool isDirty_ = true;

    std::array<uint32_t, kMaxFramesInFlight> instancingSrvIndex_{};
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kMaxFramesInFlight> instancingSrvCPU_{};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, kMaxFramesInFlight> instancingSrvGPU_{};

    PSOManager::DepthWrite depthWrite_ = PSOManager::DepthWrite::Enable;
};
