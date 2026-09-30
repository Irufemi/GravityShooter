#include "RHI/DirectX12/ShaderManager.h"
#include "Core/Utility/ErrorUtility.h"
#include "Core/Utility/Log.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>

#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
#define RUNTIME_SHADER_COMPILE 1
#endif

#ifndef RUNTIME_SHADER_COMPILE
// IDxcBlob を自作して dxcompiler.dll への依存を無くす
class CustomBlob : public IDxcBlob {
    std::vector<uint8_t> data_;
    ULONG refCount_ = 1;

public:
    CustomBlob(std::vector<uint8_t>&& data) : data_(std::move(data)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) {
            return E_POINTER;
        }
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDxcBlob)) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++refCount_;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = --refCount_;
        if (r == 0) {
            delete this;
        }
        return r;
    }
    LPVOID STDMETHODCALLTYPE GetBufferPointer() override {
        return data_.data();
    }
    SIZE_T STDMETHODCALLTYPE GetBufferSize() override {
        return data_.size();
    }
};
#endif

/**
 * @brief 初期化
 */
void ShaderManager::Initialize() {
#ifdef RUNTIME_SHADER_COMPILE
    compiler_ = std::make_unique<ShaderCompiler>();
    compiler_->Initialize();
#endif
}

/**
 * @brief シェーダーを取得またはコンパイルする
 */
Microsoft::WRL::ComPtr<IDxcBlob> ShaderManager::GetOrCompile(const std::wstring& filePath,
                                                             const ShaderCompileOptions& options,
                                                             const wchar_t* profileOverride, std::string* outErrorLog) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. キャッシュキーの構築
    ShaderKey key;
    key.filePath = filePath;
    key.entryPoint = options.entryPoint;
    key.macros = options.macros;

    // 2. キャッシュの検索
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }

    // 3. コンパイルまたはCSO読み込み
    Microsoft::WRL::ComPtr<IDxcBlob> blob;
#ifdef RUNTIME_SHADER_COMPILE
    std::wstring resolvedPath = ResolveSourcePath(filePath);
    std::wstring profile = profileOverride ? profileOverride : ShaderCompiler::GetInferredProfile(resolvedPath);
    blob = compiler_->Compile(resolvedPath, profile.c_str(), options, searchPaths_, outErrorLog);
#else
    // Releaseビルドでは binaryPath_ に格納された .cso を読み込む
    std::filesystem::path path(filePath);
    std::filesystem::path compiledPath = std::filesystem::path(binaryPath_) / path.filename();
    compiledPath.replace_extension(L".cso");
    std::ifstream file(compiledPath, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(size);
        if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            blob.Attach(new CustomBlob(std::move(buffer)));
        }
    } else {
        std::string err = "[ShaderManager] Failed to open compiled shader (.cso) file: " + compiledPath.string() + "\n";
        Log::OutPutLog(std::cerr, err);
        IRUFEMI_ASSERT_MSG(false,
                           "Failed to open compiled shader (.cso) file. Check if the shader compiled successfully.");
    }
#endif

    // 4. キャッシュに登録
    if (blob) {
        cache_[key] = blob;
    }

    return blob;
}

/**
 * @brief シェーダーを強制的に再コンパイル（または再読み込み）する
 */
Microsoft::WRL::ComPtr<IDxcBlob> ShaderManager::ReloadShader(const std::wstring& filePath,
                                                             const ShaderCompileOptions& options,
                                                             const wchar_t* profileOverride, std::string* outErrorLog) {
    ShaderKey key;
    key.filePath = filePath;
    key.entryPoint = options.entryPoint;
    key.macros = options.macros;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.erase(key);
    }

    return GetOrCompile(filePath, options, profileOverride, outErrorLog);
}

/**
 * @brief キャッシュをクリアする
 */
void ShaderManager::ClearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

/**
 * @brief シェーダーのソース検索パスを追加する（開発ビルド用）
 */
void ShaderManager::AddSearchPath(const std::wstring& path) {
    searchPaths_.push_back(path);
}

/**
 * @brief コンパイル済みバイナリ(.cso)の読み込みパスを設定する（リリースビルド用）
 */
void ShaderManager::SetBinaryPath(const std::wstring& path) {
    binaryPath_ = path;
}

/**
 * @brief 検索パスからソースファイルのフルパスを解決する
 */
std::wstring ShaderManager::ResolveSourcePath(const std::wstring& filename) const {
    for (const auto& path : searchPaths_) {
        std::filesystem::path fullPath = std::filesystem::path(path) / filename;
        if (std::filesystem::exists(fullPath)) {
            return fullPath.wstring();
        }
    }
    return filename; // 見つからない場合はそのまま返す
}
