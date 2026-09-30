#pragma once

#include "RHI/DirectX12/ShaderCompiler.h"
#include <unordered_map>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>

/**
 * @class ShaderManager
 * @brief シェーダーのコンパイル結果を管理・キャッシュするクラス
 */
class ShaderManager {
public:
    /**
     * @brief 初期化
     */
    void Initialize();

    /**
     * @brief シェーダーのソース検索パスを追加する（開発ビルド用）
     * @param[in] path 検索対象のディレクトリパス
     */
    void AddSearchPath(const std::wstring& path);

    /**
     * @brief コンパイル済みバイナリ(.cso)の読み込みパスを設定する（リリースビルド用）
     * @param[in] path コンパイル済みシェーダーが格納されるディレクトリ
     */
    void SetBinaryPath(const std::wstring& path);

    /**
     * @brief シェーダーを取得またはコンパイルする
     * @param[in] filePath HLSLファイルへのパス
     * @param[in] options コンパイルオプション
     * @param[in] profileOverride プロファイルを明示的に指定する場合（nullptrなら自動判定）
     * @return コンパイル済みシェーダーのBlob
     */
    Microsoft::WRL::ComPtr<IDxcBlob> GetOrCompile(const std::wstring& filePath,
                                                  const ShaderCompileOptions& options = {},
                                                  const wchar_t* profileOverride = nullptr,
                                                  std::string* outErrorLog = nullptr);

    /**
     * @brief シェーダーを強制的に再コンパイル（または再読み込み）する
     */
    Microsoft::WRL::ComPtr<IDxcBlob> ReloadShader(const std::wstring& filePath,
                                                  const ShaderCompileOptions& options = {},
                                                  const wchar_t* profileOverride = nullptr,
                                                  std::string* outErrorLog = nullptr);

    /**
     * @brief キャッシュをクリアする
     */
    void ClearCache();

private:
    /**
     * @struct ShaderKey
     * @brief キャッシュ用のキー構造体
     */
    struct ShaderKey {
        std::wstring filePath;
        std::wstring entryPoint;
        std::vector<std::pair<std::wstring, std::wstring>> macros;

        bool operator<(const ShaderKey& other) const {
            return std::tie(filePath, entryPoint, macros) < std::tie(other.filePath, other.entryPoint, other.macros);
        }
    };

    std::unique_ptr<ShaderCompiler> compiler_;
    std::map<ShaderKey, Microsoft::WRL::ComPtr<IDxcBlob>> cache_;
    std::mutex mutex_;

    std::vector<std::wstring> searchPaths_;
    std::wstring binaryPath_;

    /**
     * @brief 検索パスからソースファイルのフルパスを解決する
     */
    std::wstring ResolveSourcePath(const std::wstring& filename) const;
};
