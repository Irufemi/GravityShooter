#pragma once
#include <Windows.h>
#include <array>
#include "Core/Math/Vector2.h"

namespace Irufemi {

/**
 * @class Mouse
 * @brief マウス入力を管理するクラス
 * @details ボタン状態、カーソル位置、ホイール回転量の取得に加え、
 *          マウスカーソルのロック（中央固定）制御も行います。
 */
class Mouse {
public:
    /** @enum Button
     *  @brief マウスのボタン識別子
     */
    enum class Button {
        Left,   ///< 左ボタン
        Right,  ///< 右ボタン
        Middle, ///< 中ボタン（ホイールクリック）
    };

    Mouse() = default;
    ~Mouse() = default;

    /** @name 初期化・更新 */
    ///@{
    void Initialize(HWND hwnd);
    /**
     * @brief ゲーム解像度を設定する（レターボックス計算用）
     */
    void SetGameResolution(float width, float height) {
        gameResWidth_ = width;
        gameResHeight_ = height;
    }
    /**
     * @brief マウス入力状態を毎フレーム更新する
     */
    void Update();
    /**
     * @brief マウス入力状態（ボタン状態、移動量、ホイール量）をクリアする
     */
    void Clear();
    ///@}

    /** @name ボタン入力状態 */
    ///@{
    bool IsButtonDown(Button button) const;
    /** @brief ボタンが押された瞬間か判定 */
    bool IsButtonPressed(Button button) const;
    /** @brief ボタンが離された瞬間か判定 */
    bool IsButtonReleased(Button button) const;
    ///@}

    /** @name カーソル座標・移動量 */
    ///@{
    /** @brief 現在のマウス座標（スクリーン空間、または設定された仮想ローカル空間）を取得 */
    const Irufemi::Vector2& GetPosition() const {
        return useVirtualPosition_ ? virtualPosition_ : position_;
    }
    /** @brief 前フレームからの移動量を取得 */
    const Irufemi::Vector2& GetDelta() const {
        return delta_;
    }
    ///@}

    /** @name ホイール操作 */
    ///@{
    /** @brief ホイールの回転差分を取得 */
    float GetWheelDelta() const;
    /** @brief ホイール差分を設定する（Windows メッセージから呼び出し） */
    void SetWheelDelta(float delta) {
        wheelDelta_ = delta;
    }
    ///@}

    /** @name カーソル制御 */
    ///@{
    /**
     * @brief マウスをウィンドウ中央にロックするか設定する
     * @param[in] locked true でロック（FPS等で使用）、false で解除
     */
    void SetLocked(bool locked);
    ///@}

    /** @name 仮想マウス座標制御（エディタ用） */
    ///@{
    /**
     * @brief エディタのSceneViewなどのローカル座標をマウス座標として上書き設定する
     */
    void SetVirtualPosition(const Irufemi::Vector2& pos, bool enable) {
        virtualPosition_ = pos;
        useVirtualPosition_ = enable;
    }
    ///@}

    /**
     * @brief Raw Input からの生移動量を蓄積する
     */
    void AddRawDelta(float dx, float dy) {
        rawDelta_.x += dx;
        rawDelta_.y += dy;
    }

private:
    HWND hwnd_ = nullptr;
    std::array<BYTE, 3> currentButtons_{};
    std::array<BYTE, 3> prevButtons_{};
    Irufemi::Vector2 position_{};
    Irufemi::Vector2 prevPosition_{};
    Irufemi::Vector2 delta_{};
    Irufemi::Vector2 rawDelta_{}; // Raw Input からの生移動量
    float wheelDelta_ = 0.0f;
    bool isLocked_ = true; // マウスを中央に固定するかどうか

    bool useVirtualPosition_ = false;    // エディタ等からの仮想座標を使用するか
    Irufemi::Vector2 virtualPosition_{}; // 仮想座標

    // ゲーム解像度
    float gameResWidth_ = 1280.0f;
    float gameResHeight_ = 720.0f;
};

} // namespace Irufemi
