#include "Platform/Input/Keyboard.h"

#include <unordered_map>

void Keyboard::Initialize() {
    currentKeys_.fill(0);
    previousKeys_.fill(0);
}

void Keyboard::Update() {
    previousKeys_ = currentKeys_;
    if (!::GetKeyboardState(currentKeys_.data())) {
        // エラー処理: 失敗時は currentKeys_ をクリア
        currentKeys_.fill(0);
    }
}

void Keyboard::Clear() {
    currentKeys_.fill(0);
    previousKeys_.fill(0);
}

void Keyboard::ConsumeKey(uint8_t key) {
    currentKeys_[key] = 0;
    previousKeys_[key] = 0;
}

bool Keyboard::IsKeyDown(uint8_t key) const {
    return (currentKeys_[key] & 0x80) != 0;
}
bool Keyboard::IsKeyUp(uint8_t key) const {
    return (currentKeys_[key] & 0x80) == 0;
}
bool Keyboard::IsKeyPressed(uint8_t key) const {
    return !(previousKeys_[key] & 0x80) && (currentKeys_[key] & 0x80);
}
bool Keyboard::IsKeyReleased(uint8_t key) const {
    return (previousKeys_[key] & 0x80) && !(currentKeys_[key] & 0x80);
}

// よく使うキーの表引き(必要に応じて拡張)
static uint8_t LookupVKFromDIK(uint8_t dik) {
    static const std::unordered_map<uint8_t, uint8_t> kMap = {
        // 文字キー(WASDなどはVK_がそのまま)
        {0x1E /*DIK_A*/, 'A'},
        {0x30 /*DIK_B*/, 'B'},
        {0x2E /*DIK_C*/, 'C'},
        {0x20 /*DIK_D*/, 'D'},
        {0x12 /*DIK_E*/, 'E'},
        {0x21 /*DIK_F*/, 'F'},
        {0x22 /*DIK_G*/, 'G'},
        {0x23 /*DIK_H*/, 'H'},
        {0x17 /*DIK_I*/, 'I'},
        {0x24 /*DIK_J*/, 'J'},
        {0x25 /*DIK_K*/, 'K'},
        {0x26 /*DIK_L*/, 'L'},
        {0x32 /*DIK_M*/, 'M'},
        {0x31 /*DIK_N*/, 'N'},
        {0x18 /*DIK_O*/, 'O'},
        {0x19 /*DIK_P*/, 'P'},
        {0x10 /*DIK_Q*/, 'Q'},
        {0x13 /*DIK_R*/, 'R'},
        {0x1F /*DIK_S*/, 'S'},
        {0x14 /*DIK_T*/, 'T'},
        {0x16 /*DIK_U*/, 'U'},
        {0x2F /*DIK_V*/, 'V'},
        {0x11 /*DIK_W*/, 'W'},
        {0x2D /*DIK_X*/, 'X'},
        {0x15 /*DIK_Y*/, 'Y'},
        {0x2C /*DIK_Z*/, 'Z'},
        // 数字行
        {0x0B /*DIK_0*/, '0'},
        {0x02 /*DIK_1*/, '1'},
        {0x03 /*DIK_2*/, '2'},
        {0x04 /*DIK_3*/, '3'},
        {0x05 /*DIK_4*/, '4'},
        {0x06 /*DIK_5*/, '5'},
        {0x07 /*DIK_6*/, '6'},
        {0x08 /*DIK_7*/, '7'},
        {0x09 /*DIK_8*/, '8'},
        {0x0A /*DIK_9*/, '9'},
        // 修飾・制御
        {0x01 /*DIK_ESCAPE*/, VK_ESCAPE},
        {0x0F /*DIK_TAB*/, VK_TAB},
        {0x39 /*DIK_SPACE*/, VK_SPACE},
        {0x1C /*DIK_RETURN*/, VK_RETURN},
        {0x0E /*DIK_BACK*/, VK_BACK},
        {0x2A /*DIK_LSHIFT*/, VK_LSHIFT},
        {0x36 /*DIK_RSHIFT*/, VK_RSHIFT},
        {0x1D /*DIK_LCONTROL*/, VK_LCONTROL},
        {0x9D /*DIK_RCONTROL*/, VK_RCONTROL},
        {0x38 /*DIK_LALT*/, VK_LMENU},
        {0xB8 /*DIK_RALT*/, VK_RMENU},
        // 矢印キー
        {0xC8 /*DIK_UP*/, VK_UP},
        {0xD0 /*DIK_DOWN*/, VK_DOWN},
        {0xCB /*DIK_LEFT*/, VK_LEFT},
        {0xCD /*DIK_RIGHT*/, VK_RIGHT},
        // F1〜F12
        {0x3B /*DIK_F1*/, VK_F1},
        {0x3C /*DIK_F2*/, VK_F2},
        {0x3D /*DIK_F3*/, VK_F3},
        {0x3E /*DIK_F4*/, VK_F4},
        {0x3F /*DIK_F5*/, VK_F5},
        {0x40 /*DIK_F6*/, VK_F6},
        {0x41 /*DIK_F7*/, VK_F7},
        {0x42 /*DIK_F8*/, VK_F8},
        {0x43 /*DIK_F9*/, VK_F9},
        {0x44 /*DIK_F10*/, VK_F10},
        {0x57 /*DIK_F11*/, VK_F11},
        {0x58 /*DIK_F12*/, VK_F12},
        // --- テンキー(NUMPAD) ---
        {0x52 /*DIK_NUMPAD0*/, VK_NUMPAD0},
        {0x4F /*DIK_NUMPAD1*/, VK_NUMPAD1},
        {0x50 /*DIK_NUMPAD2*/, VK_NUMPAD2},
        {0x51 /*DIK_NUMPAD3*/, VK_NUMPAD3},
        {0x4B /*DIK_NUMPAD4*/, VK_NUMPAD4},
        {0x4C /*DIK_NUMPAD5*/, VK_NUMPAD5},
        {0x4D /*DIK_NUMPAD6*/, VK_NUMPAD6},
        {0x47 /*DIK_NUMPAD7*/, VK_NUMPAD7},
        {0x48 /*DIK_NUMPAD8*/, VK_NUMPAD8},
        {0x49 /*DIK_NUMPAD9*/, VK_NUMPAD9},
        {0x4A /*DIK_SUBTRACT*/, VK_SUBTRACT},
        {0x4E /*DIK_ADD*/, VK_ADD},
        {0x37 /*DIK_MULTIPLY*/, VK_MULTIPLY},
        {0xB5 /*DIK_DIVIDE*/, VK_DIVIDE},
        {0x53 /*DIK_DECIMAL*/, VK_DECIMAL},
        // ※ NUMPAD Enter は DIKで 0x9C。VKは VK_RETURN と同じ扱いになる点に注意
        {0x9C /*DIK_NUMPADENTER*/, VK_RETURN},

        // --- 追記例: OEMキー(記号系) ---
        // JP/US 配列差の影響を抑えたいときに固定
        {0x0D /*DIK_MINUS*/, VK_OEM_MINUS},   // '-' '_'
        {0x0C /*DIK_EQUALS*/, VK_OEM_PLUS},   // '=' '+'
        {0x1A /*DIK_LBRACKET*/, VK_OEM_4},    // '[' '{'
        {0x1B /*DIK_RBRACKET*/, VK_OEM_6},    // ']' '}'
        {0x2B /*DIK_BACKSLASH*/, VK_OEM_5},   // '\\' '|'
        {0x27 /*DIK_SEMICOLON*/, VK_OEM_1},   // ';' ':'
        {0x28 /*DIK_APOSTROPHE*/, VK_OEM_7},  // '\'' '"'
        {0x33 /*DIK_COMMA*/, VK_OEM_COMMA},   // ',' '<'
        {0x34 /*DIK_PERIOD*/, VK_OEM_PERIOD}, // '.' '>'
        {0x35 /*DIK_SLASH*/, VK_OEM_2},       // '/' '?'
        {0x29 /*DIK_GRAVE*/, VK_OEM_3},       // '`' '~'

        // ---(必要なら)日本語配列系(任意)---
        // 無変換/変換/かな切替/102キー(＼/ろ)など
        {0x7B /*DIK_CONVERT*/, VK_CONVERT},
        {0x79 /*DIK_NOCONVERT*/, VK_NONCONVERT},
        {0x70 /*DIK_KANA*/, VK_KANA},
        {0x73 /*DIK_ROEM_102?*/, VK_OEM_102}, // JISの＼/ろキー相当(環境差あり)
    };
    if (auto it = kMap.find(dik); it != kMap.end()) {
        return it->second;
    }
    return 0; // 0 は未定義
}

