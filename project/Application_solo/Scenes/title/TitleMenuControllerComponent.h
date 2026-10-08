#pragma once

#include "Framework/Component/Component.h"
#include "Core/Math/Vector2.h"
#include "Scenes/title/State/ITitleMenuState.h"
#include <memory>
#include <vector>
#include <string>

/**
 * @class TitleMenuControllerComponent
 * @brief タイトル画面のメニュー選択・ゲームパッド/キーボード操作・モーダルを統括するコンポーネント
 * @details State パターン（有限状態機械: FSM）に基づき、通常待機、モーダル決定演出、
 *          出撃シーケンス、背面待機を完全に分離して管理します。
 */
class TitleMenuControllerComponent : public Component {
public: // 状態定義
    using MenuState = TitleMenuStateType;

public: // メンバ関数(システム)
    TitleMenuControllerComponent() = default;
    ~TitleMenuControllerComponent() override = default;

    /**
     * @brief 初期化処理
     */
    void Initialize() override;

    /**
     * @brief 毎フレーム更新処理
     */
    void Update() override;

    /**
     * @brief コンポーネント名を取得する
     * @return コンポーネント名
     */
    std::string GetComponentName() const override {
        return "TitleMenuControllerComponent";
    }

    /**
     * @brief エディタ用プロパティ登録
     */
    void OnRegisterProperties() override;

    /**
     * @brief 現在のメニュー状態を取得する
     */
    MenuState GetState() const;

    /**
     * @brief 状態を明示的に切り替える（State Pattern）
     * @param[in] newState 遷移先のステートインスタンス
     */
    void ChangeState(std::unique_ptr<ITitleMenuState> newState);

    /**
     * @brief 状態を明示的に切り替える（互換用オーバーロード）
     * @param[in] newState 遷移先の状態
     */
    void ChangeState(MenuState newState);

    /**
     * @brief 現在選択中のメニューインデックスを取得する
     * @return 0: START, 1: HOW TO PLAY, 2: OPTIONS, 3: QUIT
     */
    int GetCurrentIndex() const {
        return currentIndex_;
    }

    /**
     * @brief HOW TO PLAY モーダルが開いているか
     */
    bool IsHowToPlayOpen() const;

    /**
     * @brief 出撃演出中かどうか
     */
    bool IsLaunching() const;

    /**
     * @brief 出撃演出開始フラグを設定する（外部連携用）
     */
    void SetLaunching(bool launching);

    /**
     * @brief GAME START 決定時のUI重力拡散・フェード消滅アニメーションを開始する
     */
    void StartDismissAnimation();

    /**
     * @brief 全画面インパクト白光フラッシュを発火する
     */
    void TriggerScreenFlash();

    /**
     * @brief UIディゾルブ消滅中かどうか
     */
    bool IsDismissing() const;

    /**
     * @brief メニューUI（タイトルロゴ・各ボタン）の一括表示/非表示を設定する
     * @param[in] visible 表示フラグ
     */
    void SetMenuVisible(bool visible);

    /**
     * @brief 上位シーン（OptionsやHowToPlay）から復帰した際に、選択中ボタンへフォーカスと仮想カーソルを同期復帰する
     */
    void RestoreFocusOnResume();

    // --- State Pattern 用アクセサ・ヘルパー群 (カプセル化準拠) ---
    void ResetTargetScalesForCurrentIndex();
    void UpdateStickCooldown(float dt);
    void HandleNavigationInput();
    void HandleSelectionInput();
    void UpdateButtonVisuals(float dt);
    void ResetStateTimer() { stateTimer_ = 0.0f; }
    void UpdateOpeningModalAnimation(float dt);
    void PushPendingModalScene();
    void HideVirtualCursor();
    void TriggerLaunchSequence();
    void UpdateDismissAnimation(float dt);

private: // 内部処理
    void UpdateVirtualCursor(float deltaTime);
    void UpdateScreenFlash(float deltaTime);
    void UpdateTitleTextVisual(float deltaTime);
    void ExecuteSelection();

    /**
     * @brief
     * カーソル座標が指定インデックスのボタン幾何領域内（動的スケール・バウンディングボックス反映）にあるか判定する
     * @param[in] index ボタンインデックス
     * @param[in] cursorPos 判定するカーソル座標
     * @return 領域内にある場合は true
     */
    bool IsCursorOverButton(int index, const Irufemi::Vector2& cursorPos) const;

private:                                // メンバ変数
    std::unique_ptr<ITitleMenuState> currentState_ = nullptr; //!< 現在のメニュー状態（State Pattern）
    float stateTimer_ = 0.0f;           //!< 各ステート内経過タイマー
    static constexpr float kModalAnimDuration_ =
        0.22f;                          //!< モーダル決定演出所要時間 (Click In 0.055s + Click Pop 0.165s)
    std::string pendingModalSceneName_; //!< 遷移待機中のモーダルシーン名

    int currentIndex_ = 0; //!< 選択中インデックス (0: Start, 1: HowToPlay, 2: Options, 3: Quit)
    int pressedButtonIndex_ = -1; //!< マウス/カーソル押下開始したボタンのインデックス（Drag-outキャンセル用）

    // 全画面インパクトフラッシュ用状態
    float flashTimer_ = 0.0f;
    static constexpr float kFlashDuration_ = 0.22f;

    // タイトルロゴ呼吸・パルス用タイマー
    float titleBreatheTimer_ = 0.0f;

    // UIディゾルブ消滅用状態
    float dismissTimer_ = 0.0f;
    static constexpr float kDismissDuration_ = 0.30f;
    std::vector<Irufemi::Vector3> dismissStartPositions_;
    Irufemi::Vector3 titleTextStartPos_{};

    float stickCooldownTimer_ = 0.0f; //!< スティック連続移動防止用タイマー
    const float kStickCooldown_ = 0.22f;

    // 仮想カーソル用
    std::shared_ptr<GameObject> virtualCursorObj_;
    class Primitive2DRendererComponent* virtualCursorRenderer_ = nullptr;
    const float kStickyFriction_ = 0.45f; //!< ボタンホバー時の減速倍率

    // ボタンのスケール補間制御用
    std::vector<float> currentScales_ = {1.0f, 1.0f, 1.0f, 1.0f};
    std::vector<float> targetScales_ = {1.15f, 1.0f, 1.0f, 1.0f};
    std::vector<Irufemi::Vector3> initialScales_; //!< エディタ(JSON)で設定された初期スケールキャッシュ

    // メニュー項目名
    const std::vector<std::string> buttonNames_ = {"Btn_Start", "Btn_HowToPlay", "Btn_Options", "Btn_Quit"};
};
