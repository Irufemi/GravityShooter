#pragma once
#include <memory>
#include "Platform/Input/Keyboard.h"
#include "Platform/Input/GamePad.h"
#include "Platform/Input/Mouse.h"
#include "Core/Math/Vector2.h"
#include "Platform/Input/InputMappingContext.h"
#include <string>
#include <unordered_map>

// 役割：具体実装(Keyboard/GamePad/Mouse)を保持し、旧APIをフォワードして互換を維持するファサード
/**
 * @class InputManager
 * @brief キーボード、マウス、ゲームパッドの入力を一括管理するクラス
 * @details 各入力デバイスの具体的なインスタンスを保持し、統一したインターフェースを提供します。
 *          既存コードとの互換性を維持するためのフォワードメソッドも備えています。
 */
class InputManager {
public:
    InputManager() = default;
    ~InputManager() = default;

    /** @name 初期化・更新 */
    ///@{
    /**
     * @brief 初期化処理
     * @param[in] hwnd ウィンドウハンドル
     */
    void Initialize(HWND hwnd);

    /**
     * @brief 毎フレームの更新処理
     */
    void Update();
    ///@}

    /** @name アクションベース入力（推奨API） */
    ///@{
    /**
     * @brief アクションに物理入力をバインドする
     * @param[in] actionName アクション名（例: "Jump", "MoveX"）
     * @param[in] inputId 割り当てる物理入力（InputId::Keyboard_Space など）
     * @param[in] scale 物理入力値を最終値に変換する際の係数（1.0f=そのまま, -1.0f=反転, 0.5f=感度半減 など）
     */
    void BindAction(const std::string& actionName, InputId inputId, float scale = 1.0f);
    void BindAction(const std::string& actionName, InputId inputId, InputModifier modifiers, float scale = 1.0f);

    /** @brief 指定アクションのアナログ値（1D/2D）を取得する */
    InputActionValue GetActionValue(const std::string& actionName) const;

    /** @brief 指定アクションが押されているか（Down） */
    bool IsActionDown(const std::string& actionName) const;
    /** @brief 指定アクションが押された瞬間か（Triggered/Pressed） */
    bool IsActionTriggered(const std::string& actionName) const;
    /** @brief 指定アクションが離された瞬間か（Released） */
    bool IsActionReleased(const std::string& actionName) const;

    /** @brief 全てのアクションバインディングを解除する */
    void ClearActionBindings();

    /**
     * @brief JSONファイルからアクションバインディングを一括読み込み・登録する
     * @param[in] filepath JSONファイルパス
     * @return 読み込みに成功した場合 true
     */
    bool LoadBindingsFromJson(const std::string& filepath);
    ///@}

    /** @name デバイス取得（推奨API） */
    ///@{
    /** @brief キーボードデバイスインスタンスを取得する */
    Keyboard* GetKeyboard() {
        return keyboard_.get();
    }
    /** @brief ゲームパッドデバイスインスタンスを取得する */
    GamePad* GetGamePad() {
        return gamepad_.get();
    }
    /** @brief マウスデバイスインスタンスを取得する */
    Mouse* GetMouse() {
        return mouse_.get();
    }
    ///@}

    /** @name キーボード入力（互換用API） */
    /** @name キー状態の取得 */
    ///@{
    bool IsKeyDown(uint8_t key) const;
    /**
     * @brief 指定したキーが今フレームで離された瞬間かどうかを判定する。
     * @param[in] key 仮想キーコード
     * @return 離された瞬間ならtrue
     */
    bool IsKeyUp(uint8_t key) const;
    /** @brief キーが押された瞬間か判定（立ち上がり） */
    bool IsKeyPressed(uint8_t key) const;
    /** @brief キーが離された瞬間か判定（立ち下がり） */
    bool IsKeyReleased(uint8_t key) const;
    /**
     * @brief 指定した仮想キーの入力状態を消費（クリア）する
     * @param key 仮想キーコード (VK_xxx)
     * @details 同一フレーム内の後続システムへキー入力を伝播させない（Handled状態にする）ために使用します。
     */
    void ConsumeKey(uint8_t key);
    ///@}

