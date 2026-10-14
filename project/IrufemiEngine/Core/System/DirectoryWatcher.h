#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <filesystem>

/**
 * @class DirectoryWatcher
 * @brief 指定したディレクトリ（サブディレクトリ含む）のファイル変更を監視するクラス
 * @details Windows API (ReadDirectoryChangesW) を用いてバックグラウンドスレッドで監視を行い、
 *          ファイル追加・削除・変更・リネームが発生した際にコールバックを発火します。
 */
class DirectoryWatcher {
public:
    /**
     * @brief コンストラクタ
     * @param targetDirectory 監視対象のディレクトリパス
     * @param onChangeCallback 変更検知時に呼ばれるコールバック関数
     * @warning
     * このコールバックは監視用ワーカースレッドから直接呼び出されます。DirectXリソースの操作などメインスレッド専用の処理を行う場合は、スレッドセーフに実装するかメインスレッドへのキューイングを行ってください。
     */
    DirectoryWatcher(const std::filesystem::path& targetDirectory, std::function<void()> onChangeCallback);

    /**
     * @brief デストラクタ
     * @details バックグラウンドスレッドを安全に終了・破棄します。
     */
    ~DirectoryWatcher();

private:
    /**
     * @brief WatchLoop を実行する。
     */
    void WatchLoop();

    std::filesystem::path targetDirectory_;  ///< 監視対象ディレクトリパス
    std::function<void()> onChangeCallback_; ///< 変更検知コールバック
    std::atomic<bool> isRunning_{false};     ///< 監視ループ実行中フラグ
    std::thread workerThread_;               ///< バックグラウンドワーカースレッド
    void* directoryHandle_ =
        nullptr; ///< ディレクトリハンドル (HANDLE, windows.hのインクルード漏れを防ぐためvoid*で保持)
    void* stopEvent_ = nullptr; ///< 終了通知用イベントハンドル (HANDLE)
};
