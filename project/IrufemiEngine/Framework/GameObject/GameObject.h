#pragma once
#include <string>
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>
#include "Framework/Component/Component.h"
#include "Core/System/ComponentPool.h"

class BaseScene;

/**
 * @brief GameObjectのライフサイクル状態
 */
enum class GameObjectLifeState : uint8_t {
    Constructed = 0, ///< インスタンス生成直後（コンストラクタ実行中）
    Awake,           ///< コンポーネント構築・プロパティ登録完了
    Spawned,         ///< シーン配置・Transform設定完了
    Started,         ///< 初回Update直前のStart実行完了
    Destroyed        ///< 破棄済み
};

/**
 * @class GameObject
 * @brief コンポーネントをアタッチできるエンティティの基底クラス
 */
class GameObject : public std::enable_shared_from_this<GameObject> {
public:
    GameObject();
    GameObject(const std::string& name);
    ~GameObject();

    /**
     * @brief 現在のライフサイクル状態を取得する
     */
    GameObjectLifeState GetLifeState() const {
        return lifeState_;
    }

    /**
     * @brief Awake済みかどうかを判定する
     */
    bool IsAwake() const {
        return lifeState_ >= GameObjectLifeState::Awake;
    }

    /**
     * @brief Spawned（シーン配置済み）かどうかを判定する
     */
    bool IsSpawned() const {
        return lifeState_ >= GameObjectLifeState::Spawned;
    }

    /**
     * @brief InstanceID を取得する。
     * @return 取得された InstanceID
     */
    uint64_t GetInstanceID() const {
        return instanceId_;
    }
    /**
     * @brief Tag を取得する。
     * @return 取得された Tag
     */
    const std::string& GetTag() const {
        return tag_;
    }
    /**
     * @brief Tag を設定する。
     * @param[in] tag 設定する Tag の値
     */
    void SetTag(const std::string& tag) {
        tag_ = tag;
    }

    /**
     * @brief 自己完結の初期化処理を行う（Phase 1: Awake）。同一GameObject内のコンポーネント取得を行う。
     */
    void Awake();

    /**
     * @brief ゲームオブジェクトの初期化処理を行う。多重呼び出しは自動的にガードされます。
     */
    void Initialize();

    /**
     * @brief シーン配置およびTransform確定時の通知を行う（Phase 2: Spawned）。
     */
    void NotifySpawned();

    /**
     * @brief ゲームオブジェクトの開始処理。最初のUpdateが呼ばれる直前に1度だけ実行される。
     */
    void Start();
    /**
     * @brief ゲームオブジェクトの毎フレームの更新処理。すべてのアクティブなコンポーネントを更新する。
     */
    void Update(bool isPlayMode = true);
    /**
     * @brief Draw を実行する。
     */
    void Draw();

    /**
     * @brief 描画ステート・コンピュートタスクの事前構築（ウォームアップ用）。
     * @details ゲームロジックを進めずに、描画前同期やComputeTask（GPUスキニング等）の予約のみを行います。
     */
    void SyncRenderState();
    /**
     * @brief DrawOutlineMask を実行する。
     */
    void DrawOutlineMask();

    /**
     * @brief Serialize を実行する。
     */
    nlohmann::json Serialize() const;
    /**
     * @brief Deserialize を実行する。
     */
    void Deserialize(const nlohmann::json& j);

    /**
     * @brief 自分自身の完全なコピー(クローン)を生成する
     */
    std::shared_ptr<GameObject> Clone();

    /**
     * @brief 内部再帰用のクローン処理 (IDマッピングを構築しながらコピーする)
     */
    std::shared_ptr<GameObject> CloneInternal(std::unordered_map<uint64_t, uint64_t>& idMap);

    /**
     * @brief 一意のインスタンスIDを再生成する（CloneやPrefabロード時のID重複回避用）
     * @param recursive 子オブジェクトも再帰的にIDを再生成するかどうか
     */
    void RegenerateInstanceID(bool recursive = true);

    /**
     * @brief IDが再生成された際（Clone等）に、内部で保持しているID参照を新しいIDに読み替えるためのコールバック
     * @param idMap 古いID(Key) と 新しいID(Value) の対応表
     */
    virtual void OnIDRemapped(const std::unordered_map<uint64_t, uint64_t>& idMap);

    /**
     * @brief JSONツリー内の全 instanceId を新しいUUIDに書き換え、新旧の対応表を作成する（Clone/Prefab展開用）
     */
    static void RemapJSONInstanceIDs(nlohmann::json& j, std::unordered_map<uint64_t, uint64_t>& outIdMap);

    /**
     * @brief 指定した型の新しいコンポーネントをアタッチして返す
     * @return アタッチされたコンポーネントの共有ポインタ
     */
    template <typename T, typename... Args> std::shared_ptr<T> AddComponent(Args&&... args) {
        std::shared_ptr<T> component;
        if constexpr (IsPooledComponent<T>::value) {
            component = ComponentPool<T>::GetInstance().Create(std::forward<Args>(args)...);
        } else {
            component = std::make_shared<T>(std::forward<Args>(args)...);
        }

        // 実際の登録処理およびライフサイクル通知は非テンプレート版へ委譲
        AddComponent(std::static_pointer_cast<Component>(component));
        return component;
    }

