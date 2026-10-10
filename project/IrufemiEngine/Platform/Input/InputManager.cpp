#include "Platform/Input/InputManager.h"
#include "Framework/Utility/CVar.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

void InputManager::Initialize(HWND hwnd) {
    hwnd_ = hwnd;
    keyboard_ = std::make_unique<Keyboard>();
    gamepad_ = std::make_unique<GamePad>();
    mouse_ = std::make_unique<Mouse>();
    keyboard_->Initialize();
    gamepad_->Initialize();
    mouse_->Initialize(hwnd);

    // ウィンドウクライアント領域から論理移動範囲を初期化
    RECT clientRect{};
    if (hwnd && GetClientRect(hwnd, &clientRect)) {
        float width = static_cast<float>(clientRect.right - clientRect.left);
        float height = static_cast<float>(clientRect.bottom - clientRect.top);
        if (width > 0.0f && height > 0.0f) {
            UpdateReferenceResolution(width, height);
            virtualCursorPos_ = {width * 0.5f, height * 0.5f};
        }
    }

    virtualCursorBaseSpeed_ = Irufemi::CVarSystem::GetFloat("i.CursorSpeed");
    if (virtualCursorBaseSpeed_ <= 0.0f) {
        virtualCursorBaseSpeed_ = 650.0f;
    }
}

void InputManager::Update() {
    if (GetForegroundWindow() != hwnd_) {
        // バックグラウンドにいるときは全入力をクリアして更新をスキップ
        keyboard_->Clear();
        gamepad_->Clear();
        mouse_->Clear();
        for (auto& pair : currentActionValues_) {
            pair.second = InputActionValue{};
        }
        for (auto& pair : previousActionValues_) {
            pair.second = InputActionValue{};
        }
        return;
    }

    keyboard_->Update();
    gamepad_->Update();
    mouse_->Update();

    // アクション値の更新
    previousActionValues_ = currentActionValues_;

    // 現在のフレームの値をリセットして再計算
    for (auto& pair : currentActionValues_) {
        pair.second = InputActionValue{};
    }

    // 現在のキーボード修飾キー状態を取得
    InputModifier currentModifiers = InputModifier::None;
    if (keyboard_) {
        if (keyboard_->IsKeyDown(VK_MENU)) {
            currentModifiers = currentModifiers | InputModifier::Alt;
        }
        if (keyboard_->IsKeyDown(VK_CONTROL)) {
            currentModifiers = currentModifiers | InputModifier::Ctrl;
        }
        if (keyboard_->IsKeyDown(VK_SHIFT)) {
            currentModifiers = currentModifiers | InputModifier::Shift;
        }
    }

    // 全てのバインディングを評価（直接参照でゼロアロケーション走査）
    for (const auto& [actionName, bindings] : mappingContext_.GetMappings()) {
        InputActionValue totalValue;

        for (const auto& binding : bindings) {
            // キーボード入力の場合は厳格なモディファイアマッチング（Strict Modifier Matching）を適用
            // 要求された修飾キーと現在の修飾キー状態が完全一致しない場合は無視（Alt押下中の単体Enter等を除外）
            if (IsKeyboardInput(binding.id)) {
                if (binding.requiredModifiers != currentModifiers) {
                    continue;
                }
            }

            float rawValue = GetPhysicalInputValue(binding.id);
            // 簡易的に X 軸にマッピング（2Dスティックなどは別途分岐が必要な場合がありますが、今回は共通化）
            if (binding.id == InputId::GamePad_LeftStickY || binding.id == InputId::GamePad_RightStickY ||
                binding.id == InputId::Mouse_Y) {
                totalValue.y += rawValue * binding.scaleY;
            } else if (binding.id == InputId::GamePad_LeftStickX || binding.id == InputId::GamePad_RightStickX ||
                       binding.id == InputId::Mouse_X) {
                totalValue.x += rawValue * binding.scaleX;
            } else {
                // デフォルトはX軸（1D入力やボタン）
                totalValue.x += rawValue * binding.scaleX;
            }
        }
        currentActionValues_[actionName] = totalValue;
    }
}

// --- アクションベースAPI ---
void InputManager::BindAction(const std::string& actionName, InputId inputId, float scale) {
    BindAction(actionName, inputId, InputModifier::None, scale);
}

