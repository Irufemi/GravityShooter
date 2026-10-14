#pragma once

#include <fstream>
#include <ostream>
#include <string>

#include <deque>
#include <mutex>
#include "Core/Utility/Synchronized.h"

class Log {
public:
    struct LogEntry {
        std::string message;
        bool isError;
    };

    using LogContainer = std::deque<LogEntry>;

private: // メンバ変数
    inline static std::ofstream logStream_;
    inline static Irufemi::Synchronized<LogContainer, std::mutex> logHistory_; // ログ履歴 (スレッドセーフ)
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
        logHistory_.WithRead(std::forward<Func>(func));
    }

    /**
     * @brief 現在のログ履歴を取得します（エディタのコンソールパネル用）
     */
    static LogContainer GetLogHistory() {
        return logHistory_.WithRead([](const LogContainer& history) { return history; });
    }

    /**
     * @brief ログ履歴をクリアします
     */
    static void ClearLogHistory() {
        logHistory_.WithWrite([](LogContainer& history) { history.clear(); });
    }
};