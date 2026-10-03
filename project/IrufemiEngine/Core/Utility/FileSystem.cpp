#include "Core/Utility/FileSystem.h"
#include <Windows.h>
#include <filesystem>
#include <algorithm>
#include "Core/Utility/StringUtility.h"

namespace fs = std::filesystem;

std::string FileSystem::projectRoot_ = "";
std::string FileSystem::engineRoot_ = "";
std::string FileSystem::exePath_ = "";

void FileSystem::Initialize() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    fs::path exeFsPath = path;
    exePath_ = ConvertString(exeFsPath.wstring());
    fs::path exeDir = exeFsPath.parent_path();

    // We will just use relative paths so that std::ifstream works properly without needing
    // UTF-8 to UTF-16 conversions, bypassing the Japanese path issue.
    projectRoot_ = ".";
    if (fs::exists("../IrufemiEngine/EngineResources")) {
        engineRoot_ = "../IrufemiEngine";
    } else {
        engineRoot_ = ".";
    }

    // Convert backslashes to forward slashes for consistency
    std::replace(projectRoot_.begin(), projectRoot_.end(), '\\', '/');
    std::replace(engineRoot_.begin(), engineRoot_.end(), '\\', '/');
}

std::string FileSystem::GetProjectRoot() {
    return projectRoot_;
}

std::string FileSystem::GetEngineRoot() {
    return engineRoot_;
}

std::string FileSystem::GetResourcePath(const std::string& relativePath) {
    std::string path = relativePath;
    // Remove leading "./" or "/" if present
    if (path.find("./") == 0) {
        path = path.substr(2);
    }
    if (path.find("/") == 0) {
        path = path.substr(1);
    }

    if (path.find("resources") != 0) {
        path = "resources/" + path;
    }

    std::string fullPath = projectRoot_ + "/" + path;
    std::replace(fullPath.begin(), fullPath.end(), '\\', '/');
    return fullPath;
}

std::string FileSystem::GetCachePath(const std::string& relativePath) {
    std::string path = "generated/cache";
    if (!relativePath.empty()) {
        std::string sub = relativePath;
        // 先頭のスラッシュを除去
        if (sub.find("./") == 0) {
            sub = sub.substr(2);
        }
        if (sub.find("/") == 0 || sub.find("\\") == 0) {
            sub = sub.substr(1);
        }
        path += "/" + sub;
    }

    std::string fullPath = projectRoot_ + "/" + path;
    std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

    // キャッシュ保存先ディレクトリが存在しない場合は自動作成して整合性を担保
    std::error_code ec;
    fs::path fsPath(fullPath);
    if (fsPath.has_extension()) {
        fs::create_directories(fsPath.parent_path(), ec);
    } else {
        fs::create_directories(fsPath, ec);
    }

    return fullPath;
}

std::string FileSystem::GetLogPath() {
    std::string path = projectRoot_ + "/Logs";
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

std::string FileSystem::GetDumpPath() {
    std::string path = projectRoot_ + "/Dumps";
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

std::string FileSystem::GetExePath() {
    return exePath_;
}
