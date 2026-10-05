#include "UI/UISound.h"
#include "Audio/AudioManager.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>

namespace {
// UIサウンドの論理キーとデフォルト音量定義
constexpr const char* kNameCursor = "se_menu_cursor";
constexpr float kBaseVolCursor = 0.6f;

constexpr const char* kNameDecide = "se_menu_decide";
constexpr float kBaseVolDecide = 0.9f;

constexpr const char* kNameCancel = "se_menu_cancel";
constexpr float kBaseVolCancel = 0.6f;

AudioManager* s_audioManager = nullptr;

// 拡張子に依存しないスマートパス解決（.mp3, .wav, .wma等を自動検出）
std::string ResolveAudioPath(const std::string& baseName) {
    namespace fs = std::filesystem;
    const std::string primaryDir = "resources/audio/SE/";
    const std::string legacyDir = "resources/audio/";
    const std::string extensions[] = {".mp3", ".wav", ".wma"};

    // 1. 新規推奨ディレクトリ (resources/audio/SE/) 配下を探索
    for (const auto& ext : extensions) {
        std::string p = primaryDir + baseName + ext;
        if (fs::exists(p)) {
            return p;
        }
    }

    // 2. 旧ディレクトリ (resources/audio/) 配下をフォールバック探索
    for (const auto& ext : extensions) {
        std::string p = legacyDir + baseName + ext;
        if (fs::exists(p)) {
            return p;
        }
    }

    // 見つからない場合は新規パスのデフォルト
    return primaryDir + baseName + ".mp3";
}

// スティック高速操作時のマシンガン鳴り（音割れ・連続再生）防止用リミッター
auto s_lastCursorPlayTime = std::chrono::steady_clock::now() - std::chrono::seconds(10);
constexpr auto kCursorThrottlingInterval = std::chrono::milliseconds(45); // 45ms以内の再発音はスキップ

void PlayOneShot(const std::string& baseName, float baseVolume, float multiplier) {
    if (!s_audioManager) {
        return;
    }

    std::string resolvedPath = ResolveAudioPath(baseName);
    auto soundData = s_audioManager->GetOrLoadSoundByFile(resolvedPath, baseName);
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
        s_audioManager->GetOrLoadSoundByFile(ResolveAudioPath(kNameCursor), kNameCursor);
        s_audioManager->GetOrLoadSoundByFile(ResolveAudioPath(kNameDecide), kNameDecide);
        s_audioManager->GetOrLoadSoundByFile(ResolveAudioPath(kNameCancel), kNameCancel);
    }
}

void PlayCursor(float volumeMultiplier) {
    auto now = std::chrono::steady_clock::now();
    if (now - s_lastCursorPlayTime < kCursorThrottlingInterval) {
        return; // 連続入力ノイズを間引く
    }
    s_lastCursorPlayTime = now;

    PlayOneShot(kNameCursor, kBaseVolCursor, volumeMultiplier);
}

void PlayDecide(float volumeMultiplier) {
    PlayOneShot(kNameDecide, kBaseVolDecide, volumeMultiplier);
}

void PlayCancel(float volumeMultiplier) {
    PlayOneShot(kNameCancel, kBaseVolCancel, volumeMultiplier);
}

void PlayInvalid(float volumeMultiplier) {
    // 選択不能・エラー音（現状はカーソル音を低音量で代替）
    PlayOneShot(kNameCursor, 0.35f, volumeMultiplier);
}

} // namespace UISound
