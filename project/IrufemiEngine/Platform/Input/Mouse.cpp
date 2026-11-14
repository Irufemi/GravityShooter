#include "Platform/Input/Mouse.h"
#include "Core/Math/Math.h"
#include "Core/Utility/Log.h"
#include <iostream>
#include <algorithm>
#include <string>

namespace Irufemi {

void Mouse::Initialize(HWND hwnd) {
    hwnd_ = hwnd;
    // 初期位置を取得
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(hwnd_, &p);
    position_ = {static_cast<float>(p.x), static_cast<float>(p.y)};
    prevPosition_ = position_;

    // 初期状態を適用
    SetLocked(isLocked_);
}

void Mouse::Clear() {
    std::fill(std::begin(currentButtons_), std::end(currentButtons_), 0);
    std::fill(std::begin(prevButtons_), std::end(prevButtons_), 0);
    delta_ = {0.0f, 0.0f};
    rawDelta_ = {0.0f, 0.0f};
    wheelDelta_ = 0.0f;
}

void Mouse::Update() {
    // 前フレームのボタン状態を保存
    std::copy(std::begin(currentButtons_), std::end(currentButtons_), std::begin(prevButtons_));

    // 現在のボタン状態を取得
    currentButtons_[0] = (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
    currentButtons_[1] = (GetKeyState(VK_RBUTTON) & 0x8000) != 0;
    currentButtons_[2] = (GetKeyState(VK_MBUTTON) & 0x8000) != 0;

    // マウス位置の更新
    POINT p;
    GetCursorPos(&p);

    if (isLocked_) {
        // ロック中: Raw Input で蓄積された移動量(rawDelta_)をそのまま差分として使う
        delta_ = rawDelta_;
        rawDelta_ = {0.0f, 0.0f}; // 読み取ったらリセット

        // カーソルは画面内に ClipCursor で閉じ込められているため、毎フレーム SetCursorPos で
        // 中央に戻す必要はない(Raw Inputは画面外判定を受けない)。
        // 内部座標は論理的に画面中央に固定しておく
        RECT rc;
        GetClientRect(hwnd_, &rc);
        position_ = {static_cast<float>((rc.right - rc.left) / 2), static_cast<float>((rc.bottom - rc.top) / 2)};
        prevPosition_ = position_;

        // 毎フレームカーソルを画面中央に固定し直すことで、ウィンドウ端でのクリック漏れ（背面ウィンドウの誤クリック）を防ぐ
        if (GetForegroundWindow() == hwnd_) {
            POINT center = {(rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2};
            ClientToScreen(hwnd_, &center);
            SetCursorPos(center.x, center.y);

            // Alt+Tabからの復帰時など、カーソルが同じ座標に固定され続けるとWM_SETCURSORが発生せず
            // 画面中央にカーソルが可視化されたまま残ってしまう不具合を防止するため、明示的に非表示にする
            SetCursor(nullptr);
        }
    } else {
        // 通常時の挙動: スクリーン座標をクライアント座標に変換して position_ を更新
        ScreenToClient(hwnd_, &p);
        prevPosition_ = position_;

        RECT rc;
        GetClientRect(hwnd_, &rc);
        float clientW = static_cast<float>(rc.right - rc.left);
        float clientH = static_cast<float>(rc.bottom - rc.top);

        if (clientW > 0.0f && clientH > 0.0f && gameResWidth_ > 0.0f && gameResHeight_ > 0.0f) {
            Irufemi::Math::LetterboxInfo view =
                Irufemi::Math::CalculateLetterbox(clientW, clientH, gameResWidth_, gameResHeight_);

            float mouseX = static_cast<float>(p.x);
            float mouseY = static_cast<float>(p.y);

            mouseX = std::clamp((mouseX - view.viewX) / view.viewW * gameResWidth_, 0.0f, gameResWidth_);
            mouseY = std::clamp((mouseY - view.viewY) / view.viewH * gameResHeight_, 0.0f, gameResHeight_);

            position_ = {mouseX, mouseY};
        } else {
            position_ = {static_cast<float>(p.x), static_cast<float>(p.y)};
        }

        delta_ = {position_.x - prevPosition_.x, position_.y - prevPosition_.y};

        // 通常時もRaw Inputの移動量は蓄積され続けるためクリアしておく
        rawDelta_ = {0.0f, 0.0f};
    }

    // ホイールの差分をリセット
    wheelDelta_ = 0.0f;
}

void Mouse::SetLocked(bool locked) {
    if (isLocked_ != locked) {
        isLocked_ = locked;
        delta_ = {0.0f, 0.0f};
        rawDelta_ = {0.0f, 0.0f};

        if (isLocked_ && hwnd_) {
            // 表示を消す
            while (ShowCursor(FALSE) >= 0)
                ;

            // クリッピング設定 (画面外に出てフォーカスを失うのを防ぐ)
            RECT clientRect;
            GetClientRect(hwnd_, &clientRect);
            POINT topLeft = {clientRect.left, clientRect.top};
            POINT bottomRight = {clientRect.right, clientRect.bottom};
            ClientToScreen(hwnd_, &topLeft);
            ClientToScreen(hwnd_, &bottomRight);
            RECT clipRect = {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
            ClipCursor(&clipRect);

            // ロック開始時のみ、一度だけ中央にカーソルを移動させておく
            POINT center = {(clientRect.right - clientRect.left) / 2, (clientRect.bottom - clientRect.top) / 2};
            position_ = {static_cast<float>(center.x), static_cast<float>(center.y)};
            prevPosition_ = position_;

            ClientToScreen(hwnd_, &center);
            SetCursorPos(center.x, center.y);
        } else {
            // 表示を戻す
            while (ShowCursor(TRUE) < 0)
                ;

            // クリッピング解除
            ClipCursor(nullptr);
        }
    }
}

bool Mouse::IsButtonDown(Button button) const {
    return currentButtons_[static_cast<int>(button)];
}

bool Mouse::IsButtonPressed(Button button) const {
    int index = static_cast<int>(button);
    return currentButtons_[index] && !prevButtons_[index];
}

bool Mouse::IsButtonReleased(Button button) const {
    int index = static_cast<int>(button);
    return !currentButtons_[index] && prevButtons_[index];
}

float Mouse::GetWheelDelta() const {
    // この実装では、外部からホイール値が設定されることを前提とします。
    // 例えば、Win32のメッセージループでWM_MOUSEWHEELを処理し、
    // このクラスのメンバー変数 wheelDelta_ を更新します。
    return wheelDelta_;
}

} // namespace Irufemi