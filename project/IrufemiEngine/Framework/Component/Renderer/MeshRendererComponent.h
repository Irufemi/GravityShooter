#pragma once
#include "Framework/Component/Component.h"
#include <memory>
#include <string>
#include <d3d12.h>
#include "Core/Shape/Sphere.h"
#include "Core/Type/BlendMode.h"
#include "Renderer/Pipeline/PSOManager.h"

namespace Irufemi {

class StaticModelObject;
class TransformComponent;

class MeshRendererComponent : public Component {
public:
    MeshRendererComponent();
    ~MeshRendererComponent() override;

    // 初期化時にモデルファイル名を指定
    /**
     * @brief LoadModel を実行する。
     */
    void LoadModel(const std::string& filename);

    /**
     * @brief Initialize を実行する。
     */
    void Initialize() override;
    /**
     * @brief 生成時の自己完結初期化（StaticModelObjectの生成・モデル初期化）を行います
     */
    void OnAwake() override;
    /**
     * @brief ワールド座標・Transform確定時の描画ステート同期を行います
     */
    void OnSpawned() override;
    /**
     * @brief Update を実行する。
     */
    void Update() override;
    /**
     * @brief Draw を実行する。
     */
    void Draw() override;
    void SyncRenderState() override;

    /**
     * @brief CanUpdateInEditMode かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool CanUpdateInEditMode() const override {
        return true;
    }

    /**
     * @brief EnableEffectMask を設定する。
     * @param[in] enable 設定する EnableEffectMask の値
     */
    void SetEnableEffectMask(bool enable);
    /**
     * @brief CustomEffectType を設定する。
     * @param[in] type 設定する CustomEffectType の値
     */
    void SetCustomEffectType(int32_t type);
    /**
     * @brief CustomEffectParam を設定する。
     * @param[in] param 設定する CustomEffectParam の値
     */
    void SetCustomEffectParam(float param);

    /**
     * @brief CustomPSO を設定する（名前指定）
     * @param psoName PSO名
     * @param blend ブレンドモード
     * @param depth 深度設定
     * @param cull カリングモード
     */
    void SetCustomPSO(const std::string& psoName, Irufemi::BlendMode blend = Irufemi::BlendMode::kBlendModeNormal,
                      PSOManager::DepthWrite depth = PSOManager::DepthWrite::Enable,
                      PSOManager::CullMode cull = PSOManager::CullMode::Back);

    /**
     * @brief 汎用カスタム定数バッファ（RootSlot::Special = b6）のGPU仮想アドレスを設定する
     * @param addr GPU仮想アドレス
     */
    void SetCustomCBVAddress(D3D12_GPU_VIRTUAL_ADDRESS addr);

    /**
     * @brief EnableLighting を設定する。
     * @param[in] enable 設定する EnableLighting の値（falseでUnlitモード）
     */
    void SetEnableLighting(bool enable);
    /**
     * @brief EnableLighting を取得する。
     * @return 取得された EnableLighting
     */
    bool GetEnableLighting() const {
        return enableLighting_;
    }

    /**
     * @brief Skydome モードを設定する（最奥深度固定＋前面カリング＋深度書き込み無効）
     * @param[in] isSkydome 設定する値
     */
    void SetIsSkydome(bool isSkydome);
    /**
     * @brief Skydome モードかどうかを取得する
     * @return 判定結果
     */
    bool GetIsSkydome() const {
        return isSkydome_;
    }

    /**
     * @brief Visible を設定する。
     * @param[in] visible 設定する Visible の値
     */
    void SetVisible(bool visible) {
        isVisible_ = visible;
    }
    /**
     * @brief IsVisible かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsVisible() const {
        return isVisible_;
    }

    /**
     * @brief Renderable を取得する。
     * @return 取得された Renderable
     */
    Irufemi::IRenderable* GetRenderable() override;

    /**
     * @brief プロパティ登録
     */
    void OnRegisterProperties() override;

    // エディタのRaycast用
    /**
     * @brief WorldSphere を取得する。
     * @return 取得された WorldSphere
     */
    Irufemi::Sphere GetWorldSphere() const;
    /**
     * @brief Raycast を実行する。
     */
    bool Raycast(const Irufemi::Ray& ray, float& outDistance) const override;

    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "MeshRendererComponent";
    }
    /**
     * @brief Serialize を実行する。
     */
    nlohmann::json Serialize() override;
    /**
     * @brief Deserialize を実行する。
     */
    void Deserialize(const nlohmann::json& j) override;

    /**
     * @brief クローンを作成する
     */
    std::shared_ptr<Component> Clone() override;

    /**
     * @brief 現在読み込まれているモデル名を取得します。
     * @return モデル名
     */
    const std::string& GetModelName() const {
        return modelName_;
    }

private:
    std::unique_ptr<Irufemi::StaticModelObject> obj_; ///< 実際の描画を担う既存クラス
    std::string modelName_ = "";                      ///< 読み込むモデル名
    std::string currentLoadedFilename_ = "";          ///< 現在ロードされているモデル名（動的変更検知用）
    bool enableLighting_ = true;                      ///< ライティング有効フラグ（falseでUnlit自己発光モード）
    bool isSkydome_ = false; ///< Skydomeモードフラグ（最奥深度固定・前面カリング・深度書き込み無効）
    bool castShadows_ = true;
    bool isVisible_ = true;
};

} // namespace Irufemi
