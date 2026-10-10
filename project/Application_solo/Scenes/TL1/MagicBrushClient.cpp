#include "Scenes/TL1/MagicBrushClient.h"
#include "RHI/DirectX12/ShaderCompiler.h"
#include "RHI/DirectX12/ShaderManager.h"
#include "Core/Utility/StringUtility.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <Windows.h>
#include <chrono>
#include <iomanip>
#include <array>
#include <string_view>

MagicBrushClient::MagicBrushClient() : state_(State::Idle) {}

MagicBrushClient::~MagicBrushClient() {
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    StopPythonServer();
}

std::vector<std::string> MagicBrushClient::GetServerLogs() const {
    std::lock_guard<std::mutex> lock(logMutex_);
    return serverLogs_;
}

void MagicBrushClient::StartGeneration(const std::string& prompt, const std::string& referenceImagePath,
                                       const std::string& shaderName, const std::string& outputDirectory,
                                       ShaderManager* shaderManager) {
    if (state_ == State::Generating || state_ == State::Compiling || state_ == State::Fixing) {
        return; // 既に実行中
    }
    state_ = State::Generating;
    errorMessage_ = "";
    resultBlob_ = nullptr;

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    workerThread_ = std::thread(&MagicBrushClient::ProcessThread, this, prompt, referenceImagePath, shaderName,
                                outputDirectory, shaderManager);
}

std::string MagicBrushClient::GetErrorMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errorMessage_;
}

Microsoft::WRL::ComPtr<IDxcBlob> MagicBrushClient::GetResultBlob() {
    std::lock_guard<std::mutex> lock(mutex_);
    return resultBlob_;
}

bool MagicBrushClient::StartPythonServer() {
    if (IsServerRunning()) {
        return true;
    }

    // サーバープロセスは死んでいるが、前回起動時のログスレッドやパイプが残っている場合に備えて
    // 完全にクリーンアップを行ってから再起動（std::threadの再代入による abort を防ぐ）
    StopPythonServer();

    // パイプの作成
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hOutRead = NULL;
    HANDLE hOutWrite = NULL;
    if (!CreatePipe(&hOutRead, &hOutWrite, &saAttr, 0)) {
        return false;
    }
    // 読み取り側ハンドルは子プロセスに継承させない
    SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);

    hChildStd_OUT_Rd_ = hOutRead;
    hChildStd_OUT_Wr_ = hOutWrite;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE; // サーバーの黒窓を出さない
    si.hStdOutput = hOutWrite;
    si.hStdError = hOutWrite;
    ZeroMemory(&pi, sizeof(pi));

    // 実行ファイル(exe)のパスを取得し、そこから相対パスでToolsディレクトリを導き出す
    char exePathBuf[MAX_PATH];
    GetModuleFileNameA(NULL, exePathBuf, MAX_PATH);
    std::filesystem::path exePath(exePathBuf);

    // exeは WP0/generated/outputs/Editor/ にあるため、3階層上がって project/Tools/ShaderGenerator へ
    std::filesystem::path toolsDir = exePath.parent_path() / "../../../project/Tools/ShaderGenerator";
    std::string currentDir = std::filesystem::absolute(toolsDir).string();

    std::string command = "cmd.exe /c py main.py";
    std::vector<char> cmdBuffer(command.begin(), command.end());
    cmdBuffer.push_back('\0');

    if (!CreateProcessA(nullptr, cmdBuffer.data(), nullptr, nullptr,
                        TRUE, // パイプハンドルを継承させるためにTRUE
                        CREATE_NO_WINDOW, nullptr, currentDir.c_str(), &si, &pi)) {
        CloseHandle(hOutRead);
        CloseHandle(hOutWrite);
        return false;
    }

    // 親プロセス側で書き込みハンドルは不要なので閉じる（閉じないと読み取りスレッドがEOFを検知できない）
    CloseHandle(hOutWrite);
    hChildStd_OUT_Wr_ = nullptr;
    pythonProcessHandle_ = pi.hProcess;
    pythonThreadHandle_ = pi.hThread;
    pythonProcessId_ = pi.dwProcessId;

    // ログ読み取りスレッドを開始
    isLogThreadRunning_ = true;
    logThread_ = std::thread(&MagicBrushClient::LogReadThread, this);
    // サーバーが立ち上がるのを少し待つ
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    return true;
}

