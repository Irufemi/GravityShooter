#pragma once
#include "Platform/Input/InputManager.h"
#include <cstdint>

/**
 * @enum GameAction
 * @brief ゲーム内で使用する論理アクション（型安全識別子）
 * @details resources/config/input_actions.json の "Actions" セクションと対応します。
 */
enum class GameAction : uint32_t {
    Pull,             ///< ガレキ引き寄せ
    Fire,             ///< ガレキ射出
    LockOn,           ///< ロックオンマーキング
    ClearLock,        ///< ロックオン全解除
    Pause,            ///< ポーズ画面開閉
    ToggleFullscreen, ///< 全画面/ウィンドウモード切り替え
    UI_Submit,        ///< メニュー決定
    UI_Cancel,        ///< メニュー戻る・キャンセル
    Count
};

/**
 * @enum GameAxis
 * @brief アナログ・連続入力軸
 * @details resources/config/input_actions.json の "Axes" セクションと対応します。
 */
enum class GameAxis : uint32_t {
    MoveX, ///< 左右移動 (-1.0 ~ 1.0)
    MoveY, ///< 上下移動 (-1.0 ~ 1.0)
    Count
};

#include <array>

/**
 * @brief GameAction enum を JSON 定義文字列へ変換する（Branchless コンパイル時データテーブル）
 */
inline constexpr const char* GameActionToString(GameAction action) noexcept {
    constexpr std::array<const char*, static_cast<size_t>(GameAction::Count)> kActionNames = {
        "Pull",             // Pull
        "Fire",             // Fire
        "LockOn",           // LockOn
        "ClearLock",        // ClearLock
        "Pause",            // Pause
        "ToggleFullscreen", // ToggleFullscreen
        "UI_Submit",        // UI_Submit
        "UI_Cancel"         // UI_Cancel
    };
    const auto idx = static_cast<size_t>(action);
    return idx < kActionNames.size() ? kActionNames[idx] : "Unknown";
}

/**
 * @brief GameAxis enum を JSON 定義文字列へ変換する（Branchless コンパイル時データテーブル）
 */
inline constexpr const char* GameAxisToString(GameAxis axis) noexcept {
    constexpr std::array<const char*, static_cast<size_t>(GameAxis::Count)> kAxisNames = {
        "MoveX", // MoveX
        "MoveY"  // MoveY
    };
    const auto idx = static_cast<size_t>(axis);
    return idx < kAxisNames.size() ? kAxisNames[idx] : "Unknown";
}

/**
 * @namespace InputHelper
 * @brief ゲーム固有のアクションマッピング（キー・マウス・パッド）判定を簡潔に行うヘルパー名前空間
 */
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
} // namespace InputHelper
