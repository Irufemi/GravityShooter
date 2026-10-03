#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <fstream>
#include <filesystem>
#include <iostream>
#include "Core/Utility/Log.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"

namespace Irufemi {
/**
 * @namespace Irufemi::JsonUtility
 * @brief nlohmann/json を用いた数学型（Vector/Matrix等）のシリアライズ・ファイル保存・読込支援名前空間
 */
namespace JsonUtility {

/**
 * @brief Vector2 を JSON に変換する
 */
inline nlohmann::json ToJson(const Irufemi::Vector2& v) {
    return nlohmann::json::array({v.x, v.y});
}

/**
 * @brief JSON から Vector2 に変換する
 */
inline Irufemi::Vector2 ToVector2(const nlohmann::json& j,
                                  const Irufemi::Vector2& defaultValue = Irufemi::Vector2(0.0f, 0.0f)) {
    if (j.is_array() && j.size() >= 2) {
        return Irufemi::Vector2(j[0].get<float>(), j[1].get<float>());
    }
    return defaultValue;
}

/**
 * @brief Vector3 を JSON に変換する
 */
inline nlohmann::json ToJson(const Irufemi::Vector3& v) {
    return nlohmann::json::array({v.x, v.y, v.z});
}

/**
 * @brief JSON から Vector3 に変換する
 */
inline Irufemi::Vector3 ToVector3(const nlohmann::json& j,
                                  const Irufemi::Vector3& defaultValue = Irufemi::Vector3(0.0f, 0.0f, 0.0f)) {
    if (j.is_array() && j.size() >= 3) {
        return Irufemi::Vector3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    }
    return defaultValue;
}

/**
 * @brief Vector4 を JSON に変換する
 */
inline nlohmann::json ToJson(const Irufemi::Vector4& v) {
    return nlohmann::json::array({v.x, v.y, v.z, v.w});
}

/**
 * @brief JSON から Vector4 に変換する
 */
inline Irufemi::Vector4 ToVector4(const nlohmann::json& j,
                                  const Irufemi::Vector4& defaultValue = Irufemi::Vector4(0.0f, 0.0f, 0.0f, 0.0f)) {
    if (j.is_array() && j.size() >= 4) {
        return Irufemi::Vector4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
    }
    return defaultValue;
}

/**
 * @brief ファイルからJSONを安全に読み込む
 * @param filepath 読み込み対象のファイルパス
 * @param[out] outJson 読み込んだJSONを格納するオブジェクト
 * @return 読み込みおよびパースに成功した場合は true
 */
inline bool LoadFromFile(const std::string& filepath, nlohmann::json& outJson) {
    try {
        std::filesystem::path path(filepath);
        if (!std::filesystem::exists(path)) {
            return false;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        file >> outJson;
        file.close();
        return true;
    } catch (const std::exception& e) {
        Log::OutPutLog(std::cerr,
                       "[JsonUtility] LoadFromFile failed (" + filepath + "): " + std::string(e.what()) + "\n");
        return false;
    }
}

/**
 * @brief JSONをファイルに安全に保存する（必要に応じて親ディレクトリを自動生成）
 * @param filepath 保存先のファイルパス
 * @param json 保存するJSONオブジェクト
 * @param indent インデント幅（-1 で最小化、デフォルトは 2）
 * @return 保存に成功した場合は true
 */
inline bool SaveToFile(const std::string& filepath, const nlohmann::json& json, int indent = 2) {
    try {
        std::filesystem::path path(filepath);
        std::filesystem::path dir = path.parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }

        if (indent >= 0) {
            file << json.dump(indent);
        } else {
            file << json.dump();
        }
        file.close();
        return true;
    } catch (const std::exception& e) {
        Log::OutPutLog(std::cerr,
                       "[JsonUtility] SaveToFile failed (" + filepath + "): " + std::string(e.what()) + "\n");
        return false;
    }
}

} // namespace JsonUtility
} // namespace Irufemi