void MagicBrushClient::StopPythonServer() {
    if (pythonProcessHandle_) {
        // cmd.exe経由で起動したため、子プロセスのpython.exeもまとめてツリー終了させる
        if (pythonProcessId_ != 0) {
            std::string killCmd = "taskkill /F /PID " + std::to_string(pythonProcessId_) + " /T";
            STARTUPINFOA si = {sizeof(si)};
            PROCESS_INFORMATION pi = {};
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            if (CreateProcessA(nullptr, killCmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                               &si, &pi)) {
                WaitForSingleObject(pi.hProcess, 3000);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }

        CloseHandle((HANDLE)pythonProcessHandle_);
        CloseHandle((HANDLE)pythonThreadHandle_);
        pythonProcessHandle_ = nullptr;
        pythonThreadHandle_ = nullptr;
        pythonProcessId_ = 0;
    }

    isLogThreadRunning_ = false;
    if (hChildStd_OUT_Rd_) {
        CloseHandle((HANDLE)hChildStd_OUT_Rd_);
        hChildStd_OUT_Rd_ = nullptr;
    }
    if (logThread_.joinable()) {
        logThread_.join();
    }
}

void MagicBrushClient::LogReadThread() {
    DWORD dwRead;
    CHAR chBuf[4096];
    std::string currentLine;

    HANDLE hRead = (HANDLE)hChildStd_OUT_Rd_;

    while (isLogThreadRunning_ && hRead != nullptr) {
        bool success = ReadFile(hRead, chBuf, sizeof(chBuf) - 1, &dwRead, NULL);
        if (!success || dwRead == 0) {
            break; // エラーまたはパイプのクローズ
        }

        chBuf[dwRead] = '\0';
        std::string chunk(chBuf);

        // chunk を改行で分割してログに追加
        for (char c : chunk) {
            if (c == '\n') {
                // タイムスタンプの取得
                auto now = std::chrono::system_clock::now();
                auto in_time_t = std::chrono::system_clock::to_time_t(now);
                std::tm bt{};
                localtime_s(&bt, &in_time_t);
                std::stringstream ss;
                ss << "[" << std::put_time(&bt, "%H:%M:%S") << "] " << currentLine;

                {
                    std::lock_guard<std::mutex> lock(logMutex_);
                    serverLogs_.push_back(ss.str());
                }
                currentLine.clear();
            } else if (c != '\r') {
                currentLine += c;
            }
        }
    }
}

void MagicBrushClient::RestartPythonServer() {
    StopPythonServer();
    StartPythonServer();
}

bool MagicBrushClient::IsServerRunning() const {
    if (!pythonProcessHandle_) {
        return false;
    }
    DWORD exitCode = 0;
    if (GetExitCodeProcess((HANDLE)pythonProcessHandle_, &exitCode)) {
        return exitCode == STILL_ACTIVE;
    }
    return false;
}

namespace {
constexpr auto CreateEscapeTable() {
    std::array<std::string_view, 256> table{};
    table['"'] = "\\\"";
    table['\\'] = "\\\\";
    table['\b'] = "\\b";
    table['\f'] = "\\f";
    table['\n'] = "\\n";
    table['\r'] = "\\r";
    table['\t'] = "\\t";
    return table;
}
constexpr auto kJsonEscapeTable = CreateEscapeTable();
} // namespace

std::string MagicBrushClient::EscapeJSON(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        auto escaped = kJsonEscapeTable[static_cast<uint8_t>(c)];
        if (!escaped.empty()) {
            out.append(escaped.data(), escaped.size());
        } else {
            out += c;
        }
    }
    return out;
}

