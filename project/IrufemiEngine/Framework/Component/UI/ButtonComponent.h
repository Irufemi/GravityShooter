#pragma once
#include "Framework/Component/Component.h"
#include <string>
#include <functional>
#include "Core/Math/Vector4.h"
#include "Framework/UI/UIAnimator.h"

class TransformComponent;
class SpriteRendererComponent;
class TextRendererComponent;

/**
 * @class ButtonComponent
 * @brief スプライト画像またはテキストメッシュのホバー・クリックを自動判定する汎用UIコンポーネント
 * @details 仮想カーソル（VirtualCursor）およびマウス入力の両対応、アンカー考慮のバウンディングボックス算出、
 *          ホバー・押下アニメーション、およびクリック完了コールバックを提供します。
 */
class ButtonComponent : public Component {
public:
    ButtonComponent() = default;
    ~ButtonComponent() override = default;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize() override;
    void OnAwake() override;
    /**
     * @brief Update を実行する。
     */
    void Update() override;

    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "ButtonComponent";
    }
    /**
     * @brief OnRegisterProperties を実行する。
     */
    void OnRegisterProperties() override;

    /**
     * @brief IsHovered かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsHovered() const {
        return isHovered_;
    }
    /**
     * @brief IsClicked かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsClicked() const {
        return isClicked_;
    }

    /**
     * @brief クリック完了時に呼び出されるコールバック関数を設定する
     * @param[in] callback 実行するコールバック関数
     */
    void SetOnClickCallback(std::function<void()> callback) {
        onClickCallback_ = std::move(callback);
    }

    /**
     * @brief 指定したUI座標がボタンの当たり判定内にあるか判定
     */
    bool CheckBounds(const Irufemi::Vector2& mousePos);

private:
    Irufemi::Vector4 normalColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
    Irufemi::Vector4 hoverColor_ = {0.8f, 0.8f, 0.8f, 1.0f};
    Irufemi::Vector4 clickColor_{0.5f, 0.5f, 0.5f, 1.0f};

    bool enableHoverPulse_ = true; // ホバー時にサイン波で明滅するかどうか
    bool enableIdlePulse_ = true;  // 待機中（ホバーしていない時）も明滅するかどうか

    // 当たり判定のスケール調整（画像自体の余白などを省くため）
    Irufemi::Vector2 hitboxScale_{1.0f, 1.0f};

    // --- 内部状態 ---
    UIAnimator animator_;

    bool isHovered_ = false;
    bool isClicked_ = false;         // クリックされた瞬間
    bool isPressedOnButton_ = false; // ボタン上で押下中かどうか
    SpriteRendererComponent* sprite_ = nullptr;
    TextRendererComponent* text_ = nullptr;
    std::function<void()> onClickCallback_;
};
