#include "Core/Utility/ErrorUtility.h"
#include "Audio/AudioManager.h"
#include "Core/System/ThreadPool.h"
#include "Core/System/TaskGroup.h"
#include <cassert>
#include <filesystem> // フォルダ内のファイルを探索するために使用
#include <algorithm>  // 文字列を小文字に変換するために使用
#include <vector>
#include <Windows.h>
#include <iostream>
#include "Framework/Utility/CVar.h"
#include "Core/Utility/Log.h"

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace {
// UTF-8 -> UTF-16
std::wstring ToWide(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    int size = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(size, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), size);
    return w;
}
// パス正規化(キーとして安定化させる)
std::string NormalizePath(const std::string& path) {
    std::filesystem::path p(path);
    p.make_preferred();
    return p.generic_string(); // すべて'/'に統一
}
} // namespace

AudioManager::~AudioManager() {
    Finalize();
}

void AudioManager::Initialize() {
    // 再初期化に備えて
    finalized_ = false;

    // Media Foundationの初期化
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    ASSERT_IF_FAILED(hr);

    // XAudio2エンジンの生成
    hr = XAudio2Create(&pXAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    ASSERT_IF_FAILED(hr);

    // マスターボイスの生成
    hr = pXAudio2_->CreateMasteringVoice(&pMasteringVoice_);
    ASSERT_IF_FAILED(hr);

    // マスターボリュームの変更時コールバックを登録（初期値もSetOnChangeCallbackにより即時適用される）
    cachedMasterVolume_ = -1.0f;
    Irufemi::CVarSystem::SetOnChangeCallback("a.MasterVolume", [this]() {
        if (pMasteringVoice_) {
            float masterVol = Irufemi::CVarSystem::GetFloat("a.MasterVolume");
            if (masterVol != cachedMasterVolume_) {
                cachedMasterVolume_ = masterVol;
                pMasteringVoice_->SetVolume(masterVol);
            }
        }
    });
}

void AudioManager::Finalize() {
    if (finalized_) {
        return; // 多重 Finalize 防止
    }
    finalized_ = true; // 以降の操作は無効化

    // CVarコールバックの解除（ダングリング参照防止）
    Irufemi::CVarSystem::SetOnChangeCallback("a.MasterVolume", nullptr);

    StopAll(); // すべてのVoiceを安全に停止＆Destroy

    if (pMasteringVoice_) {
        pMasteringVoice_->DestroyVoice();
        pMasteringVoice_ = nullptr;
    }

    // ComPtrが自動で解放するが、明示的にリセット
    pXAudio2_.Reset();

    HRESULT hr = MFShutdown();
    ASSERT_IF_FAILED(hr);
}

bool AudioManager::IsManagedVoice(std::shared_ptr<VoiceInstance> instance) const {
    std::lock_guard<std::mutex> lock(voiceMutex_);
    return std::find(activeVoices_.begin(), activeVoices_.end(), instance) != activeVoices_.end();
}

void AudioManager::Update() {
    // 終了したボイスをリストから削除
    // 削除されると shared_ptr の参照が外れ、VoiceInstance のデストラクタで DestroyVoice される
    std::lock_guard<std::mutex> lock(voiceMutex_);
    std::erase_if(activeVoices_,
                  [](const std::shared_ptr<VoiceInstance>& instance) { return instance->GetCallback()->IsFinished(); });
}

void AudioManager::LoadAllSoundsFromFolder(const std::string& folderPath, ThreadPool* threadPool) {
    {
        std::lock_guard<std::mutex> lock(registryMutex_);
        soundRegistry_.clear();
        categoryMap_.clear();
    }
    namespace fs = std::filesystem;
    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
        Log::OutPutLog(std::cerr,
                       "[AudioManager] Warning: Sound folder not found or is not a directory: " + folderPath + "\n");
        return;
    }

    std::shared_ptr<TaskGroup> taskGroup = nullptr;
    if (threadPool) {
        taskGroup = std::make_shared<TaskGroup>();
    }

    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.is_directory()) {
            std::string category = entry.path().filename().string();
            LoadSoundsFromFolder(entry.path().string(), category, threadPool, taskGroup);
        }
    }

    if (taskGroup) {
        taskGroup->Wait();
    }

    // 全てのカテゴリ名リストをソート
    std::lock_guard<std::mutex> lock(registryMutex_);
    for (auto& [cat, names] : categoryMap_) {
        std::sort(names.begin(), names.end());
    }
}

// サブフォルダ単位でロードするオーバーロード版
void AudioManager::LoadSoundsFromFolder(const std::string& folderPath, const std::string& category,
                                        ThreadPool* threadPool, std::shared_ptr<TaskGroup> group) {
    namespace fs = std::filesystem;

    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
        Log::OutPutLog(std::cerr,
                       "[AudioManager] Warning: Category folder not found or is not a directory: " + folderPath + "\n");
        return;
    }

    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string ext = entry.path().extension().string();
        if (_stricmp(ext.c_str(), ".wav") != 0 && _stricmp(ext.c_str(), ".mp3") != 0 &&
            _stricmp(ext.c_str(), ".wma") != 0) {
            continue;
        }

        std::string filename = entry.path().filename().string();
        std::wstring wpath = entry.path().wstring();

        // キーは "カテゴリ/ファイル名" にする
        std::string key = category + "/" + filename;

        auto loadSoundTask = [this, wpath, key, category, filename]() {
            // ワーカースレッド用のCOM初期化
            HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

            auto sd = std::make_shared<Sound>();
            if (sd->Load(wpath)) {
                std::lock_guard<std::mutex> lock(registryMutex_);
                soundRegistry_[key] = sd;
                categoryMap_[category].push_back(filename);
            } else {
                Log::OutPutLog(std::cerr, "[AudioManager] Warning: Failed to load sound file: " + key + "\n");
            }

            if (SUCCEEDED(hrCo)) {
                CoUninitialize();
            }
        };

        if (threadPool && group) {
            threadPool->Enqueue(group, loadSoundTask);
        } else {
            loadSoundTask();
        }
    }

    // threadPoolがない同期ロード時のみここでソート
    if (!threadPool) {
        std::lock_guard<std::mutex> lock(registryMutex_);
        auto& names = categoryMap_[category];
        std::sort(names.begin(), names.end());
    }
}