std::string MagicBrushClient::ExecuteCommandHidden(const std::string& command) {
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hOutRead = NULL;
    HANDLE hOutWrite = NULL;
    if (!CreatePipe(&hOutRead, &hOutWrite, &saAttr, 0)) {
        return "ERROR: CreatePipe failed.";
    }
    SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE; // 完全に隠す
    si.hStdOutput = hOutWrite;
    si.hStdError = hOutWrite;

    ZeroMemory(&pi, sizeof(pi));

    std::vector<char> cmdBuffer(command.begin(), command.end());
    cmdBuffer.push_back('\0');

    if (!CreateProcessA(nullptr, cmdBuffer.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si,
                        &pi)) {
        CloseHandle(hOutRead);
        CloseHandle(hOutWrite);
        return "ERROR: CreateProcess failed.";
    }

    CloseHandle(hOutWrite); // 子プロセスが書き終わるのを待つために親側の書き込みハンドルは閉じる

    std::string result;
    DWORD dwRead;
    CHAR chBuf[4096];
    while (ReadFile(hOutRead, chBuf, sizeof(chBuf) - 1, &dwRead, NULL) && dwRead > 0) {
        chBuf[dwRead] = '\0';
        result += chBuf;
    }

    CloseHandle(hOutRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return result;
}

std::string MagicBrushClient::SendPostRequest(const std::string& endpoint, const std::string& jsonPayload) {
    // テンポラリファイルにJSONを書き出す（エスケープ問題を避けるため）
    std::string tempFileName = "magic_brush_temp_req.json";
    std::ofstream ofs(tempFileName);
    if (!ofs) {
        return "ERROR: Failed to create temp request file.";
    }
    ofs << jsonPayload;
    ofs.close();

    // curl.exe をプロセスとして起動し、標準出力を取得する
    std::string command = "cmd.exe /c curl -s -X POST -H \"Content-Type: application/json\" -d @" + tempFileName +
                          " http://127.0.0.1:8000" + endpoint;

    return ExecuteCommandHidden(command);
}

bool MagicBrushClient::RestoreHistory(size_t index, ShaderManager* shaderManager) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= history_.size()) {
        return false;
    }

    const auto& item = history_[index];

    // 一時ファイルにHLSLを書き出す
    std::string tempHlslPath = "resources/shaders/generated/temp_restored_shader.hlsl";
    std::filesystem::create_directories("resources/shaders/generated");
    std::ofstream ofsHlsl(tempHlslPath);
    if (!ofsHlsl) {
        return false;
    }
    ofsHlsl << item.hlslCode;
    ofsHlsl.close();

    // 再コンパイル
    ShaderCompileOptions options;
    options.entryPoint = L"main";
    std::string errorLog;

    // ShaderManager でコンパイル (キャッシュが残っている可能性があるので ReloadShader か GetOrCompile か)
    std::wstring tempHlslPathW = ConvertString(tempHlslPath);
    auto blob = shaderManager->ReloadShader(tempHlslPathW, options, L"ps_6_0", &errorLog);
    if (blob) {
        resultBlob_ = blob;
        state_ = State::Success;
        return true;
    } else {
        errorMessage_ = "Failed to restore history.\n" + errorLog;
        state_ = State::Error;
        return false;
    }
}

void MagicBrushClient::ProcessThread(std::string prompt, std::string referenceImagePath, std::string shaderName,
                                     std::string outputDirectory, ShaderManager* shaderManager) {
    std::string currentHlsl = "";

    // 1. 初回リクエストの作成
    std::string jsonReq =
        "{ \"prompt\": \"" + EscapeJSON(prompt) + "\", \"image_path\": \"" + EscapeJSON(referenceImagePath) + "\" }";

    std::string hlslCode = SendPostRequest("/generate", jsonReq);

    if (hlslCode.find("ERROR:") == 0 || hlslCode.find("Internal Server Error") != std::string::npos) {
        std::lock_guard<std::mutex> lock(mutex_);
        errorMessage_ = "Failed to connect to Python Server or API Error.\n" + hlslCode;
        state_ = State::Error;
        return;
    }

    // 修復ループ
    int attempts = 0;
    while (attempts <= kMaxFixAttempts) {
        state_ = State::Compiling;

        // 取得したHLSLを一時ファイルに保存（コンパイル用）
        std::error_code ec;
        std::filesystem::create_directories(outputDirectory, ec); // ディレクトリが無ければ作成

        std::string filename = outputDirectory + shaderName + ".hlsl";
        std::wstring tempHlslPath = ConvertString(filename);

        {
            std::ofstream hlslFile(filename);
            hlslFile << hlslCode;
        }

        std::string errorLog;
        ShaderCompileOptions options;
        options.entryPoint = L"main";

        // コンパイル実行（キャッシュ破棄＋再コンパイル）
        auto blob = shaderManager->ReloadShader(tempHlslPath, options, L"ps_6_0", &errorLog);

        if (blob) {
            // コンパイル成功
            std::lock_guard<std::mutex> lock(mutex_);
            resultBlob_ = blob;
            state_ = State::Success;

            // 成功履歴に保存
            GenerationHistory h;
            h.prompt = prompt;
            h.hlslCode = hlslCode;
            h.shaderName = shaderName;
            history_.push_back(h);
            return;
        }

        // コンパイル失敗 -> 修復処理へ
        attempts++;
        if (attempts > kMaxFixAttempts) {
            std::lock_guard<std::mutex> lock(mutex_);
            errorMessage_ = "Max fix attempts reached.\nLast Error:\n" + errorLog;
            state_ = State::Error;
            return;
        }

        state_ = State::Fixing;

        // エラー修復リクエストの作成
        std::string fixReq =
            "{ \"error_log\": \"" + EscapeJSON(errorLog) + "\", \"code\": \"" + EscapeJSON(hlslCode) + "\" }";
        hlslCode = SendPostRequest("/fix_error", fixReq);

        if (hlslCode.find("ERROR:") == 0 || hlslCode.find("Internal Server Error") != std::string::npos) {
            std::lock_guard<std::mutex> lock(mutex_);
            errorMessage_ = "Failed to call /fix_error API.\n" + hlslCode;
            state_ = State::Error;
            return;
        }
    }
}

