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
/**
 * @brief UTF-16 文字列（std::wstring）を走査し、サロゲートペアを復号して各コードポイント（char32_t）に関数を適用します
 * @tparam Func 呼び出し可能オブジェクト void(char32_t)
 * @param[in] str 走査する UTF-16 ワイド文字列
 * @param[in] func コードポイントごとに実行するコールバック関数
 */
template <typename Func>
void ForEachCodePoint(const std::wstring& str, Func&& func) {
    for (size_t i = 0; i < str.size(); ++i) {
        wchar_t c1 = str[i];
        char32_t char32 = 0;
        if (c1 >= 0xD800 && c1 <= 0xDBFF && (i + 1 < str.size())) {
            wchar_t c2 = str[i + 1];
            if (c2 >= 0xDC00 && c2 <= 0xDFFF) {
                char32 = 0x10000 + ((static_cast<char32_t>(c1) - 0xD800) << 10) + (static_cast<char32_t>(c2) - 0xDC00);
                ++i;
            } else {
                char32 = static_cast<char32_t>(c1);
            }
        } else {
            char32 = static_cast<char32_t>(c1);
        }
        func(char32);
    }
}
} // namespace StringUtility

//};
