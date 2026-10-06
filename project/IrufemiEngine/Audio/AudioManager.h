#pragma once

#include "Audio/Sound.h"
#include "Audio/VoiceInstance.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <wrl/client.h> // ComPtr用

// IXAudio2SourceVoice構造体およびIXAudio2SubmixVoiceを前方宣言
struct IXAudio2SourceVoice;
struct IXAudio2SubmixVoice;
class ThreadPool;
class TaskGroup;

/**
 * @class AudioManager
 * @brief XAudio2 を使用した音声再生とリソース管理を行うマネージャクラス
 * @details サウンドデータのロード、再生中のボイス（VoiceInstance）の管理、およびカテゴリごとの整理を行います。
 */
class AudioManager {
private:
    // コピー禁止
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // XAudio2のコアインターフェース
    Microsoft::WRL::ComPtr<IXAudio2> pXAudio2_;
    IXAudio2MasteringVoice* pMasteringVoice_{nullptr}; ///< IUnknownを継承しないため生ポインタ管理

    // 階層型サブミックス・バス（Audio Submix Graph）
    IXAudio2SubmixVoice* pSubmixBgm_{nullptr}; ///< BGM専用バス
    IXAudio2SubmixVoice* pSubmixSe_{nullptr};  ///< 効果音専用バス
    IXAudio2SubmixVoice* pSubmixUi_{nullptr};  ///< UI効果音専用バス

    // ロードした音声データをファイル名をキーにして保持するマップ
    mutable std::mutex registryMutex_; ///< soundRegistry_ / categoryMap_ 保護用ミューテックス
    std::unordered_map<std::string, std::shared_ptr<Sound>> soundRegistry_;

    // 再生中の VoiceInstance を一元管理
    mutable std::mutex voiceMutex_; ///< activeVoices_ 保護用ミューテックス
    std::vector<std::shared_ptr<VoiceInstance>> activeVoices_;

    // カテゴリ名 → その中にあるファイル名リスト
    std::unordered_map<std::string, std::vector<std::string>> categoryMap_;

    // ファイナライズ済みフラグ
    bool finalized_{false};

    // ボリュームキャッシュ（不要なSetVolume呼び出し防止用）
    float cachedMasterVolume_{-1.0f};
    float cachedBgmVolume_{-1.0f};
    float cachedSeVolume_{-1.0f};

    /**
     * @brief 指定カテゴリに対応するサブミックスボイスを取得する
     * @param[in] category オーディオカテゴリ
     * @return サブミックスボイスへのポインタ（Masterの場合はnullptr）
     */
    IXAudio2SubmixVoice* GetSubmixVoice(AudioCategory category) const;

    /**
     * @brief 管理対象のボイスかどうか判定する
     * @param[in] instance 判定対象のVoiceInstance
     * @return 管理対象であればtrue
     */
    bool IsManagedVoice(std::shared_ptr<VoiceInstance> instance) const;

public:
    /**
     * @brief 指定したオーディオカテゴリのバス音量を設定する（再生中ボイスに即座に反映）
     * @param[in] category 対象カテゴリ (Master, BGM, SE, UI)
     * @param[in] volume 音量 (0.0f ～ 1.0f)
     */
    void SetCategoryVolume(AudioCategory category, float volume);

    /**
     * @brief 指定したオーディオカテゴリのバス音量を取得する
     * @param[in] category 対象カテゴリ
     * @return バス音量
     */
    float GetCategoryVolume(AudioCategory category) const;
    /**
     * @brief コンストラクタ
     */
    AudioManager() = default;

    /**
     * @brief デストラクタ
     */
    ~AudioManager();

    /**
     * @brief XAudio2 エンジンの初期化
     */
    void Initialize();

    /**
     * @brief 終了処理
     * @details すべての再生中ボイスを停止し、リソースを解放します。
     */
    void Finalize();

    /**
     * @brief 指定フォルダから対応する音声ファイルをすべてロードする（ThreadPool指定時は並列ロード）
     * @param[in] folderPath ロード対象のフォルダパス
     * @param[in] threadPool 並列ロードに使用するThreadPool（nullptr時は同期ロード）
     */
    void LoadAllSoundsFromFolder(const std::string& folderPath, class ThreadPool* threadPool = nullptr);

