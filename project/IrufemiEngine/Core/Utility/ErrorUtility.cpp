#include "Core/Utility/ErrorUtility.h"
#include "Core/Utility/Log.h"
#include "Core/Utility/StringUtility.h"
#include <iostream>
#include <format>

void ErrorUtility::ThrowIfFailed(HRESULT hr, const std::string& msg, const char* file, int line) {
    if (FAILED(hr)) {
        std::string errMsg = GetErrorMessage(hr);
        std::string hexHr = std::format("0x{:08X}", static_cast<uint32_t>(hr));
        std::string fullMsg = "[DirectX Fatal Error] " + hexHr + ": " + errMsg;
        if (!msg.empty()) {
            fullMsg += " | Info: " + msg;
        }
        if (file) {
            fullMsg += "\nFile: " + std::string(file);
            fullMsg += "\nLine: " + std::to_string(line);
        }

        // ログ出力（コンソールやファイル）
        Log::OutPutLog(std::cerr, fullMsg);

#if defined(_DEBUG)
        int result = MessageBoxW(
            nullptr,
            ConvertString(fullMsg + "\nPress Abort to break into debugger, Retry to ignore, Ignore to continue.")
                .c_str(),
            L"Irufemi Engine - DirectX Fatal Error", MB_ABORTRETRYIGNORE | MB_ICONERROR);
        if (result == IDABORT) {
            __debugbreak();
        }
#elif defined(EditorMode)
        ShowErrorBox("Irufemi Editor - DirectX Fatal Error", fullMsg);
        throw std::runtime_error(fullMsg);
#elif defined(DEVELOPMENT)
        ShowErrorBox("DirectX Fatal Error", fullMsg);
        throw std::runtime_error(fullMsg);
#else
        ShowErrorBox("Error", "DirectX encountered an unrecoverable error and will now exit.");
        std::exit(static_cast<int>(hr));
#endif
    }
}

bool ErrorUtility::ReportDxError(HRESULT hr, const std::string& msg, const char* func, const char* file, int line) {
    if (SUCCEEDED(hr)) {
        return true;
    }

    std::string errMsg = GetErrorMessage(hr);
    std::string hexHr = std::format("0x{:08X}", static_cast<uint32_t>(hr));
    std::string fullMsg = "[DirectX Recoverable Error] " + hexHr + ": " + errMsg;
    if (!msg.empty()) {
        fullMsg += " | Info: " + msg;
    }
    if (func) {
        fullMsg += "\nFunction: " + std::string(func);
    }
    if (file) {
        fullMsg += "\nFile: " + std::string(file);
    }
    if (line) {
        fullMsg += "\nLine: " + std::to_string(line);
    }

    // ログ出力（std::cerr に送ることで Log::logHistory_ に isError = true で入り、Editorの ConsolePanel
    // に赤字表示される）
    Log::OutPutLog(std::cerr, fullMsg);

#if defined(_DEBUG)
    // Debugビルド: プログラマがアタッチしているため、デバッガでブレークして原因特定を可能にする
    if (IsDebuggerPresent()) {
        __debugbreak();
    }
#elif defined(EditorMode)
    // EditorMode: エディタ本体を落とさず、ConsolePanelに赤字通知して呼び出し元で安全に早期リターン
#elif defined(DEVELOPMENT)
    // Development: テスターや展示会でのプレイを中断させず、ログ記録のみでゲーム続行
#else
    // Release: プレイヤーに何も見せず、サイレントに早期リターン
#endif

    return false;
}

std::string ErrorUtility::GetErrorMessage(HRESULT hr) {
    _com_error err(hr);
#ifdef UNICODE
    return ConvertString(err.ErrorMessage());
#else
    return std::string(err.ErrorMessage());
#endif
}

void ErrorUtility::ShowErrorBox(const std::string& title, const std::string& message) {
    MessageBoxW(nullptr, ConvertString(message).c_str(), ConvertString(title).c_str(), MB_OK | MB_ICONERROR);
}

void ErrorUtility::Assert(bool condition, const std::string& msg, const char* file, int line) {
    if (!condition) {
        std::string fullMsg = "Assertion Failed!\n";
        if (!msg.empty()) {
            fullMsg += "Message: " + msg + "\n";
        }
        if (file) {
            fullMsg += "File: " + std::string(file) + "\n";
            fullMsg += "Line: " + std::to_string(line) + "\n";
        }

        // ログ出力
        Log::OutPutLog(std::cerr, fullMsg);

#ifdef _DEBUG
        // Debugビルド時はダイアログを出してからデバッガで停止
        int result = MessageBoxW(
            nullptr,
            ConvertString(fullMsg + "\nPress Abort to break into debugger, Retry to ignore, Ignore to continue.")
                .c_str(),
            L"Irufemi Engine - Assertion Failed", MB_ABORTRETRYIGNORE | MB_ICONERROR);
        if (result == IDABORT) {
            __debugbreak();
        }
#else
        // Releaseビルド時は例外を投げて安全に終了
        throw std::runtime_error(fullMsg);
#endif
    }
}

void ErrorUtility::Warning(bool condition, const std::string& msg, const char* file, int line) {
    if (!condition) {
        std::string fullMsg = "[WARNING] ";
        if (!msg.empty()) {
            fullMsg += msg + " ";
        }
        if (file) {
            fullMsg += "(File: " + std::string(file) + ", Line: " + std::to_string(line) + ")\n";
        }

        // std::cerr 出力を通じて Log::logHistory_ に捕捉され、エディタの ConsolePanel に自動転送される
        Log::OutPutLog(std::cerr, fullMsg);

        // 処理は停止せずそのまま続行
    }
}