void InputManager::BindAction(const std::string& actionName, InputId inputId, InputModifier modifiers, float scale) {
    mappingContext_.AddBinding(actionName, InputBinding(inputId, modifiers, scale, scale));
    // キーが追加されたら、現在のマップにもエントリーを作っておく
    if (currentActionValues_.find(actionName) == currentActionValues_.end()) {
        currentActionValues_[actionName] = InputActionValue{};
        previousActionValues_[actionName] = InputActionValue{};
    }
}

InputActionValue InputManager::GetActionValue(const std::string& actionName) const {
    auto it = currentActionValues_.find(actionName);
    if (it != currentActionValues_.end()) {
        return it->second;
    }
    return InputActionValue{};
}

bool InputManager::IsActionDown(const std::string& actionName) const {
    return GetActionValue(actionName).GetAsBool();
}

bool InputManager::IsActionTriggered(const std::string& actionName) const {
    bool current = IsActionDown(actionName);
    bool previous = false;
    auto it = previousActionValues_.find(actionName);
    if (it != previousActionValues_.end()) {
        previous = it->second.GetAsBool();
    }
    return current && !previous;
}

bool InputManager::IsActionReleased(const std::string& actionName) const {
    bool current = IsActionDown(actionName);
    bool previous = false;
    auto it = previousActionValues_.find(actionName);
    if (it != previousActionValues_.end()) {
        previous = it->second.GetAsBool();
    }
    return !current && previous;
}

void InputManager::ClearActionBindings() {
    mappingContext_.Clear();
    currentActionValues_.clear();
    previousActionValues_.clear();
}

InputId StringToInputId(const std::string& name) {
    static const std::unordered_map<std::string, InputId> kMap = {
        // Keyboard A-Z
        {"Key_A", InputId::Keyboard_A},
        {"Key_B", InputId::Keyboard_B},
        {"Key_C", InputId::Keyboard_C},
        {"Key_D", InputId::Keyboard_D},
        {"Key_E", InputId::Keyboard_E},
        {"Key_F", InputId::Keyboard_F},
        {"Key_G", InputId::Keyboard_G},
        {"Key_H", InputId::Keyboard_H},
        {"Key_I", InputId::Keyboard_I},
        {"Key_J", InputId::Keyboard_J},
        {"Key_K", InputId::Keyboard_K},
        {"Key_L", InputId::Keyboard_L},
        {"Key_M", InputId::Keyboard_M},
        {"Key_N", InputId::Keyboard_N},
        {"Key_O", InputId::Keyboard_O},
        {"Key_P", InputId::Keyboard_P},
        {"Key_Q", InputId::Keyboard_Q},
        {"Key_R", InputId::Keyboard_R},
        {"Key_S", InputId::Keyboard_S},
        {"Key_T", InputId::Keyboard_T},
        {"Key_U", InputId::Keyboard_U},
        {"Key_V", InputId::Keyboard_V},
        {"Key_W", InputId::Keyboard_W},
        {"Key_X", InputId::Keyboard_X},
        {"Key_Y", InputId::Keyboard_Y},
        {"Key_Z", InputId::Keyboard_Z},
        // Keyboard 0-9
        {"Key_0", InputId::Keyboard_0},
        {"Key_1", InputId::Keyboard_1},
        {"Key_2", InputId::Keyboard_2},
        {"Key_3", InputId::Keyboard_3},
        {"Key_4", InputId::Keyboard_4},
        {"Key_5", InputId::Keyboard_5},
        {"Key_6", InputId::Keyboard_6},
        {"Key_7", InputId::Keyboard_7},
        {"Key_8", InputId::Keyboard_8},
        {"Key_9", InputId::Keyboard_9},
        // Special
        {"Key_Space", InputId::Keyboard_Space},
        {"Key_Enter", InputId::Keyboard_Enter},
        {"Key_Escape", InputId::Keyboard_Escape},
        {"Key_Tab", InputId::Keyboard_Tab},
        {"Key_Shift", InputId::Keyboard_Shift},
        {"Key_Ctrl", InputId::Keyboard_Ctrl},
        {"Key_Alt", InputId::Keyboard_Alt},
        {"Key_Back", InputId::Keyboard_Backspace},
        {"Key_Backspace", InputId::Keyboard_Backspace},
        // Arrows
        {"Key_Up", InputId::Keyboard_Up},
        {"Key_Down", InputId::Keyboard_Down},
        {"Key_Left", InputId::Keyboard_Left},
        {"Key_Right", InputId::Keyboard_Right},
        // Mouse
        {"Mouse_Left", InputId::Mouse_Left},
        {"Mouse_Right", InputId::Mouse_Right},
        {"Mouse_Middle", InputId::Mouse_Middle},
        {"Mouse_X", InputId::Mouse_X},
        {"Mouse_Y", InputId::Mouse_Y},
        {"Mouse_Wheel", InputId::Mouse_Wheel},
        // GamePad
        {"Pad_A", InputId::GamePad_A},
        {"Pad_B", InputId::GamePad_B},
        {"Pad_X", InputId::GamePad_X},
        {"Pad_Y", InputId::GamePad_Y},
        {"Pad_DPadUp", InputId::GamePad_DPadUp},
        {"Pad_DPadDown", InputId::GamePad_DPadDown},
        {"Pad_DPadLeft", InputId::GamePad_DPadLeft},
        {"Pad_DPadRight", InputId::GamePad_DPadRight},
        {"Pad_Start", InputId::GamePad_Start},
        {"Pad_Select", InputId::GamePad_Select},
        {"Pad_LeftBumper", InputId::GamePad_LeftBumper},
        {"Pad_LB", InputId::GamePad_LeftBumper},
        {"Pad_RightBumper", InputId::GamePad_RightBumper},
        {"Pad_RB", InputId::GamePad_RightBumper},
        {"Pad_LeftShoulder", InputId::GamePad_LeftBumper},
        {"Pad_RightShoulder", InputId::GamePad_RightBumper},
        {"Pad_LeftThumbClick", InputId::GamePad_LeftThumbClick},
        {"Pad_RightThumbClick", InputId::GamePad_RightThumbClick},
        {"Pad_LeftStickX", InputId::GamePad_LeftStickX},
        {"Pad_LeftStickY", InputId::GamePad_LeftStickY},
        {"Pad_RightStickX", InputId::GamePad_RightStickX},
        {"Pad_RightStickY", InputId::GamePad_RightStickY},
        {"Pad_LeftTrigger", InputId::GamePad_LeftTrigger},
        {"Pad_LT", InputId::GamePad_LeftTrigger},
        {"Pad_RightTrigger", InputId::GamePad_RightTrigger},
        {"Pad_RT", InputId::GamePad_RightTrigger}};
    auto it = kMap.find(name);
    return (it != kMap.end()) ? it->second : InputId::Unknown;
}

