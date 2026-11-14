#include "Core/Utility/ErrorUtility.h"
#include "RHI/DirectX12/ShaderCompiler.h"
#include "Core/Utility/Log.h"
#include "Core/Utility/FileSystem.h"
#include "Core/Utility/StringUtility.h"
#include <format>
#include <cassert>
#include <Windows.h>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <vector>

namespace Irufemi {

/**
 * @brief 初期化
 */
void ShaderCompiler::Initialize() {
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxcUtils_.GetAddressOf()));
    ASSERT_IF_FAILED(hr);
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(dxcCompiler_.GetAddressOf()));
    ASSERT_IF_FAILED(hr);
    hr = dxcUtils_->CreateDefaultIncludeHandler(includeHandler_.GetAddressOf());
    ASSERT_IF_FAILED(hr);
}

/**
 * @brief シェーダのコンパイル
 * @param[in] filePath HLSLファイルへのパス
 * @param[in] profile コンパイルプロファイル
 * @param[in] options コンパイルオプション
 * @return コンパイルされたシェーダのBlob
 */
Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::Compile(const std::wstring& filePath, const wchar_t* profile,
                                                         const ShaderCompileOptions& options,
                                                         const std::vector<std::wstring>& includeDirs,
                                                         std::string* outErrorLog) {
    // 1. HLSLファイルの読み込み
    // DirectX Shader Compiler doesn't always play nice with forward slashes
    std::wstring osPath = filePath;
    std::replace(osPath.begin(), osPath.end(), L'/', L'\\');
    HRESULT hr;

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource;
    std::ifstream shaderFile(osPath, std::ios::binary | std::ios::ate);
    if (!shaderFile.is_open()) {
        std::string errStr = "Failed to load shader file: " + ConvertString(filePath);
        std::wstring logPathW = ConvertString(FileSystem::GetLogPath() + "/CRASH.log");
        std::ofstream crashLog(logPathW);
        crashLog << errStr << std::endl;
        crashLog.close();
        IRUFEMI_ASSERT_MSG(false, errStr.c_str());
        return nullptr;
    }
    std::streamsize size = shaderFile.tellg();
    shaderFile.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (shaderFile.read(buffer.data(), size)) {
        hr = dxcUtils_->CreateBlob(buffer.data(), static_cast<UINT32>(size), DXC_CP_UTF8, &shaderSource);
        if (FAILED(hr)) {
            IRUFEMI_ASSERT_MSG(false, "Failed to create blob from shader file data.");
            return nullptr;
        }
    } else {
        IRUFEMI_ASSERT_MSG(false, "Failed to read shader file.");
        return nullptr;
    }

    DxcBuffer shaderSourceBuffer;
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8;

    // 2. コンパイル引数の構築
    std::vector<LPCWSTR> arguments;
    arguments.push_back(filePath.c_str());
    arguments.push_back(L"-E");
    arguments.push_back(options.entryPoint.c_str());
    arguments.push_back(L"-T");
    arguments.push_back(profile);
    arguments.push_back(L"-Zpr"); // 行優先(Row Major)

    // デバッグ設定
    if (options.isDebug) {
        arguments.push_back(L"-Zi");           // デバッグ情報の生成
        arguments.push_back(L"-Qembed_debug"); // PDBをBlobに埋め込む
        arguments.push_back(L"-Od");           // 最適化オフ
    } else {
        arguments.push_back(L"-O3"); // 最大最適化
    }

    // マクロ定義の追加
    std::vector<std::wstring> macroStorage;
    macroStorage.reserve(options.macros.size());
    for (const auto& macro : options.macros) {
        arguments.push_back(L"-D");
        if (macro.second.empty()) {
            macroStorage.push_back(macro.first);
        } else {
            macroStorage.push_back(macro.first + L"=" + macro.second);
        }
        arguments.push_back(macroStorage.back().c_str());
    }

    // インクルードディレクトリの追加
    for (const auto& dir : includeDirs) {
        arguments.push_back(L"-I");
        arguments.push_back(dir.c_str());
    }

    // 3. コンパイル実行
    Microsoft::WRL::ComPtr<IDxcResult> shaderResult;
    hr = dxcCompiler_->Compile(&shaderSourceBuffer, arguments.data(), static_cast<UINT32>(arguments.size()),
                               includeHandler_.Get(), IID_PPV_ARGS(&shaderResult));
    ASSERT_IF_FAILED(hr);

    // 4. エラー・警告の確認
    HRESULT hrStatus = S_OK;
    shaderResult->GetStatus(&hrStatus);

    Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        std::string errStr = shaderError->GetStringPointer();
        std::string fileStr = ConvertString(filePath);

        if (FAILED(hrStatus)) {
            // コンパイルエラーの場合
            std::string fullErr = "Shader Compile Error in " + fileStr + ":\n" + errStr;
            Log::OutPutLog(std::cerr, fullErr);

            // ログファイルにも出力
            FILE* f = nullptr;
            fopen_s(&f, "shader_error.txt", "w");
            if (f) {
                fprintf(f, "%ws: %s\n", filePath.c_str(), errStr.c_str());
                fclose(f);
            }

            if (outErrorLog) {
                *outErrorLog = errStr;
                return nullptr;
            } else {
                IRUFEMI_ASSERT(false && "Shader Compile Error");
            }
        } else {
            // コンパイル警告（Warning）の場合：ログ出力のみ行い、処理は継続する
            std::string fullWarn = "Shader Compile Warning in " + fileStr + ":\n" + errStr;
            Log::OutPutLog(std::cout, fullWarn);
        }
    }

    // 5. コンパイル済みバイナリの取得
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    ASSERT_IF_FAILED(hr);

    return shaderBlob;
}

/**
 * @brief ファイル名からプロファイルを推論する
 */
std::wstring ShaderCompiler::GetInferredProfile(const std::wstring& filePath) {
    if (filePath.find(L".VS.") != std::wstring::npos) {
        return L"vs_6_0";
    }
    if (filePath.find(L".PS.") != std::wstring::npos) {
        return L"ps_6_0";
    }
    if (filePath.find(L".GS.") != std::wstring::npos) {
        return L"gs_6_0";
    }
    if (filePath.find(L".CS.") != std::wstring::npos) {
        return L"cs_6_0";
    }
    if (filePath.find(L".DS.") != std::wstring::npos) {
        return L"ds_6_0";
    }
    if (filePath.find(L".HS.") != std::wstring::npos) {
        return L"hs_6_0";
    }

    // デフォルト
    return L"ps_6_0";
}

} // namespace Irufemi
