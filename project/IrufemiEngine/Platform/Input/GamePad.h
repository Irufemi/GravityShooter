#pragma once
#include <Windows.h>
#include <Xinput.h>
#include <utility>
#pragma comment(lib, "Xinput.lib")

namespace Irufemi {

enum class Stick8 { Neutral, Up, UpRight, Right, DownRight, Down, DownLeft, Left, UpLeft };

/**
 * @class GamePad
 * @brief ゲームパッド入力を管理するクラス
 * @details XInput を使用してゲームパッドの状態を取得します。
 *          ボタン入力、アナログスティック、デッドゾーン処理、8方向変換などの機能を提供します。
 */
class GamePad {
public:
    GamePad() = default;
    ~GamePad() = default;

    /** @name 初期化・更新 */
    ///@{
    void Initialize();
    /**
     * @brief ゲームパッド（XInput等）の最新の入力状態を取得して更新する。
     */
    void Update();
    /**
     * @brief Clear を実行する。
     */
    void Clear();
    ///@}

    /** @name ボタン入力状態 */
    ///@{
    bool IsButtonDown(WORD button) const;
    /**
     * @brief 指定したボタンが今フレームで離された瞬間かどうかを判定する。
     * @param[in] button ボタンのフラグ
     * @return 離されたならtrue
     */
    bool IsButtonUp(WORD button) const;
    /** @brief ボタンが押された瞬間か判定（立ち上がり） */
    bool IsButtonPressed(WORD button) const;
    /** @brief ボタンが離された瞬間か判定（立ち下がり） */
    bool IsButtonReleased(WORD button) const;
    ///@}

    /** @name アナログスティック・トリガー */
    ///@{
    float GetLeftStickX() const;
    /**
     * @brief 左スティックのY軸（垂直）の傾きを取得する。
     * @return 傾き (-1.0f 〜 1.0f)
     */
    float GetLeftStickY() const;
    /**
     * @brief 右スティックのX軸（水平）の傾きを取得する。
     * @return 傾き (-1.0f 〜 1.0f)
     */
    float GetRightStickX() const;
    /**
     * @brief 右スティックのY軸（垂直）の傾きを取得する。
     * @return 傾き (-1.0f 〜 1.0f)
     */
    float GetRightStickY() const;
    /**
     * @brief 左トリガー（LT）のアナログ押し込み量を取得する
     * @return 押し込み量 (0.0f 〜 1.0f)
     */
    float GetLeftTrigger() const;
    /**
     * @brief 右トリガー（RT）のアナログ押し込み量を取得する
     * @return 押し込み量 (0.0f 〜 1.0f)
     */
    float GetRightTrigger() const;

    /** @brief スティックのデッドゾーンを設定 */
    void SetLeftDeadZone(float dz) {
        deadZoneLeft_ = dz;
    }
    /**
     * @brief RightDeadZone を設定する。
     * @param[in] dz 設定する RightDeadZone の値
     */
    void SetRightDeadZone(float dz) {
        deadZoneRight_ = dz;
    }
    ///@}

    /** @name 接続状態・インデックス */
    ///@{
    /** @brief コントローラーが接続されているか取得 */
    bool IsConnected() const {
        return connected_;
    }
    /** @brief 使用するコントローラーのインデックス (0~3) を設定 */
    void SetIndex(int idx) {
        index_ = idx;
        retryCountdown_ = 0;
    }
    ///@}

    /** @name 状態取得 */
    ///@{
    /** @brief XINPUT_STATE を直接取得 */
    const XINPUT_STATE& GetState() const {
        return state_;
    }
    ///@}

    /** @name スティック8方向入力 */
    ///@{
    /** @brief 左スティックの状態を8方向に変換して取得 */
    Stick8 Left8(float threshold = 0.30f) const;
    /**
     * @brief Left8Is を実行する。
     */
    bool Left8Is(Stick8 dir, float threshold = 0.30f) const;
    /**
     * @brief Left8Pressed を実行する。
     */
    bool Left8Pressed(Stick8 dir, float threshold = 0.30f) const;
    /**
     * @brief Left8Released を実行する。
     */
    bool Left8Released(Stick8 dir, float threshold = 0.30f) const;

    // 方向ショートカット
    /**
     * @brief LUp を実行する。
     */
    bool LUp(float th = 0.30f) const;
    /**
     * @brief LDown を実行する。
     */
    bool LDown(float th = 0.30f) const;
    /**
     * @brief LLeft を実行する。
     */
    bool LLeft(float th = 0.30f) const;
    /**
     * @brief LRight を実行する。
     */
    bool LRight(float th = 0.30f) const;
    /**
     * @brief LUpRight を実行する。
     */
    bool LUpRight(float th = 0.30f) const;
    /**
     * @brief LUpLeft を実行する。
     */
    bool LUpLeft(float th = 0.30f) const;
    /**
     * @brief LDownRight を実行する。
     */
    bool LDownRight(float th = 0.30f) const;
    /**
     * @brief LDownLeft を実行する。
     */
    bool LDownLeft(float th = 0.30f) const;
    ///@}