void MagicBrushClient::StartVisualFix(const std::string& referenceImagePath, const std::string& screenshotPath,
                                      const std::string& currentHlslCode, const std::string& shaderName,
                                      ShaderManager* shaderManager) {
    if (state_ == State::VisualEvaluating || state_ == State::Compiling || state_ == State::Generating ||
        state_ == State::Fixing) {
        return;
    }
    state_ = State::VisualEvaluating;

    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    workerThread_ = std::thread(&MagicBrushClient::VisualFixThread, this, referenceImagePath, screenshotPath,
                                currentHlslCode, shaderName, shaderManager);
}

void MagicBrushClient::VisualFixThread(std::string referenceImagePath, std::string screenshotPath,
                                       std::string currentHlslCode, std::string shaderName,
                                       ShaderManager* shaderManager) {
    std::string jsonReq = "{ \"reference_image_path\": \"" + EscapeJSON(referenceImagePath) + "\", " +
                          "\"current_output_image_path\": \"" + EscapeJSON(screenshotPath) + "\", " + "\"code\": \"" +
                          EscapeJSON(currentHlslCode) + "\" }";

    std::string newHlslCode = SendPostRequest("/evaluate_visual", jsonReq);

    if (newHlslCode.find("ERROR:") == 0 || newHlslCode.find("Internal Server Error") != std::string::npos) {
        std::lock_guard<std::mutex> lock(mutex_);
        errorMessage_ = "Failed to connect to Python Server or API Error during Visual Fix.\n" + newHlslCode;
        state_ = State::Error;
        return;
    }

    // 修復後コンパイルループ
    int attempts = 0;
    while (attempts <= kMaxFixAttempts) {
        state_ = State::Compiling;

        std::error_code ec;
        std::filesystem::create_directories("resources/shaders/generated", ec);
        std::string hlslFilePath = "resources/shaders/generated/" + shaderName + ".hlsl";
        std::ofstream ofs(hlslFilePath);
        if (ofs) {
            ofs << newHlslCode;
            ofs.close();
        }

        ShaderCompileOptions options;
        options.entryPoint = L"main";
        std::string compileError;
        std::wstring hlslFilePathW = ConvertString(hlslFilePath);
        auto blob = shaderManager->ReloadShader(hlslFilePathW, options, L"ps_6_0", &compileError);

        if (blob) {
            std::lock_guard<std::mutex> lock(mutex_);
            resultBlob_ = blob;
            GenerationHistory hist;
            hist.prompt = "[Visual Fixed] Target: " + referenceImagePath;
            hist.hlslCode = newHlslCode;
            hist.shaderName = shaderName;
            history_.push_back(hist);
            state_ = State::Success;
            return;
        } else {
            state_ = State::Fixing;
            std::string fixReq = "{ \"error_log\": \"" + EscapeJSON(compileError) + "\", \"code\": \"" +
                                 EscapeJSON(newHlslCode) + "\" }";
            newHlslCode = SendPostRequest("/fix_error", fixReq);

            if (newHlslCode.find("ERROR:") == 0 || newHlslCode.find("Internal Server Error") != std::string::npos) {
                std::lock_guard<std::mutex> lock(mutex_);
                errorMessage_ = "Failed during auto-fix.\n" + newHlslCode;
                state_ = State::Error;
                return;
            }
            attempts++;
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    errorMessage_ =
        "Visual fix failed. Could not resolve compile errors after " + std::to_string(kMaxFixAttempts) + " attempts.";
    state_ = State::Error;
}
