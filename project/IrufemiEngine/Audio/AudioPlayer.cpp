#include "Audio/AudioPlayer.h"

namespace Irufemi {

AudioPlayer::AudioPlayer(AudioManager* audioManager, AudioType type) : audioManager_(audioManager), type_(type) {
    // BGMの場合はデフォルト音量を少し下げるなどの調整も可能ですが、
    // ここでは1.0fをベースとして扱います。
}

AudioPlayer::~AudioPlayer() {
    Stop();
    sound_.reset();
}

void AudioPlayer::Initialize(const std::string& filePath, const std::string& key) {
    Stop();
    sound_.reset();
    soundKey_.clear();

    if (!audioManager_) {
        return;
    }

    auto sd = audioManager_->GetOrLoadSoundByFile(filePath, key);
    if (!sd) {
        return;
    }

    sound_ = sd;
    soundKey_ = key.empty() ? filePath : key;
}

void AudioPlayer::Play(std::optional<bool> loop) {
    if (!audioManager_ || !sound_) {
        return;
    }

    // 再生開始前に既存の再生を止める(同一インスタンスで再生を上書き)
    Stop();

    // ループ指定がなければAudioTypeから判断
    bool shouldLoop = loop.has_value() ? loop.value() : (type_ == AudioType::BGM);
    AudioCategory category = (type_ == AudioType::BGM) ? AudioCategory::BGM : AudioCategory::SE;

    voice_ = audioManager_->Play(sound_, shouldLoop, volume_, category);
}

void AudioPlayer::Stop() {
    if (audioManager_) {
        audioManager_->Stop(voice_);
    }
}

void AudioPlayer::Pause() {
    if (auto v = voice_.lock()) {
        v->Pause();
    }
}

void AudioPlayer::Resume() {
    if (auto v = voice_.lock()) {
        v->Resume();
    }
}

void AudioPlayer::SetVolume(float volume) {
    volume_ = volume;
    if (auto v = voice_.lock()) {
        v->SetVolume(volume_);
    }
}

} // namespace Irufemi
