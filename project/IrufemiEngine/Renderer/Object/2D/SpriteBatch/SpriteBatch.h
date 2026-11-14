#pragma once

#include "Renderer/System/Core/IRenderable.h"
#include <d3d12.h>
#include <vector>
#include <string>
#include <memory>
#include <array>
#include <wrl.h>
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Transform.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Renderer/Data/RenderPackets.h"

// 前方宣言
namespace Irufemi {
class TextureManager;
}
class DrawManager;
class CameraManager;
class Object2DResource;
class Camera;

/**
 * @class SpriteBatch
 * @brief 2Dスプライトのインスタンシング描画（バッチ描画）を行うクラス
 */
class SpriteBatch : public IRenderable {
public:
    SpriteBatch();
    ~SpriteBatch() override;

    /**
     * @brief スプライトバッチを初期化し、テクスチャロードおよび基底メッシュリソースを構築する。
     * @param[in] textureName 使用するテクスチャのファイルパス
     */
    void Initialize(const std::string& textureName = "resources/uvChecker.png");
    /**
     * @brief スプライトバッチの内部状態を更新する。
     */
    void Update();

    // インスタンスの追加
    /**
     * @brief Transform とカラーを指定してスプライトインスタンスを追加する。
     * @param[in] transform スプライトのトランスフォーム（拡縮・回転・位置）
     * @param[in] color 乗算カラー (RGBA)
     */
    void AddInstance(const Irufemi::Transform& transform, const Irufemi::Vector4& color = {1.0f, 1.0f, 1.0f, 1.0f});
    /**
     * @brief 座標・サイズ・回転・カラー・アンカーを指定してスプライトインスタンスを追加する。
     * @param[in] position スクリーン座標 (X, Y)
     * @param[in] size スプライトの描画サイズ (幅, 高さ)
     * @param[in] rotation 回転角度（ラジアン）
     * @param[in] color 乗算カラー (RGBA)
     * @param[in] anchor 原点アンカーポイント (0.0~1.0, デフォルトは中心 {0.5f, 0.5f})
     */
    void AddInstance(const Irufemi::Vector2& position, const Irufemi::Vector2& size, float rotation = 0.0f,
                     const Irufemi::Vector4& color = {1.0f, 1.0f, 1.0f, 1.0f},
                     const Irufemi::Vector2& anchor = {0.5f, 0.5f});

    /**
     * @brief 登録されたすべてのスプライトインスタンスをクリアする。
     */
    void ClearInstances();

    /**
     * @brief 描画直前にインスタンスバッファを同期・転送する。
     */
    void SyncBeforeDraw() override;
    /**
     * @brief スプライトバッチを描画キューへ発行する。
     */
    void Draw() override;
    /**
     * @brief 最前面フラグを指定してスプライトバッチを描画キューへ発行する。
     * @param[in] isTopMost 最前面に描画するかどうかのフラグ
     */
    void Draw(bool isTopMost);

    // Getters
    /**
     * @brief 内部で保持している Object2DResource へのポインタを取得する。
     * @return Object2DResource へのポインタ
     */
    Object2DResource* GetD3D12Resource() const {
        return baseResource_.get();
    }
    /**
     * @brief 現在フレームのインスタンシングバッファ SRV の GPU ハンドルを取得する。
     * @return D3D12_GPU_DESCRIPTOR_HANDLE
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetInstancingSrvHandleGPU() const;
    /**
     * @brief 描画対象の可視インスタンス総数を取得する。
     * @return 有効なインスタンス数
     */
    UINT GetInstanceCount() const {
        return static_cast<UINT>(visibleInstanceCount_);
    }

