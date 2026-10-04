#pragma once

/**
 * @namespace UISound
 * @brief UIシステム共通の効果音再生ユーティリティ
 * @details 階層型サブミックス・バス（UIBus）へのルーティング、スティック連続操作時の
 *          マシンガン鳴り防止（スロットリング）、およびアセットの一元管理を提供します。
 */
class AudioManager;

namespace UISound {

/**
 * @brief UISound システムを初期化します
 * @param[in] audioManager エンジンのAudioManagerポインタ
 */
void Initialize(AudioManager* audioManager);

/**
 * @brief UIサウンドリソースの事前ロードを行います
 */
void Preload();

/**
 * @brief メニューカーソル移動音を再生します
 * @param[in] volumeMultiplier 基本音量に対する乗数（デフォルト: 1.0f）
 */
void PlayCursor(float volumeMultiplier = 1.0f);

/**
 * @brief メニュー決定音を再生します
 * @param[in] volumeMultiplier 基本音量に対する乗数（デフォルト: 1.0f）
 */
void PlayDecide(float volumeMultiplier = 1.0f);

/**
 * @brief メニューキャンセル・戻る音を再生します
 * @param[in] volumeMultiplier 基本音量に対する乗数（デフォルト: 1.0f）
 */
void PlayCancel(float volumeMultiplier = 1.0f);

/**
 * @brief 選択不能・エラー時の警告音を再生します
 * @param[in] volumeMultiplier 基本音量に対する乗数（デフォルト: 1.0f）
 */
void PlayInvalid(float volumeMultiplier = 1.0f);

} // namespace UISound
