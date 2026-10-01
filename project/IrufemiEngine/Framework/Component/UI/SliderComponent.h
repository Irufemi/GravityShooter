#pragma once
#include "Framework/Component/Component.h"
#include <string>
#include <functional>
#include <memory>
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector4.h"

class SpriteRendererComponent;
class GameObject;

/**
 * @class SliderComponent
 * @brief マウスドラッグで数値を変更できるスライダーUIコンポーネント
 */
class SliderComponent : public Component {
public:
    SliderComponent() = default;
    ~SliderComponent() override = default;

    void Initialize() override;
    void OnAwake() override;
    void Start() override;
    void Update() override;

    std::string GetComponentName() const override {
        return "SliderComponent";
    }
    void OnRegisterProperties() override;

    /**
     * @brief スライダーの現在値 (0.0 ~ 1.0) を取得
     */
    float GetValue() const {
        return value_;
    }

    /**
     * @brief スライダーの現在値 (0.0 ~ 1.0) を設定し、UIのツマミ位置も同期する
     */
    void SetValue(float value);

    /**
     * @brief 値が変更されたときに呼ばれるコールバックを設定
     */
    void SetOnValueChangedCallback(std::function<void(float)> callback) {
        onValueChangedCallback_ = std::move(callback);
    }

    /**
     * @brief ハンドル（ツマミ）となるGameObjectのIDを設定
     */
    void SetHandleObjectID(uint64_t id);

    /**
     * @brief 指定したUI座標がスライダーの当たり判定内にあるか判定
     */
    bool CheckBounds(const Irufemi::Vector2& mousePos);

private:
    void UpdateHandlePosition();
    void ResolveHandleObject();

    SpriteRendererComponent* backgroundSprite_ = nullptr;
    std::weak_ptr<GameObject> handleObject_;

    uint64_t handleObjectID_ = 0;              // Prefabからの復元用
    float value_ = 1.0f;                       // 0.0 ~ 1.0 の割合
    bool isDragging_ = false;                  // ドラッグ中か
    Irufemi::Vector2 hitboxScale_{1.0f, 1.0f}; // 当たり判定のスケール

    std::function<void(float)> onValueChangedCallback_;
};
