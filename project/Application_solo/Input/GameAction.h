#pragma once
#include "Platform/Input/InputManager.h"
#include <cstdint>

/**
 * @enum GameAction
 * @brief ゲーム内で使用する論理アクション（型安全識別子）
 * @details resources/config/input_actions.json の "Actions" セクションと対応します。
 */
enum class GameAction : uint32_t {
    Pull,       ///< ガレキ引き寄せ
    Fire,       ///< ガレキ射出
    LockOn,     ///< ロックオンマーキング
    ClearLock,  ///< ロックオン全解除
    Pause,      ///< ポーズ画面開閉
    ToggleFullscreen, ///< 全画面/ウィンドウモード切り替え
    UI_Submit,  ///< メニュー決定
    UI_Cancel,  ///< メニュー戻る・キャンセル
    Count
};

/**
 * @enum GameAxis
 * @brief アナログ・連続入力軸
 * @details resources/config/input_actions.json の "Axes" セクションと対応します。
 */
enum class GameAxis : uint32_t {
    MoveX,      ///< 左右移動 (-1.0 ~ 1.0)
    MoveY,      ///< 上下移動 (-1.0 ~ 1.0)
    Count
};

/**
 * @brief GameAction enum を JSON 定義文字列へ変換する
 */
inline const char* GameActionToString(GameAction action) {
    switch (action) {
    case GameAction::Pull:             return "Pull";
    case GameAction::Fire:             return "Fire";
    case GameAction::LockOn:           return "LockOn";
    case GameAction::ClearLock:        return "ClearLock";
    case GameAction::Pause:            return "Pause";
    case GameAction::ToggleFullscreen: return "ToggleFullscreen";
    case GameAction::UI_Submit:        return "UI_Submit";
    case GameAction::UI_Cancel:        return "UI_Cancel";
    default:                           return "Unknown";
    }
}

/**
 * @brief GameAxis enum を JSON 定義文字列へ変換する
 */
inline const char* GameAxisToString(GameAxis axis) {
    switch (axis) {
    case GameAxis::MoveX: return "MoveX";
    case GameAxis::MoveY: return "MoveY";
    default:              return "Unknown";
    }
}

namespace InputHelper {
    inline bool IsActionDown(const InputManager* input, GameAction action) {
        return input ? input->IsActionDown(GameActionToString(action)) : false;
    }
    inline bool IsActionPressed(const InputManager* input, GameAction action) {
        return input ? input->IsActionTriggered(GameActionToString(action)) : false;
    }
    inline bool IsActionReleased(const InputManager* input, GameAction action) {
        return input ? input->IsActionReleased(GameActionToString(action)) : false;
    }
    inline float GetAxisValue(const InputManager* input, GameAxis axis) {
        return input ? input->GetActionValue(GameAxisToString(axis)).x : 0.0f;
    }
}

