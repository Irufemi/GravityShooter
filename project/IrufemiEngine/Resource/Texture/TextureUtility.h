#pragma once
#include <string>

/**
 * @namespace TextureUtility
 * @brief テクスチャ関連のユーティリティ関数群
 */
namespace Irufemi::TextureUtility {

/**
 * @enum TextureFileType
 * @brief テクスチャファイルの形式
 */
enum class TextureFileType {
    DDS, ///< DirectDraw Surface
    WIC, ///< Windows Imaging Component (PNG, JPG, etc.)
    TGA, ///< Targa Image
    Unknown
};

/**
 * @brief ファイルパスから拡張子を取得する（小文字）
 * @param[in] filePath ファイルパス
 * @return 拡張子（ドットを含む）
 */
std::wstring GetExtension(const std::wstring& filePath);

/**
 * @brief ファイルパスからテクスチャ形式を判定する
 * @param[in] filePath ファイルパス
 * @return テクスチャ形式
 */
TextureFileType GetTextureFileType(const std::wstring& filePath);

} // namespace Irufemi::TextureUtility
