#pragma once

#include <cstdint>
#include <string>
#include "Core/Math/Vector2.h"

/**
 * @enum InputDeviceType
 * @brief 入力デバイスの種類
 */
enum class InputDeviceType { Keyboard, Mouse, GamePad };

/**
 * @enum InputId
 * @brief 物理的な入力（キー、ボタン、軸）を一意に識別するためのID
 */
enum class InputId : uint16_t {
    Unknown = 0,

    // Keyboard (A-Z)
    Keyboard_A,
    Keyboard_B,
    Keyboard_C,
    Keyboard_D,
    Keyboard_E,
    Keyboard_F,
    Keyboard_G,
    Keyboard_H,
    Keyboard_I,
    Keyboard_J,
    Keyboard_K,
    Keyboard_L,
    Keyboard_M,
    Keyboard_N,
    Keyboard_O,
    Keyboard_P,
    Keyboard_Q,
    Keyboard_R,
    Keyboard_S,
    Keyboard_T,
    Keyboard_U,
    Keyboard_V,
    Keyboard_W,
    Keyboard_X,
    Keyboard_Y,
    Keyboard_Z,

    // Keyboard (Numbers)
    Keyboard_0,
    Keyboard_1,
    Keyboard_2,
    Keyboard_3,
    Keyboard_4,
    Keyboard_5,
    Keyboard_6,
    Keyboard_7,
    Keyboard_8,
    Keyboard_9,

    // Keyboard (Special)
    Keyboard_Space,
    Keyboard_Enter,
    Keyboard_Escape,
    Keyboard_Tab,
    Keyboard_Shift,
    Keyboard_Ctrl,
    Keyboard_Alt,
    Keyboard_Backspace,

    // Keyboard (Arrows)
    Keyboard_Up,
    Keyboard_Down,
    Keyboard_Left,
    Keyboard_Right,

    // Mouse Buttons
    Mouse_Left,
    Mouse_Right,
    Mouse_Middle,

    // Mouse Axes
    Mouse_X,
    Mouse_Y,
    Mouse_Wheel,

    // GamePad Buttons
    GamePad_A,
    GamePad_B,
    GamePad_X,
    GamePad_Y,
    GamePad_DPadUp,
    GamePad_DPadDown,
    GamePad_DPadLeft,
    GamePad_DPadRight,
    GamePad_Start,
    GamePad_Select,
    GamePad_LeftBumper,
    GamePad_RightBumper,
    GamePad_LeftThumbClick,
    GamePad_RightThumbClick,

    // GamePad Axes
    GamePad_LeftStickX,
    GamePad_LeftStickY,
    GamePad_RightStickX,
    GamePad_RightStickY,
    GamePad_LeftTrigger,
    GamePad_RightTrigger
};

/**
 * @struct InputActionValue
 * @brief アクションの現在値（1D/2Dアナログ、またはデジタル）を保持する構造体
 */
struct InputActionValue {
    float x = 0.0f;
    float y = 0.0f;

    /** @brief デジタル（ボタン）としての入力があるか */
    bool GetAsBool() const {
        return x != 0.0f || y != 0.0f;
    }

    /** @brief 1Dアナログ（トリガーや単一軸）としての値を取得 */
    float GetAsAxis1D() const {
        return x;
    }

    /** @brief 2Dアナログ（スティックやマウス移動）としての値を取得 */
    Irufemi::Vector2 GetAsAxis2D() const {
        return {x, y};
    }
};

/**
 * @enum InputModifier
 * @brief 入力修飾キー（モディファイア）ビットフラグ
 */
enum class InputModifier : uint8_t {
    None = 0,
    Alt = 1 << 0,  ///< VK_MENU (Altキー)
    Ctrl = 1 << 1, ///< VK_CONTROL (Ctrlキー)
    Shift = 1 << 2 ///< VK_SHIFT (Shiftキー)
};

inline InputModifier operator|(InputModifier a, InputModifier b) {
    return static_cast<InputModifier>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline InputModifier operator&(InputModifier a, InputModifier b) {
    return static_cast<InputModifier>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline bool HasModifier(InputModifier flags, InputModifier test) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(test)) != 0;
}

/**
 * @struct InputBinding
 * @brief どのアクションにどの物理入力を割り当てるかのバインディング情報
 */
struct InputBinding {
    InputId id = InputId::Unknown;
    InputModifier requiredModifiers = InputModifier::None; ///< 要求される修飾キー（None は修飾キー押下なしを要求）
    /**
     * @brief X軸に対するスケール値
     * @details 物理入力値を最終的なアクション値に変換する際の係数です。
     *          - 1.0f : そのまま（右や上などプラス方向）
     *          - -1.0f : 反転（左や下などマイナス方向）
     *          - 0.5f など : 感度を半分にする（マウスとパッドの感度合わせ等）
     */
    float scaleX = 1.0f;

    /** @brief Y軸に対するスケール値（用途は scaleX と同じ） */
    float scaleY = 1.0f;

    InputBinding() = default;
    InputBinding(InputId inputId, float sx = 1.0f, float sy = 1.0f)
        : id(inputId), requiredModifiers(InputModifier::None), scaleX(sx), scaleY(sy) {}
    InputBinding(InputId inputId, InputModifier modifiers, float sx = 1.0f, float sy = 1.0f)
        : id(inputId), requiredModifiers(modifiers), scaleX(sx), scaleY(sy) {}
};

/**
 * @brief 指定した InputId がキーボード入力かどうかを判定する
 */
inline bool IsKeyboardInput(InputId id) {
    return id >= InputId::Keyboard_A && id <= InputId::Keyboard_Right;
}

/**
 * @brief 文字列から InputId を解決する
 * @param name "Key_E", "Pad_A", "Mouse_Left" などの識別文字列
 * @return 対応する InputId（見つからない場合は InputId::Unknown）
 */
InputId StringToInputId(const std::string& name);

/**
 * @brief "Alt+Key_Enter", "Ctrl+Key_S" などの修飾子付き文字列から InputId と InputModifier を解決する
 */
bool StringToInputBinding(const std::string& name, InputId& outId, InputModifier& outModifiers);

/**
 * @brief InputId から標準文字列表現を取得する
 */
const char* InputIdToString(InputId id);
