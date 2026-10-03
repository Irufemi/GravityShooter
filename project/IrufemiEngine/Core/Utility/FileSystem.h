#pragma once

#include <string>

/**
 * @brief パス解決を行うクラス
 * @details 実行環境（開発環境・配布環境）の違いを吸収し、常に安全な絶対パスを提供します。
 */
class FileSystem {
public:
    /**
     * @brief システムを初期化し、ルートディレクトリを特定します
     */
    static void Initialize();

    /**
     * @brief プロジェクトのルートディレクトリを取得します
     */
    static std::string GetProjectRoot();

    /**
     * @brief エンジンのルートディレクトリ（IrufemiEngine）を取得します
     */
    static std::string GetEngineRoot();

    /**
     * @brief リソースの絶対パスを取得します
     * @param relativePath "resources/..." から始まる相対パス
     */
    static std::string GetResourcePath(const std::string& relativePath);

    /**
     * @brief キャッシュ・派生データ（generated/cache/）の保存先パスを取得します
     * @param relativePath "generated/cache/" 配下の相対パス（例: "shaders"）
     * @return 正規化されたキャッシュディレクトリパス
     */
    static std::string GetCachePath(const std::string& relativePath = "");

    /**
     * @brief ログ保存先の絶対パスを取得します
     */
    static std::string GetLogPath();

    /**
     * @brief ダンプ保存先の絶対パスを取得します
     */
    static std::string GetDumpPath();

    /**
     * @brief 実行ファイルの絶対パスを取得します
     */
    static std::string GetExePath();

private:
    static std::string projectRoot_;
    static std::string engineRoot_;
    static std::string exePath_;
};