    /**
     * @brief サブフォルダをカテゴリとしてロードする
     * @param[in] folderPath ロード対象のパス
     * @param[in] category カテゴリ名
     * @param[in] threadPool 並列ロードに使用するThreadPool（nullptr時は同期ロード）
     * @param[in] group 待機用のTaskGroup（内部用）
     */
    void LoadSoundsFromFolder(const std::string& folderPath, const std::string& category,
                              class ThreadPool* threadPool = nullptr, std::shared_ptr<class TaskGroup> group = nullptr);

    /**
     * @brief カテゴリ内のサウンド名一覧を取得（ソート済み）
     * @param[in] category カテゴリ名
     * @return サウンド名のリスト
     */
    std::vector<std::string> GetSoundNames(const std::string& category) const;

    /**
     * @brief ロード済みのサウンドデータを取得
     * @param[in] name サウンドキー名
     * @return サウンドデータへのポインタ
     */
    std::shared_ptr<Sound> GetSoundData(const std::string& name) const;

    /**
     * @brief 利用可能なサウンドカテゴリ一覧を取得
     * @return カテゴリ名のリスト
     */
    std::vector<std::string> GetCategories() const;

    /**
     * @brief 毎フレームの更新処理
     * @details 再生が終了したボイスのクリーンアップなどを行います。
     */
    void Update();

    /**
     * @brief サウンドを再生する
     * @param[in] soundData ロード済みのサウンドデータ
     * @param[in] loop ループ再生するか
     * @param[in] volume 音量 (0.0 ～ 1.0)
     * @param[in] category 再生するカテゴリ (デフォルト: SE)
     * @return 再生中インスタンスへの弱参照。操作が必要な場合に保持してください。
     */
    std::weak_ptr<VoiceInstance> Play(std::shared_ptr<Sound> soundData, bool loop = false, float volume = 1.0f,
                                      AudioCategory category = AudioCategory::SE);

    /**
     * @brief サウンドをピッチ（周波数比率）指定で再生する
     * @param[in] soundData ロード済みのサウンドデータ
     * @param[in] loop ループ再生するか
     * @param[in] volume 音量 (0.0 ～ 1.0)
     * @param[in] pitch 周波数比率 (1.0fが等倍, 0.5f〜2.0f等)
     * @param[in] category 再生するカテゴリ (デフォルト: SE)
     * @return 再生中インスタンスへの弱参照
     */
    std::weak_ptr<VoiceInstance> Play(std::shared_ptr<Sound> soundData, bool loop, float volume, float pitch,
                                      AudioCategory category = AudioCategory::SE);

    /**
     * @brief ファイルパスから直接サウンドをロード・再生する（フォールバック指定・ピッチ指定対応）
     * @param[in] filePath 再生対象のサウンドファイルパス
     * @param[in] volume 音量 (0.0 ～ 1.0)
     * @param[in] pitch 周波数比率 (1.0fが等倍)
     * @param[in] category 再生するカテゴリ (デフォルト: SE)
     * @param[in] fallbackPath 指定ファイルが存在しない場合の代替ファイルパス（空文字可）
     * @return 再生中インスタンスへの弱参照
     */
    std::weak_ptr<VoiceInstance> PlayByFile(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f,
                                            AudioCategory category = AudioCategory::SE,
                                            const std::string& fallbackPath = "");

    /**
     * @brief 再生中のサウンドを停止する
     * @param[in] instance 停止させたいインスタンスの弱参照
     */
    void Stop(std::weak_ptr<VoiceInstance>& instance);

    /**
     * @brief すべての再生中サウンドを強制停止する
     */
    void StopAll();

    /**
     * @brief すべての再生中サウンドを一時停止する
     */
    void PauseAll();

    /**
     * @brief すべての一時停止中のサウンドを再開する
     */
    void ResumeAll();

    /**
     * @brief 特定のカテゴリのサウンドをすべて一時停止する
     * @param[in] category 対象カテゴリ
     */
    void PauseCategory(AudioCategory category);

    /**
     * @brief 特定のカテゴリのサウンドをすべて再開する
     * @param[in] category 対象カテゴリ
     */
    void ResumeCategory(AudioCategory category);

    /**
     * @brief 重複を避けてファイルからロードまたは取得する
     * @param[in] filePath ファイルパス
     * @param[in] key 識別キー（省略時はファイルパスをキーにする）
     * @return サウンドデータへの共有ポインタ
     */
    std::shared_ptr<Sound> GetOrLoadSoundByFile(const std::string& filePath, const std::string& key = "");

    /**
     * @brief 指定キーのサウンドがロード済みか確認する
     * @param[in] key サウンドキー
     * @return ロード済みならtrue
     */
    bool HasSound(const std::string& key) const;
};
