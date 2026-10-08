#pragma once

#include "Core/System/IEngineExtension.h"
#include "Core/Math/Vector2.h"
#include <string>
#include <memory>
#include <Windows.h>

class IrufemiEngine;

/**
 * @class AttractDemoManager
 * @brief アトラクトデモおよびF8キオスク固定展示ループを統括する単一エンジン拡張
 * @details シーン遷移を跨いで常駐し、以下の機能を一元管理します：
 *          - F8キーによるキオスク展示ループのトグル（Title / InGame どこからでも即時停止・通常復帰）
 *          - Titleシーン：無操作8秒放置での自律デモ、ベジェ曲線マウススイープ、UIホバー、GAME START出撃自動発火
 *          - InGameシーン：デモ出撃後の3.5秒レール飛行映像の実演およびTitleへの自動フェード帰還
 */
class AttractDemoManager : public IEngineExtension {
public:
    AttractDemoManager() = default;
    ~AttractDemoManager() override = default;

    void OnInitialize(IrufemiEngine* engine) override;
    void OnUpdate(float deltaTime) override;
    void OnFinalize() override;

    /**
     * @brief キオスク固定展示ループモード中かどうか
     */
    bool IsKioskLoopMode() const {
        return isKioskLoopMode_;
    }

    /**
     * @brief 現在デモシーケンス進行中かどうか
     */
    bool IsDemoActive() const {
        return isDemoPlaying_ || isDemoInGame_;
    }

    /**
     * @brief キオスク固定展示モード（操作入力完全遮断）が有効かどうか
     */
    static bool IsKioskModeActive() {
        return s_isKioskModeActive_;
    }

    /**
     * @brief デモ実演中（放置デモまたはキオスク展示中）かどうか
     */
    static bool IsAttractModeActive() {
        return s_isAttractModeActive_;
    }

private:
    static inline bool s_isKioskModeActive_ = false;
    static inline bool s_isAttractModeActive_ = false;
    void UpdateF8Input(float deltaTime);
    void UpdateTitleScene(float deltaTime);
    void UpdateInGameScene(float deltaTime);

    void StartTitleDemo();
    void StopTitleDemo();
    void StopAllDemo(bool returnToTitle);

    Irufemi::Vector2 GetButtonCenter(const std::string& btnName, const Irufemi::Vector2& fallbackPos) const;

    /**
     * @brief デモ案内HUD（AUTO DEMO / DEMO LOOP）の動的描画・点滅更新
     */
    void UpdateDemoHud(float deltaTime);

    /**
     * @brief デモ案内HUDの破棄・クリーンアップ
     */
    void CleanupDemoHud();

private:
    IrufemiEngine* engine_ = nullptr;

    // --- 状態管理 ---
    std::string previousScene_ = "";  ///< 前フレームのシーン名（遷移検知用）
    bool isKioskLoopMode_ = false;    ///< F8による永久ループ展示フラグ
    bool isDemoPlaying_ = false;      ///< Titleシーンでのデモタイムライン進行中フラグ
    bool isDemoInGame_ = false;       ///< InGameシーンでの3.5秒帰還待ちフラグ
    bool hasSubmitted_ = false;       ///< GAME STARTクリック決定済みフラグ
    bool hasTriggeredReturn_ = false; ///< InGameからTitleへの遷移開始フラグ

    bool wasF8Down_ = false;       ///< 前フレームのF8キー押下状態（エッジ検知）
    float f8CooldownTimer_ = 0.0f; ///< 多重発火防止用クールダウンタイマー（秒）
    static constexpr float kF8CooldownDuration_ = 0.30f; ///< クールダウン時間（300ms）

    float idleTimer_ = 0.0f;    ///< Titleでの無操作タイマー
    float demoTimeline_ = 0.0f; ///< Titleデモタイムライン（秒）
    float inGameTimer_ = 0.0f;  ///< InGameデモタイマー（秒）

    static constexpr float kIdleThreshold_ = 8.0f; ///< 自動デモ開始までの無操作秒数
    static constexpr float kInGameDuration_ = 5.50f; ///< 本編デモ映像の表示秒数（じっくり魅せる5.5秒）
    static constexpr uint8_t kToggleKey_ = VK_F8; ///< キオスク固定展示トグルキー (F8)

    // タイムライン用ターゲット座標キャッシュ
    Irufemi::Vector2 posSweepStart_{640.0f, 650.0f};
    Irufemi::Vector2 posSweepRight_{820.0f, 310.0f};
    Irufemi::Vector2 posSweepCenter_{640.0f, 210.0f};
    Irufemi::Vector2 posSweepLeft_{460.0f, 330.0f};
    Irufemi::Vector2 posHowToPlay_{640.0f, 490.0f};
    Irufemi::Vector2 posOptions_{640.0f, 560.0f};
    Irufemi::Vector2 posStart_{640.0f, 420.0f};
    bool hasTriggeredDemoPulse_ = false; ///< デモ中の重力光彩パルス発火済みフラグ

    // --- デモ案内HUD表示用 ---
    std::shared_ptr<class GameObject> demoHudObj_;
    class TextRendererComponent* demoHudText_ = nullptr;
    float demoHudBlinkTimer_ = 0.0f;
};
