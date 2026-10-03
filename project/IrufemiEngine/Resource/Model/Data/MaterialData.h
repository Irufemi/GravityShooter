#pragma once

#include <string>

/*objファイルを読んでみよう*/


namespace Irufemi {

/**
 * @class MaterialData
 * @brief 3Dモデルのマテリアル（質感）設定を保持する構造体
 * @details アルベド、メタリック、ラフネス等のパラメータや、使用するテクスチャパスを管理します。
 */
struct MaterialData {
    /** @brief 使用するテクスチャのファイルパス */
    std::string textureFilePath;
};

} // namespace Irufemi

using Irufemi::MaterialData;