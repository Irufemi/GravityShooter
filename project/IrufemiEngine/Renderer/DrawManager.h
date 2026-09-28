#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>
#include <array>
#include <wrl.h>
#include "Renderer/Data/TransformationMatrix.h"
#include "Renderer/Data/LightCommonData.h"
#include "Renderer/Data/PointLight.h"
#include "Renderer/Data/SpotLight.h"
#include "Renderer/Data/AreaLight.h"
#include "Renderer/Data/DirectionalLight.h"
#include "Renderer/Data/SceneGPUStructs.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "RHI/DirectX12/DirectXCommon.h" // kMaxFramesInFlight のために追加
#include "RHI/DirectX12/RootSignatureConfig.h"
#include "Core/Math/Vector4.h"
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include "Renderer/Compute/IComputeTask.h"
#include "Renderer/Data/RenderPackets.h"

class ShadowMap;

// 前方宣言
class TextureManager;
class DirectXCommon;
class BaseParticle;
class BaseGPU_Particle;
class Primitive3DObject;
class PrimitiveBatch;
struct GpuMesh;
struct ManagedModel;
class Line2DClass;
class Line3DClass;
class Line3DBatch;
class Skybox;
namespace Irufemi {
struct SkinCluster;
}
struct GpuMaterial;

// 構造体を前方宣言

// 描画のCommandListを積む順番
// Viewport → RootSignature → Pipeline → Topology → Buffers → CBV → SRV → Draw

/**
 * @class DrawManager
 * @brief 描画コマンドの発行とパイプライン管理を担うクラス
 * @details 各レンダラーからの描画リクエストを受け取り、適切な順序でコマンドリストに積みます。
 *          ライト情報の管理や、RenderTexture を用いたポストプロセス実行の制御も行います。
 */
class DrawManager {
public:
    /**
     * @brief スレッドごとのローカル描画キュー構造体
     * @details 各スレッドが独立してキューイングを行い、描画実行前にメインキューへ一括マージされます。
     */
    struct LocalRenderQueues {
        std::vector<RenderPackets::Standard3DPacket> standard3DQueue;
        std::vector<RenderPackets::Standard3DPacket> transparent3DQueue;
        std::vector<RenderPackets::Standard3DPacket> ui3DQueue;
        std::vector<RenderPackets::Standard3DPacket> selectionMaskQueue;
        std::vector<RenderPackets::SpritePacket> selectionMaskQueue2D;
        std::vector<RenderPackets::SpritePacket> spriteQueue;
        std::vector<RenderPackets::SpriteBatchPacket> spriteBatchQueue;
        std::vector<RenderPackets::LinePacket> lineQueue;
        std::vector<RenderPackets::GPUParticlePacket> gpuParticleQueue;
        std::vector<RenderPackets::VoxelParticlePacket> voxelParticleQueue;
        std::vector<RenderPackets::SkyboxPacket> skyboxQueue;
        std::vector<RenderPackets::SkydomePacket> skydomeQueue;
        std::vector<RenderPackets::PrimitiveBatchPacket> primitiveBatchQueue;
        std::vector<RenderPackets::Primitive2DBatchPacket> primitive2DBatchQueue;
        std::vector<RenderPackets::ModelBatchPacket> modelBatchQueue;
        std::vector<RenderPackets::DebugPrimitivePacket> debugPrimitiveQueue;
        std::vector<std::function<void()>> postRenderQueue;
        std::vector<RenderPackets::SpritePacket> topMostSpriteQueue;
        std::vector<RenderPackets::SpriteBatchPacket> topMostSpriteBatchQueue;
        std::vector<RenderPackets::SpritePacket> textQueue;
        std::vector<RenderPackets::SpritePacket> topMostTextQueue;

