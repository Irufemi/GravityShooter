#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>

#include "Core/Utility/Synchronized.h"

class GameObject;

/**
 * @class PrefabManager
 * @brief プレハブアセットのロード、キャッシュ管理、およびインスタンス生成を担当するマネージャークラス
 * @details SceneSerializer
 * からプレハブ管理の責務を分離し、シーン遷移時のメモリ解放やテンプレートのクローン生成を一元管理します。
 */
class PrefabManager {
public:
    PrefabManager() = default;
    ~PrefabManager();

    PrefabManager(const PrefabManager&) = delete;
    PrefabManager& operator=(const PrefabManager&) = delete;

    /**
     * @brief プレハブのJSONデータを取得する（キャッシュ対応）
     * @param filepath プレハブファイルのパス
     * @return 読み込まれたJSONデータ参照。失敗時は空オブジェクト
     */
    const nlohmann::json& GetPrefabJson(const std::string& filepath);

    /**
     * @brief プレハブのテンプレートGameObjectを取得する（キャッシュ対応）
     * @param filepath プレハブファイルのパス
     * @return テンプレートGameObjectの共有ポインタ。失敗時はnullptr
     */
    std::shared_ptr<GameObject> GetTemplate(const std::string& filepath);

    /**
     * @brief プレハブから新しいGameObjectインスタンスを生成（クローン）する
     * @param filepath プレハブファイルのパス
     * @return 生成されたGameObjectの共有ポインタ。失敗時はnullptr
     */
    std::shared_ptr<GameObject> Instantiate(const std::string& filepath);

    /**
     * @brief キャッシュされているすべてのプレハブデータ（JSONおよびテンプレート）を解放する
     */
    void ClearCache();

private:
    struct PrefabCacheData {
        std::unordered_map<std::string, std::shared_ptr<nlohmann::json>> jsonCache;
        std::unordered_map<std::string, std::shared_ptr<GameObject>> templateCache;
    };
    Irufemi::Synchronized<PrefabCacheData> cache_;
};
