#pragma once
#include <array>
#include <cstdint>
#include <string_view>

/**
 * @enum GameSE
 * @brief ゲーム内で使用する論理効果音識別子（型安全）
 * @details resources/config/sound_actions.json の "SE" セクションと 1 対 1 で対応します。
 */
enum class GameSE : uint32_t {
    DebrisShatter, ///< デブリ粉砕音
    FcsOnline,     ///< FCSシステム起動音
    GravityWarp,   ///< 重力ワープ演出音
    HudSnap,       ///< UIスナップ・決定カチッ音
    LaunchCharge,  ///< 出撃チャージ音
    MenuCancel,    ///< メニューキャンセル音
    MenuCursor,    ///< メニューカーソル移動音
    MenuDecide,    ///< メニュー決定音
    PlayerBoost,   ///< プレイヤーブースト音
    ResultImpact,  ///< リザルトインパクト衝撃音
    WaterBreach,   ///< 空間突破・突入音
    Count
};

/**
 * @enum GameBGM
 * @brief ゲーム内で使用する論理BGM識別子（型安全）
 * @details resources/config/sound_actions.json の "BGM" セクションと 1 対 1 で対応します。
 */
enum class GameBGM : uint32_t {
    Title,  ///< タイトルBGM
    Stage1, ///< ステージ1 BGM
    Count
};

/**
 * @brief GameSE enum を JSON 定義キー名へ変換する（Branchless コンパイル時データテーブル）
 */
inline constexpr const char* GameSEToString(GameSE se) noexcept {
    constexpr std::array<const char*, static_cast<size_t>(GameSE::Count)> kNames = {
        "DebrisShatter", "FcsOnline",  "GravityWarp", "HudSnap",      "LaunchCharge", "MenuCancel",
        "MenuCursor",    "MenuDecide", "PlayerBoost", "ResultImpact", "WaterBreach"};
    const auto idx = static_cast<size_t>(se);
    return idx < kNames.size() ? kNames[idx] : "Unknown";
}

/**
 * @brief GameBGM enum を JSON 定義キー名へ変換する（Branchless コンパイル時データテーブル）
 */
inline constexpr const char* GameBGMToString(GameBGM bgm) noexcept {
    constexpr std::array<const char*, static_cast<size_t>(GameBGM::Count)> kNames = {"Title", "Stage1"};
    const auto idx = static_cast<size_t>(bgm);
    return idx < kNames.size() ? kNames[idx] : "Unknown";
}
