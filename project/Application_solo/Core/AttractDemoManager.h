#pragma once

#include "Core/System/IEngineExtension.h"
#include "Core/Math/Vector2.h"
#include <string>
#include <memory>
#include <Windows.h>

class IrufemiEngine;
class TitleSceneDirectorComponent;

namespace Irufemi {
class GameObject;
class TextRendererComponent;
class BaseScene;
} // namespace Irufemi

/**
 * @class AttractDemoManager
 * @brief アトラクトデモおよびF8キオスク固定展示ループを統括する単一エンジン拡張
 * @details シーン遷移を跨いで常駐し、以下の機能を一元管理します：
 *          - F8キーによるキオスク展示ループのトグル（Title / InGame どこからでも即時停止・通常復帰）
 *          - Titleシーン：無操作8秒放置での自律デモ、ベジェ曲線マウススイープ、UIホバー、GAME START出撃自動発火
 *          - InGameシーン：デモ出撃後の3.5秒レール飛行映像の実演およびTitleへの自動フェード帰還
 */
class AttractDemoManager : public Irufemi::IEngineExtension {
public:
    /**
     * @enum DemoMode
     * @brief 実演するデモの種類
     */
    enum class DemoMode {
        ClearShowcase, ///< 【クリア演出実演】出撃後ボス即時撃破・STAGE CLEARリザルト接続を実演ループ
        GameOverShowcase, ///< 【ゲームオーバー演出実演】出撃後2.5秒で特異点収縮・スーパーノヴァ大爆発・ResultScene接続を実演ループ
        TransitionShowcase, ///< 【画面遷移ショーケース】起動直後0.3秒で出撃し、重力の壁突入・突破を即座に実演ループ
        StandardAttract,    ///< 【通常アトラクト】ガレキスイープ・メニューホバーを経てから出撃
        ParticleShowcase    ///< 【パーティクル完全実演】タイトル流体星雲〜InGame全エフェクト連続展示ループ
    };

    /**
     * @struct Config
     * @brief アトラクトデモおよびキオスク展示の起動構成設定
     */
    struct Config {
        bool enabledOnStart = true;                                       ///< 起動時にデモプレイ（キオスクループ）を開始するか（デフォルト: ON）
        DemoMode defaultMode = DemoMode::ParticleShowcase;                 ///< 初期デモモード（パーティクル完全実演）
        bool enableDevHotkeys = true;                                     ///< F8（デモトグル）やF7（モード切替）等の開発キーを有効にするか
    };

    /**
     * @brief コンストラクタ
     * @param config アトラクトデモの初期構成設定
     */
    explicit AttractDemoManager(const Config& config = Config{});
    ~AttractDemoManager() override = default;

    void OnInitialize(IrufemiEngine* engine) override;
    void OnUpdate(float deltaTime) override;
    void OnFinalize() override;

    /**
     * @brief 起動構成設定を取得する
     */
    const Config& GetConfig() const {
        return config_;
    }

    /**
     * @brief 現在のデモモードを取得する
     */
    DemoMode GetDemoMode() const {
        return demoMode_;
    }

    /**
     * @brief デモモードを設定する
     */
    void SetDemoMode(DemoMode mode) {
        demoMode_ = mode;
    }

    /**
     * @brief デモモードを次のモードへ循環切り替えする
     */
    void CycleNextDemoMode() {
        switch (demoMode_) {
        case DemoMode::ClearShowcase:
            demoMode_ = DemoMode::GameOverShowcase;
            break;
        case DemoMode::GameOverShowcase:
            demoMode_ = DemoMode::TransitionShowcase;
            break;
        case DemoMode::TransitionShowcase:
            demoMode_ = DemoMode::StandardAttract;
            break;
        case DemoMode::StandardAttract:
            demoMode_ = DemoMode::ParticleShowcase;
            break;
        case DemoMode::ParticleShowcase:
        default:
            demoMode_ = DemoMode::ClearShowcase;
            break;
        }
    }

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
        return isKioskModeActive_;
    }

    /**
     * @brief デモ実演中（放置デモまたはキオスク展示中）かどうか
     */
    static bool IsAttractModeActive() {
        return isAttractModeActive_;
    }

    /**
     * @brief ParticleShowcase時の次回実演ターゲットがクリア実演（true）かゲームオーバー実演（false）か
     */
    static bool IsParticleShowcaseTargetClear() {
        return particleShowcaseTargetIsClearStatic_;
    }

    /**
     * @brief ParticleShowcaseの実演ターゲットを交互に反転
     */
    static void ToggleParticleShowcaseTarget() {
        particleShowcaseTargetIsClearStatic_ = !particleShowcaseTargetIsClearStatic_;
    }

