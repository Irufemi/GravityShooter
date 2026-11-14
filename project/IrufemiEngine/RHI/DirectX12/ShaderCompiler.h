#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <utility>

namespace Irufemi {

/**
 * @struct ShaderCompileOptions
 * @brief シェーダーコンパイル時の詳細設定を保持する構造体
 */
struct ShaderCompileOptions {
    std::wstring entryPoint = L"main";                         ///< エントリポイント名
    std::vector<std::pair<std::wstring, std::wstring>> macros; ///< マクロ定義 (Name, Value)
    bool isDebug = false;                                      ///< デバッグ情報を埋め込むか
};

/**
 * @class ShaderCompiler
 * @brief DXC (DirectX Shader Compiler) を使用してシェーダをコンパイルするクラス
 */
class ShaderCompiler {
public:
    /**
     * @brief 初期化
     */
    void Initialize();

    /**
     * @brief シェーダのコンパイルを実行する
     * @param[in] filePath HLSLファイルへのパス
     * @param[in] profile コンパイルプロファイル (vs_6_0, ps_6_0, cs_6_0 等)
     * @param[in] options コンパイルオプション（エントリポイント、マクロ、デバッグフラグ等）
     * @return コンパイルされたシェーダのBlob。失敗時はnullptrを返す。
     */
    Microsoft::WRL::ComPtr<IDxcBlob> Compile(const std::wstring& filePath, const wchar_t* profile,
                                             const ShaderCompileOptions& options = {},
                                             const std::vector<std::wstring>& includeDirs = {},
                                             std::string* outErrorLog = nullptr);

    /**
     * @brief ファイル名からプロファイルを推論する
     * @param[in] filePath HLSLファイルへのパス
     * @return 推論されたプロファイル文字列
     */
    static std::wstring GetInferredProfile(const std::wstring& filePath);

private:
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};

} // namespace Irufemi
