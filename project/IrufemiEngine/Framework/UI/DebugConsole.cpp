#include "Framework/UI/DebugConsole.h"
#include "Framework/Utility/ConsoleCommandSystem.h"
#include "Core/Utility/Log.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#endif

#include <iostream>
#include <cstring>
#include <algorithm>

namespace Irufemi {

DebugConsole::DebugConsole() {
    AddLog("=== Irufemi Developer Console ===");
    AddLog("Type 'help' to view all registered commands.");
}

void DebugConsole::AddLog(const std::string& message) {
    logHistory_.push_back(message);
    if (logHistory_.size() > kMaxLogCount) {
        logHistory_.pop_front();
    }
    scrollToBottom_ = true;
}

void DebugConsole::ClearLog() {
    logHistory_.clear();
}

void DebugConsole::Execute(const std::string& commandLine) {
    if (commandLine.empty()) {
        return;
    }

    AddLog("> " + commandLine);

    commandHistory_.push_back(commandLine);
    historyPos_ = -1;

    if (commandLine == "clear" || commandLine == "cls") {
        ClearLog();
        return;
    }

    if (commandLine == "help") {
        AddLog("--- Available Commands ---");
        auto commands = ConsoleCommandSystem::GetAllCommands();
        for (const auto& cmd : commands) {
            AddLog("  " + cmd.name + " : " + cmd.description);
        }
        AddLog("--------------------------");
        return;
    }

    bool success = ConsoleCommandSystem::ExecuteCommand(commandLine);
    if (!success) {
        AddLog("[Error] Failed to execute: '" + commandLine + "'");
    }
}

void DebugConsole::Draw([[maybe_unused]] bool isF1Triggered) {
#ifdef USE_IMGUI
    if (isF1Triggered) {
        isOpen_ = !isOpen_;
        if (isOpen_) {
            reclaimFocus_ = true;
        }
    }

    if (!isOpen_) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    float consoleWidth = io.DisplaySize.x * 0.7f;
    float consoleHeight = io.DisplaySize.y * 0.45f;

    ImGui::SetNextWindowSize(ImVec2(consoleWidth, consoleHeight), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - consoleWidth) * 0.5f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.88f);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Developer Console (F1 to Close)", &isOpen_, windowFlags)) {
        ImGui::End();
        return;
    }

    // クイックコマンドボタンバー
    if (ImGui::Button("💥 10,000 Debris")) {
        Execute("spawn_debris 10000");
    }
    ImGui::SameLine();
    if (ImGui::Button("📦 1,000 Debris")) {
        Execute("spawn_debris 1000");
    }
    ImGui::SameLine();
    if (ImGui::Button("🧹 Clear Debris")) {
        Execute("clear_debris");
    }
    ImGui::SameLine();
    if (ImGui::Button("❓ Help")) {
        Execute("help");
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Log")) {
        ClearLog();
    }

    ImGui::Separator();

    // ログスクロール領域
    const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& item : logHistory_) {
        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        if (item.rfind("> ", 0) == 0) {
            color = ImVec4(0.0f, 0.9f, 1.0f, 1.0f); // シアン (コマンド入力)
        } else if (item.rfind("[Error]", 0) == 0) {
            color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // 赤 (エラー)
        } else if (item.rfind("[Command]", 0) == 0) {
            color = ImVec4(0.0f, 1.0f, 0.6f, 1.0f); // 緑 (実行ログ)
        }
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(item.c_str());
        ImGui::PopStyleColor();
    }

    if (scrollToBottom_) {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottom_ = false;
    }
    ImGui::EndChild();

    ImGui::Separator();

    // コマンド入力欄
    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;
    auto historyCallback = [](ImGuiInputTextCallbackData* data) -> int {
        auto* console = static_cast<DebugConsole*>(data->UserData);
        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            const int prevHistoryPos = console->historyPos_;
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (console->historyPos_ == -1) {
                    console->historyPos_ = static_cast<int>(console->commandHistory_.size()) - 1;
                } else if (console->historyPos_ > 0) {
                    console->historyPos_--;
                }
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (console->historyPos_ != -1) {
                    if (++console->historyPos_ >= static_cast<int>(console->commandHistory_.size())) {
                        console->historyPos_ = -1;
                    }
                }
            }

            if (prevHistoryPos != console->historyPos_) {
                const char* historyStr =
                    (console->historyPos_ >= 0) ? console->commandHistory_[console->historyPos_].c_str() : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, historyStr);
            }
        }
        return 0;
    };

    ImGui::PushItemWidth(-100);
    if (ImGui::InputText("##ConsoleInput", inputBuffer_, IM_ARRAYSIZE(inputBuffer_), inputFlags, historyCallback,
                         this)) {
        std::string command(inputBuffer_);
        Execute(command);
        inputBuffer_[0] = '\0';
        reclaimFocus_ = true;
    }
    ImGui::PopItemWidth();

    ImGui::SetItemDefaultFocus();
    if (reclaimFocus_) {
        ImGui::SetKeyboardFocusHere(-1);
        reclaimFocus_ = false;
    }

    ImGui::SameLine();
    if (ImGui::Button("Execute", ImVec2(90, 0))) {
        std::string command(inputBuffer_);
        Execute(command);
        inputBuffer_[0] = '\0';
        reclaimFocus_ = true;
    }

    ImGui::End();
#endif // USE_IMGUI
}

} // namespace Irufemi
