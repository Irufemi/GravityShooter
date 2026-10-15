#include "Framework/UI/UINavigationController.h"
#include "Platform/Input/InputManager.h"

bool UINavigationController::Update(InputManager* input, float deltaTime) {
    if (!input || itemCount_ <= 0) {
        return false;
    }

    // 1. 物理マウスの有意な移動、またはマウスクリックを検出したら Mouse モードに復帰
    if (input->HasPhysicalMouseMoved(2.0f) || input->IsMouseButtonPressed(Mouse::Button::Left) ||
        input->IsMouseButtonPressed(Mouse::Button::Right)) {
        activeInputMode_ = NavigationInputMode::Mouse;
    }

    // 2. 移動入力の検出（デジタルボタン：十字キー / キーボード）
    int digitalMove = 0;
    if (isHorizontal_) {
        if (input->IsNavLeftPressed()) {
            digitalMove = -1;
        } else if (input->IsNavRightPressed()) {
            digitalMove = 1;
        }
    } else {
        if (input->IsNavUpPressed()) {
            digitalMove = -1;
        } else if (input->IsNavDownPressed()) {
            digitalMove = 1;
        }
    }

    // 3. アナログスティック入力（左スティック）と長押しリピート
    float stickVal = isHorizontal_ ? input->GetLeftStickX() : input->GetLeftStickY();
    // 垂直ナビゲーションの場合、スティック上(+)はインデックス減少(-1)、スティック下(-)はインデックス増加(+1)
    int currentStickDir = 0;
    if (isHorizontal_) {
        if (stickVal > kStickThreshold_) {
            currentStickDir = 1;
        } else if (stickVal < -kStickThreshold_) {
            currentStickDir = -1;
        }
    } else {
        if (stickVal > kStickThreshold_) {
            currentStickDir = -1; // 上
        } else if (stickVal < -kStickThreshold_) {
            currentStickDir = 1; // 下
        }
    }

    int moveDelta = 0;

    // デジタル入力があった場合は即座に1ステップ移動
    if (digitalMove != 0) {
        moveDelta = digitalMove;
        activeInputMode_ = NavigationInputMode::GamepadOrKeyboard;
        // デジタル入力時はリピートタイマーをリセット
        repeatTimer_ = 0.0f;
        isRepeating_ = false;
        lastStickDir_ = 0;
    } else if (currentStickDir != 0) {
        activeInputMode_ = NavigationInputMode::GamepadOrKeyboard;

        // スティックが倒された方向の変化を検出
        if (currentStickDir != lastStickDir_) {
            // スティック倒し始め：初回即時発火
            moveDelta = currentStickDir;
            lastStickDir_ = currentStickDir;
            repeatTimer_ = 0.0f;
            isRepeating_ = false;
        } else {
            // スティック倒しっぱなし：長押しリピート
            repeatTimer_ += deltaTime;
            if (!isRepeating_) {
                if (repeatTimer_ >= kInitialRepeatDelay_) {
                    isRepeating_ = true;
                    repeatTimer_ = 0.0f;
                    moveDelta = currentStickDir;
                }
            } else {
                if (repeatTimer_ >= kRepeatInterval_) {
                    repeatTimer_ = 0.0f;
                    moveDelta = currentStickDir;
                }
            }
        }
    } else {
        // スティックがニュートラル付近に戻った場合
        if (std::abs(stickVal) < kStickReleaseThreshold_) {
            lastStickDir_ = 0;
            repeatTimer_ = 0.0f;
            isRepeating_ = false;
        }
    }

    // 4. フォーカス移動の適用
    if (moveDelta != 0) {
        int oldIndex = selectedIndex_;
        if (wrapEnabled_) {
            selectedIndex_ = (selectedIndex_ + moveDelta + itemCount_) % itemCount_;
        } else {
            selectedIndex_ = std::clamp(selectedIndex_ + moveDelta, 0, itemCount_ - 1);
        }

        if (oldIndex != selectedIndex_) {
            if (onSelectionChanged_) {
                onSelectionChanged_(selectedIndex_);
            }
            return true;
        }
    }

    return false;
}

bool UINavigationController::TrySetHoveredIndex(int hoveredIndex, InputManager* input) {
    if (hoveredIndex < 0 || hoveredIndex >= itemCount_) {
        return false;
    }

    // 物理マウスが動いたかマウスクリックされた場合は即座にMouseモードへ遷移
    if (input && (input->HasPhysicalMouseMoved(2.0f) || input->IsMouseButtonPressed(Mouse::Button::Left))) {
        activeInputMode_ = NavigationInputMode::Mouse;
    }

    // GamepadOrKeyboard モード中は、静止したマウスによるホバー上書きを完全にブロックする
    if (activeInputMode_ != NavigationInputMode::Mouse) {
        return false;
    }

    if (selectedIndex_ != hoveredIndex) {
        selectedIndex_ = hoveredIndex;
        if (onSelectionChanged_) {
            onSelectionChanged_(selectedIndex_);
        }
        return true;
    }
    return false;
}