private:
    static inline bool isKioskModeActive_ = false;
    static inline bool isAttractModeActive_ = false;
    static inline bool particleShowcaseTargetIsClearStatic_ = true;
    void UpdateF8Input(float deltaTime);
    void UpdateTitleScene(float deltaTime);
    void UpdateInGameScene(float deltaTime);
    void UpdateResultScene(float deltaTime);

    void StartTitleDemo();
    void StopTitleDemo();
    void StopAllDemo(bool returnToTitle);

    /**
     * @brief タイトル画面でのGAME START決定・出撃シーケンスを発火する
     */
    void TriggerDemoGameLaunch(Irufemi::BaseScene* currentScene, TitleSceneDirectorComponent* dirComp);

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
    std::string previousScene_ = "";                   ///< 前フレームのシーン名（遷移検知用）
    Config config_{};                                  ///< 起動時構成設定
    bool isKioskLoopMode_ = false;                     ///< 常時ループ展示フラグ（デフォルトOFF）
    DemoMode demoMode_ = DemoMode::TransitionShowcase; ///< 現在のデモモード（画面遷移ショーケースがデフォルト）
    bool isDemoPlaying_ = false;                       ///< Titleシーンでのデモタイムライン進行中フラグ
    bool isDemoInGame_ = false;                        ///< InGameシーンでの3.5秒帰還待ちフラグ
    bool hasSubmitted_ = false;                        ///< GAME STARTクリック決定済みフラグ
    bool hasTriggeredReturn_ = false;                  ///< InGameからTitleへの遷移開始フラグ

    bool wasF8Down_ = false;                             ///< 前フレームのF8キー押下状態（エッジ検知）
    bool wasF7Down_ = false;                             ///< 前フレームのF7キー押下状態（エッジ検知）
    bool wasF6Down_ = false;                             ///< 前フレームのF6キー押下状態（デバッグ用即時撃破）
    bool wasF5Down_ = false;                             ///< 前フレームのF5キー押下状態（即時パーティクルデモテスト）
    float f8CooldownTimer_ = 0.0f;                       ///< 多重発火防止用クールダウンタイマー（秒）
    static constexpr float kF8CooldownDuration_ = 0.30f; ///< クールダウン時間（300ms）

    float idleTimer_ = 0.0f;    ///< Titleでの無操作タイマー
    float demoTimeline_ = 0.0f; ///< Titleデモタイムライン（秒）
    float inGameTimer_ = 0.0f;  ///< InGameデモタイマー（秒）
    float combatTimer_ = 0.0f;  ///< イントロ完了後の通常交戦タイマー（秒）
    float resultTimer_ = 0.0f;  ///< Resultデモタイマー（秒）
    int particleShowcasePhase_ = 0; ///< パーティクル実演フェーズステップ [0〜5]
    bool particleShowcaseTargetIsClear_ = true; ///< ParticleShowcase時の実演ターゲット（true: ボス撃破クリア実演 / false: 自機撃墜ゲームオーバー実演）
    float demoCombatActionTimer_ = 0.0f;       ///< デモ自律戦闘アクション（引き寄せ・投擲）用タイマー

    static constexpr float kIdleThreshold_ = 8.0f;               ///< 自動デモ開始までの無操作秒数
    static constexpr float kInGameDuration_ = 9.00f;             ///< 本編デモ映像の表示秒数（じっくり魅せる9.0秒）
    static constexpr float kBossCombatShowcaseDuration_ = 5.00f; ///< ボス対峙・通常交戦の展示秒数（5.0秒）
    static constexpr uint8_t kToggleKey_ = VK_F8;                ///< キオスク固定展示トグルキー (F8)

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
    std::shared_ptr<Irufemi::GameObject> demoHudObj_;
    Irufemi::TextRendererComponent* demoHudText_ = nullptr;
    float demoHudBlinkTimer_ = 0.0f;
};

using AttractDemoConfig = AttractDemoManager::Config;
