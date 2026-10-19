#include "Framework/Utility/ConsoleCommandSystem.h"
#include "Core/Utility/Log.h"
#include <iostream>
#include <sstream>

namespace Irufemi {

std::unordered_map<std::string, ConsoleCommandSystem::CommandEntry> ConsoleCommandSystem::registry_;
std::mutex ConsoleCommandSystem::mutex_;

void ConsoleCommandSystem::RegisterCommand(const std::string& name, const std::string& description,
                                           CommandCallback callback) {
    if (name.empty() || !callback) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    registry_[name] = CommandEntry{description, std::move(callback)};
}

void ConsoleCommandSystem::UnregisterCommand(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    registry_.erase(name);
}

bool ConsoleCommandSystem::HasCommand(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    return registry_.find(name) != registry_.end();
}

std::vector<CommandInfo> ConsoleCommandSystem::GetAllCommands() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CommandInfo> result;
    result.reserve(registry_.size());
    for (const auto& [name, entry] : registry_) {
        result.push_back({name, entry.description});
    }
    return result;
}

void ConsoleCommandSystem::ClearAllCommands() {
    std::lock_guard<std::mutex> lock(mutex_);
    registry_.clear();
}

std::vector<std::string> ConsoleCommandSystem::Tokenize(const std::string& commandLine) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < commandLine.length(); ++i) {
        char c = commandLine[i];
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if ((c == ' ' || c == '\t') && !inQuotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

bool ConsoleCommandSystem::ExecuteCommand(const std::string& commandLine) {
    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) {
        return false;
    }

    std::string commandName = tokens[0];
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());

    CommandCallback callbackToExecute = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registry_.find(commandName);
        if (it != registry_.end()) {
            callbackToExecute = it->second.callback;
        }
    }

    if (!callbackToExecute) {
        std::string warnMsg = "[ConsoleCommand] Unknown command: '" + commandName + "'\n";
        Log::OutPutLog(std::cout, warnMsg);
        return false;
    }

    try {
        callbackToExecute(args);
        return true;
    } catch (const std::exception& e) {
        std::string errMsg = "[ConsoleCommand] Exception in command '" + commandName + "': " + e.what() + "\n";
        Log::OutPutLog(std::cerr, errMsg);
        return false;
    } catch (...) {
        std::string errMsg = "[ConsoleCommand] Unknown exception in command '" + commandName + "'\n";
        Log::OutPutLog(std::cerr, errMsg);
        return false;
    }
}

} // namespace Irufemi
