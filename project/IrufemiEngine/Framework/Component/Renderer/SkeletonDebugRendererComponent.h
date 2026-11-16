#pragma once

#include "Framework/Component/Component.h"
#include <memory>
#include <string>

namespace Irufemi {

class PrimitiveBatch;
class Line3DBatch;

/**
 * @class SkeletonDebugRendererComponent
 * @brief スケルトンのデバッグ表示を行うコンポーネント。
 * @details 同じGameObjectにアタッチされている SkinnedMeshRendererComponent から
 *          SkeletonPose を取得し、ジョイント間の接続（八面体）やローカルXYZ軸（線分）を描画します。
 */
class SkeletonDebugRendererComponent : public Component {
public:
    SkeletonDebugRendererComponent();
    ~SkeletonDebugRendererComponent() override;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize() override;
    /**
     * @brief 生成時の自己完結初期化（ボーンメッシュ・軸描画バッチの生成）を行います
     */
    void OnAwake() override;
    /**
     * @brief Update を実行する。
     */
    void Update() override;
    /**
     * @brief Draw を実行する。
     */
    void Draw() override;
    /**
     * @brief CanUpdateInEditMode かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool CanUpdateInEditMode() const override {
        return true;
    }

    /**
     * @brief OnRegisterProperties を実行する。
     */
    void OnRegisterProperties() override;
    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "SkeletonDebugRendererComponent";
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
     * @brief ShowBones を設定する。
     * @param[in] show 設定する ShowBones の値
     */
    void SetShowBones(bool show) {
        showBones_ = show;
    }
    /**
     * @brief ShowAxes を設定する。
     * @param[in] show 設定する ShowAxes の値
     */
    void SetShowAxes(bool show) {
        showAxes_ = show;
    }

private:
    std::unique_ptr<Irufemi::PrimitiveBatch> boneMeshes_;
    std::unique_ptr<Irufemi::Line3DBatch> debugAxesLines_;

    bool showBones_ = true;
    bool showAxes_ = true;
    float axisScale_ = 1.0f;
};

} // namespace Irufemi
