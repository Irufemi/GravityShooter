#include "Core/Utility/StringUtility.h"
#include <Windows.h>
#include <algorithm>

namespace StringUtility {

bool EndsWith(const std::wstring& str, const std::wstring& suffix) {
    return str.ends_with(suffix);
}

std::string GetCacheFilePath(const std::string& fullPath, const std::string& cacheCategory,
                             const std::string& extension) {
    std::string pathStr = fullPath;
    std::replace(pathStr.begin(), pathStr.end(), '\\', '/');

    size_t pos = pathStr.find("resources/");
    if (pos != std::string::npos) {
        std::string relative = pathStr.substr(pos + 10);
        return "generated/cache/" + cacheCategory + "/" + relative + extension;
    }
    return fullPath + extension;
}
} // namespace StringUtility

std::wstring ConvertString(const std::string& str) {
    if (str.empty()) {
        return std::wstring();
    }

    auto sizeNeeded =
        MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
    if (sizeNeeded == 0) {
        return std::wstring();
    }
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0],
                        sizeNeeded);
    return result;
}

std::string ConvertString(const std::wstring& str) {
    if (str.empty()) {
        return std::string();
    }

    auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
    if (sizeNeeded == 0) {
        return std::string();
    }
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
    return result;
}