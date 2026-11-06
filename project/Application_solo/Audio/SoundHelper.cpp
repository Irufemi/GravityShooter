#include "Audio/SoundHelper.h"
#include "Audio/AudioManager.h"
#include "Core/Utility/Log.h"
#include "Core/Utility/JsonUtility.h"
#include "Core/Utility/FileSystem.h"
#include <unordered_map>
#include <shared_mutex>
#include <iostream>

namespace {

AudioManager* s_audioManager = nullptr;
std::unordered_map<std::string, SoundActionDescriptor> s_seMap;
std::unordered_map<std::string, SoundActionDescriptor> s_bgmMap;
std::shared_mutex s_soundMutex;

} // namespace

namespace SoundHelper {

bool Initialize(AudioManager* audioManager, const std::string& configPath) {
    s_audioManager = audioManager;
    if (!s_audioManager) {
        Log::OutPutLog(std::cerr, "[SoundHelper] AudioManager is null.\n");
        return false;
    }

    nlohmann::json root;
    // エンジンの FileSystem 基盤を用いてリソースパスを自動解決
    std::string resolvedPath = FileSystem::GetResourcePath(configPath);
    if (!Irufemi::JsonUtility::LoadFromFile(resolvedPath, root)) {
        // 開発環境ルート（カレントディレクトリ）からの相対パスでもフォールバック試行
        if (!Irufemi::JsonUtility::LoadFromFile(configPath, root)) {
            std::string fallback = "project/Application_solo/" + configPath;
            if (!Irufemi::JsonUtility::LoadFromFile(fallback, root)) {
                Log::OutPutLog(std::cerr, "[SoundHelper] Failed to load config via JsonUtility: " + configPath + "\n");
                return false;
            }
            resolvedPath = fallback;
        } else {
            resolvedPath = configPath;
        }
    }
    std::string loadedPath = resolvedPath;

    std::unique_lock<std::shared_mutex> lock(s_soundMutex);
    s_seMap.clear();
    s_bgmMap.clear();

    if (root.contains("SE") && root["SE"].is_object()) {
        for (auto& [key, val] : root["SE"].items()) {
            SoundActionDescriptor desc;
            desc.path = val.value("Path", "");
            desc.volume = val.value("Volume", 1.0f);
            desc.pitch = val.value("Pitch", 1.0f);
            desc.loop = val.value("Loop", false);
            s_seMap[key] = desc;
        }
    }

    if (root.contains("BGM") && root["BGM"].is_object()) {
        for (auto& [key, val] : root["BGM"].items()) {
            SoundActionDescriptor desc;
            desc.path = val.value("Path", "");
            desc.volume = val.value("Volume", 1.0f);
            desc.pitch = val.value("Pitch", 1.0f);
            desc.loop = val.value("Loop", true);
            s_bgmMap[key] = desc;
        }
    }

    Log::OutPutLog(std::cout, "[SoundHelper] Initialized successfully from " + loadedPath +
                              " (SE: " + std::to_string(s_seMap.size()) +
                              ", BGM: " + std::to_string(s_bgmMap.size()) + ")\n");
    return true;
}

void PreloadAll() {
    if (!s_audioManager) {
        return;
    }
    std::shared_lock<std::shared_mutex> lock(s_soundMutex);
    for (const auto& [name, desc] : s_seMap) {
        if (!desc.path.empty()) {
            s_audioManager->GetOrLoadSoundByFile(desc.path);
        }
    }
    for (const auto& [name, desc] : s_bgmMap) {
        if (!desc.path.empty()) {
            s_audioManager->GetOrLoadSoundByFile(desc.path);
        }
    }
}

const SoundActionDescriptor* GetDescriptor(GameSE se) {
    std::shared_lock<std::shared_mutex> lock(s_soundMutex);
    const char* key = GameSEToString(se);
    auto it = s_seMap.find(key);
    if (it != s_seMap.end()) {
        return &it->second;
    }
    return nullptr;
}

const SoundActionDescriptor* GetDescriptor(GameBGM bgm) {
    std::shared_lock<std::shared_mutex> lock(s_soundMutex);
    const char* key = GameBGMToString(bgm);
    auto it = s_bgmMap.find(key);
    if (it != s_bgmMap.end()) {
        return &it->second;
    }
    return nullptr;
}

void Play(GameSE se, std::optional<float> volumeMultiplier, std::optional<float> pitchMultiplier) {
    if (!s_audioManager) {
        return;
    }
    const auto* desc = GetDescriptor(se);
    if (!desc || desc->path.empty()) {
        return;
    }

    float finalVol = volumeMultiplier.has_value() ? (desc->volume * (*volumeMultiplier)) : desc->volume;
    float finalPitch = pitchMultiplier.has_value() ? (desc->pitch * (*pitchMultiplier)) : desc->pitch;

    s_audioManager->PlayByFile(desc->path, finalVol, finalPitch, AudioCategory::SE);
}

void PlayByName(std::string_view name, std::optional<float> volumeMultiplier, std::optional<float> pitchMultiplier) {
    if (!s_audioManager) {
        return;
    }
    std::shared_lock<std::shared_mutex> lock(s_soundMutex);
    auto it = s_seMap.find(std::string(name));
    if (it == s_seMap.end() || it->second.path.empty()) {
        return;
    }
    const auto& desc = it->second;
    float finalVol = volumeMultiplier.has_value() ? (desc.volume * (*volumeMultiplier)) : desc.volume;
    float finalPitch = pitchMultiplier.has_value() ? (desc.pitch * (*pitchMultiplier)) : desc.pitch;

    s_audioManager->PlayByFile(desc.path, finalVol, finalPitch, AudioCategory::SE);
}

void PlayBGM(GameBGM bgm, std::optional<float> volumeMultiplier) {
    if (!s_audioManager) {
        return;
    }
    const auto* desc = GetDescriptor(bgm);
    if (!desc || desc->path.empty()) {
        return;
    }

    float finalVol = volumeMultiplier.has_value() ? (desc->volume * (*volumeMultiplier)) : desc->volume;
    auto soundData = s_audioManager->GetOrLoadSoundByFile(desc->path);
    if (soundData) {
        s_audioManager->Play(soundData, desc->loop, finalVol, AudioCategory::BGM);
    }
}

void StopBGM() {
    if (s_audioManager) {
        s_audioManager->StopCategory(AudioCategory::BGM);
    }
}

} // namespace SoundHelper

