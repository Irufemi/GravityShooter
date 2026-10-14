#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <vector>
#include <nlohmann/json.hpp>
#include <memory>

/**
 * @class TelemetrySender
 * @brief ゲーム内の各種データ（FPS, CPU/GPU負荷, 任意の変数）を
 *        外部の監視ツールへUDPで送信するための汎用テレメトリシステム。
 * @details 独立したスレッドで動くため、ゲーム本体の処理に影響を与えません。
 */
class TelemetrySender {
public:
    TelemetrySender();
    ~TelemetrySender();

    // コピー禁止
    TelemetrySender(const TelemetrySender&) = delete;
    TelemetrySender& operator=(const TelemetrySender&) = delete;

    /**
     * @brief システムを初期化し、裏スレッドを起動します。
     * @param targetIp 送信先IP（通常は自PCである 127.0.0.1）
     * @param targetPort 送信先ポート（デフォルト: 8888）
     */
    void Initialize(const std::string& targetIp = "127.0.0.1", uint16_t targetPort = 8888);

    /**
     * @brief スレッドを安全に停止し、ソケットを閉じます。
     */
    void Finalize();

    /**
     * @brief 任意のデータをテレメトリとして登録します。
     * @param key データの名前（例: "Perf/FPS"）
     * @param value データ本体（数値、文字列、配列など json になるものなら何でも可）
     */
    void SetMetric(const std::string& key, const nlohmann::json& value);

    /**
     * @brief 一発限りのイベントログを記録します。
     * @param message ログメッセージ
     */
    void LogEvent(const std::string& message);

    /**
     * @brief フレームの最後に呼び出すことで、溜まったデータを送信スレッドに送るようキックします。
     */
    void OnFrameEnd();

private:
    void ThreadLoop();

    std::thread workerThread_;
    // NOTE: std::condition_variable::wait に渡す std::unique_lock<std::mutex> が
    // 言語仕様上必須のため、Synchronized<T> ではなく生の std::mutex を意図して維持しています。
    std::mutex dataMutex_;
    std::condition_variable cv_;
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> triggerSend_{false};

    // メインスレッドから書き込まれるデータ
    nlohmann::json currentMetrics_;
    std::vector<std::string> pendingEvents_;

    // UDP通信用データ (WinSock依存を隠蔽するためPimplを使用)
    struct NetworkData;
    std::unique_ptr<NetworkData> networkData_;

    bool isWsaInitialized_ = false;
};
