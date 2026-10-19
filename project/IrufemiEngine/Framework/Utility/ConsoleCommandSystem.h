#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <mutex>

namespace Irufemi {

/**
 * @struct CommandInfo
 * @brief 登録されたコンソールコマンドの情報
 */
struct CommandInfo {
    std::string name;
    std::string description;
};

/**
 * @class ConsoleCommandSystem
 * @brief ゲーム内コンソール、外部ツール（TelemetryMonitor）、CLI引数等から文字列コマンドを安全に実行するためのシステム
 * @details スレッドセーフにコマンドの登録・解除・実行を管理します。
 */
class ConsoleCommandSystem {
public:
    using CommandCallback = std::function<void(const std::vector<std::string>& args)>;

    /**
     * @brief 新しいコンソールコマンドを登録する
     * @param[in] name コマンド名（スペースを含まない一意な文字列）
     * @param[in] description コマンドの説明文・ヘルプ
     * @param[in] callback コマンド実行時に呼び出されるコールバック関数
     */
    static void RegisterCommand(const std::string& name, const std::string& description, CommandCallback callback);

    /**
     * @brief 登録済みのコマンドを解除する
     * @param[in] name 解除するコマンド名
     */
    static void UnregisterCommand(const std::string& name);

    /**
     * @brief コマンドライン文字列をパースして実行する
     * @param[in] commandLine スペース区切りのコマンド文字列（例: "spawn_debris 10000"）
     * @param[out] outResult 実行結果メッセージ（成功・エラーログ等）の格納先（省略可能）
     * @return 実行に成功した場合は true、未登録コマンド等の場合は false
     */
    static bool ExecuteCommand(const std::string& commandLine, std::string* outResult = nullptr);

    /**
     * @brief 指定したコマンドが登録されているか判定する
     * @param[in] name コマンド名
     * @return 登録されていれば true
     */
    static bool HasCommand(const std::string& name);

    /**
     * @brief 登録されている全コマンドの情報を取得する（UI補完・ヘルプ用）
     * @return コマンド情報のリスト
     */
    static std::vector<CommandInfo> GetAllCommands();

    /**
     * @brief 登録されている全コマンドをクリアする
     */
    static void ClearAllCommands();

private:
    struct CommandEntry {
        std::string description;
        CommandCallback callback;
    };

    static std::unordered_map<std::string, CommandEntry> registry_;
    static std::mutex mutex_;

    static std::vector<std::string> Tokenize(const std::string& commandLine);
};

} // namespace Irufemi