    /**
     * @brief 既存のコンポーネントをアタッチする (Undo用)
     */
    void AddComponent(std::shared_ptr<Component> component);

    /**
     * @brief 指定した型のコンポーネントを取得する
     * @return 見つかった場合はそのポインタ、無ければnullptr
     */
    template <typename T> T* GetComponent() const {
        std::lock_guard<std::recursive_mutex> lock(structureMutex_);
        auto it = componentMap_.find(typeid(T));
        if (it != componentMap_.end() && !it->second.empty()) {
            return static_cast<T*>(it->second.front());
        }
        return nullptr;
    }

    /**
     * @brief インターフェースや基底クラスを実装しているコンポーネントを動的に検索して取得する
     * @return 見つかった場合はそのポインタ、無ければnullptr
     */
    template <typename T> T* GetComponentByInterface() const {
        std::lock_guard<std::recursive_mutex> lock(structureMutex_);
        for (const auto& comp : components_) {
            if (auto target = dynamic_cast<T*>(comp.get())) {
                return target;
            }
        }
        return nullptr;
    }

    /**
     * @brief 自身のアタッチされている TransformComponent を取得するショートカット
     * @return TransformComponent* (GameObjectは必ずTransformを持つため、基本的にはnullptrにならない)
     */
    class TransformComponent* GetTransform() const {
        return transformCache_;
    }

    /**
     * @brief 自身およびすべての子孫から、指定した型のコンポーネントを1つ探して取得する
     * @return 見つかった場合はそのポインタ、無ければnullptr
     */
    template <typename T> T* GetComponentInChildren() {
        if (T* comp = GetComponent<T>()) {
            return comp;
        }
        for (auto& child : children_) {
            if (T* comp = child->GetComponentInChildren<T>()) {
                return comp;
            }
        }
        return nullptr;
    }

    /**
     * @brief 自身およびすべての子孫から、指定した型のコンポーネントをすべて取得する
     * @return 見つかったコンポーネントポインタのリスト
     */
    template <typename T> std::vector<T*> GetComponentsInChildren() {
        std::vector<T*> results;
        GetComponentsInChildrenRecursive<T>(results);
        return results;
    }

    /**
     * @brief コンポーネントを削除する
     */
    void RemoveComponent(Component* component);

    /**
     * @brief アタッチされているすべてのコンポーネントのリストを取得する
     */
    const std::vector<std::shared_ptr<Component>>& GetComponents() const {
        return components_;
    }

    // --- アクセッサ ---
    /**
     * @brief Name を取得する。
     * @return 取得された Name
     */
    const std::string& GetName() const {
        return name_;
    }
    /**
     * @brief Name を設定する。
     * @param[in] name 設定する Name の値
     */
    void SetName(const std::string& name);
    /**
     * @brief IsActive を設定する。
     * @param[in] isActive 設定する IsActive の値
     */
    void SetIsActive(bool isActive);
    /**
     * @brief ゲームオブジェクトの有効/無効を切り替える。無効なオブジェクトはUpdateや描画が行われない。
     * @param[in] active trueで有効、falseで無効
     */
    void SetActive(bool isActive) {
        SetIsActive(isActive);
    }
    /**
     * @brief IsActive を取得する。
     * @return 取得された IsActive
     */
    bool GetIsActive() const {
        return isActive_;
    }

    /**
     * @brief Scene を設定する。
     * @param[in] scene 設定する Scene の値
     */
    void SetScene(BaseScene* scene);
    /**
     * @brief Scene を取得する。
     * @return 取得された Scene
     */
    BaseScene* GetScene() const {
        return scene_;
    }

    // --- 親子関係 ---
    /**
     * @brief AddChild を実行する。
     */
    void AddChild(std::shared_ptr<GameObject> child);
    /**
     * @brief InsertChild を実行する。
     */
    void InsertChild(std::shared_ptr<GameObject> child, size_t index);
    /**
     * @brief RemoveChild を実行する。
     */
    void RemoveChild(std::shared_ptr<GameObject> child);
    /**
     * @brief Parent を取得する。
     * @return 取得された Parent
     */
    std::shared_ptr<GameObject> GetParent() const {
        return parent_.lock();
    }
    /**
     * @brief Children を取得する。
     * @return 取得された Children
     */
    const std::vector<std::shared_ptr<GameObject>>& GetChildren() const {
        return children_;
    }
    /**
     * @brief Parent を設定する。
     * @param[in] parent 設定する Parent の値
     */
    void SetParent(std::shared_ptr<GameObject> parent);
    /**
     * @brief ChildIndex を取得する。
     * @return 取得された ChildIndex
     */
    size_t GetChildIndex(std::shared_ptr<GameObject> child) const;

