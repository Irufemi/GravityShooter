#include "Resource/Texture/TextureUtility.h"
#include <algorithm>
#include <filesystem>

/**
 * @namespace TextureUtility
 * @brief テクスチャ関連のユーティリティ関数群の実装
 */
namespace Irufemi::TextureUtility {

std::wstring GetExtension(const std::wstring& filePath) {
    std::filesystem::path path(filePath);
    std::wstring ext = path.extension().wstring();
    // 小文字に変換
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    return ext;
}

TextureFileType GetTextureFileType(const std::wstring& filePath) {
    std::wstring ext = GetExtension(filePath);

    if (ext == L".dds") {
        return TextureFileType::DDS;
    }

    if (ext == L".tga") {
        return TextureFileType::TGA;
    }

    // 一般的なWIC対応拡張子
    if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp" || ext == L".gif" || ext == L".tiff") {
        return TextureFileType::WIC;
    }

    return TextureFileType::Unknown;
}

} // namespace Irufemi::TextureUtility
