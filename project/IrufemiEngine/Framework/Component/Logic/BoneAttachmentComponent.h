#pragma once
#include "Framework/Component/Component.h"
#include <string>

namespace Irufemi {

/**
 * @class BoneAttachmentComponent
 * @brief モデルの特定のボーン（関節）に追従するコンポーネント
 * @details 武器やアクセサリーなどをキャラクターの手に持たせる際、対象ボーンのワールド行列に同期して移動・回転させます。
 */
class BoneAttachmentComponent : public Component {
public:
    /**
     * @brief コンストラクタ
     */
    BoneAttachmentComponent();

    /**
     * @brief デストラクタ
     */
    ~BoneAttachmentComponent() override;

    /**
     * @brief コンポーネントの初期化処理
     * @details 追従対象のGameObjectやボーン名の初期解決を試みます。
     */
    void Initialize() override;

    /**
     * @brief 毎フレームの更新処理
     * @details ターゲットオブジェクトの指定されたボーンのワールド行列を取得し、自身のTransformに同期させます。
     */
    void Update() override;

    /**
     * @brief インスペクタ用プロパティの登録
     * @details エディタから targetName_ や targetBoneName_ を編集できるように登録します。
     */
    void OnRegisterProperties() override;

    /**
     * @brief コンポーネント名の取得
     * @return "BoneAttachmentComponent"
     */
    std::string GetComponentName() const override {
        return "BoneAttachmentComponent";
    }

    /**
     * @brief コンポーネントの状態をJSON形式にシリアライズする
     * @return 追従対象の名前やボーン名を含んだJSONオブジェクト
     */
    nlohmann::json Serialize() override;

    /**
     * @brief JSONデータからコンポーネントの状態をデシリアライズする
     * @param[in] j 読み込むJSONオブジェクト
     */
    void Deserialize(const nlohmann::json& j) override;

    /**
     * @brief 追従対象となるGameObjectの名前を設定する
     * @param[in] targetName 対象のGameObject名
     */
    void SetTargetName(const std::string& targetName) {
        if (targetName_ != targetName) {
            targetName_ = targetName;
            InvalidateCache();
        }
    }

    /**
     * @brief 追従対象となるボーン（関節）名を設定する
     * @param[in] boneName 対象のボーン名（例: "RightHand"）
     */
    void SetTargetBoneName(const std::string& boneName) {
        if (targetBoneName_ != boneName) {
            targetBoneName_ = boneName;
            InvalidateCache();
        }
    }

    /**
     * @brief 解決済みボーンおよびオブジェクトのキャッシュを無効化する
     */
    void InvalidateCache() {
        cachedTargetObj_.reset();
        cachedRenderer_ = nullptr;
        cachedTargetTransform_ = nullptr;
        cachedBoneIndex_ = -1;
    }

private:
    std::string targetName_ = "";
    std::string targetBoneName_ = "";

    std::weak_ptr<GameObject> cachedTargetObj_;
    class SkinnedMeshRendererComponent* cachedRenderer_ = nullptr;
    class TransformComponent* cachedTargetTransform_ = nullptr;
    int cachedBoneIndex_ = -1;
};

} // namespace Irufemi
