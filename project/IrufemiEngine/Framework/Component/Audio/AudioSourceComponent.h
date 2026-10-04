#pragma once
#include "Framework/Component/Component.h"
#include <string>
#include <memory>
#include "Audio/AudioPlayer.h"

/**
 * @class AudioSourceComponent
 * @brief 音声再生（BGM/SE）を管理するコンポーネント
 * @details 描画システムのMeshRendererComponent等と同様に、内部でAudioPlayer(Object層)をカプセル化します。
 */
class AudioSourceComponent : public Component {
public:
    AudioSourceComponent();
    ~AudioSourceComponent() override;

    /**
     * @brief 初期化処理
     */
    void Initialize() override;
    /**
     * @brief スポーン時の処理（シーンバインド後の確実なオーディオ初期化）
     */
    void OnSpawned() override;
    /**
     * @brief ゲーム開始時の処理
     */
    void Start() override;
    /**
     * @brief コンポーネント無効化時の処理
     */
    void OnDisable() override;
    /**
     * @brief コンポーネント破棄時の処理
     */
    void OnDestroy() override;
    /**
     * @brief 毎フレーム更新処理
     */
    void Update() override;

    /**
     * @brief コンポーネント名を取得する
     * @return コンポーネント名文字列
     */
    std::string GetComponentName() const override {
        return "AudioSourceComponent";
    }

    /**
     * @brief Editモード時にUpdateを実行するかどうか
     */
    bool CanUpdateInEditMode() const override {
        return true;
    }
    /**
     * @brief インスペクター編集用プロパティの登録
     */
    void OnRegisterProperties() override;

    /** @brief 音声を再生する */
    void Play();
    /** @brief 音声を停止する */
    void Stop();

    /**
     * @brief 再生するオーディオのファイルパスを設定する
     * @param[in] path 音声ファイルの相対パス
     */
    void SetAudioPath(const std::string& path);
    /**
     * @brief 再生音量を設定する
     * @param[in] volume 音量 (0.0f ~ 1.0f)
     */
    void SetVolume(float volume);
    /**
     * @brief ループ再生の有効/無効を設定する
     * @param[in] loop ループ再生するかどうか
     */
    void SetLoop(bool loop);
    /**
     * @brief オーディオカテゴリ種別を設定する
     * @param[in] type オーディオ種別 (BGM or SE)
     */
    void SetAudioType(AudioType type);

private:
    void InitializeAudio();

    std::string audioPath_ = "audio/BGM/bgm_default.wav"; // デフォルト
    int audioType_ = static_cast<int>(AudioType::SE);     // シリアライズ用
    bool playOnAwake_ = false;
    bool loop_ = false;
    float volume_ = 1.0f;
    float lastVolume_ = -1.0f; ///< 不要な音量再設定を抑止するためのキャッシュ値

    std::unique_ptr<AudioPlayer> player_; ///< 実際の再生を担うObject層のインスタンス
};
