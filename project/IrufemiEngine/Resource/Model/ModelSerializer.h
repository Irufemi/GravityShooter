#pragma once
#include <string>
#include <cstdint>
#include "Resource/Model/Data/ObjModel.h"

namespace Irufemi {

class ModelSerializer {
public:
    static constexpr uint32_t kMagicNumber = 0x4C444D49; // "IMDL"
    static constexpr uint32_t kVersion = 2;

    struct Header {
        uint32_t magic;
        uint32_t version;
        uint64_t sourceLastWriteTime;
    };

    /**
     * @brief ObjModelをバイナリファイルに書き出す
     * @param filepath 出力先のバイナリファイルパス
     * @param model 書き出すモデルデータ
     * @param sourceLastWriteTime 元ファイル(.obj/.gltf)の最終更新日時
     * @return 成功したか
     */
    static bool Serialize(const std::string& filepath, const ObjModel& model, uint64_t sourceLastWriteTime);

    /**
     * @brief バイナリファイルからObjModelを読み込む
     * @param filepath 読み込むバイナリファイルパス
     * @param outModel 読み込んだデータの格納先
     * @param outSourceLastWriteTime ヘッダーに記録されている元ファイルの最終更新日時の格納先
     * @return 成功したか
     */
    static bool Deserialize(const std::string& filepath, ObjModel& outModel, uint64_t& outSourceLastWriteTime);

    /**
     * @brief バイナリファイルのヘッダーのみを読み込む（バージョンと更新日時の確認用）
     * @param filepath 読み込むバイナリファイルパス
     * @param outHeader 読み込んだヘッダーの格納先
     * @return 成功したか
     */
    static bool ReadHeader(const std::string& filepath, Header& outHeader);
};

} // namespace Irufemi
