#pragma once
#include "Audio/GameSoundAction.h"
#include <string>
#include <string_view>
#include <optional>

class AudioManager;

/**
 * @struct SoundActionDescriptor
 * @brief サウンド定義情報（パス・デフォルト音量・デフォルトピッチ等）
 */
struct SoundActionDescriptor {
    std::string path;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
};

/**
 * @namespace SoundHelper
 * @brief ゲーム固有のサウンド定義（sound_actions.json）に基づく再生・事前ロード管理
 */
namespace SoundHelper {

/**
 * @brief サウンドシステムを初期化し、sound_actions.json を読み込みます
 * @param audioManager エンジンのAudioManager
 * @param configPath 設定JSONファイルのパス
 * @return 読み込み成功時 true
 */
bool Initialize(AudioManager* audioManager, const std::string& configPath = "resources/config/sound_actions.json");

/**
 * @brief 定義済みの全サウンドアセットをメモリに事前ロード（キャッシュ）します
 */
void PreloadAll();

/**
 * @brief 指定した効果音（GameSE）の定義情報を取得します
 */
const SoundActionDescriptor* GetDescriptor(GameSE se);

/**
 * @brief 指定したBGM（GameBGM）の定義情報を取得します
 */
const SoundActionDescriptor* GetDescriptor(GameBGM bgm);

/**
 * @brief 効果音（GameSE）をワンショット再生します
 * @param se 論理SE識別子
 * @param volumeMultiplier 音量乗数（std::nullopt の場合は JSON の定義値そのまま）
 * @param pitchMultiplier ピッチ乗数（std::nullopt の場合は JSON の定義値そのまま）
 */
void Play(GameSE se, std::optional<float> volumeMultiplier = std::nullopt,
          std::optional<float> pitchMultiplier = std::nullopt);

/**
 * @brief 論理キー名（文字列）から効果音を再生します（JSONイベント・スクリプト連携用）
 */
void PlayByName(std::string_view name, std::optional<float> volumeMultiplier = std::nullopt,
                std::optional<float> pitchMultiplier = std::nullopt);

/**
 * @brief BGM（GameBGM）を再生します
 * @param bgm 論理BGM識別子
 * @param volumeMultiplier 音量乗数
 */
void PlayBGM(GameBGM bgm, std::optional<float> volumeMultiplier = std::nullopt);

/**
 * @brief 現在再生中のBGMを停止します
 */
void StopBGM();

} // namespace SoundHelper
