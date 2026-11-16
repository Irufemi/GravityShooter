#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"

namespace Irufemi {

/**
 * @class SplineNodeComponent
 * @brief スプラインの制御点を視覚化し、エディタ上で選択可能にするためのコンポーネント
 */
class SplineNodeComponent : public Component {
public:
    SplineNodeComponent() = default;
    ~SplineNodeComponent() override = default;

    /**
     * @brief OnRegisterProperties を実行する。
     */
    void OnRegisterProperties() override;
    /**
     * @brief Draw を実行する。
     */
    void Draw() override;
    /**
     * @brief Raycast を実行する。
     */
    bool Raycast(const Irufemi::Ray& ray, float& outDistance) const override;

    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "SplineNodeComponent";
    }

private:
    float radius_ = 0.5f;                               ///< 球の半径
    Irufemi::Vector4 color_ = {0.0f, 1.0f, 1.0f, 1.0f}; ///< 球の色 (Cyan)
    bool drawDebug_ = true;                             ///< デバッグ描画を行うかどうか
};

} // namespace Irufemi
