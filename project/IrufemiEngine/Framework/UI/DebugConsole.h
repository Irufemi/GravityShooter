#pragma once

#include <string>
#include <vector>
#include <deque>

namespace Irufemi {

/**
 * @class DebugConsole
 * @brief ゲーム画面内で F1 キーを押すことで表示・操作できるインゲーム開発者コンソール
 * @details ImGui を用いて半透明コンソールを描画し、コマンド入力、履歴補完、一覧表示を提供します。
 */
class DebugConsole {
public:
    DebugConsole();
    ~DebugConsole() = default;

    /**
     * @brief 毎フレームの描画・更新処理
     * @param[in] isF1Triggered F1キーが押されたかどうかのフラグ
     */
    void Draw(bool isF1Triggered);

    /**
     * @brief コンソールの開閉をトグルする
     */
    void ToggleOpen() {
        isOpen_ = !isOpen_;
    }

    /**
     * @brief コンソールが開いているか判定する
     */
    bool IsOpen() const {
        return isOpen_;
    }

    /**
     * @brief ログメッセージをコンソール履歴に追加する
     */
    void AddLog(const std::string& message);

    /**
     * @brief 履歴ログをクリアする
     */
    void ClearLog();

private:
    bool isOpen_ = false;
    char inputBuffer_[256] = "";
    std::deque<std::string> logHistory_;
    std::vector<std::string> commandHistory_;
    int historyPos_ = -1;
    bool scrollToBottom_ = false;
    bool reclaimFocus_ = false;

    static constexpr size_t kMaxLogCount = 100;

    void Execute(const std::string& commandLine);
};

} // namespace Irufemi