        void Clear() {
            standard3DQueue.clear();
            transparent3DQueue.clear();
            ui3DQueue.clear();
            selectionMaskQueue.clear();
            selectionMaskQueue2D.clear();
            spriteQueue.clear();
            spriteBatchQueue.clear();
            lineQueue.clear();
            gpuParticleQueue.clear();
            voxelParticleQueue.clear();
            skyboxQueue.clear();
            skydomeQueue.clear();
            primitiveBatchQueue.clear();
            primitive2DBatchQueue.clear();
            modelBatchQueue.clear();
            debugPrimitiveQueue.clear();
            postRenderQueue.clear();
            topMostSpriteQueue.clear();
            topMostSpriteBatchQueue.clear();
            textQueue.clear();
            topMostTextQueue.clear();
        }
    };

private:
    // --- Render Queues (メインスレッド統合用キュー) ---
    std::vector<RenderPackets::Standard3DPacket> standard3DQueue_;
    std::vector<RenderPackets::Standard3DPacket> transparent3DQueue_; // 半透明・エフェクト用キュー
    std::vector<RenderPackets::Standard3DPacket> ui3DQueue_;
    std::vector<RenderPackets::Standard3DPacket> selectionMaskQueue_;
    std::vector<RenderPackets::SpritePacket> selectionMaskQueue2D_;
    std::vector<RenderPackets::SpritePacket> spriteQueue_;
    std::vector<RenderPackets::SpriteBatchPacket> spriteBatchQueue_;

    std::vector<RenderPackets::LinePacket> lineQueue_;
    std::vector<RenderPackets::GPUParticlePacket> gpuParticleQueue_;
    std::vector<RenderPackets::VoxelParticlePacket> voxelParticleQueue_;
    std::vector<RenderPackets::SkyboxPacket> skyboxQueue_;
    std::vector<RenderPackets::SkydomePacket> skydomeQueue_;
    std::vector<RenderPackets::PrimitiveBatchPacket> primitiveBatchQueue_;
    std::vector<RenderPackets::Primitive2DBatchPacket> primitive2DBatchQueue_;
    std::vector<RenderPackets::ModelBatchPacket> modelBatchQueue_;
    std::vector<RenderPackets::DebugPrimitivePacket> debugPrimitiveQueue_;
    std::vector<std::function<void()>> postRenderQueue_;

    // 最前面UI描画用キュー (PostProcess適用後のバックバッファに直接描画)
    std::vector<RenderPackets::SpritePacket> topMostSpriteQueue_;
    std::vector<RenderPackets::SpriteBatchPacket> topMostSpriteBatchQueue_;
    std::vector<RenderPackets::SpritePacket> textQueue_;
    std::vector<RenderPackets::SpritePacket> topMostTextQueue_;

    // --- スレッドローカルキュー管理 ---
    std::mutex registryMutex_; ///< スレッドローカルキュー登録用ミューテックス
    std::vector<LocalRenderQueues*> registeredQueues_; ///< 登録された全スレッドローカルキューのポインタ一覧
    std::shared_ptr<std::atomic<bool>> isAlive_; ///< DrawManager生存フラグ（スレッド終了時の安全判定）

    // レンダーグラフ
    std::unique_ptr<class RenderGraph> renderGraph_;

public:
    // --- Queue Getters for RenderPasses ---
    /**
     * @brief Standard3DQueue を取得する。
     * @return 取得された Standard3DQueue
     */
    const std::vector<RenderPackets::Standard3DPacket>& GetStandard3DQueue() const {
        return standard3DQueue_;
    }
    /**
     * @brief Transparent3DQueue を取得する。
     * @return 取得された Transparent3DQueue
     */
    const std::vector<RenderPackets::Standard3DPacket>& GetTransparent3DQueue() const {
        return transparent3DQueue_;
    }
    /**
     * @brief UI3DQueue を取得する。
     * @return 取得された UI3DQueue
     */
    const std::vector<RenderPackets::Standard3DPacket>& GetUI3DQueue() const {
        return ui3DQueue_;
    }
    /**
     * @brief SelectionMaskQueue を取得する。
     * @return 取得された SelectionMaskQueue
     */
    const std::vector<RenderPackets::Standard3DPacket>& GetSelectionMaskQueue() const {
        return selectionMaskQueue_;
    }
    /**
     * @brief SelectionMaskQueue2D を取得する。
     * @return 取得された SelectionMaskQueue2D
     */
    const std::vector<RenderPackets::SpritePacket>& GetSelectionMaskQueue2D() const {
        return selectionMaskQueue2D_;
    }
    /**
     * @brief SpriteQueue を取得する。
     * @return 取得された SpriteQueue
     */
    const std::vector<RenderPackets::SpritePacket>& GetSpriteQueue() const {
        return spriteQueue_;
    }
    /**
     * @brief SpriteBatchQueue を取得する。
     * @return 取得された SpriteBatchQueue
     */
    const std::vector<RenderPackets::SpriteBatchPacket>& GetSpriteBatchQueue() const {
        return spriteBatchQueue_;
    }

