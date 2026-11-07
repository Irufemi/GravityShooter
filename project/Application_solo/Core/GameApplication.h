#pragma once

#include <string_view>

/**
 * @namespace GameScenes
 * @brief ゲーム内で使用される全シーン名のSSOT（Single Source of Truth）定義
 * @details シーン登録および TransitionTo / PushScene で使用するキーを一元管理します。
 */
namespace GameScenes {
inline constexpr std::string_view Title = "Title";
inline constexpr std::string_view Select = "Select";
inline constexpr std::string_view InGame = "InGame";
inline constexpr std::string_view Pause = "Pause";
inline constexpr std::string_view TL1 = "TL1";
inline constexpr std::string_view Result = "Result";
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
inline constexpr std::string_view Debug = "Debug";
#endif
inline constexpr std::string_view Options = "OptionsScene";
inline constexpr std::string_view HowToPlay = "HowToPlayScene";
} // namespace GameScenes

/**
 * @class GameApplication
 * @brief ゲームアプリケーション全体のエントリーポイントと実行を管理するクラス
 * @details エンジンの初期化、シーンの登録、ゲームループの開始など、
 *          アプリケーションのライフサイクル全体を統括します。
 */
class GameApplication {
public:
    /**
     * @brief コンストラクタ
     */
    GameApplication();
    /**
     * @brief デストラクタ
     */
    ~GameApplication();

    /**
     * @brief ゲームアプリケーションを実行します
     * @details エンジンの初期化からゲームループの終了まで、全ての処理を管理します。
     */
    void Run();

private:
    // コピー禁止
    GameApplication(const GameApplication&) = delete;
    GameApplication& operator=(const GameApplication&) = delete;
};