    /**
     * @brief TopMost を設定する。
     * @param[in] isTopMost 設定する TopMost の値
     */
    void SetTopMost(bool isTopMost) {
        isTopMost_ = isTopMost;
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
     * @param[in] blend ブレンドモード
     * @param[in] depth 深度設定
     * @param[in] cull カリングモード
     */
    void SetCustomPSO(const std::string& psoName, Irufemi::BlendMode blend = Irufemi::BlendMode::kBlendModeNormal,
                      PSOManager::DepthWrite depth = PSOManager::DepthWrite::Off,
                      PSOManager::CullMode cull = PSOManager::CullMode::None) {
        customPSOName_ = psoName;
        customBlend_ = blend;
        customDepth_ = depth;
        customCull_ = cull;
    }
    /**
     * @brief CustomPSO を取得する。名前指定がある場合は PSOManager から動的解決する。
     * @return 取得された CustomPSO
     */
    ID3D12PipelineState* GetCustomPSO() const;

    /**
     * @brief CustomCBV を設定する。
     * @param[in] cbv 設定する CustomCBV の値
     */
    void SetCustomCBV(D3D12_GPU_VIRTUAL_ADDRESS cbv) {
        customCBVAddress_ = cbv;
    }

    /**
     * @brief TextureManager を設定する。
     * @param[in] tm 設定する TextureManager の値
     */
    static void SetTextureManager(Irufemi::TextureManager* tm) {
        textureManager_ = tm;
    }
    /**
     * @brief DrawManager を設定する。
     * @param[in] dm 設定する DrawManager の値
     */
    static void SetDrawManager(DrawManager* dm) {
        drawManager_ = dm;
    }
    /**
     * @brief CameraManager を設定する。
     * @param[in] cm 設定する CameraManager の値
     */
    static void SetCameraManager(CameraManager* cm) {
        cameraManager_ = cm;
    }
    /**
     * @brief DirectXCommon を設定する。
     * @param[in] dx 設定する DirectXCommon の値
     */
    static void SetDirectXCommon(Irufemi::DirectXCommon* dx) {
        dx_ = dx;
    }
    /**
     * @brief SrvAllocator を設定する。
     * @param[in] pool 設定する SrvAllocator の値
     */
    static void SetSrvAllocator(Irufemi::DescriptorPool* pool) {
        srvPool_ = pool;
    }

private:
    struct SpriteInstance {
        Irufemi::Transform transform;
        Irufemi::Vector4 color;
        Irufemi::Vector2 anchor;
        Irufemi::Vector2 size;
    };

    struct InstanceData {
        Irufemi::Matrix4x4 WVP;
        Irufemi::Vector4 color;
    };

    /**
     * @brief 要求されたインスタンス数に応じてインスタンスバッファを生成または容量を拡張する。
     * @param[in] instanceCount 必要なインスタンス数
     */
    void CreateOrResizeInstanceBuffer(uint32_t instanceCount);
    /**
     * @brief 登録された各スプライトの WVP 行列とカラーを計算し、GPU インスタンスバッファへ書き込む。
     * @param[in] force true の場合はダーティフラグに関わらず強制再構築
     */
    void BuildInstanceBuffer(bool force = false);
    /**
     * @brief アンカーポイント設定に応じてスプライトの基準頂点座標オフセットを再計算・適用する。
     */
    void ApplyAnchorToVertices();

private:
    std::unique_ptr<Object2DResource> baseResource_ = nullptr;
    std::vector<SpriteInstance> instances_;

    uint32_t visibleInstanceCount_ = 0;
    bool instanceDirty_ = false;
    bool isTopMost_ = false;
    Irufemi::Vector2 textureSize_{0.0f, 0.0f};

    ID3D12PipelineState* customPSO_ = nullptr;
    std::string customPSOName_ = "";
    Irufemi::BlendMode customBlend_ = Irufemi::BlendMode::kBlendModeNormal;
    PSOManager::DepthWrite customDepth_ = PSOManager::DepthWrite::Off;
    PSOManager::CullMode customCull_ = PSOManager::CullMode::None;
    D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress_ = 0;

    // インスタンシング用バッファ
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, Irufemi::kMaxFramesInFlight> instanceBuffer_;
    std::array<InstanceData*, Irufemi::kMaxFramesInFlight> instanceData_{};
    std::array<uint32_t, Irufemi::kMaxFramesInFlight> instanceCapacity_{};
    std::array<uint32_t, Irufemi::kMaxFramesInFlight> instancingSrvIndex_{};
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, Irufemi::kMaxFramesInFlight> instancingSrvCPU_{};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, Irufemi::kMaxFramesInFlight> instancingSrvGPU_{};

    static Irufemi::TextureManager* textureManager_;
    static DrawManager* drawManager_;
    static CameraManager* cameraManager_;
    static Irufemi::DirectXCommon* dx_;
    static Irufemi::DescriptorPool* srvPool_;
};