bool StringToInputBinding(const std::string& name, InputId& outId, InputModifier& outModifiers) {
    outId = InputId::Unknown;
    outModifiers = InputModifier::None;
    if (name.empty()) {
        return false;
    }

    size_t lastPlus = name.find_last_of('+');
    std::string keyPart = (lastPlus != std::string::npos) ? name.substr(lastPlus + 1) : name;
    outId = StringToInputId(keyPart);
    if (outId == InputId::Unknown) {
        return false;
    }

    if (lastPlus != std::string::npos) {
        std::string modPart = name.substr(0, lastPlus);
        if (modPart.find("Alt") != std::string::npos) {
            outModifiers = outModifiers | InputModifier::Alt;
        }
        if (modPart.find("Ctrl") != std::string::npos || modPart.find("Control") != std::string::npos) {
            outModifiers = outModifiers | InputModifier::Ctrl;
        }
        if (modPart.find("Shift") != std::string::npos) {
            outModifiers = outModifiers | InputModifier::Shift;
        }
    }
    return true;
}

const char* InputIdToString(InputId id) {
    static const std::unordered_map<InputId, const char*> kInputIdNames = {
        {InputId::Keyboard_Space, "Key_Space"},
        {InputId::Keyboard_Enter, "Key_Enter"},
        {InputId::Keyboard_Escape, "Key_Escape"},
        {InputId::Mouse_Left, "Mouse_Left"},
        {InputId::Mouse_Right, "Mouse_Right"},
        {InputId::GamePad_A, "Pad_A"},
        {InputId::GamePad_B, "Pad_B"},
        {InputId::GamePad_X, "Pad_X"},
        {InputId::GamePad_Y, "Pad_Y"},
        {InputId::GamePad_Start, "Pad_Start"},
        {InputId::GamePad_LeftTrigger, "Pad_LT"},
        {InputId::GamePad_RightTrigger, "Pad_RT"},
    };

    if (auto it = kInputIdNames.find(id); it != kInputIdNames.end()) {
        return it->second;
    }
    return "Unknown";
}

