#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <memory>
#include <vector>

/**
 * @class TelemetryReceiver
 * @brief 外部ツール（TelemetryMonitorなど）からUDP経由で送られてくるコマンドを受信するクラス
 * @details 専用のワーカースレッドで非ブロッキング受信し、スレッドセーフキューに蓄積します。
 *          メインスレッドのフレーム更新時に ProcessPendingCommands() を呼び出すことで、
 *          安全に ConsoleCommandSystem 経由でコマンドを実行します。
 */
class TelemetryReceiver {
public:
    TelemetryReceiver();
    ~TelemetryReceiver();

    TelemetryReceiver(const TelemetryReceiver&) = delete;
    TelemetryReceiver& operator=(const TelemetryReceiver&) = delete;

    /**
     * @brief システムを初期化し、受信ワーカースレッドを起動します。
     * @param[in] listenPort リッスンするUDPポート（デフォルト: 8889）
     */
    void Initialize(uint16_t listenPort = 8889);

    /**
     * @brief ワーカースレッドを安全に停止し、ソケットを解放します。
     */
    void Finalize();

    /**
     * @brief メインスレッドから毎フレーム呼び出され、受信したコマンドを順次実行します。
     * @details ConsoleCommandSystem::ExecuteCommand() をメインスレッド上で安全にディスパッチします。
     */
    void ProcessPendingCommands();

private:
    void ThreadLoop();

    std::thread workerThread_;
    std::atomic<bool> isRunning_{false};
    bool isWsaInitialized_ = false;

    std::mutex commandQueueMutex_;
    std::queue<std::string> pendingCommands_;

    struct NetworkData;
    std::unique_ptr<NetworkData> networkData_;
};
