#pragma once
#include <string>

// namespace StringUtility
//{

/*ログを出そう*/

/**
 * @brief ConvertString を実行する。
 */
std::wstring ConvertString(const std::string& str);

/**
 * @brief ConvertString を実行する。
 */
std::string ConvertString(const std::wstring& str);

/**
 * @namespace StringUtility
 * @brief 文字列変換（UTF-8 / WideChar 相互変換、サフィックス判定、キャッシュパス生成等）を提供する名前空間
 */
namespace StringUtility {
/**
 * @brief EndsWith を実行する。
 */
bool EndsWith(const std::wstring& str, const std::wstring& suffix);
/**
 * @brief CacheFilePath を取得する。
 * @return 取得された CacheFilePath
 */
std::string GetCacheFilePath(const std::string& fullPath, const std::string& cacheCategory,
                             const std::string& extension);
} // namespace StringUtility

//};