    /**
     * @brief LineQueue を取得する。
     * @return 取得された LineQueue
     */
    const std::vector<RenderPackets::LinePacket>& GetLineQueue() const {
        return lineQueue_;
    }
    /**
     * @brief GPUParticleQueue を取得する。
     * @return 取得された GPUParticleQueue
     */
    const std::vector<RenderPackets::GPUParticlePacket>& GetGPUParticleQueue() const {
        return gpuParticleQueue_;
    }
    /**
     * @brief VoxelParticleQueue を取得する。
     * @return 取得された VoxelParticleQueue
     */
    const std::vector<RenderPackets::VoxelParticlePacket>& GetVoxelParticleQueue() const {
        return voxelParticleQueue_;
    }
    /**
     * @brief SkyboxQueue を取得する。
     * @return 取得された SkyboxQueue
     */
    const std::vector<RenderPackets::SkyboxPacket>& GetSkyboxQueue() const {
        return skyboxQueue_;
    }
    /**
     * @brief SkydomeQueue を取得する。
     * @return 取得された SkydomeQueue
     */
    const std::vector<RenderPackets::SkydomePacket>& GetSkydomeQueue() const {
        return skydomeQueue_;
    }
    /**
     * @brief PrimitiveBatchQueue を取得する。
     * @return 取得された PrimitiveBatchQueue
     */
    const std::vector<RenderPackets::PrimitiveBatchPacket>& GetPrimitiveBatchQueue() const {
        return primitiveBatchQueue_;
    }
    /**
     * @brief Primitive2DBatchQueue を取得する。
     * @return 取得された Primitive2DBatchQueue
     */
    const std::vector<RenderPackets::Primitive2DBatchPacket>& GetPrimitive2DBatchQueue() const {
        return primitive2DBatchQueue_;
    }
    /**
     * @brief ModelBatchQueue を取得する。
     * @return 取得された ModelBatchQueue
     */
    const std::vector<RenderPackets::ModelBatchPacket>& GetModelBatchQueue() const {
        return modelBatchQueue_;
    }
    /**
     * @brief DebugPrimitiveQueue を取得する。
     * @return 取得された DebugPrimitiveQueue
     */
    const std::vector<RenderPackets::DebugPrimitivePacket>& GetDebugPrimitiveQueue() const {
        return debugPrimitiveQueue_;
    }
    const std::vector<std::function<void()>>& GetPostRenderQueue() const {
        return postRenderQueue_;
    }
    /**
     * @brief TopMostSpriteQueue を取得する。
     * @return 取得された TopMostSpriteQueue
     */
    const std::vector<RenderPackets::SpritePacket>& GetTopMostSpriteQueue() const {
        return topMostSpriteQueue_;
    }
    /**
     * @brief TopMostSpriteBatchQueue を取得する。
     * @return 取得された TopMostSpriteBatchQueue
     */
    const std::vector<RenderPackets::SpriteBatchPacket>& GetTopMostSpriteBatchQueue() const {
        return topMostSpriteBatchQueue_;
    }