    /** @name DIK互換API */
    ///@{
    bool IsKeyDownDIK(uint8_t dik) const;
    /**
     * @brief IsKeyUpDIK かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsKeyUpDIK(uint8_t dik) const;
    /**
     * @brief IsKeyPressedDIK かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsKeyPressedDIK(uint8_t dik) const;
    /**
     * @brief IsKeyReleasedDIK かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsKeyReleasedDIK(uint8_t dik) const;
    ///@}
    ///@}

    /** @name ゲームパッド入力（互換用API） */
    ///@{
    /** @name ボタン入力状態 */
    ///@{
    bool IsButtonDown(WORD button) const;
    /**
     * @brief IsButtonUp かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsButtonUp(WORD button) const;
    /** @brief ボタンが押された瞬間か判定 */
    bool IsButtonPressed(WORD button) const;
    /** @brief ボタンが離された瞬間か判定 */
    bool IsButtonReleased(WORD button) const;
    ///@}

    /**
     * @brief LeftStickX を取得する。
     * @return 取得された LeftStickX
     */
    float GetLeftStickX() const;
    /**
     * @brief LeftStickY を取得する。
     * @return 取得された LeftStickY
     */
    float GetLeftStickY() const;
    /**
     * @brief RightStickX を取得する。
     * @return 取得された RightStickX
     */
    float GetRightStickX() const;
    /**
     * @brief RightStickY を取得する。
     * @return 取得された RightStickY
     */
    float GetRightStickY() const;

    /** @brief 左トリガー（LT）のアナログ押し込み量を取得する */
    float GetLeftTrigger() const;
    /** @brief 右トリガー（RT）のアナログ押し込み量を取得する */
    float GetRightTrigger() const;

    /** @brief 左トリガー（LT）がしきい値以上押されているか判定 */
    bool IsLeftTriggerDown(float threshold = 0.4f) const;
    /** @brief 左トリガー（LT）が押された瞬間か判定（立ち上がり検出） */
    bool IsLeftTriggerPressed(float threshold = 0.4f) const;
    /** @brief 右トリガー（RT）がしきい値以上押されているか判定 */
    bool IsRightTriggerDown(float threshold = 0.4f) const;
    /** @brief 右トリガー（RT）が押された瞬間か判定（立ち上がり検出） */
    bool IsRightTriggerPressed(float threshold = 0.4f) const;

    /** @brief STARTボタンが押されているか判定 */
    bool StartDown() const;
    /** @brief STARTボタンが押された瞬間か判定（立ち上がり検出） */
    bool StartPressed() const;
    /** @brief STARTボタンが離された瞬間か判定（立ち下がり検出） */
    bool StartReleased() const;

    /** @brief D-Pad 上が押されているか判定 */
    bool DPadUp() const;
    /** @brief D-Pad 下が押されているか判定 */
    bool DPadDown() const;
    /** @brief D-Pad 左が押されているか判定 */
    bool DPadLeft() const;
    /** @brief D-Pad 右が押されているか判定 */
    bool DPadRight() const;
    /** @brief D-Pad 上が押された瞬間か判定（立ち上がり検出） */
    bool DPadUpPressed() const;
    /** @brief D-Pad 下が押された瞬間か判定（立ち上がり検出） */
    bool DPadDownPressed() const;
    /** @brief D-Pad 左が押された瞬間か判定（立ち上がり検出） */
    bool DPadLeftPressed() const;
    /** @brief D-Pad 右が押された瞬間か判定（立ち上がり検出） */
    bool DPadRightPressed() const;
    ///@}

    /** @name マウス入力（互換用API） */
    ///@{
    /** @brief 指定したマウスボタンが押されているか判定 */
    bool IsMouseButtonDown(Mouse::Button button) const;
    /** @brief 指定したマウスボタンが押された瞬間か判定（立ち上がり検出） */
    bool IsMouseButtonPressed(Mouse::Button button) const;
    /** @brief 指定したマウスボタンが離された瞬間か判定（立ち下がり検出） */
    bool IsMouseButtonReleased(Mouse::Button button) const;
    /** @brief 現在のマウス座標を取得する */
    const Irufemi::Vector2& GetMousePosition() const;
    /** @brief 前フレームからのマウス移動量を取得する */
    const Irufemi::Vector2& GetMouseDelta() const;
    /** @brief マウスホイールの回転差分を取得する */
    float GetMouseWheelDelta() const;

    /** @brief エディタ用：仮想的なマウスローカル座標を上書き設定する */
    void SetVirtualMousePosition(const Irufemi::Vector2& pos, bool enable) {
        if (mouse_) {
            mouse_->SetVirtualPosition(pos, enable);
        }
    }

    /** @brief ゲーム解像度を設定する（マウスのレターボックス計算用） */
    void SetGameResolution(float width, float height) {
        if (mouse_) {
            mouse_->SetGameResolution(width, height);
        }
    }
    ///@}

