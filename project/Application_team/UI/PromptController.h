#pragma once
#include <cstdint>
#include "Framework/UI/UIAnimator.h"

// 前方宣言
class StaticModelObject;
class Sprite;
class InputManager;

/**
 * @class PromptController
 * @brief 「Push to Space」などの単一キー入力待ちUIを管理するコントローラー
 * @details 指定された StaticModelObject または Sprite のアルファ値と描画を代行し、
 *          決定入力後のフラッシュ演出と遷移遅延を管理します。
 */
class PromptController {
public:
    PromptController();
    ~PromptController() = default;

    /**
     * @brief 対象とする StaticModelObject を設定する
     */
    void SetTarget(StaticModelObject* targetObj);

    /**
     * @brief 対象とする Sprite を設定する
     */
    void SetTarget(Sprite* targetSprite);

    /**
     * @brief 決定のトリガーとなるキーコード（VK_SPACE 等）を設定する
     * @param key 仮想キーコード (デフォルト: VK_SPACE)
     */
    void SetTriggerKey(uint8_t key) {
        triggerKey_ = key;
    }

    /**
     * @brief 更新処理（アニメーション進行、キー入力判定）
     * @param input InputManagerへのポインタ
     * @param dt デルタ時間（秒、デフォルト: 1/60秒）
     */
    void Update(InputManager* input, float dt = 1.0f / 60.0f);

    /**
     * @brief 描画処理（対象オブジェクトの Draw を呼び出す。フラッシュ時のスキップも行う）
     */
    void Draw();

    /**
     * @brief 遷移すべきタイミング（決定後、ディレイが経過したか）を返す
     * @return 遷移すべきなら true
     */
    bool ShouldTransition() const;

    /**
     * @brief 決定入力がされたかどうかを返す
     */
    bool IsDecided() const {
        return isDecided_;
    }

private:
    StaticModelObject* targetObj_ = nullptr;
    Sprite* targetSprite_ = nullptr;

    UIAnimator animator_;
    uint8_t triggerKey_ = 0x20; // VK_SPACE のデフォルト値 (0x20)

    bool isDecided_ = false;
    float transitionDelayTimer_ = 0.0f;
    bool isVisible_ = true; // 描画フラグ（フラッシュ用）

    // 定数
    static constexpr float kTransitionDelayLimit = 0.8f;
};
