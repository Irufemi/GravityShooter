#pragma once

namespace Irufemi {

/**
 * @enum AudioType
 * @brief 音の種類（BGMかSEかなど）を表す列挙型
 * @details デフォルトのループ設定や音量管理の区分として利用します。
 */
enum class AudioType {
    BGM, ///< バックグラウンドミュージック（通常ループ再生）
    SE   ///< サウンドエフェクト（通常単発再生）
};

} // namespace Irufemi
