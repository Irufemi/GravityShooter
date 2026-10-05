#pragma once
#include <xaudio2.h>
#include <memory>
#include "Audio/VoiceCallback.h"

enum class AudioCategory {
    Master, // 特殊な用途
    BGM,
    SE,
    UI
};

class VoiceInstance {
public:
    VoiceInstance(IXAudio2SourceVoice* voice, std::unique_ptr<VoiceCallback> callback,
                  AudioCategory category = AudioCategory::SE)
        : voice_(voice), callback_(std::move(callback)), category_(category) {}

    ~VoiceInstance() {
        Destroy();
    }

    /**
     * @brief ボイスリソースを安全に停止・破棄し、無効化する
     * @details 一線級エンジンの設計規約に準拠し、エンジン終了時の一括無効化に対応します
     */
    void Destroy() {
        if (voice_) {
            voice_->Stop(0);
            voice_->FlushSourceBuffers();
            voice_->DestroyVoice();
            voice_ = nullptr;
        }
    }

    /**
     * @brief Volume を設定する。
     * @param[in] volume 設定する Volume の値
     */
    void SetVolume(float volume) {
        if (voice_) {
            voice_->SetVolume(volume);
        }
    }

    /**
     * @brief Stop を実行する。
     */
    void Stop() {
        if (voice_) {
            voice_->Stop(0);
        }
    }

    /**
     * @brief Pause を実行する。
     */
    void Pause() {
        if (voice_) {
            voice_->Stop(0);
        }
    }

    /**
     * @brief Resume を実行する。
     */
    void Resume() {
        if (voice_) {
            voice_->Start(0);
        }
    }

    /**
     * @brief Voice を取得する。
     * @return 取得された Voice
     */
    IXAudio2SourceVoice* GetVoice() const {
        return voice_;
    }

    /**
     * @brief Callback を取得する。
     * @return 取得された Callback
     */
    VoiceCallback* GetCallback() const {
        return callback_.get();
    }

    bool IsFinished() const {
        return callback_ && callback_->IsFinished();
    }

    AudioCategory GetCategory() const {
        return category_;
    }

private:
    IXAudio2SourceVoice* voice_;
    std::unique_ptr<VoiceCallback> callback_;
    AudioCategory category_;
};
