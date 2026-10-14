#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>

class GameObject;

/**
 * @class SceneObjectRegistry
 * @brief シーン内のGameObjectを名前・ID・タグ等で高速(O(1))に検索・管理するためのインデックスレジストリ
 * @details
 * 循環参照を防ぐため、オブジェクトへの参照は std::weak_ptr で保持します。
 * オブジェクトの破棄や名前変更、重複名採番を安全に処理します。
 */
class SceneObjectRegistry {
public:
    SceneObjectRegistry() = default;
    ~SceneObjectRegistry() = default;

    /**
     * @brief オブジェクトをインデックスに登録する
     * @param[in] obj 登録するGameObject
     */
    void Register(const std::shared_ptr<GameObject>& obj);

    /**
     * @brief オブジェクトをインデックスから解除する
     * @param[in] obj 解除するGameObject
     */
    void Unregister(const std::shared_ptr<GameObject>& obj);

    /**
     * @brief 全てのインデックスをクリアする
     */
    void Clear();

    /**
     * @brief 名前から該当するGameObjectを1件探す (O(1))
     * @param[in] name オブジェクト名
     * @return 見つかったGameObject（存在しないか破棄済みの場合はnullptr）
     */
    std::shared_ptr<GameObject> FindByName(const std::string& name);

    /**
     * @brief 指定した名前を持つ全てのGameObjectを取得する
     * @param[in] name オブジェクト名
     * @return 該当するGameObjectのリスト
     */
    std::vector<std::shared_ptr<GameObject>> FindAllByName(const std::string& name);

    /**
     * @brief インスタンスIDから該当するGameObjectを探す (O(1))
     * @param[in] instanceId インスタンスID
     * @return 見つかったGameObject
     */
    std::shared_ptr<GameObject> FindById(uint64_t instanceId);

    /**
     * @brief 指定したタグを持つ全てのGameObjectを取得する
     * @param[in] tag タグ名
     * @param[in] allObjects シーン内の全オブジェクトリスト
     * @return 該当するGameObjectのリスト
     */
    std::vector<std::shared_ptr<GameObject>> FindByTag(const std::string& tag,
                                                       const std::vector<std::shared_ptr<GameObject>>& allObjects);

    /**
     * @brief GameObjectの名前が変更された際にインデックスを更新する
     * @param[in] obj 対象オブジェクト
     * @param[in] oldName 変更前の名前
     * @param[in] newName 変更後の名前
     */
    void OnNameChanged(const std::shared_ptr<GameObject>& obj, const std::string& oldName, const std::string& newName);

    /**
     * @brief シーン内で一意となる名前を生成する (重複解決の最適化済み)
     * @param[in] baseName 基本となる名前
     * @param[in] pendingAdds シーンに追加待機中のオブジェクトリスト
     * @return 重複しないユニークな名前
     */
    std::string GenerateUniqueName(const std::string& baseName,
                                   const std::vector<std::shared_ptr<GameObject>>& pendingAdds);

private:
    // NOTE: 子階層の一括登録/解除や名前重複解決において再帰呼出し（再入）が発生するため、
    // std::recursive_mutex を意図して使用しています。
    std::recursive_mutex registryMutex_;

    // 名前インデックス (同名オブジェクトが存在し得るため vector<weak_ptr> で管理)
    std::unordered_map<std::string, std::vector<std::weak_ptr<GameObject>>> nameIndex_;

    // インスタンスIDインデックス (IDは一意)
    std::unordered_map<uint64_t, std::weak_ptr<GameObject>> idIndex_;
};
