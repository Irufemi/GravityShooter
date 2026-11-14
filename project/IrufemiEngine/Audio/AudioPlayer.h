#pragma once
#include "Audio/AudioManager.h"
#include "Audio/AudioType.h"
#include <string>
#include <memory>
#include <optional>

namespace Irufemi {

/**
 * @class AudioPlayer
 * @brief プログラマーが直接コードから再生命令を出せるAudio Objectクラス
 * @details AudioManagerと連携し、VoiceInstanceのライフサイクルをカプセル化します。
 * 描画システムの `StaticModelObject` のように、コンポーネントを介さずコードから直接利用可能です。
 */
class AudioPlayer {
public:
    /**
     * @brief コンストラクタ
     * @param[in] audioManager 音声再生を司るAudioManagerへのポインタ
     * @param[in] type オーディオの種類 (BGM or SE)
     */
    AudioPlayer(AudioManager* audioManager, AudioType type = AudioType::SE);
    ~AudioPlayer();

    /**
     * @brief 音源ファイルを読み込んで準備する
     * @param[in] filePath 音声ファイルのパス
     * @param[in] key キャッシュ用のキー（空の場合はfilePathがキーになる）
     */
    void Initialize(const std::string& filePath, const std::string& key = "");

    /**
     * @brief 再生する
     * @param[in] loop ループ再生するか。未指定(std::nullopt)の場合は AudioType に応じたデフォルト値(BGM=true,
     * SE=false)を使用します。
     */
    void Play(std::optional<bool> loop = std::nullopt);

    /**
     * @brief 停止する
     */
    void Stop();

    /**
     * @brief 一時停止する
     */
    void Pause();

    /**
     * @brief 再開する
     */
    void Resume();

    /**
     * @brief 音量を設定する
     * @param[in] volume 音量 (0.0 ～ 1.0)
     */
    void SetVolume(float volume);

    /**
     * @brief 現在の音量を取得する
     * @return 音量 (0.0 ～ 1.0)
     */
    float GetVolume() const {
        return volume_;
    }

    /**
     * @brief 再生中かどうか判定する
     * @return 再生中ならtrue
     */
    bool IsPlaying() const {
        return !voice_.expired();
    }

    /**
     * @brief オーディオタイプを取得する
     * @return BGMまたはSE
     */
    AudioType GetAudioType() const {
        return type_;
    }

    /**
     * @brief オーディオタイプを設定する
     * @param[in] type BGMまたはSE
     */
    void SetAudioType(AudioType type) {
        type_ = type;
    }

private:
    AudioManager* audioManager_{nullptr}; ///< 再生管理マネージャへのポインタ（DIによる注入）
    AudioType type_{AudioType::SE};       ///< BGMかSEかの区分

    std::shared_ptr<Sound> sound_{nullptr}; ///< ロード済みのサウンドデータ参照
    std::weak_ptr<VoiceInstance> voice_;    ///< 再生中のボイスインスタンス弱参照

    float volume_{1.0f};   ///< 設定音量
    std::string soundKey_; ///< キャッシュ用キー
};

} // namespace Irufemi