    // --- Text Queues ---
    /**
     * @brief TextQueue を取得する。
     * @return 取得された TextQueue
     */
    const std::vector<RenderPackets::SpritePacket>& GetTextQueue() const {
        return textQueue_;
    }
    /**
     * @brief TopMostTextQueue を取得する。
     * @return 取得された TopMostTextQueue
     */
    const std::vector<RenderPackets::SpritePacket>& GetTopMostTextQueue() const {
        return topMostTextQueue_;
    }

    // --- Execute Queues ---
    /**
     * @brief ExecuteRenderQueues を実行する。
     */
    void ExecuteRenderQueues(class IrufemiEngine* engine);
    /**
     * @brief ClearRenderQueues を実行する。
     */
    void ClearRenderQueues();
    /**
     * @brief ClearAllQueues を実行する。
     */
    void ClearAllQueues() {
        ClearRenderQueues();
        computeTasks_.clear();
    }

    class IrufemiEngine* engine_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;
    ID3D12GraphicsCommandList* commandList_ = nullptr; // コマンドリストをキャッシュ

    std::vector<IComputeTask*> computeTasks_;

    // 各フレームごとの動的リソース
    struct FrameResource {
        Microsoft::WRL::ComPtr<ID3D12Resource> frameResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> areaLightResource;

        PerFrameData* perFrameData = nullptr;
        LightCommonData* lightCommonData = nullptr;

        D3D12_GPU_DESCRIPTOR_HANDLE lightSrvHandle{};
        uint32_t lightSrvBaseIndex = 0xFFFFFFFFu;

        // カメラやライト共通情報を格納するデータ
        struct FrameData {
            D3D12_GPU_VIRTUAL_ADDRESS camera;
            D3D12_GPU_VIRTUAL_ADDRESS lightCommon; // register b1
        } frameData{};
    };
    std::array<FrameResource, kMaxFramesInFlight> frameResources_;

    D3D12_GPU_DESCRIPTOR_HANDLE environmentMapHandle_{}; // 環境マップ用SRVハンドル

    // シャドウマップ・レンダーターゲット関連
    std::array<std::unique_ptr<ShadowMap>, kMaxFramesInFlight> shadowMaps_;
    bool isShadowPass_ = false;
    class RenderTexture* currentRenderTexture_ = nullptr;
    RenderTexture* currentRenderTexture2_ = nullptr;

public: // メンバ関数
    /** @name 初期化・終了処理 */
    ///@{
    DrawManager();
    ~DrawManager();

    /**
     * @brief Initialize を実行する。
     */
    void Initialize(class IrufemiEngine* engine, DirectXCommon* dx);
    /**
     * @brief Finalize を実行する。
     */
    void Finalize();
    /**
     * @brief OnResize を実行する。
     */
    void OnResize(int32_t width, int32_t height);

