#pragma once
#include "Framework/Component/Component.h"
#include <string>

namespace Irufemi {

/**
 * @enum TimeoutAction
 * @brief 寿命を迎えた際のオブジェクトの振る舞い
 */
enum class TimeoutAction : int {
    Destroy = 0, ///< 完全にオブジェクトを破棄する
    Disable = 1  ///< オブジェクトを非アクティブ（isActive_ = false）にする（プール用）
};

/**
 * @class LifetimeComponent
 * @brief 指定した時間が経過するとオブジェクトをDestroyまたはDisableするコンポーネント
 */
class LifetimeComponent : public Component {
public:
    LifetimeComponent() = default;
    ~LifetimeComponent() override = default;

    /**
     * @brief OnRegisterProperties を実行する。
     */
    void OnRegisterProperties() override;
    /**
     * @brief Initialize を実行する。
     */
    void Initialize() override;
    /**
     * @brief スポーン時にタイマーをリセットします
     */
    void OnSpawned() override;
    /**
     * @brief 有効化時にタイマーをリセットします（オブジェクトプール再利用対応）
     */
    void OnEnable() override;
    /**
     * @brief Update を実行する。
     */
    void Update() override;

    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "LifetimeComponent";
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
     * @brief LifeTime を設定する。
     * @param[in] lifeTime 設定する LifeTime の値
     */
    void SetLifeTime(float lifeTime) {
        lifeTime_ = lifeTime;
    }
    /**
     * @brief LifeTime を取得する。
     * @return 取得された LifeTime
     */
    float GetLifeTime() const {
        return lifeTime_;
    }

    /**
     * @brief TimeoutAction を設定する。
     * @param[in] action 設定する TimeoutAction の値
     */
    void SetTimeoutAction(TimeoutAction action) {
        timeoutAction_ = action;
        timeoutActionInt_ = static_cast<int>(action);
    }
    /**
     * @brief TimeoutAction を取得する。
     * @return 取得された TimeoutAction
     */
    TimeoutAction GetTimeoutAction() const {
        return timeoutAction_;
    }

private:
    float lifeTime_ = 1.0f;        // 寿命（秒）
    float currentLifeTime_ = 0.0f; // 経過時間
    TimeoutAction timeoutAction_ = TimeoutAction::Destroy;
    int timeoutActionInt_ = 0; // プロパティ登録用の安全な整数メンバ
};

} // namespace Irufemi
