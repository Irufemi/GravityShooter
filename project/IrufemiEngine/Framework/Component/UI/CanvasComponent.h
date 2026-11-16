#pragma once
#include "Framework/Component/Component.h"
#include <vector>

namespace Irufemi {

/**
 * @class CanvasComponent
 * @brief UI要素をグループ化し、一括で透明度を操作するキャンバスコンポーネント
 */
class CanvasComponent : public Component {
public:
    CanvasComponent() = default;
    ~CanvasComponent() override = default;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize() override;
    /**
     * @brief Update を実行する。
     */
    void Update() override;

    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "CanvasComponent";
    }
    /**
     * @brief OnRegisterProperties を実行する。
     */
    void OnRegisterProperties() override;

    /**
     * @brief グループ全体のアルファ値を設定します
     * @param[in] alpha 設定するアルファ値 (0.0f〜1.0f)
     */
    void SetGroupAlpha(float alpha) {
        groupAlpha_ = alpha;
    }

    /**
     * @brief グループ全体のアルファ値を取得します
     * @return 現在のグループアルファ値
     */
    float GetGroupAlpha() const {
        return groupAlpha_;
    }

private:
    float groupAlpha_ = 1.0f;        // グループ全体のアルファ値
    float lastAppliedAlpha_ = -1.0f; // 前回適用したアルファ値（ダーティ判定用）
};

} // namespace Irufemi
