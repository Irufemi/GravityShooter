#define NOMINMAX
#include "Platform/Input/GamePad.h"
#include <cmath>

void GamePad::Initialize() {
    ZeroMemory(&state_, sizeof(state_));
    ZeroMemory(&prev_, sizeof(prev_));
    retryCountdown_ = 0;
}

void GamePad::Clear() {
    ZeroMemory(&state_, sizeof(state_));
    ZeroMemory(&prev_, sizeof(prev_));
    retryCountdown_ = 0;
}

void GamePad::Update() {
    prev_ = state_;
    ZeroMemory(&state_, sizeof(state_));

    // 未接続時は毎フレームポーリングすると Windows のデバイス探索スパイクが発生するため、
    // 約60フレーム（約1秒）に1回の頻度に間引いて再接続を試行する
    if (!connected_ && retryCountdown_ > 0) {
        --retryCountdown_;
        return;
    }

    DWORD result = ::XInputGetState(index_, &state_);
    connected_ = (result == ERROR_SUCCESS);
    if (!connected_) {
        ZeroMemory(&state_, sizeof(state_));
        retryCountdown_ = 60; // 60フレーム待機
    } else {
        retryCountdown_ = 0;
    }
}

// ラジアル正規化（円形デッドゾーン処理）
std::pair<float, float> GamePad::RadialNormalize(short x, short y, int dz) {
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);
    float mag = std::sqrt(fx * fx + fy * fy);
    if (mag <= dz) {
        return {0.f, 0.f};
    }
    float scaled = (std::min(mag, 32767.0f) - static_cast<float>(dz)) / (32767.0f - static_cast<float>(dz));
    float nx = (fx / mag) * scaled;
    float ny = (fy / mag) * scaled;
    return {nx, ny}; // -1..1
}

// 8方向判定
Stick8 GamePad::Stick8From(short x, short y, int dz, bool invertY, float threshold) {
    auto [nx, ny] = RadialNormalize(x, y, dz);
    if (invertY) {
        ny = -ny;
    }
    float mag = std::sqrt(nx * nx + ny * ny);
    if (mag < threshold) {
        return Stick8::Neutral;
    }
    float ang = std::atan2(ny, nx); // -π..π
    const float pi = 3.14159265358979323846f;
    int oct = static_cast<int>(std::floor((ang + pi / 8.0f) / (pi / 4.0f))) & 7;
    static const Stick8 table[8] = {Stick8::Right, Stick8::UpRight,  Stick8::Up,   Stick8::UpLeft,
                                    Stick8::Left,  Stick8::DownLeft, Stick8::Down, Stick8::DownRight};
    return table[oct];
}

bool GamePad::IsButtonDown(WORD b) const {
    return (state_.Gamepad.wButtons & b) != 0;
}
bool GamePad::IsButtonUp(WORD b) const {
    return (state_.Gamepad.wButtons & b) == 0;
}
bool GamePad::IsButtonPressed(WORD b) const {
    return !(prev_.Gamepad.wButtons & b) && (state_.Gamepad.wButtons & b);
}
bool GamePad::IsButtonReleased(WORD b) const {
    return (prev_.Gamepad.wButtons & b) && !(state_.Gamepad.wButtons & b);
}

float GamePad::GetLeftStickX() const {
    float v = state_.Gamepad.sThumbLX / 32767.0f;
    return (std::fabs(v) < deadZoneLeft_) ? 0.0f : v;
}
float GamePad::GetLeftStickY() const {
    float v = state_.Gamepad.sThumbLY / 32767.0f;
    return (std::fabs(v) < deadZoneLeft_) ? 0.0f : v;
}
float GamePad::GetRightStickX() const {
    float v = state_.Gamepad.sThumbRX / 32767.0f;
    return (std::fabs(v) < deadZoneRight_) ? 0.0f : v;
}
float GamePad::GetRightStickY() const {
    float v = state_.Gamepad.sThumbRY / 32767.0f;
    return (std::fabs(v) < deadZoneRight_) ? 0.0f : v;
}
float GamePad::GetLeftTrigger() const {
    return state_.Gamepad.bLeftTrigger / 255.0f;
}
float GamePad::GetRightTrigger() const {
    return state_.Gamepad.bRightTrigger / 255.0f;
}

