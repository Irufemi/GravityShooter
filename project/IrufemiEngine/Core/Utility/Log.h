#pragma once

#include <fstream>
#include <ostream>
#include <string>

#include <deque>
#include <mutex>

class Log {
public:
    struct LogEntry {
        std::string message;
        bool isError;
    };

    using LogContainer = std::deque<LogEntry>;

private: // メンバ変数
    inline static std::ofstream logStream_;
    inline static LogContainer logHistory_;      // ログの履歴バッファ (O(1) pop_front)
    inline static std::mutex logMutex_;          // ログ履歴保護用ミューテックス
    static constexpr size_t kMaxLogLines = 1000; // メモリ保護のための最大行数

public: // メンバ関数
    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize();

    // ゲッター
    /**
     * @brief LogStream を取得する。
     * @return 取得された LogStream
     */
    std::ofstream& GetLogStream() {
        return logStream_;
    }

    // 出力ウィンドウに文字を出す
    /**
     * @brief OutPutLog を実行する。
     */
    static void OutPutLog(std::ostream& os, const std::string& message);

    /**
     * @brief ログ履歴を排他ロック下でゼロコピー走査・処理します（エディタコンソール等での高速描画用）
     * @tparam Func 呼び出し可能オブジェクト型 (void(const std::vector<LogEntry>&))
     * @param[in] func ログ履歴を受け取る関数またはラムダ式
     */
    template <typename Func> static void WithLogHistory(Func&& func) {
        std::lock_guard<std::mutex> lock(logMutex_);
        func(logHistory_);
    }

    /**
     * @brief 現在のログ履歴を取得します（エディタのコンソールパネル用）
     */
    static LogContainer GetLogHistory() {
        std::lock_guard<std::mutex> lock(logMutex_);
        return logHistory_;
    }

    /**
     * @brief ログ履歴をクリアします
     */
    static void ClearLogHistory() {
        std::lock_guard<std::mutex> lock(logMutex_);
        logHistory_.clear();
    }
};