    /**
     * @brief RenderGraphにリソースの初期ステートを登録する（リサイズ時用）
     */
    void SetInitialResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);

    /**
     * @brief RenderGraphにリソースの期待する最終ステートを登録する
     */
    void SetFinalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);
    ///@}

    /** @name パイプライン・描画フロー制御 */
    ///@{
    /**
     * @brief パイプラインステートをバインドする
     * @param[in] pso バインドするパイプラインステート
     */
    void BindPSO(ID3D12PipelineState* pso);
    /**
     * @brief BeginShadowPass を実行する。
     */
    void BeginShadowPass();
    /**
     * @brief EndShadowPass を実行する。
     */
    void EndShadowPass();
    /**
     * @brief IsShadowPass かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsShadowPass() const {
        return isShadowPass_;
    }

    /**
     * @brief フレームの描画開始処理
     * @details レンダーターゲットのクリアとビューポートの設定を行います。
     */
    void PreDraw(std::array<float, 4> clearColor = {0.1f, 0.25f, 0.5f, 1.0f}, float clearDepth = 1.0f,
                 uint8_t clearStencil = 0);

    /**
     * @brief フレームの描画終了処理
     * @details リソースバリアの変更とコマンドリストのクローズ準備を行います。
     */
    void PostDraw();

    /**
     * @brief フレーム共通のルートパラメータをバインドする
     * @details カメラ、ライト、各種管理用定数バッファを一括でレジスタに設定します。
     */
    void BindCommonParameters();

    /** @name Computeタスク管理 */
    ///@{
    /**
     * @brief 今フレームで実行すべきComputeタスクを登録する
     * @param task 登録するタスク（GPUParticleSystem等）
     */
    void RegisterComputeTask(IComputeTask* task) {
        computeTasks_.push_back(task);
    }

    const std::vector<IComputeTask*>& GetComputeTasks() const {
        return computeTasks_;
    }

    /**
     * @brief 登録された全Computeタスクを一括実行し、リストをクリアする
     */
    void ExecuteComputePasses();

    /**
     * @brief 現在のスレッドに対応するローカル描画キューを取得する
     * @return スレッドローカルな LocalRenderQueues への参照
     */
    LocalRenderQueues& GetLocalQueues();

    /**
     * @brief スレッドローカルキューをDrawManagerに登録する
     */
    void RegisterThreadQueues(LocalRenderQueues* queues);

    /**
     * @brief スレッドローカルキューの登録を解除する
     */
    void UnregisterThreadQueues(LocalRenderQueues* queues);

    /**
     * @brief 全スレッドのローカルキューをメインキューへ一括マージする
     */
    void MergeThreadLocalQueues();

    // カスタム描画コールバック用キュー
    void SubmitPostRender(std::function<void()> drawFunc);
    ///@}

    /** @name GPU Culling */
    ///@{
    void DispatchGPUCulling(const RenderPackets::ModelBatchPacket& packet);
    ///@}
    ///@}

    /** @name レンダーターゲット・ポストプロセス操作 */
    ///@{
    /**
     * @brief 指定した複数の RenderTexture への描画を開始する (MRT用)
     * @param[in] renderTargets 出力先の RenderTexture のリスト
     * @param[in] clearColors 各 RenderTexture の背景クリア色
     */
    void BeginRenderTextures(const std::vector<class RenderTexture*>& renderTargets,
                             const std::vector<Irufemi::Vector4>& clearColors);

    /**
     * @brief 複数の RenderTexture への描画を終了する
     */
    void EndRenderTextures(const std::vector<class RenderTexture*>& renderTargets);

    /**
     * @brief 指定した RenderTexture への描画を開始する
     * @param[in] rt 出力先の RenderTexture
     * @param[in] clearColor 背景クリア色
     */
    void BeginRenderTexture(class RenderTexture* rt, const Irufemi::Vector4& clearColor,
                            class RenderTexture* rt2 = nullptr,
                            const Irufemi::Vector4& clearColor2 = Irufemi::Vector4{0.0f, 0.0f, 0.0f, 0.0f});

    /**
     * @brief RenderTexture への描画を終了する
     * @param[in] rt 描画していた RenderTexture
     */
    void EndRenderTexture(class RenderTexture* rt, class RenderTexture* rt2 = nullptr);

    /**
     * @brief レンダーターゲットをバックバッファ（画面）に戻す
     * @param[in] useDepth 深度バッファを使用するかどうか
     */
    void SetRenderTargetToBackBuffer(bool useDepth = true);

    /**
     * @brief RenderTexture を全画面に描画（ポストプロセス用）
     * @param[in] renderTexture 描画元のテクスチャ
     * @param[in] pso 使用するポストプロセス用パイプラインステート
     * @param[in] cbvAddress 追加の定数バッファアドレス（オプション）
     * @param[in] depthSrvHandle 深度テクスチャのハンドル（オプション）
     */
    void DrawRenderTexture(class RenderTexture* renderTexture, ID3D12PipelineState* pso = nullptr,
                           D3D12_GPU_VIRTUAL_ADDRESS cbvAddress = 0, D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle = {0});
    ///@}

    /** @name 共通データ設定 */
    ///@{
    /**
     * @brief フレーム単位の共通データを定数バッファに書き込む
     */
    void SetFrameData(const CameraForGPU& camera, float time, float deltaTime, const DirectionalLight& light,
                      const std::vector<PointLight*>& pointLights, const std::vector<SpotLight*>& spotLights,
                      const std::vector<AreaLight*>& areaLights,
                      const Irufemi::Vector2& resolution = {1280.0f, 720.0f});

    /**
     * @brief
     * キャッシュされたフレームデータを用いて現在のフレームバッファを同期する（ポーズなどでSetFrameDataが呼ばれなかった時用）
     */
    void SyncCachedFrameData();