    // --- ライフサイクル ---
    /**
     * @brief オブジェクトを破棄状態にする（OnDestroyを呼び出し、現在のフレームの終わりに削除される）
     */
    void Destroy();
    /**
     * @brief IsDestroyed かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsDestroyed() const {
        return isDestroyed_ || lifeState_ == GameObjectLifeState::Destroyed;
    }
    /**
     * @brief IsStarted かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsStarted() const {
        return isStarted_;
    }

    /**
     * @brief 破棄フラグが立った子オブジェクトを再帰的に削除する (GC)
     * @details マルチスレッドUpdate完了後のメインスレッド同期フェーズで呼び出されます。
     */
    void CleanupDestroyedChildren();

    // --- イベント伝達 ---
    /**
     * @brief SendCollisionEnter を実行する。
     */
    void SendCollisionEnter(GameObject* hitObject);
    /**
     * @brief SendCollisionStay を実行する。
     */
    void SendCollisionStay(GameObject* hitObject);
    /**
     * @brief SendCollisionExit を実行する。
     */
    void SendCollisionExit(GameObject* hitObject);

    // --- 動的生成 ---
    /**
     * @brief 所属するシーンにプレハブから新しい GameObject を生成する
     * @param[in] prefabPath プレハブファイルのパス
     * @param[in] position 配置座標
     * @param[in] makeChild trueの場合、生成されたオブジェクトを自身の子（AddChild）として階層化する
     * @return 生成された GameObject のポインタ
     */
    std::shared_ptr<GameObject> Instantiate(const std::string& prefabPath,
                                            const Irufemi::Vector3& position = {0, 0, 0},
                                            bool makeChild = false);

    // --- エディタ用フラグ ---
    /**
     * @brief IsFolder を設定する。
     * @param[in] isFolder 設定する IsFolder の値
     */
    void SetIsFolder(bool isFolder) {
        isFolder_ = isFolder;
    }
    /**
     * @brief IsFolder を取得する。
     * @return 取得された IsFolder
     */
    bool GetIsFolder() const {
        return isFolder_;
    }
    /**
     * @brief IsLocked を設定する。
     * @param[in] isLocked 設定する IsLocked の値
     */
    void SetIsLocked(bool isLocked) {
        isLocked_ = isLocked;
    }
    /**
     * @brief IsLocked を取得する。
     * @return 取得された IsLocked
     */
    bool GetIsLocked() const {
        return isLocked_;
    }
    /**
     * @brief IsSerializable を設定する。
     * @param[in] isSerializable 設定する IsSerializable の値
     */
    void SetIsSerializable(bool isSerializable) {
        isSerializable_ = isSerializable;
    }
    /**
     * @brief IsSerializable かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsSerializable() const {
        return isSerializable_;
    }

    /**
     * @brief SourcePrefabPath を設定する。
     * @param[in] path 設定する SourcePrefabPath の値
     */
    void SetSourcePrefabPath(const std::string& path) {
        sourcePrefabPath_ = path;
    }
    /**
     * @brief SourcePrefabPath を取得する。
     * @return 取得された SourcePrefabPath
     */
    const std::string& GetSourcePrefabPath() const {
        return sourcePrefabPath_;
    }

    /**
     * @brief エディタのヒエラルキーパネルでの非表示フラグを設定する
     * @param[in] hide trueの場合、エディタのヒエラルキーに表示しない（内部管理・プール用など）
     */
    void SetHideInHierarchy(bool hide) {
        hideInHierarchy_ = hide;
    }
    /**
     * @brief エディタのヒエラルキーパネルでの非表示フラグを取得する
     * @return 非表示フラグ
     */
    bool GetHideInHierarchy() const {
        return hideInHierarchy_;
    }

private:
    uint64_t instanceId_ = 0;
    std::string tag_ = "Untagged";
    std::string name_ = "GameObject";
    bool isActive_ = true;
    bool isStarted_ = false;
    bool isDestroyed_ = false;
    GameObjectLifeState lifeState_ = GameObjectLifeState::Constructed;
    bool isFolder_ = false;
    bool isLocked_ = false;
    bool hideInHierarchy_ = false; ///< エディタのヒエラルキーパネル非表示フラグ（プール・内部管理用）
    bool isSerializable_ = false; // デフォルトはfalse（動的生成とみなす）
    std::string sourcePrefabPath_ = "";
    BaseScene* scene_ = nullptr;

    std::weak_ptr<GameObject> parent_;
    std::vector<std::shared_ptr<GameObject>> children_;

    std::vector<std::shared_ptr<Component>> components_;
    std::unordered_map<std::type_index, std::vector<Component*>> componentMap_;
    class TransformComponent* transformCache_ = nullptr;
    mutable std::recursive_mutex structureMutex_; ///< 構造変更（子オブジェクト・コンポーネント着脱）用ミューテックス

private:
    template <typename T>
    /**
     * @brief ComponentsInChildrenRecursive を取得する。
     */
    void GetComponentsInChildrenRecursive(std::vector<T*>& results) {
        if (T* comp = GetComponent<T>()) {
            results.push_back(comp);
        }
        for (auto& child : children_) {
            child->GetComponentsInChildrenRecursive<T>(results);
        }
    }
};