// DIK -> VK 変換(表引き→フォールバック)
uint8_t Keyboard::DIKToVK(uint8_t dik) {
    if (uint8_t vk = LookupVKFromDIK(dik)) {
        return vk;
    }

    // フォールバック：スキャンコード→VK(拡張キー考慮)
    // DIK の拡張系(例: 0xE0 系)は数値が 0x80 以上に出ることが多いので、拡張ビットを与える
    UINT sc = dik;
    if (dik & 0x80u) {
        sc |= 0x100u; // 拡張の保険(必要に応じて調整)
    }
    UINT vk = MapVirtualKeyEx(sc, MAPVK_VSC_TO_VK_EX, GetKeyboardLayout(0));
    return static_cast<uint8_t>(vk);
}

// DIK 版 判定API
bool Keyboard::IsKeyDownDIK(uint8_t dik) const {
    uint8_t vk = DIKToVK(dik);
    return vk ? IsKeyDown(vk) : false;
}
bool Keyboard::IsKeyUpDIK(uint8_t dik) const {
    uint8_t vk = DIKToVK(dik);
    return vk ? IsKeyUp(vk) : false;
}
bool Keyboard::IsKeyPressedDIK(uint8_t dik) const {
    uint8_t vk = DIKToVK(dik);
    return vk ? IsKeyPressed(vk) : false;
}
bool Keyboard::IsKeyReleasedDIK(uint8_t dik) const {
    uint8_t vk = DIKToVK(dik);
    return vk ? IsKeyReleased(vk) : false;
}