private:
    PerFrameData cachedPerFrame_{};
    DirectionalLight cachedDirectionalLight_{};
    std::vector<PointLight> cachedPointLights_;
    std::vector<SpotLight> cachedSpotLights_;
    std::vector<AreaLight> cachedAreaLights_;

    // シャドウマップのカスタムパラメータ
    Irufemi::Vector3 shadowTargetPos_{0, 0, 0};
    float shadowOrthoSize_{128.0f};
    bool useCustomShadowParams_{false};

    TextureManager* textureManager_ = nullptr; ///< 環境マップフォールバック等に使用するテクスチャマネージャー

public:
    /**
     * @brief シャドウマップの注視点とサイズをカスタマイズする
     * @param targetPos 注視点（通常はプレイヤーとボスの中心）
     * @param orthoSize 描画範囲（通常はプレイヤーとボスの距離に基づく）
     */
    void SetShadowParameters(const Irufemi::Vector3& targetPos, float orthoSize) {
        shadowTargetPos_ = targetPos;
        shadowOrthoSize_ = orthoSize;
        useCustomShadowParams_ = true;
    }

    /**
     * @brief カスタムシャドウパラメータをリセットし、デフォルト（カメラ追従）に戻す
     */
    void ResetShadowParameters() {
        useCustomShadowParams_ = false;
    }

    /**
     * @brief テクスチャマネージャーのポインタを設定する
     * @param[in] textureManager 依存注入するテクスチャマネージャーへのポインタ
     */
    void SetTextureManager(TextureManager* textureManager) {
        textureManager_ = textureManager;
    }

    /**
     * @brief 環境マップを設定する
     * @param[in] envMapHandle 環境マップテクスチャのGPUハンドル
     */
    void SetEnvironmentMap(D3D12_GPU_DESCRIPTOR_HANDLE envMapHandle);
    /**
     * @brief EnvironmentMap を取得する。
     * @return 取得された EnvironmentMap
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetEnvironmentMap() const {
        return environmentMapHandle_;
    }
    ///@}

    /** @name 各種オブジェクト描画メソッド */
    ///@{

    /**
     * @brief 矩形領域（Region）の描画
     */
    void SubmitModelBatch(const RenderPackets::ModelBatchPacket& packet);
    /**
     * @brief DrawModelBatch を実行する。
     */
    void DrawModelBatch(const RenderPackets::ModelBatchPacket& packet);

    /**
     * @brief 汎用的な領域描画（頂点バッファ・インデックスバッファ直接指定）
     */
    void SubmitPrimitiveBatch(const RenderPackets::PrimitiveBatchPacket& packet);
    /**
     * @brief DrawPrimitiveBatch を実行する。
     */
    void DrawPrimitiveBatch(const RenderPackets::PrimitiveBatchPacket& packet);

    /**
     * @brief 2D汎用図形領域描画（インスタンシング対応の2Dプリミティブバッチ）
     */
    void SubmitPrimitive2DBatch(const RenderPackets::Primitive2DBatchPacket& packet);
    /**
     * @brief DrawPrimitive2DBatch を実行する。
     */
    void DrawPrimitive2DBatch(const RenderPackets::Primitive2DBatchPacket& packet);

    /**
     * @brief インスタンス化された線の描画
     */
    void SubmitLineInstanced(const class LineResource* resource,
                             const D3D12_GPU_DESCRIPTOR_HANDLE& instancingSrvHandleGPU, const UINT& instanceCount,
                             PSOManager::DepthWrite depthWrite = PSOManager::DepthWrite::Enable);
    /**
     * @brief DrawLineInstanced を実行する。
     */
    void DrawLineInstanced(const RenderPackets::LinePacket& packet);

    /**
     * @brief SubmitDebugPrimitive を実行する。
     */
    void SubmitDebugPrimitive(const RenderPackets::DebugPrimitivePacket& packet);
    /**
     * @brief DrawDebugPrimitive を実行する。
     */
    void DrawDebugPrimitive(const RenderPackets::DebugPrimitivePacket& packet);

    /**
     * @brief 標準的な3Dオブジェクトの描画 (Object3d.hlsl)
     * @param vertexBufferViewOverride スキニング等でVBVを差し替えたい場合に指定
     */
    void SubmitStandard3D(const class Object3DResource* resource,
                          const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride = nullptr, bool castShadows = true,
                          ID3D12Resource* vertexBufferResourceOverride = nullptr,
                          D3D12_GPU_VIRTUAL_ADDRESS overrideMaterialCBV = 0);
    /**
     * @brief 半透明・エフェクト用の3D標準描画のキューに追加（距離ソート用）
     */
    void SubmitTransparent3D(const class Object3DResource* resource,
                             const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride = nullptr,
                             bool castShadows = false, ID3D12Resource* vertexBufferResourceOverride = nullptr,
                             D3D12_GPU_VIRTUAL_ADDRESS overrideMaterialCBV = 0);
    /**
     * @brief SubmitUI3D を実行する。
     */
    void SubmitUI3D(const class Object3DResource* resource,
                    const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride = nullptr);
    /**
     * @brief SubmitOutlineMask を実行する。
     */
    void SubmitOutlineMask(const class Object3DResource* resource,
                           const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride = nullptr,
                           ID3D12Resource* vertexBufferResourceOverride = nullptr);
    /**
     * @brief SubmitTextOutlineMask を実行する。
     */
    void SubmitTextOutlineMask(const class Object2DResource* resource);
    /**
     * @brief DrawStandard3D を実行する。
     */
    void DrawStandard3D(const RenderPackets::Standard3DPacket& packet);

    /**
     * @brief 2Dオブジェクト（スプライト等）の標準描画 (Sprite.hlsl)
     */
    void SubmitSprite(const class Object2DResource* resource);
    /**
     * @brief DrawSprite を実行する。
     */
    void DrawSprite(const RenderPackets::SpritePacket& packet);

    /**
     * @brief SubmitSpriteBatch を実行する。
     */
    void SubmitSpriteBatch(const RenderPackets::SpriteBatchPacket& packet);
    /**
     * @brief DrawSpriteBatch を実行する。
     */
    void DrawSpriteBatch(const RenderPackets::SpriteBatchPacket& packet);

    /**
     * @brief SubmitTopMostSpriteBatch を実行する。
     */
    void SubmitTopMostSpriteBatch(const RenderPackets::SpriteBatchPacket& packet);
    /**
     * @brief DrawTopMostSpriteBatch を実行する。
     */
    void DrawTopMostSpriteBatch(const RenderPackets::SpriteBatchPacket& packet);

    /**
     * @brief SubmitTopMostSprite を実行する。
     */
    void SubmitTopMostSprite(const class Object2DResource* resource);

    // --- Text ---
    /**
     * @brief SubmitText を実行する。
     */
    void SubmitText(const class Object2DResource* resource);
    /**
     * @brief SubmitTopMostText を実行する。
     */
    void SubmitTopMostText(const class Object2DResource* resource);
    /**
     * @brief DrawText を実行する。
     */
    void DrawText(const RenderPackets::SpritePacket& packet);

    /**
     * @brief スカイボックスの描画コマンドを送信する
     */
    void SubmitSkybox(const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView, const D3D12_INDEX_BUFFER_VIEW& indexBufferView,
                      D3D12_GPU_VIRTUAL_ADDRESS materialAddress, D3D12_GPU_VIRTUAL_ADDRESS transformationAddress,
                      const UINT& indexCount);
    /**
     * @brief DrawSkybox を実行する。
     */
    void DrawSkybox(const RenderPackets::SkyboxPacket& packet);

    /**
     * @brief プロシージャルスカイドームの描画コマンドを送信する
     * @param[in] materialAddress マテリアル定数バッファのGPU仮想アドレス
     */
    void SubmitSkydome(D3D12_GPU_VIRTUAL_ADDRESS materialAddress);
    /**
     * @brief DrawSkydome を実行する。
     */
    void DrawSkydome(const RenderPackets::SkydomePacket& packet);

    /**
     * @brief GPUパーティクルのインスタンス描画 (GPUParticle.hlsl)
     */
    void SubmitGPUParticle(const RenderPackets::GPUParticlePacket& packet);
    /**
     * @brief DrawGPUParticle を実行する。
     */
    void DrawGPUParticle(const RenderPackets::GPUParticlePacket& packet);

    // VoxelParticle 用の描画 (VoxelParticle.hlsl)
    void SubmitVoxelParticle(uint32_t instanceCount, const D3D12_VERTEX_BUFFER_VIEW& vbv,
                             const D3D12_INDEX_BUFFER_VIEW& ibv, uint32_t indexCount,
                             D3D12_GPU_VIRTUAL_ADDRESS systemCbAddress, D3D12_GPU_DESCRIPTOR_HANDLE emitterHandle,
                             D3D12_GPU_DESCRIPTOR_HANDLE particleDataHandle, ID3D12Resource* particleResource,
                             ID3D12PipelineState* drawPSO);
    /**
     * @brief DrawVoxelParticle を実行する。
     */
    void DrawVoxelParticle(const RenderPackets::VoxelParticlePacket& packet);
    ///@}

    /** @name コンピュートシェーダ（GPGPU）操作 */
    ///@{
    /**
     * @brief スキニング計算（Compute Shader）の実行
     */
    void DispatchSkinning(const Irufemi::SkinCluster& skinCluster, const ManagedModel* model, uint32_t numVertices);

    /**
     * @brief UAVバリアの実行（リソース競合の解決）
     */
    void ExecuteUAVBarrier(ID3D12Resource* resource = nullptr);
    ///@}

    /** @name 状態取得・ユーティリティ */
    ///@{
    PerFrameData* GetPerFrameData() const {
        return frameResources_[dxCommon_->GetFrameIndex()].perFrameData;
    }
    /**
     * @brief RenderGraph を取得する。
     * @return 取得された RenderGraph
     */
    class RenderGraph* GetRenderGraph() const {
        return renderGraph_.get();
    }
    /**
     * @brief DxCommon を取得する。
     * @return 取得された DxCommon
     */
    DirectXCommon* GetDxCommon() const {
        return dxCommon_;
    }
    /**
     * @brief ShadowMap を取得する。
     * @return 取得された ShadowMap
     */
    ShadowMap* GetShadowMap() const {
        return shadowMaps_[dxCommon_->GetFrameIndex()].get();
    }

    /**
     * @brief VSyncの有効/無効を設定する。
     */
    void SetVSync(bool enable) {
        vSyncEnabled_ = enable;
    }

    /**
     * @brief VSyncが有効か取得する。
     */
    bool IsVSyncEnabled() const {
        return vSyncEnabled_;
    }
    ///@}

private:
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> commandSignature_;
    bool vSyncEnabled_ = true;
};
