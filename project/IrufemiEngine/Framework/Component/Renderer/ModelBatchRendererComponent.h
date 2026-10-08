#pragma once
#include "Framework/Component/Component.h"
#include <memory>
#include <string>
#include "Core/Shape/Sphere.h"
#include "Core/Math/Math.h"
#include "Core/Math/Transform.h"

// 前方宣言
class ModelBatch;
class TransformComponent;

/**
 * @class ModelBatchRendererComponent
 * @brief インスタンシング（バッチ）描画を行うためのレンダラーコンポーネント
 * @details 同一3Dモデルの複数インスタンスを一括でGPU描画（バッチ描画）します。
 *          デフォルトで描画後にインスタンスを自動クリアするため、毎フレーム AddInstance() で登録して使用します。
 */
class ModelBatchRendererComponent : public Component {
public:
    ModelBatchRendererComponent();
    ~ModelBatchRendererComponent() override;

    /**
     * @brief 描画するモデルファイル（.obj 等）を指定して読み込みます
     * @param[in] filename モデルファイル名
     */
    void LoadModel(const std::string& filename);

    /**
     * @brief 初期化処理を実行します
     */
    void Initialize() override;

    /**
     * @brief 生成時の自己完結初期化（ModelBatchインスタンスの生成とモデル読み込み）を行います
     */
    void OnAwake() override;

    /**
     * @brief 毎フレームの更新処理を実行します
     */
    void Update() override;

    /**
     * @brief 登録された全インスタンスをバッチ描画します（autoClearEveryFrame が有効な場合は描画後にクリアします）
     */
    void Draw() override;

    /**
     * @brief エディタモード中も更新を行うかを判定します
     * @return 常に true
     */
    bool CanUpdateInEditMode() const override {
        return true;
    }

    /**
     * @brief 描画可能な内部オブジェクト（ModelBatch）へのポインタを取得します
     * @return IRenderable インターフェースポインタ
     */
    IRenderable* GetRenderable() override;

    // エディタのRaycast用
    /**
     * @brief エディタピッキング用のワールドバウンディングスフィアを取得します
     * @return バウンディングスフィア
     */
    Irufemi::Sphere GetWorldSphere() const;

    /**
     * @brief エディタピッキング用のレイキャスト判定を行います
     * @param[in] ray 判定用レイ
     * @param[out] outDistance ヒット時の距離
     * @return ヒットした場合は true
     */
    bool Raycast(const Irufemi::Ray& ray, float& outDistance) const override;

    /**
     * @brief コンポーネントの識別名を取得します
     * @return クラス名文字列
     */
    std::string GetComponentName() const override {
        return "ModelBatchRendererComponent";
    }

    /**
     * @brief コンポーネントの状態を JSON にシリアライズします
     * @return シリアライズされた JSON オブジェクト
     */
    nlohmann::json Serialize() override;

    /**
     * @brief JSON からコンポーネントの状態を復元します
     * @param[in] j 読み込む JSON オブジェクト
     */
    void Deserialize(const nlohmann::json& j) override;

    /**
     * @brief 現在読み込まれているモデル名を取得します
     * @return モデル名文字列
     */
    const std::string& GetModelName() const {
        return modelName_;
    }

    /**
     * @brief バッチ描画するインスタンスを追加します
     * @param[in] t インスタンスのローカルトランスフォーム
     * @param[in] effectType 適用するエフェクトタイプ（シェーダー側で解釈）
     * @param[in] effectParam エフェクトパラメータ
     * @param[in] enableMask マスク描画を有効にするか
     */
    void AddInstance(const Irufemi::Transform& t, int32_t effectType = 0, float effectParam = 0.0f,
                     bool enableMask = false);

    /**
     * @brief ワールド行列を直接指定してインスタンスを追加します
     * @param[in] world ワールド変換行列
     * @param[in] effectType 適用するエフェクトタイプ
     * @param[in] effectParam エフェクトパラメータ
     * @param[in] enableMask マスク描画を有効にするか
     */
    void AddInstanceWorld(const Irufemi::Matrix4x4& world, int32_t effectType = 0, float effectParam = 0.0f,
                          bool enableMask = false);

    /**
     * @brief 登録されたインスタンスをすべてクリアします
     */
    void ClearInstances();

    /**
     * @brief GPUフラスタムカリングを有効にするか設定します
     * @param[in] use 有効にする場合は true
     */
    void SetUseGPUCulling(bool use);

    /**
     * @brief 描画後にインスタンスを自動クリアするかどうかを設定します
     * @param[in] enable 自動クリアする場合は true（デフォルト: true）
     */
    void SetAutoClearEveryFrame(bool enable) {
        autoClearEveryFrame_ = enable;
    }

    /**
     * @brief 描画後の自動クリアが有効かどうかを取得します
     * @return 自動クリアが有効な場合は true
     */
    bool IsAutoClearEveryFrame() const {
        return autoClearEveryFrame_;
    }



private:
    std::unique_ptr<ModelBatch> batch_;   ///< 実際のバッチ描画を担うクラス
    std::string modelName_ = "plane.obj"; ///< 読み込むモデル名
    bool useGPUCulling_ = false;          ///< GPUカリングの有効フラグ
    bool autoClearEveryFrame_ = true;     ///< 描画後にインスタンスを自動クリアするかどうか
};
