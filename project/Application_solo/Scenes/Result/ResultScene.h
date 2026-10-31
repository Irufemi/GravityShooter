#pragma once
#include "Framework/Scene/BaseScene.h"

/**
 * @brief ゲームクリアおよびゲームオーバー時の結果表示シーン
 * @details SceneManager の PushScene() を利用してインゲームシーンの上に被せる（オーバーレイする）設計。
 * IsUpdateBlocking = false, IsDrawBlocking = false
 * とすることで、背景でゲーム世界が等倍速で動き続ける余韻演出を実現します。
 */
class ResultScene : public BaseScene {
public:
    ResultScene() = default;
    ~ResultScene() override;
    void OnExit() override;
    bool IsClearResult() const {
        return isClearResult_;
    }

    void Initialize(IrufemiEngine* engine) override;

    // --- シーンスタック・オーバーレイの設定 ---
    // 下のシーン（GameScene）を等倍速で動かし続けるため、UpdateBlockingはfalse
    bool IsUpdateBlocking() const override {
        return false;
    }
    // 下のシーン（GameScene）を描画し続けるため、DrawBlockingはfalse
    bool IsDrawBlocking() const override {
        return false;
    }
    // 音楽は止めない（または余韻を残す）ため、AudioBlockingもfalse
    bool IsAudioBlocking() const override {
        return false;
    }

    // --- クリア結果受け渡し用のフラグ ---
    static void SetIsClear(bool isClear) {
        s_isClearPending_ = isClear;
    }
    static bool IsClear() {
        return s_currentSessionClear_;
    }

private:
    inline static bool s_isClearPending_ = false;
    inline static bool s_currentSessionClear_ = false;
    bool isClearResult_ = false;
};
