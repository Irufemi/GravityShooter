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
 * @brief 外部ツール（TelemetryMonitorなど）からTCP経由で送られてくるコマンドを受信・実行し、結果を返信するクラス
 * @details 専用のワーカースレッドで非ブロッキングにTCP接続をリッスン・受信し、スレッドセーフキューに蓄積します。
 *          メインスレッドのフレーム更新時に ProcessPendingCommands() を呼び出すことで、
 *          安全に ConsoleCommandSystem 経由でコマンドを実行し、実行結果をクライアントへTCP返信します。
 */
class TelemetryReceiver {
public:
    TelemetryReceiver();
    ~TelemetryReceiver();

    TelemetryReceiver(const TelemetryReceiver&) = delete;
    TelemetryReceiver& operator=(const TelemetryReceiver&) = delete;

    /**
     * @brief システムを初期化し、TCP受信ワーカースレッドを起動します。
     * @param[in] listenPort リッスンするTCPポート（デフォルト: 8889）
     */
    void Initialize(uint16_t listenPort = 8889);

    /**
     * @brief ワーカースレッドを安全に停止し、ソケットを解放します。
     */
    void Finalize();

    /**
     * @brief メインスレッドから毎フレーム呼び出され、受信したコマンドを順次実行し、結果をTCP返信します。
     * @details ConsoleCommandSystem::ExecuteCommand() をメインスレッド上で安全にディスパッチします。
     */
    void ProcessPendingCommands();

private:
    void ThreadLoop();

    struct PendingCommand {
        std::string command;
        uint64_t clientId = 0;
    };

    std::thread workerThread_;
    std::atomic<bool> isRunning_{false};
    bool isWsaInitialized_ = false;

    std::mutex commandQueueMutex_;
    std::queue<PendingCommand> pendingCommands_;

    std::mutex responseQueueMutex_;
    struct ResponseMessage {
        uint64_t clientId = 0;
        std::string message;
    };
    std::vector<ResponseMessage> pendingResponses_;

    struct NetworkData;
    std::unique_ptr<NetworkData> networkData_;
};
