#include "UI/UISound.h"
#include "Audio/AudioManager.h"
#include <algorithm>
#include <chrono>

namespace {
// UIサウンドのファイルパスとキーの一元管理定数
constexpr const char* kPathCursor = "resources/audio/se_menu_cursor.wav";
constexpr const char* kKeyCursor = "se_menu_cursor";
constexpr float kBaseVolCursor = 0.6f;

constexpr const char* kPathDecide = "resources/audio/se_menu_decide.wav";
constexpr const char* kKeyDecide = "se_menu_decide";
constexpr float kBaseVolDecide = 0.9f;

AudioManager* s_audioManager = nullptr;

// スティック高速操作時のマシンガン鳴り（音割れ・連続再生）防止用リミッター
auto s_lastCursorPlayTime = std::chrono::steady_clock::now() - std::chrono::seconds(10);
constexpr auto kCursorThrottlingInterval = std::chrono::milliseconds(45); // 45ms以内の再発音はスキップ

void PlayOneShot(const char* filePath, const char* soundKey, float baseVolume, float multiplier) {
    if (!s_audioManager) {
        return;
    }

    auto soundData = s_audioManager->GetOrLoadSoundByFile(filePath, soundKey);
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
    }
}

void PlayCursor(float volumeMultiplier) {
    auto now = std::chrono::steady_clock::now();
    if (now - s_lastCursorPlayTime < kCursorThrottlingInterval) {
        return; // 連続入力ノイズを間引く
    }
    s_lastCursorPlayTime = now;

    PlayOneShot(kPathCursor, kKeyCursor, kBaseVolCursor, volumeMultiplier);
}

void PlayDecide(float volumeMultiplier) {
    PlayOneShot(kPathDecide, kKeyDecide, kBaseVolDecide, volumeMultiplier);
}

void PlayCancel(float volumeMultiplier) {
    // キャンセル専用WAVがない場合はカーソル音をやや控えめに再生
    PlayOneShot(kPathCursor, kKeyCursor, 0.5f, volumeMultiplier);
}

void PlayInvalid(float volumeMultiplier) {
    // 選択不能・エラー音（現状はカーソル音を低音量で代替）
    PlayOneShot(kPathCursor, kKeyCursor, 0.35f, volumeMultiplier);
}

} // namespace UISound

