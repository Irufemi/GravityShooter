#pragma once

#include <cstdint>

namespace Irufemi {

/**
 * @enum UILayer
 * @brief UI描画階層の大分類（レンダリングパスおよび画面遷移フェードとの契約）
 */
enum class UILayer : uint8_t {
    GameHUD = 0,       ///< 通常ゲームHUD（HP, スコア, レティクル等。UIPassで描画、画面遷移暗転で消える）
    Menu = 1,          ///< ポーズメニュー, ダイアログ（UIPassの後段で描画、画面遷移暗転で消える）
    SystemOverlay = 2, ///< デモ案内, ロード画面, キオスク通知（TopMostPassで描画、暗転の上、絶対最前面）
    Debug = 3          ///< デバッグコンソール, プロファイラ, ImGui（TopMostPassの最前面）
};

} // namespace Irufemi
