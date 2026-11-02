#include <Windows.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include "Core/GameApplication.h"
#include "Core/Utility/Log.h"

/**
 * @brief Windowsアプリケーションのエントリーポイント
 * @param hInstance インスタンスハンドル
 * @param hPrevInstance 以前のインスタンスハンドル (常にNULL)
 * @param lpCmdLine コマンドライン引数
 * @param nCmdShow ウィンドウの表示状態
 * @return int 終了コード
 * @details この関数からGameApplicationを生成し、ゲームループを開始します。
 */
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    try {
        // ゲームアプリケーションの生成と実行
        auto game = std::make_unique<GameApplication>();
        game->Run();
    } catch (const std::exception& e) {
        // 予期せぬ例外が発生した場合はメッセージボックスで通知
        Log::OutPutLog(std::cerr, std::string("Fatal Error: ") + e.what());
        MessageBoxA(nullptr, e.what(), "Irufemi Engine - Fatal Error", MB_OK | MB_ICONERROR);
    } catch (...) {
        Log::OutPutLog(std::cerr, "Fatal Error: Unknown Error Occurred.");
        MessageBoxA(nullptr, "Unknown Error Occurred.", "Irufemi Engine - Fatal Error", MB_OK | MB_ICONERROR);
    }
    return 0;
}