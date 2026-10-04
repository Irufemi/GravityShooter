#include "Framework/Component/Audio/AudioSourceComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/System/IrufemiEngine.h"
#include "Audio/AudioManager.h"

AudioSourceComponent::AudioSourceComponent() {}

AudioSourceComponent::~AudioSourceComponent() {
    OnDestroy();
}

void AudioSourceComponent::OnRegisterProperties() {
    RegisterProperty("Audio Type (0:BGM, 1:SE)", &audioType_);
    RegisterProperty("Audio Path", &audioPath_);
    RegisterProperty("Play On Awake", &playOnAwake_);
    RegisterProperty("Loop", &loop_);
    RegisterProperty("Volume", &volume_);
}

void AudioSourceComponent::Initialize() {
    InitializeAudio();
}

void AudioSourceComponent::OnSpawned() {
    if (!player_) {
        InitializeAudio();
    }
}

void AudioSourceComponent::Start() {
    if (!player_) {
        InitializeAudio();
    }
    auto engine = GetEngine();
    if (playOnAwake_ && engine && engine->IsPlayMode()) {
        if (player_ && !player_->IsPlaying()) {
            Play();
        }
    }
}

void AudioSourceComponent::OnDisable() {
    Stop();
}

void AudioSourceComponent::OnDestroy() {
    Stop();
}

void AudioSourceComponent::InitializeAudio() {
    if (player_) {
        return;
    }
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    auto audioManager = engine->GetAudioManager();
    if (audioManager) {
        player_ = std::make_unique<AudioPlayer>(audioManager, static_cast<AudioType>(audioType_));

        std::string fullPath = "resources/" + audioPath_;
        if (audioPath_.find("resources/") == 0) {
            fullPath = audioPath_;
        }
        player_->Initialize(fullPath);
        player_->SetVolume(volume_);
        lastVolume_ = volume_;
    }

    // スタンドアロン実行時、またはPlayMode実行中のみ即時再生する（Editモードでの誤発火を防止）
    if (playOnAwake_ && engine->IsPlayMode()) {
        Play();
    }
}

void AudioSourceComponent::Update() {
    auto engine = GetEngine();
    bool isPlayMode = engine ? engine->IsPlayMode() : true;

    // 非Playモード（Editモード）の場合は音声を停止し、エディタでの再生暴発を防止
    if (!isPlayMode) {
        if (player_ && player_->IsPlaying()) {
            player_->Stop();
        }
        return;
    }

    // インスペクターからの動的変更を反映（変更があった場合のみSetVolumeを実行）
    if (player_ && lastVolume_ != volume_) {
        player_->SetVolume(volume_);
        lastVolume_ = volume_;
    }
}

void AudioSourceComponent::Play() {
    if (player_) {
        player_->Play(loop_);
    }
}

void AudioSourceComponent::Stop() {
    if (player_) {
        player_->Stop();
    }
}

void AudioSourceComponent::SetAudioPath(const std::string& path) {
    audioPath_ = path;
    if (player_) {
        std::string fullPath = "resources/" + audioPath_;
        if (audioPath_.find("resources/") == 0) {
            fullPath = audioPath_;
        }
        player_->Initialize(fullPath);
    }
}

void AudioSourceComponent::SetVolume(float volume) {
    volume_ = volume;
    if (player_ && lastVolume_ != volume_) {
        player_->SetVolume(volume);
        lastVolume_ = volume;
    }
}

void AudioSourceComponent::SetLoop(bool loop) {
    loop_ = loop;
}

void AudioSourceComponent::SetAudioType(AudioType type) {
    audioType_ = static_cast<int>(type);
    if (player_) {
        player_->SetAudioType(type);
    }
}