bool InputManager::LoadBindingsFromJson(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    nlohmann::json root;
    try {
        file >> root;
    } catch (...) {
        return false;
    }

    if (root.contains("Actions") && root["Actions"].is_object()) {
        for (auto& [actionName, keys] : root["Actions"].items()) {
            if (keys.is_array()) {
                for (const auto& keyItem : keys) {
                    if (keyItem.is_string()) {
                        InputId id = InputId::Unknown;
                        InputModifier mod = InputModifier::None;
                        if (StringToInputBinding(keyItem.get<std::string>(), id, mod)) {
                            BindAction(actionName, id, mod, 1.0f);
                        }
                    }
                }
            }
        }
    }

    if (root.contains("Axes") && root["Axes"].is_object()) {
        for (auto& [axisName, sources] : root["Axes"].items()) {
            if (sources.is_array()) {
                for (const auto& item : sources) {
                    if (item.is_object() && item.contains("Source")) {
                        std::string src = item["Source"].get<std::string>();
                        float scale = item.value("Scale", 1.0f);
                        InputId id = InputId::Unknown;
                        InputModifier mod = InputModifier::None;
                        if (StringToInputBinding(src, id, mod)) {
                            BindAction(axisName, id, mod, scale);
                        }
                    }
                }
            }
        }
    }
    return true;
}

