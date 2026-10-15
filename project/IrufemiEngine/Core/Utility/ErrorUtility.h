#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <stdexcept>
#include <comdef.h>

class ErrorUtility {
public:
    /**
     * @brief HRESULTをチェックし、FAILEDであれば例外をスローする
     * @param hr チェックするHRESULT
     * @param msg 任意のエラーメッセージ
     * @param file ファイル名 (__FILE__)
     * @param line 行番号 (__LINE__)
     */
    static void ThrowIfFailed(HRESULT hr, const std::string& msg = "", const char* file = nullptr, int line = 0);

    /**
     * @brief DirectX HRESULTエラーの詳細ログを出力し、ビルド構成に応じたデバッグ・リカバリ処理を行う（例外は投げない）
     * @param hr 失敗したHRESULT
     * @param msg 任意のエラーメッセージ
     * @param func 関数名 (__FUNCTION__)
     * @param file ファイル名 (__FILE__)
     * @param line 行番号 (__LINE__)
     * @return 常に false (SUCCEEDED(hr)が偽だったことを表す)
     */
    [[nodiscard]] static bool ReportDxError(HRESULT hr, const std::string& msg = "", const char* func = nullptr,
                                            const char* file = nullptr, int line = 0);

    /**
     * @brief HRESULTのエラーコードから詳細なエラーメッセージ文字列を生成する
     */
    static std::string GetErrorMessage(HRESULT hr);

    /**
     * @brief メッセージボックスでエラーを通知する
     */
    static void ShowErrorBox(const std::string& title, const std::string& message);

    /**
     * @brief 致命的なエラーアサーション
     */
    static void Assert(bool condition, const std::string& msg, const char* file, int line);

    /**
     * @brief 非致命的な警告アサーション
     */
    static void Warning(bool condition, const std::string& msg, const char* file, int line);
};

// =============================================================================
// DirectX 12 HRESULT エラーハンドリングマクロ (商用AAAエンジン基準)
// =============================================================================

/**
 * @brief 非致命的・リカバリ可能なDirectXチェックマクロ
 * @details 成否のboolを[[nodiscard]]で返すため、呼び出し元で早期リターン(Graceful Degradation)を強制します。
 *          例: if (!IRUFEMI_CHECK_HR(hr, "Failed to create texture!")) { return; }
 */
#define IRUFEMI_CHECK_HR(hr, msg)                                                                                      \
    (SUCCEEDED(hr) ? true : ErrorUtility::ReportDxError((hr), (msg), __FUNCTION__, __FILE__, __LINE__))

/**
 * @brief 致命的なDirectXアサーションマクロ
 * @details デバイス初期化やスワップチェーン生成等、失敗時にゲームが物理的に成立しない箇所用です。
 */
#define IRUFEMI_ASSERT_HR(hr, msg)                                                                                     \
    do {                                                                                                               \
        HRESULT hr_ = (hr);                                                                                            \
        if (FAILED(hr_)) {                                                                                             \
            ErrorUtility::ThrowIfFailed(hr_, (msg), __FILE__, __LINE__);                                               \
        }                                                                                                              \
    } while (0)

// 後方互換用エイリアス
#define ASSERT_IF_FAILED(hr)          IRUFEMI_ASSERT_HR((hr), "")
#define ASSERT_IF_FAILED_MSG(hr, msg) IRUFEMI_ASSERT_HR((hr), (msg))

// =============================================================================
// エンジン標準アサーションマクロ
// =============================================================================

#define IRUFEMI_ASSERT(condition)                                                                                      \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            ErrorUtility::Assert(false, #condition, __FILE__, __LINE__);                                               \
        }                                                                                                              \
    } while (0)

#define IRUFEMI_ASSERT_MSG(condition, msg)                                                                             \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            ErrorUtility::Assert(false, (msg), __FILE__, __LINE__);                                                    \
        }                                                                                                              \
    } while (0)

#define IRUFEMI_WARNING(condition, msg)                                                                                \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            ErrorUtility::Warning(false, (msg), __FILE__, __LINE__);                                                   \
        }                                                                                                              \
    } while (0)