    /** @name 仮想カーソル（Virtual Cursor / マウス・ゲームパッド統合カーソル） */
    ///@{
    /** @brief 統合仮想カーソルの現在座標（1280x720 空間）を取得する */
    const Irufemi::Vector2& GetVirtualCursorPosition() const {
        return virtualCursorPos_;
    }

    /** @brief 統合仮想カーソルの座標を設定する */
    void SetVirtualCursorPosition(const Irufemi::Vector2& pos) {
        virtualCursorPos_ = pos;
    }

    /** @brief 現在ゲームパッドスティックでカーソルを操作中かどうか */
    bool IsUsingGamepadCursor() const {
        return isUsingGamepadCursor_;
    }

    /** @brief 仮想カーソルの基準移動速度（ピクセル/秒）を取得する */
    float GetVirtualCursorBaseSpeed() const {
        return virtualCursorBaseSpeed_;
    }

    /** @brief 仮想カーソルの基準移動速度（ピクセル/秒）を設定する */
    void SetVirtualCursorBaseSpeed(float speed) {
        virtualCursorBaseSpeed_ = speed;
    }

    /** @brief 仮想カーソルの移動可能範囲（クランプ矩形）を設定する */
    void SetVirtualCursorBounds(const Irufemi::Vector2& minBounds, const Irufemi::Vector2& maxBounds) {
        virtualCursorBoundsMin_ = minBounds;
        virtualCursorBoundsMax_ = maxBounds;
    }

    /**
     * @brief 論理参照解像度（Reference Resolution）に基づいて仮想カーソルの移動可能範囲を更新する
     * @param[in] width 画面・論理ビューポート幅
     * @param[in] height 画面・論理ビューポート高さ
     * @param[in] padding 画面端の安全マージン（初期値: 15.0f）
     */
    void UpdateReferenceResolution(float width, float height, float padding = 15.0f) {
        float safePad = (std::max)(0.0f, padding);
        virtualCursorBoundsMin_ = {safePad, safePad};
        virtualCursorBoundsMax_ = {(std::max)(safePad, width - safePad), (std::max)(safePad, height - safePad)};
    }

    /** @brief 仮想カーソルの移動可能最小座標を取得する */
    const Irufemi::Vector2& GetVirtualCursorBoundsMin() const {
        return virtualCursorBoundsMin_;
    }

    /** @brief 仮想カーソルの移動可能最大座標を取得する */
    const Irufemi::Vector2& GetVirtualCursorBoundsMax() const {
        return virtualCursorBoundsMax_;
    }

    /**
     * @brief 仮想カーソルを更新する（マウス移動検知・スティック移動・画面クランプ）
     * @param[in] deltaTime 経過時間
     * @param[in] speedMultiplier 移動速度乗数（UIホバー時の摩擦減速など）
     */
    void UpdateVirtualCursor(float deltaTime, float speedMultiplier = 1.0f);

    /** @brief カーソルの決定・選択アクションが押されているか（Aボタン or マウス左ボタン） */
    bool IsCursorActionDown() const;

    /** @brief カーソルの決定・選択アクションが押された瞬間か（Aボタン or マウス左ボタン） */
    bool IsCursorActionPressed() const;

    /** @brief カーソルの決定・選択アクションが離された瞬間か（Aボタン or マウス左ボタン） */
    bool IsCursorActionReleased() const;

    /** @brief キャンセル・戻る操作が押された瞬間か（Bボタン or ESC or BackSpace） */
    bool IsCancelPressed() const;
    ///@}

private:
    /** @brief 物理入力デバイスから現在の状態（アナログ値または0/1）を取得する内部関数 */
    float GetPhysicalInputValue(InputId id) const;

    std::unique_ptr<Keyboard> keyboard_{};
    std::unique_ptr<GamePad> gamepad_{};
    std::unique_ptr<Mouse> mouse_{};
    HWND hwnd_ = nullptr;

    InputMappingContext mappingContext_{};

    // 前フレームと現在のフレームのアクション値を保持（Triggered等の判定用）
    std::unordered_map<std::string, InputActionValue> currentActionValues_{};
    std::unordered_map<std::string, InputActionValue> previousActionValues_{};

    // 仮想カーソル管理
    Irufemi::Vector2 virtualCursorPos_ = {640.0f, 360.0f};
    Irufemi::Vector2 virtualCursorBoundsMin_ = {15.0f, 15.0f};
    Irufemi::Vector2 virtualCursorBoundsMax_ = {1265.0f, 705.0f};
    Irufemi::Vector2 lastPhysicalMousePos_ = {640.0f, 360.0f};
    bool isUsingGamepadCursor_ = false;
    float virtualCursorBaseSpeed_ = 650.0f;
};
