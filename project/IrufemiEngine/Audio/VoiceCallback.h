#pragma once
#include <xaudio2.h>
#include <atomic>

namespace Irufemi {

// SourceVoiceの再生が完了したことを受け取り、自身を破棄するためのコールバック
class VoiceCallback : public IXAudio2VoiceCallback {
public:
    virtual ~VoiceCallback() {}
    /**
     * @brief OnStreamEnd を実行する。
     */
    void OnStreamEnd() override {}
    /**
     * @brief OnVoiceProcessingPassEnd を実行する。
     */
    void OnVoiceProcessingPassEnd() override {}
    /**
     * @brief OnVoiceProcessingPassStart を実行する。
     */
    void OnVoiceProcessingPassStart(UINT32 SamplesRequired) override {}
    /**
     * @brief OnBufferEnd を実行する。
     */
    void OnBufferEnd(void* pBufferContext) override {
        finished_ = true;
    }
    /**
     * @brief OnBufferStart を実行する。
     */
    void OnBufferStart(void* pBufferContext) override {}
    /**
     * @brief OnLoopEnd を実行する。
     */
    void OnLoopEnd(void* pBufferContext) override {}
    /**
     * @brief OnVoiceError を実行する。
     */
    void OnVoiceError(void* pBufferContext, HRESULT Error) override {}

    /**
     * @brief IsFinished かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsFinished() const {
        return finished_;
    }

private:
    std::atomic<bool> finished_{false};
};

} // namespace Irufemi