#include "UI/UISound.h"
#include "Audio/AudioManager.h"
#include <algorithm>
#include <chrono>

namespace {
// UIサウンドのファイルパスとキーの一元管理定数（resources/audio/SE/ 階層）
constexpr const char* kPathCursor = "resources/audio/SE/se_menu_cursor.wav";
constexpr const char* kPathCursorLegacy = "resources/audio/se_menu_cursor.wav";
constexpr const char* kKeyCursor = "se_menu_cursor";
constexpr float kBaseVolCursor = 0.6f;

constexpr const char* kPathDecide = "resources/audio/SE/se_menu_decide.wav";
constexpr const char* kPathDecideLegacy = "resources/audio/se_menu_decide.wav";
constexpr const char* kKeyDecide = "se_menu_decide";
constexpr float kBaseVolDecide = 0.9f;

constexpr const char* kPathCancel = "resources/audio/SE/se_menu_cancel.wav";
constexpr const char* kPathCancelLegacy = "resources/audio/se_menu_cancel.wav";
constexpr const char* kKeyCancel = "se_menu_cancel";
constexpr float kBaseVolCancel = 0.6f;

AudioManager* s_audioManager = nullptr;

// スティック高速操作時のマシンガン鳴り（音割れ・連続再生）防止用リミッター
auto s_lastCursorPlayTime = std::chrono::steady_clock::now() - std::chrono::seconds(10);
constexpr auto kCursorThrottlingInterval = std::chrono::milliseconds(45); // 45ms以内の再発音はスキップ

void PlayOneShot(const char* filePath, const char* legacyPath, const char* soundKey, float baseVolume, float multiplier) {
    if (!s_audioManager) {
        return;
    }

    auto soundData = s_audioManager->GetOrLoadSoundByFile(filePath, soundKey);
    if (!soundData && legacyPath) {
        soundData = s_audioManager->GetOrLoadSoundByFile(legacyPath, soundKey);
    }
    if (!soundData) {
        return;
    }

    float finalVol = std::clamp(baseVolume * multiplier, 0.0f, 1.0f);
    // AudioCategory::UI を指定することで、UIBus（サブミックス）経由でハードウェア一括制御
    s_audioManager->Play(soundData, false, finalVol, AudioCategory::UI);
}
} // namespace

namespace UISound {

void Initialize(AudioManager* audioManager) {
    s_audioManager = audioManager;
}

void Preload() {
    if (s_audioManager) {
        s_audioManager->GetOrLoadSoundByFile(kPathCursor, kKeyCursor);
        s_audioManager->GetOrLoadSoundByFile(kPathDecide, kKeyDecide);
        s_audioManager->GetOrLoadSoundByFile(kPathCancel, kKeyCancel);
    }
}

void PlayCursor(float volumeMultiplier) {
    auto now = std::chrono::steady_clock::now();
    if (now - s_lastCursorPlayTime < kCursorThrottlingInterval) {
        return; // 連続入力ノイズを間引く
    }
    s_lastCursorPlayTime = now;

    PlayOneShot(kPathCursor, kPathCursorLegacy, kKeyCursor, kBaseVolCursor, volumeMultiplier);
}

void PlayDecide(float volumeMultiplier) {
    PlayOneShot(kPathDecide, kPathDecideLegacy, kKeyDecide, kBaseVolDecide, volumeMultiplier);
}

void PlayCancel(float volumeMultiplier) {
    // キャンセル専用WAVがある場合は優先、無ければカーソル音で代替
    PlayOneShot(kPathCancel, kPathCursor, kKeyCancel, kBaseVolCancel, volumeMultiplier);
}

void PlayInvalid(float volumeMultiplier) {
    // 選択不能・エラー音（現状はカーソル音を低音量で代替）
    PlayOneShot(kPathCursor, kPathCursorLegacy, kKeyCursor, 0.35f, volumeMultiplier);
}

} // namespace UISound

