#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <wrl.h>
#include <dxcapi.h>

namespace Irufemi {
class ShaderCompiler; // 前方宣言
class ShaderManager;
}

/**
 * @class MagicBrushClient
 * @brief Python側のローカルサーバーと通信し、AIシェーダーの生成および自己修復を管理する非同期クライアント
 */
class MagicBrushClient {
public:
    enum class State { Idle, Generating, Compiling, Fixing, Success, WaitingForScreenshot, VisualEvaluating, Error };

    // 履歴管理用構造体
    struct GenerationHistory {
        std::string prompt;
        std::string hlslCode;
        std::string shaderName;
    };
    MagicBrushClient();
    ~MagicBrushClient();

    /**
     * @brief シェーダー生成リクエストを非同期で開始する
     */
    void StartGeneration(const std::string& prompt, const std::string& referenceImagePath,
                         const std::string& shaderName, const std::string& outputDirectory,
                         Irufemi::ShaderManager* shaderManager);

    /**
     * @brief スクリーンショット撮影完了後、AIによる視覚的自己修復（フィードバックループ）を開始する
     */
    void StartVisualFix(const std::string& referenceImagePath, const std::string& screenshotPath,
                        const std::string& currentHlslCode, const std::string& shaderName,
                        Irufemi::ShaderManager* shaderManager);

    // サーバープロセス管理
    bool StartPythonServer();
    void StopPythonServer();
    void RestartPythonServer();
    bool IsServerRunning() const;

    /**
     * @brief 現在のステータスを取得する
     */
    State GetState() const {
        return state_.load();
    }

    /**
     * @brief エラー時のメッセージを取得する
     */
    std::string GetErrorMessage() const;

    /**
     * @brief 成功時にコンパイル済みのシェーダーBlobを取得する
     */
    Microsoft::WRL::ComPtr<IDxcBlob> GetResultBlob();

    /**
     * @brief サーバーからのログ一覧を取得する
     */
    std::vector<std::string> GetServerLogs() const;

    /**
     * @brief 生成履歴を取得する
     */
    const std::vector<GenerationHistory>& GetHistory() const {
        return history_;
    }

    /**
     * @brief 指定したインデックスの履歴からHLSLを復元・再コンパイルする
     */
    bool RestoreHistory(size_t index, Irufemi::ShaderManager* shaderManager);

private:
    void ProcessThread(std::string prompt, std::string referenceImagePath, std::string shaderName,
                       std::string outputDirectory, Irufemi::ShaderManager* shaderManager);
    void VisualFixThread(std::string referenceImagePath, std::string screenshotPath, std::string currentHlslCode,
                         std::string shaderName, Irufemi::ShaderManager* shaderManager);
    void LogReadThread();

    // HTTPリクエスト（curl.exe をプロセスとして呼び出す簡易実装）
    std::string SendPostRequest(const std::string& endpoint, const std::string& jsonPayload);

    // 黒窓を出さずにコマンドを実行して標準出力を取得する
    std::string ExecuteCommandHidden(const std::string& command);
    // JSON用エスケープ
    std::string EscapeJSON(const std::string& input);

private:
    std::thread workerThread_;
    std::atomic<State> state_;

    mutable std::mutex mutex_;
    std::string errorMessage_;
    Microsoft::WRL::ComPtr<IDxcBlob> resultBlob_;

    // 生成履歴
    std::vector<GenerationHistory> history_;

    // Pythonサーバープロセス用ハンドル
    void* pythonProcessHandle_ = nullptr; // HANDLE (Windows.hを含めないため void* にしておく)
    void* pythonThreadHandle_ = nullptr;
    unsigned long pythonProcessId_ = 0; // DWORD

    // パイプ・ログ管理
    void* hChildStd_OUT_Rd_ = nullptr; // HANDLE
    void* hChildStd_OUT_Wr_ = nullptr; // HANDLE
    std::thread logThread_;
    std::atomic<bool> isLogThreadRunning_{false};
    mutable std::mutex logMutex_;
    std::vector<std::string> serverLogs_;

    // 試行回数の上限
    const int32_t kMaxFixAttempts = 3;
};
