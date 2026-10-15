#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>

class IScene;
class GameObject;
class PrefabManager;

/**
 * @class SceneSerializer
 * @brief シーン内の GameObject 群を JSON 形式で保存・読み込みするクラス
 */
class SceneSerializer {
public:
    /**
     * @brief PrefabManager を設定する（エンジン初期化時にバインド）
     */
    static void SetPrefabManager(PrefabManager* prefabManager) {
        prefabManager_ = prefabManager;
    }

    /**
     * @brief シーンの状態をファイルに保存する
     * @param scene 保存対象のシーン
     * @param sceneName シーン名（ファイル名として使用）
     * @return 成功すれば true
     */
    static bool Save(IScene* scene, const std::string& sceneName);

    /**
     * @brief ファイルからシーンの状態を読み込む
     * @param scene 展開先のシーン
     * @param sceneName シーン名
     * @return 成功すれば true
     */
    static bool Load(IScene* scene, const std::string& sceneName);

    /**
     * @brief 指定したシーン名の JSON ファイルが存在するか確認する
     */
    static bool Exists(IScene* scene, const std::string& sceneName);

    /**
     * @brief 特定の GameObject をプレハブとして保存する
     * @param obj 保存対象の GameObject
     * @param filepath 保存先のパス (例: resources/prefabs/MyObject.json)
     */
    static bool SavePrefab(const std::shared_ptr<GameObject>& obj, const std::string& filepath);

    /**
     * @brief プレハブファイルから GameObject を復元・生成する
     * @param filepath 読み込むファイルのパス
     * @return 生成された GameObject の共有ポインタ
     */
    static std::shared_ptr<GameObject> LoadPrefab(const std::string& filepath);

    /**
     * @brief キャッシュされているプレハブのJSONデータをクリアする
     */
    static void ClearCache();

    /**
     * @brief プレハブのJSONデータを取得する（キャッシュ対応）
     */
    static nlohmann::json GetPrefabJson(const std::string& filepath);

private:
    /**
     * @brief シーン名からファイルパスを生成する(設定されたパス/[Name].json)
     */
    static std::string GetSceneFilePath(IScene* scene, const std::string& sceneName);

    static inline PrefabManager* prefabManager_ = nullptr;
};