float InputManager::GetPhysicalInputValue(InputId id) const {
    switch (id) {
    // --- Keyboard (一部抜粋、必要に応じて拡充) ---
    case InputId::Keyboard_A:
        return keyboard_->IsKeyDown('A') ? 1.0f : 0.0f;
    case InputId::Keyboard_B:
        return keyboard_->IsKeyDown('B') ? 1.0f : 0.0f;
    case InputId::Keyboard_C:
        return keyboard_->IsKeyDown('C') ? 1.0f : 0.0f;
    case InputId::Keyboard_D:
        return keyboard_->IsKeyDown('D') ? 1.0f : 0.0f;
    case InputId::Keyboard_E:
        return keyboard_->IsKeyDown('E') ? 1.0f : 0.0f;
    case InputId::Keyboard_F:
        return keyboard_->IsKeyDown('F') ? 1.0f : 0.0f;
    case InputId::Keyboard_G:
        return keyboard_->IsKeyDown('G') ? 1.0f : 0.0f;
    case InputId::Keyboard_H:
        return keyboard_->IsKeyDown('H') ? 1.0f : 0.0f;
    case InputId::Keyboard_I:
        return keyboard_->IsKeyDown('I') ? 1.0f : 0.0f;
    case InputId::Keyboard_J:
        return keyboard_->IsKeyDown('J') ? 1.0f : 0.0f;
    case InputId::Keyboard_K:
        return keyboard_->IsKeyDown('K') ? 1.0f : 0.0f;
    case InputId::Keyboard_L:
        return keyboard_->IsKeyDown('L') ? 1.0f : 0.0f;
    case InputId::Keyboard_M:
        return keyboard_->IsKeyDown('M') ? 1.0f : 0.0f;
    case InputId::Keyboard_N:
        return keyboard_->IsKeyDown('N') ? 1.0f : 0.0f;
    case InputId::Keyboard_O:
        return keyboard_->IsKeyDown('O') ? 1.0f : 0.0f;
    case InputId::Keyboard_P:
        return keyboard_->IsKeyDown('P') ? 1.0f : 0.0f;
    case InputId::Keyboard_Q:
        return keyboard_->IsKeyDown('Q') ? 1.0f : 0.0f;
    case InputId::Keyboard_R:
        return keyboard_->IsKeyDown('R') ? 1.0f : 0.0f;
    case InputId::Keyboard_S:
        return keyboard_->IsKeyDown('S') ? 1.0f : 0.0f;
    case InputId::Keyboard_T:
        return keyboard_->IsKeyDown('T') ? 1.0f : 0.0f;
    case InputId::Keyboard_U:
        return keyboard_->IsKeyDown('U') ? 1.0f : 0.0f;
    case InputId::Keyboard_V:
        return keyboard_->IsKeyDown('V') ? 1.0f : 0.0f;
    case InputId::Keyboard_W:
        return keyboard_->IsKeyDown('W') ? 1.0f : 0.0f;
    case InputId::Keyboard_X:
        return keyboard_->IsKeyDown('X') ? 1.0f : 0.0f;
    case InputId::Keyboard_Y:
        return keyboard_->IsKeyDown('Y') ? 1.0f : 0.0f;
    case InputId::Keyboard_Z:
        return keyboard_->IsKeyDown('Z') ? 1.0f : 0.0f;

    case InputId::Keyboard_Space:
        return keyboard_->IsKeyDown(VK_SPACE) ? 1.0f : 0.0f;
    case InputId::Keyboard_Enter:
        return keyboard_->IsKeyDown(VK_RETURN) ? 1.0f : 0.0f;
    case InputId::Keyboard_Escape:
        return keyboard_->IsKeyDown(VK_ESCAPE) ? 1.0f : 0.0f;
    case InputId::Keyboard_Shift:
        return (keyboard_->IsKeyDown(VK_LSHIFT) || keyboard_->IsKeyDown(VK_RSHIFT)) ? 1.0f : 0.0f;
    case InputId::Keyboard_Up:
        return keyboard_->IsKeyDown(VK_UP) ? 1.0f : 0.0f;
    case InputId::Keyboard_Down:
        return keyboard_->IsKeyDown(VK_DOWN) ? 1.0f : 0.0f;
    case InputId::Keyboard_Left:
        return keyboard_->IsKeyDown(VK_LEFT) ? 1.0f : 0.0f;
    case InputId::Keyboard_Right:
        return keyboard_->IsKeyDown(VK_RIGHT) ? 1.0f : 0.0f;

    // --- Mouse ---
    case InputId::Mouse_Left:
        return mouse_->IsButtonDown(Mouse::Button::Left) ? 1.0f : 0.0f;
    case InputId::Mouse_Right:
        return mouse_->IsButtonDown(Mouse::Button::Right) ? 1.0f : 0.0f;
    case InputId::Mouse_Middle:
        return mouse_->IsButtonDown(Mouse::Button::Middle) ? 1.0f : 0.0f;
    case InputId::Mouse_X:
        return mouse_->GetDelta().x;
    case InputId::Mouse_Y:
        return mouse_->GetDelta().y;
    case InputId::Mouse_Wheel:
        return mouse_->GetWheelDelta();

    // --- GamePad ---
    case InputId::GamePad_A:
        return gamepad_->IsButtonDown(XINPUT_GAMEPAD_A) ? 1.0f : 0.0f;
    case InputId::GamePad_B:
        return gamepad_->IsButtonDown(XINPUT_GAMEPAD_B) ? 1.0f : 0.0f;
    case InputId::GamePad_X:
        return gamepad_->IsButtonDown(XINPUT_GAMEPAD_X) ? 1.0f : 0.0f;
    case InputId::GamePad_Y:
        return gamepad_->IsButtonDown(XINPUT_GAMEPAD_Y) ? 1.0f : 0.0f;
    case InputId::GamePad_DPadUp:
        return gamepad_->IsButtonDown(XINPUT_GAMEPAD_DPAD_UP) ? 1.0f : 0.0f;
    case InputId::GamePad_DPadDown:
        return gamepad_->IsButtonDown(XINPUT_GAMEPAD_DPAD_DOWN) ? 1.0f : 0.0f;
    case InputId::GamePad_DPadLeft:
        return gamepad_->IsButtonDown(XINPUT_GAMEPAD_DPAD_LEFT) ? 1.0f : 0.0f;
    case InputId::GamePad_DPadRight:
        return gamepad_->IsButtonDown(XINPUT_GAMEPAD_DPAD_RIGHT) ? 1.0f : 0.0f;
    case InputId::GamePad_Start:
        return gamepad_->IsButtonDown(XINPUT_GAMEPAD_START) ? 1.0f : 0.0f;
    case InputId::GamePad_Select:
        return gamepad_->IsButtonDown(XINPUT_GAMEPAD_BACK) ? 1.0f : 0.0f;
    case InputId::GamePad_LeftBumper:
        return gamepad_->IsButtonDown(XINPUT_GAMEPAD_LEFT_SHOULDER) ? 1.0f : 0.0f;
    case InputId::GamePad_RightBumper:
        return gamepad_->IsButtonDown(XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1.0f : 0.0f;
    case InputId::GamePad_LeftStickX:
        return gamepad_->GetLeftStickX();
    case InputId::GamePad_LeftStickY:
        return gamepad_->GetLeftStickY();
    case InputId::GamePad_RightStickX:
        return gamepad_->GetRightStickX();
    case InputId::GamePad_RightStickY:
        return gamepad_->GetRightStickY();
    case InputId::GamePad_LeftTrigger:
        return gamepad_->GetLeftTrigger();
    case InputId::GamePad_RightTrigger:
        return gamepad_->GetRightTrigger();
    default:
        return 0.0f;
    }
}

// --- 旧APIフォワード(互換維持)---
bool InputManager::IsKeyDown(uint8_t k) const {
    return keyboard_->IsKeyDown(k);
}
bool InputManager::IsKeyUp(uint8_t k) const {
    return keyboard_->IsKeyUp(k);
}
bool InputManager::IsKeyPressed(uint8_t k) const {
    return keyboard_->IsKeyPressed(k);
}
bool InputManager::IsKeyReleased(uint8_t k) const {
    return keyboard_->IsKeyReleased(k);
}
void InputManager::ConsumeKey(uint8_t key) {
    if (keyboard_) {
        keyboard_->ConsumeKey(key);
    }
}

bool InputManager::IsButtonDown(WORD b) const {
    return gamepad_->IsButtonDown(b);
}
bool InputManager::IsButtonUp(WORD b) const {
    return gamepad_->IsButtonUp(b);
}
bool InputManager::IsButtonPressed(WORD b) const {
    return gamepad_->IsButtonPressed(b);
}
bool InputManager::IsButtonReleased(WORD b) const {
    return gamepad_->IsButtonReleased(b);
}

float InputManager::GetLeftStickX() const {
    return gamepad_->GetLeftStickX();
}
float InputManager::GetLeftStickY() const {
    return gamepad_->GetLeftStickY();
}
float InputManager::GetRightStickX() const {
    return gamepad_->GetRightStickX();
}
float InputManager::GetRightStickY() const {
    return gamepad_->GetRightStickY();
}
float InputManager::GetLeftTrigger() const {
    return gamepad_ ? gamepad_->GetLeftTrigger() : 0.0f;
}
float InputManager::GetRightTrigger() const {
    return gamepad_ ? gamepad_->GetRightTrigger() : 0.0f;
}
bool InputManager::IsLeftTriggerDown(float threshold) const {
    if (!gamepad_) {
        return false;
    }
    uint8_t byteTh = static_cast<uint8_t>(std::clamp(threshold * 255.0f, 0.0f, 255.0f));
    return gamepad_->LeftTriggerDown(byteTh);
}
bool InputManager::IsLeftTriggerPressed(float threshold) const {
    if (!gamepad_) {
        return false;
    }
    uint8_t byteTh = static_cast<uint8_t>(std::clamp(threshold * 255.0f, 0.0f, 255.0f));
    return gamepad_->LeftTriggerPressed(byteTh);
}
bool InputManager::IsRightTriggerDown(float threshold) const {
    if (!gamepad_) {
        return false;
    }
    uint8_t byteTh = static_cast<uint8_t>(std::clamp(threshold * 255.0f, 0.0f, 255.0f));
    return gamepad_->RightTriggerDown(byteTh);
}
bool InputManager::IsRightTriggerPressed(float threshold) const {
    if (!gamepad_) {
        return false;
    }
    uint8_t byteTh = static_cast<uint8_t>(std::clamp(threshold * 255.0f, 0.0f, 255.0f));
    return gamepad_->RightTriggerPressed(byteTh);
}

bool InputManager::IsKeyDownDIK(uint8_t d) const {
    return keyboard_->IsKeyDownDIK(d);
}
bool InputManager::IsKeyUpDIK(uint8_t d) const {
    return keyboard_->IsKeyUpDIK(d);
}
bool InputManager::IsKeyPressedDIK(uint8_t d) const {
    return keyboard_->IsKeyPressedDIK(d);
}
bool InputManager::IsKeyReleasedDIK(uint8_t d) const {
    return keyboard_->IsKeyReleasedDIK(d);
}

// START フォワード
bool InputManager::StartDown() const {
    return gamepad_->StartDown();
}
bool InputManager::StartPressed() const {
    return gamepad_->StartPressed();
}
bool InputManager::StartReleased() const {
    return gamepad_->StartReleased();
}

// --- D-Pad フォワード ---
bool InputManager::DPadUp() const {
    return gamepad_->DPadUp();
}
bool InputManager::DPadDown() const {
    return gamepad_->DPadDown();
}
bool InputManager::DPadLeft() const {
    return gamepad_->DPadLeft();
}
bool InputManager::DPadRight() const {
    return gamepad_->DPadRight();
}
bool InputManager::DPadUpPressed() const {
    return gamepad_->DPadUpPressed();
}
bool InputManager::DPadDownPressed() const {
    return gamepad_->DPadDownPressed();
}
bool InputManager::DPadLeftPressed() const {
    return gamepad_->DPadLeftPressed();
}
bool InputManager::DPadRightPressed() const {
    return gamepad_->DPadRightPressed();
}

// --- マウスAPIフォワード ---
bool InputManager::IsMouseButtonDown(Mouse::Button button) const {
    return mouse_->IsButtonDown(button);
}

bool InputManager::IsMouseButtonPressed(Mouse::Button button) const {
    return mouse_->IsButtonPressed(button);
}

bool InputManager::IsMouseButtonReleased(Mouse::Button button) const {
    return mouse_->IsButtonReleased(button);
}

const Irufemi::Vector2& InputManager::GetMousePosition() const {
    return mouse_->GetPosition();
}

const Irufemi::Vector2& InputManager::GetMouseDelta() const {
    return mouse_->GetDelta();
}

float InputManager::GetMouseWheelDelta() const {
    return mouse_->GetWheelDelta();
}

// --- 仮想カーソル（Virtual Cursor） ---
void InputManager::UpdateVirtualCursor(float deltaTime, float speedMultiplier) {
    // 1. 物理マウスの移動検出
    if (mouse_) {
        const Irufemi::Vector2& currentMousePos = mouse_->GetPosition();
        float mouseDist =
            std::hypot(currentMousePos.x - lastPhysicalMousePos_.x, currentMousePos.y - lastPhysicalMousePos_.y);
        bool mouseClicked = mouse_->IsButtonPressed(Mouse::Button::Left) || IsKeyPressed(VK_LBUTTON);

        // 物理マウスの現在地を常に記録（スティック操作では書き換えない）
        lastPhysicalMousePos_ = currentMousePos;

        // 実際にマウスが動かされたか、クリックされた場合のみマウス操作に切り替える
        if (mouseDist > 2.0f || mouseClicked) {
            isUsingGamepadCursor_ = false;
            virtualCursorPos_ = currentMousePos;
        }
    }

    // 2. ゲームパッドスティック入力による移動（右スティックまたは左スティック）
    if (gamepad_ && gamepad_->IsConnected()) {
        float rStickX = gamepad_->GetRightStickX();
        float rStickY = gamepad_->GetRightStickY();
        float rMagnitude = std::hypot(rStickX, rStickY);

        float lStickX = gamepad_->GetLeftStickX();
        float lStickY = gamepad_->GetLeftStickY();
        float lMagnitude = std::hypot(lStickX, lStickY);

        constexpr float kDeadZone = 0.15f;
        float moveX = 0.0f;
        float moveY = 0.0f;

        // 右スティック（エイミング用）の入力を優先し、なければ左スティック（メニュー等）を採用
        if (rMagnitude > kDeadZone) {
            moveX = rStickX;
            moveY = rStickY;
        } else if (lMagnitude > kDeadZone) {
            moveX = lStickX;
            moveY = lStickY;
        }

        float rawMag = std::hypot(moveX, moveY);
        if (rawMag > kDeadZone) {
            isUsingGamepadCursor_ = true;

            // 円形デッドゾーンを正規化 (0.0f ~ 1.0f)
            float normalizedMag = std::clamp((rawMag - kDeadZone) / (1.0f - kDeadZone), 0.0f, 1.0f);

            // AAA水準のレスポンス曲線（Apex/Destiny調の指数カーブ: x^1.4）
            // 微小入力では精密に、深く倒した時は素早く移動
            float curvedMag = std::pow(normalizedMag, 1.4f);

            // 方向正規化
            float dirX = moveX / rawMag;
            float dirY = moveY / rawMag;

            // CVar設定値との動的同期
            float cvarSpeed = Irufemi::CVarSystem::GetFloat("i.CursorSpeed");
            if (cvarSpeed > 0.0f) {
                virtualCursorBaseSpeed_ = cvarSpeed;
            }
            float speed = virtualCursorBaseSpeed_ * speedMultiplier;

            virtualCursorPos_.x += dirX * curvedMag * speed * deltaTime;
            virtualCursorPos_.y -= dirY * curvedMag * speed * deltaTime; // スティック上(+)は画面上(-)
        }
    }

    // 画面解像度枠内にクランプ（動的設定境界に準拠）
    virtualCursorPos_.x = std::clamp(virtualCursorPos_.x, virtualCursorBoundsMin_.x, virtualCursorBoundsMax_.x);
    virtualCursorPos_.y = std::clamp(virtualCursorPos_.y, virtualCursorBoundsMin_.y, virtualCursorBoundsMax_.y);
}

bool InputManager::IsCursorActionDown() const {
    bool down = (gamepad_ && gamepad_->IsButtonDown(XINPUT_GAMEPAD_A));
    if (mouse_ && mouse_->IsButtonDown(Mouse::Button::Left)) {
        down = true;
    }
    if (IsKeyDown(VK_LBUTTON)) {
        down = true;
    }
    return down;
}

bool InputManager::IsCursorActionPressed() const {
    bool pressed = (gamepad_ && gamepad_->IsButtonPressed(XINPUT_GAMEPAD_A));
    if (mouse_ && mouse_->IsButtonPressed(Mouse::Button::Left)) {
        pressed = true;
    }
    if (IsKeyPressed(VK_LBUTTON)) {
        pressed = true;
    }
    return pressed;
}

bool InputManager::IsCursorActionReleased() const {
    bool released = (gamepad_ && gamepad_->IsButtonReleased(XINPUT_GAMEPAD_A));
    if (mouse_ && mouse_->IsButtonReleased(Mouse::Button::Left)) {
        released = true;
    }
    if (IsKeyReleased(VK_LBUTTON)) {
        released = true;
    }
    return released;
}

bool InputManager::IsCancelPressed() const {
    bool cancel = (gamepad_ && gamepad_->IsButtonPressed(XINPUT_GAMEPAD_B));
    if (IsKeyPressed(VK_ESCAPE) || IsKeyPressed(VK_BACK)) {
        cancel = true;
    }
    return cancel;
}

bool InputManager::HasAnyInput() const {
    // 1. ゲームパッドの入力判定（接続時のみ）
    if (gamepad_ && gamepad_->IsConnected()) {
        const auto& state = gamepad_->GetState();
        // A. いずれかのボタンが押されているか（十字キー、ABXY、LB/RB、START/BACK、LS/RS等）
        if (state.Gamepad.wButtons != 0) {
            return true;
        }

        // B. アナログトリガー（LT / RT）が押されているか
        if (gamepad_->GetLeftTrigger() > 0.15f || gamepad_->GetRightTrigger() > 0.15f) {
            return true;
        }

        // C. アナログスティック（左 / 右）がドリフト耐性閾値を超えて意図的に倒されているか
        constexpr float kIntentionalStickThreshold = 0.20f;
        float lMag = std::hypot(gamepad_->GetLeftStickX(), gamepad_->GetLeftStickY());
        float rMag = std::hypot(gamepad_->GetRightStickX(), gamepad_->GetRightStickY());
        if (lMag > kIntentionalStickThreshold || rMag > kIntentionalStickThreshold) {
            return true;
        }
    }

    // 2. マウスの入力判定
    if (mouse_) {
        // A. いずれかのマウスボタンが押されているか（左 / 右 / 中）
        if (mouse_->IsButtonDown(Mouse::Button::Left) || mouse_->IsButtonDown(Mouse::Button::Right) ||
            mouse_->IsButtonDown(Mouse::Button::Middle)) {
            return true;
        }

        // B. マウスホイールが回転されたか
        if (std::abs(mouse_->GetWheelDelta()) > 0.001f) {
            return true;
        }

        // C. 物理マウスが机の振動ノイズ（ジッター）を超えて意図的に移動されたか
        const auto& delta = mouse_->GetDelta();
        if (std::hypot(delta.x, delta.y) > 2.0f) {
            return true;
        }
    }

    // 3. キーボードの入力判定（標準キー 0x08[BackSpace] 〜 0xDF[OEM記号] の走査）
    // ※ 0x01〜0x06 (マウス仮想キー) はマウス側で判定済み、0xE0〜0xFF は日本語IME/OEM常時ONビットのため除外
    if (keyboard_) {
        for (size_t i = 0x08; i <= 0xDF; ++i) {
            if (keyboard_->IsKeyDown(static_cast<uint8_t>(i))) {
                return true;
            }
        }
    }

    return false;
}