Stick8 GamePad::Left8(float threshold) const {
    // 既存 deadZoneLeft_ は 0..1 の想定なので 0..32767 に変換
    int dz = static_cast<int>(deadZoneLeft_ * 32767.0f);
    return Stick8From(state_.Gamepad.sThumbLX, state_.Gamepad.sThumbLY, dz, invertY_, threshold);
}
bool GamePad::Left8Is(Stick8 d, float th) const {
    return Left8(th) == d;
}
bool GamePad::Left8Pressed(Stick8 d, float th) const {
    int dz = static_cast<int>(deadZoneLeft_ * 32767.0f);
    Stick8 prev = Stick8From(prev_.Gamepad.sThumbLX, prev_.Gamepad.sThumbLY, dz, invertY_, th);
    return prev != d && Left8(th) == d;
}
bool GamePad::Left8Released(Stick8 d, float th) const {
    int dz = static_cast<int>(deadZoneLeft_ * 32767.0f);
    Stick8 prev = Stick8From(prev_.Gamepad.sThumbLX, prev_.Gamepad.sThumbLY, dz, invertY_, th);
    return prev == d && Left8(th) != d;
}

// 方向ショートカット
bool GamePad::LUp(float th) const {
    return Left8Is(Stick8::Up, th);
}
bool GamePad::LDown(float th) const {
    return Left8Is(Stick8::Down, th);
}
bool GamePad::LLeft(float th) const {
    return Left8Is(Stick8::Left, th);
}
bool GamePad::LRight(float th) const {
    return Left8Is(Stick8::Right, th);
}
bool GamePad::LUpRight(float th) const {
    return Left8Is(Stick8::UpRight, th);
}
bool GamePad::LUpLeft(float th) const {
    return Left8Is(Stick8::UpLeft, th);
}
bool GamePad::LDownRight(float th) const {
    return Left8Is(Stick8::DownRight, th);
}
bool GamePad::LDownLeft(float th) const {
    return Left8Is(Stick8::DownLeft, th);
}