std::vector<std::string> AudioManager::GetSoundNames(const std::string& category) const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    auto it = categoryMap_.find(category);
    if (it == categoryMap_.end()) {
        return {};
    }
    return it->second;
}

std::shared_ptr<Sound> AudioManager::GetSoundData(const std::string& name) const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    auto it = soundRegistry_.find(name);
    if (it != soundRegistry_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> AudioManager::GetCategories() const {
    std::lock_guard<std::mutex> lock(registryMutex_);
    std::vector<std::string> cats;
    cats.reserve(categoryMap_.size());
    for (auto const& [cat, _] : categoryMap_) {
        cats.push_back(cat);
    }
    std::sort(cats.begin(), cats.end());
    return cats;
}

std::weak_ptr<VoiceInstance> AudioManager::Play(std::shared_ptr<Sound> soundData, bool loop, float volume,
                                                AudioCategory category) {
    if (finalized_) {
        return {};
    }
    if (!pXAudio2_ || !soundData) {
        return {};
    }

    auto callback = std::make_unique<VoiceCallback>();
    IXAudio2SourceVoice* pSourceVoice{nullptr};
    HRESULT hr = pXAudio2_->CreateSourceVoice(&pSourceVoice, soundData->GetFormat(), 0, XAUDIO2_DEFAULT_FREQ_RATIO,
                                              callback.get());
    if (FAILED(hr) || !pSourceVoice) {
        return {};
    }

    // 音量を設定
    pSourceVoice->SetVolume(volume);

    // 再生するオーディオバッファの準備
    XAUDIO2_BUFFER buffer{0};
    buffer.pAudioData = soundData->GetData();
    buffer.Flags = XAUDIO2_END_OF_STREAM; // これで再生が最後まで行くとストリームが終了したとみなされる
    buffer.AudioBytes = soundData->GetSize();
    if (loop) {
        buffer.LoopCount = XAUDIO2_LOOP_INFINITE; // ループ再生
    }

    // バッファをソースボイスに送信
    hr = pSourceVoice->SubmitSourceBuffer(&buffer);
    ASSERT_IF_FAILED(hr);

    // 再生開始
    hr = pSourceVoice->Start(0);
    ASSERT_IF_FAILED(hr);

    // 管理インスタンスを生成してリストに追加
    auto instance = std::make_shared<VoiceInstance>(pSourceVoice, std::move(callback), category);
    {
        std::lock_guard<std::mutex> lock(voiceMutex_);
        activeVoices_.push_back(instance);
    }
    return instance;
}

void AudioManager::Stop(std::weak_ptr<VoiceInstance>& instance) {
    auto locked = instance.lock();
    if (!locked) {
        return;
    }

    if (finalized_) {
        return;
    }

    // 管理リストから除去 (shared_ptr が外れて VoiceInstance のデストラクタで破棄される)
    std::lock_guard<std::mutex> lock(voiceMutex_);
    auto it = std::remove(activeVoices_.begin(), activeVoices_.end(), locked);
    if (it != activeVoices_.end()) {
        activeVoices_.erase(it, activeVoices_.end());
    }
}

void AudioManager::StopAll() {
    std::lock_guard<std::mutex> lock(voiceMutex_);
    for (auto& voice : activeVoices_) {
        if (voice) {
            voice->Stop();
        }
    }
}

void AudioManager::PauseAll() {
    std::lock_guard<std::mutex> lock(voiceMutex_);
    for (auto& voice : activeVoices_) {
        if (voice) {
            voice->Pause();
        }
    }
}

void AudioManager::ResumeAll() {
    std::lock_guard<std::mutex> lock(voiceMutex_);
    for (auto& voice : activeVoices_) {
        if (voice) {
            voice->Resume();
        }
    }
}

void AudioManager::PauseCategory(AudioCategory category) {
    std::lock_guard<std::mutex> lock(voiceMutex_);
    for (auto& voice : activeVoices_) {
        if (voice && voice->GetCategory() == category) {
            voice->Pause();
        }
    }
}

void AudioManager::ResumeCategory(AudioCategory category) {
    std::lock_guard<std::mutex> lock(voiceMutex_);
    for (auto& voice : activeVoices_) {
        if (voice && voice->GetCategory() == category) {
            voice->Resume();
        }
    }
}

bool AudioManager::HasSound(const std::string& key) const {
    return soundRegistry_.find(key) != soundRegistry_.end();
}

std::shared_ptr<Sound> AudioManager::GetOrLoadSoundByFile(const std::string& filePath, const std::string& key) {
    std::string normPath = NormalizePath(filePath);
    std::string useKey = key.empty() ? normPath : key;

    auto it = soundRegistry_.find(useKey);
    if (it != soundRegistry_.end()) {
        return it->second;
    }

    auto sound = std::make_shared<Sound>();
    if (!sound->Load(ToWide(normPath))) {
        // 読み込み失敗時は nullptr を返す
        return nullptr;
    }

    soundRegistry_.emplace(useKey, sound);
    return sound;
}