    /** @name D-Pad 8方向入力 */
    ///@{
    Stick8 DPad8Now() const;
    /**
     * @brief D-Pad が指定した8方向に押されているか判定
     * @param[in] dir 判定対象の8方向
     */
    bool DPad8Is(Stick8 dir) const;
    /**
     * @brief D-Pad が指定した8方向に押された瞬間か判定（立ち上がり検出）
     * @param[in] dir 判定対象の8方向
     */
    bool DPad8Pressed(Stick8 dir) const;
    /**
     * @brief D-Pad が指定した8方向から離された瞬間か判定（立ち下がり検出）
     * @param[in] dir 判定対象の8方向
     */
    bool DPad8Released(Stick8 dir) const;

    // D-Pad ショートカット
    /**
     * @brief DPadUp を実行する。
     */
    bool DPadUp() const;
    bool DPadUpPressed() const;
    bool DPadUpReleased() const;
    /**
     * @brief DPadDown を実行する。
     */
    bool DPadDown() const;
    bool DPadDownPressed() const;
    bool DPadDownReleased() const;
    /**
     * @brief DPadLeft を実行する。
     */
    bool DPadLeft() const;
    bool DPadLeftPressed() const;
    bool DPadLeftReleased() const;
    /**
     * @brief DPadRight を実行する。
     */
    bool DPadRight() const;
    bool DPadRightPressed() const;
    bool DPadRightReleased() const;

    // 斜め方向ショートカット
    /**
     * @brief DPadUpRightDown を実行する。
     */
    bool DPadUpRightDown() const;
    bool DPadUpRightPressed() const;
    bool DPadUpRightReleased() const;
    /**
     * @brief DPadUpLeftDown を実行する。
     */
    bool DPadUpLeftDown() const;
    bool DPadUpLeftPressed() const;
    bool DPadUpLeftReleased() const;
    /**
     * @brief DPadDownRightDown を実行する。
     */
    bool DPadDownRightDown() const;
    bool DPadDownRightPressed() const;
    bool DPadDownRightReleased() const;
    /**
     * @brief DPadDownLeftDown を実行する。
     */
    bool DPadDownLeftDown() const;
    bool DPadDownLeftPressed() const;
    bool DPadDownLeftReleased() const;
    ///@}

    /** @name トリガー（しきい値デジタル判定） */
    ///@{
    float LeftTriggerAnalog(float deadzone = 30.0f) const;
    /**
     * @brief RightTriggerAnalog を実行する。
     */
    float RightTriggerAnalog(float deadzone = 30.0f) const;
    /**
     * @brief LeftTriggerDown を実行する。
     */
    bool LeftTriggerDown(uint8_t threshold = 30) const;
    /**
     * @brief LeftTriggerPressed を実行する。
     */
    bool LeftTriggerPressed(uint8_t threshold = 30) const;
    /**
     * @brief LeftTriggerReleased を実行する。
     */
    bool LeftTriggerReleased(uint8_t threshold = 30) const;
    /**
     * @brief RightTriggerDown を実行する。
     */
    bool RightTriggerDown(uint8_t threshold = 30) const;
    /**
     * @brief RightTriggerPressed を実行する。
     */
    bool RightTriggerPressed(uint8_t threshold = 30) const;
    /**
     * @brief RightTriggerReleased を実行する。
     */
    bool RightTriggerReleased(uint8_t threshold = 30) const;

    /** @brief 左右いずれかのトリガーの状態を判定
     *  @param[in] right true で右トリガー、false で左トリガーを判定
     */
    bool TriggerDown(bool right, uint8_t threshold = 30) const;
    /**
     * @brief TriggerPressed を実行する。
     */
    bool TriggerPressed(bool right, uint8_t threshold = 30) const;
    /**
     * @brief TriggerReleased を実行する。
     */
    bool TriggerReleased(bool right, uint8_t threshold = 30) const;
    ///@}

    /** @name 特殊ボタンショートカット */
    ///@{
    bool LBDown() const;
    bool LBPressed() const;
    bool LBReleased() const;
    /**
     * @brief RBDown を実行する。
     */
    bool RBDown() const;
    bool RBPressed() const;
    bool RBReleased() const;
    /**
     * @brief StartDown を実行する。
     */
    bool StartDown() const;
    bool StartPressed() const;
    bool StartReleased() const;
    ///@}

    /** @brief Y軸の判定を反転させるか設定 */
    void SetInvertY(bool inv) {
        invertY_ = inv;
    }

private:
    /** @brief ラジアル正規化（円形デッドゾーン処理） */
    static std::pair<float, float> RadialNormalize(short x, short y, int dz);

    /** @brief スティック入力の8方向変換ヘルパー */
    static Stick8 Stick8From(short x, short y, int dz, bool invertY, float threshold);

    XINPUT_STATE state_{};
    XINPUT_STATE prev_{};
    float deadZoneLeft_ = 0.2f;
    float deadZoneRight_ = 0.2f;
    bool connected_ = false;
    int index_ = 0;
    bool invertY_ = false;
    uint32_t retryCountdown_ = 0;
};

} // namespace Irufemi