// D-Pad 8方向判定（wButtonsから8方向へのマッピング）
static Stick8 DPad8FromButtons(WORD w) {
    const bool up = (w & XINPUT_GAMEPAD_DPAD_UP) != 0;
    const bool right = (w & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
    const bool down = (w & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
    const bool left = (w & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
    if (up && right) {
        return Stick8::UpRight;
    }
    if (up && left) {
        return Stick8::UpLeft;
    }
    if (down && right) {
        return Stick8::DownRight;
    }
    if (down && left) {
        return Stick8::DownLeft;
    }
    if (up) {
        return Stick8::Up;
    }
    if (right) {
        return Stick8::Right;
    }
    if (down) {
        return Stick8::Down;
    }
    if (left) {
        return Stick8::Left;
    }
    return Stick8::Neutral;
}
Stick8 GamePad::DPad8Now() const {
    return DPad8FromButtons(state_.Gamepad.wButtons);
}
bool GamePad::DPad8Is(Stick8 d) const {
    return DPad8Now() == d;
}
bool GamePad::DPad8Pressed(Stick8 d) const {
    return DPad8FromButtons(prev_.Gamepad.wButtons) != d && DPad8FromButtons(state_.Gamepad.wButtons) == d;
}
bool GamePad::DPad8Released(Stick8 d) const {
    return DPad8FromButtons(prev_.Gamepad.wButtons) == d && DPad8FromButtons(state_.Gamepad.wButtons) != d;
}

bool GamePad::DPadUp() const {
    return IsButtonDown(XINPUT_GAMEPAD_DPAD_UP);
}
bool GamePad::DPadDown() const {
    return IsButtonDown(XINPUT_GAMEPAD_DPAD_DOWN);
}
bool GamePad::DPadLeft() const {
    return IsButtonDown(XINPUT_GAMEPAD_DPAD_LEFT);
}
bool GamePad::DPadRight() const {
    return IsButtonDown(XINPUT_GAMEPAD_DPAD_RIGHT);
}
bool GamePad::DPadUpPressed() const {
    return IsButtonPressed(XINPUT_GAMEPAD_DPAD_UP);
}
bool GamePad::DPadUpReleased() const {
    return IsButtonReleased(XINPUT_GAMEPAD_DPAD_UP);
}
bool GamePad::DPadDownPressed() const {
    return IsButtonPressed(XINPUT_GAMEPAD_DPAD_DOWN);
}
bool GamePad::DPadDownReleased() const {
    return IsButtonReleased(XINPUT_GAMEPAD_DPAD_DOWN);
}
bool GamePad::DPadLeftPressed() const {
    return IsButtonPressed(XINPUT_GAMEPAD_DPAD_LEFT);
}
bool GamePad::DPadLeftReleased() const {
    return IsButtonReleased(XINPUT_GAMEPAD_DPAD_LEFT);
}
bool GamePad::DPadRightPressed() const {
    return IsButtonPressed(XINPUT_GAMEPAD_DPAD_RIGHT);
}
bool GamePad::DPadRightReleased() const {
    return IsButtonReleased(XINPUT_GAMEPAD_DPAD_RIGHT);
}

bool GamePad::DPadUpRightDown() const {
    return DPad8Is(Stick8::UpRight);
}
bool GamePad::DPadUpRightPressed() const {
    return DPad8Pressed(Stick8::UpRight);
}
bool GamePad::DPadUpRightReleased() const {
    return DPad8Released(Stick8::UpRight);
}
bool GamePad::DPadUpLeftDown() const {
    return DPad8Is(Stick8::UpLeft);
}
bool GamePad::DPadUpLeftPressed() const {
    return DPad8Pressed(Stick8::UpLeft);
}
bool GamePad::DPadUpLeftReleased() const {
    return DPad8Released(Stick8::UpLeft);
}
bool GamePad::DPadDownRightDown() const {
    return DPad8Is(Stick8::DownRight);
}
bool GamePad::DPadDownRightPressed() const {
    return DPad8Pressed(Stick8::DownRight);
}
bool GamePad::DPadDownRightReleased() const {
    return DPad8Released(Stick8::DownRight);
}
bool GamePad::DPadDownLeftDown() const {
    return DPad8Is(Stick8::DownLeft);
}
bool GamePad::DPadDownLeftPressed() const {
    return DPad8Pressed(Stick8::DownLeft);
}
bool GamePad::DPadDownLeftReleased() const {
    return DPad8Released(Stick8::DownLeft);
}

// トリガー: アナログ入力判定としきい値処理
static inline bool _down(uint8_t now, uint8_t th) {
    return now > th;
}
static inline bool _pressed(uint8_t now, uint8_t pre, uint8_t th) {
    return (now > th) && (pre <= th);
}
static inline bool _released(uint8_t now, uint8_t pre, uint8_t th) {
    return (now <= th) && (pre > th);
}

float GamePad::LeftTriggerAnalog(float dz) const {
    float v = std::max(0.0f, static_cast<float>(state_.Gamepad.bLeftTrigger) - dz);
    return v / (255.0f - dz);
}
float GamePad::RightTriggerAnalog(float dz) const {
    float v = std::max(0.0f, static_cast<float>(state_.Gamepad.bRightTrigger) - dz);
    return v / (255.0f - dz);
}
bool GamePad::LeftTriggerDown(uint8_t th) const {
    return _down(state_.Gamepad.bLeftTrigger, th);
}
bool GamePad::LeftTriggerPressed(uint8_t th) const {
    return _pressed(state_.Gamepad.bLeftTrigger, prev_.Gamepad.bLeftTrigger, th);
}
bool GamePad::LeftTriggerReleased(uint8_t th) const {
    return _released(state_.Gamepad.bLeftTrigger, prev_.Gamepad.bLeftTrigger, th);
}
bool GamePad::RightTriggerDown(uint8_t th) const {
    return _down(state_.Gamepad.bRightTrigger, th);
}
bool GamePad::RightTriggerPressed(uint8_t th) const {
    return _pressed(state_.Gamepad.bRightTrigger, prev_.Gamepad.bRightTrigger, th);
}
bool GamePad::RightTriggerReleased(uint8_t th) const {
    return _released(state_.Gamepad.bRightTrigger, prev_.Gamepad.bRightTrigger, th);
}
bool GamePad::TriggerDown(bool right, uint8_t th) const {
    return right ? RightTriggerDown(th) : LeftTriggerDown(th);
}
bool GamePad::TriggerPressed(bool right, uint8_t th) const {
    return right ? RightTriggerPressed(th) : LeftTriggerPressed(th);
}
bool GamePad::TriggerReleased(bool right, uint8_t th) const {
    return right ? RightTriggerReleased(th) : LeftTriggerReleased(th);
}

// RB / LB ボタン判定
bool GamePad::LBDown() const {
    return IsButtonDown(XINPUT_GAMEPAD_LEFT_SHOULDER);
}
bool GamePad::LBPressed() const {
    return IsButtonPressed(XINPUT_GAMEPAD_LEFT_SHOULDER);
}
bool GamePad::LBReleased() const {
    return IsButtonReleased(XINPUT_GAMEPAD_LEFT_SHOULDER);
}
bool GamePad::RBDown() const {
    return IsButtonDown(XINPUT_GAMEPAD_RIGHT_SHOULDER);
}
bool GamePad::RBPressed() const {
    return IsButtonPressed(XINPUT_GAMEPAD_RIGHT_SHOULDER);
}
bool GamePad::RBReleased() const {
    return IsButtonReleased(XINPUT_GAMEPAD_RIGHT_SHOULDER);
}

// START ボタン判定
bool GamePad::StartDown() const {
    return IsButtonDown(XINPUT_GAMEPAD_START);
}
bool GamePad::StartPressed() const {
    return IsButtonPressed(XINPUT_GAMEPAD_START);
}
bool GamePad::StartReleased() const {
    return IsButtonReleased(XINPUT_GAMEPAD_START);
}