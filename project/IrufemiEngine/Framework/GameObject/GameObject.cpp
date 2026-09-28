#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/SceneObjectRegistry.h"

#include "Framework/Component/Component.h"
#include "Framework/Component/ComponentFactory.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Collider/AABBColliderComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/Component/Collider/OBBColliderComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/Log.h"
#include "Framework/Scene/SceneSerializer.h"
#include <iostream>
#include <atomic>
#include "Core/Math/Random/Random.h"

GameObject::GameObject() : instanceId_(Irufemi::Random::GeneratorUint64(1, ULLONG_MAX)) {
    AddComponent<TransformComponent>();
}

GameObject::GameObject(const std::string& name)
    : instanceId_(Irufemi::Random::GeneratorUint64(1, ULLONG_MAX)), name_(name) {
    AddComponent<TransformComponent>();
}

GameObject::~GameObject() {
    // デストラクタ内でのOnDestroy呼び出し（shared_from_this等によるbad_weak_ptrリスク）は避け、
    // 未破棄の場合のみ警告ログを出力する（破棄はBaseSceneや明示的なDestroyで完結させる）
    if (lifeState_ != GameObjectLifeState::Destroyed) {
        Log::OutPutLog(std::cerr,
                       "[GameObject] Warning: GameObject '" + name_ + "' was destructed without calling Destroy().\n");
    }
}

// GetTransform() is now inline in GameObject.h

void GameObject::SetIsActive(bool isActive) {
    if (isActive_ == isActive) {
        return;
    }
    isActive_ = isActive;

    if (isActive_) {
        for (auto& comp : components_) {
            comp->OnEnable();
        }
    } else {
        for (auto& comp : components_) {
            comp->OnDisable();
        }
    }

    for (const auto& child : children_) {
        if (child) {
            child->SetIsActive(isActive);
        }
    }
}

void GameObject::Awake() {
    if (lifeState_ >= GameObjectLifeState::Awake) {
        return;
    }
    lifeState_ = GameObjectLifeState::Awake;

    for (const auto& comp : components_) {
        comp->OnAwake();
        if (!comp->IsInitialized()) {
            comp->Initialize();
            comp->SetInitialized(true);
        }
    }
    for (const auto& child : children_) {
        if (child) {
            child->Awake();
        }
    }
}

void GameObject::Initialize() {
    if (lifeState_ >= GameObjectLifeState::Awake) {
        return;
    }
    Awake();
    for (const auto& comp : components_) {
        if (!comp->IsInitialized()) {
            comp->Initialize();
            comp->SetInitialized(true);
        }
    }
    for (const auto& child : children_) {
        if (child) {
            child->Initialize();
        }
    }
}

void GameObject::NotifySpawned() {
    if (lifeState_ >= GameObjectLifeState::Spawned) {
        return;
    }
    if (lifeState_ < GameObjectLifeState::Awake) {
        Initialize();
    }
    lifeState_ = GameObjectLifeState::Spawned;

    for (const auto& comp : components_) {
        comp->OnSpawned();
    }
    for (const auto& child : children_) {
        if (child) {
            child->NotifySpawned();
        }
    }
}

void GameObject::Start() {
    if (lifeState_ >= GameObjectLifeState::Started) {
        return;
    }
    if (lifeState_ < GameObjectLifeState::Spawned) {
        NotifySpawned();
    }
    lifeState_ = GameObjectLifeState::Started;
    isStarted_ = true;

    // Use index-based loop to allow components to add components/children during Start
    for (size_t i = 0; i < components_.size(); ++i) {
        components_[i]->Start();
    }
    for (size_t i = 0; i < children_.size(); ++i) {
        if (children_[i]) {
            children_[i]->Start();
        }
    }
}

void GameObject::Destroy() {
    if (lifeState_ == GameObjectLifeState::Destroyed) {
        return;
    }
    if (isActive_) {
        for (const auto& comp : components_) {
            comp->OnDisable();
        }
        isActive_ = false;
    }
    isDestroyed_ = true;
    lifeState_ = GameObjectLifeState::Destroyed;

    for (const auto& comp : components_) {
        comp->OnDestroy();
    }
    for (const auto& child : children_) {
        if (child) {
            child->Destroy();
        }
    }
}

void GameObject::SetName(const std::string& name) {
    if (name_ == name) {
        return;
    }
    std::string oldName = name_;
    name_ = name;
    if (scene_) {
        scene_->OnGameObjectNameChanged(shared_from_this(), oldName, name);
    }
}

void GameObject::SetScene(BaseScene* scene) {
    scene_ = scene;
    if (scene_ && !name_.empty()) {
        scene_->OnGameObjectNameChanged(shared_from_this(), "", name_);
    }
    for (auto& comp : components_) {
        if (comp) {
            comp->OnSetScene(scene);
        }
    }
    for (auto& child : children_) {
        if (child) {
            child->SetScene(scene);
        }
    }
}

void GameObject::Update(bool isPlayMode) {
    if (!isActive_) {
        return;
    }

    bool isPaused = false;
    if (scene_) {
        if (auto engine = scene_->GetEngine()) {
            isPaused = (engine->GetTimeScale() == 0.0f);
        }
    }

    // コンポーネント更新（単一ロック下でスナップショットを取得し、ループ毎のロック/アンロックを排除）
    std::vector<std::shared_ptr<Component>> activeComps;
    {
        std::lock_guard<std::recursive_mutex> lock(structureMutex_);
        activeComps = components_;
    }
    for (const auto& comp : activeComps) {
        if (!comp) {
            continue;
        }

        // PlayModeでない場合は、エディタで更新可能なコンポーネントのみ更新する
        if (!isPlayMode && !comp->CanUpdateInEditMode()) {
            continue;
        }

        // ポーズ中（TimeScale == 0.0f）かつ、ポーズ中も動作する設定になっていない場合はスキップ
        if (isPlayMode && isPaused && !comp->CanUpdateWhenPaused()) {
            continue;
        }

        comp->Update();
    }

    // 子オブジェクト更新（単一ロック下でスナップショットを取得）
    std::vector<std::shared_ptr<GameObject>> activeChildren;
    {
        std::lock_guard<std::recursive_mutex> lock(structureMutex_);
        activeChildren = children_;
    }
    for (const auto& child : activeChildren) {
        if (child && !child->IsDestroyed()) {
            child->Update(isPlayMode);
        }
    }
}

void GameObject::CleanupDestroyedChildren() {
    std::lock_guard<std::recursive_mutex> lock(structureMutex_);
    for (auto& child : children_) {
        if (child) {
            child->CleanupDestroyedChildren();
        }
    }
    children_.erase(
        std::remove_if(children_.begin(), children_.end(),
                       [](const std::shared_ptr<GameObject>& child) { return !child || child->IsDestroyed(); }),
        children_.end());
}

void GameObject::Draw() {
    if (!isActive_) {
        return;
    }
    for (size_t i = 0; i < components_.size(); ++i) {
        components_[i]->Draw();
    }
    for (size_t i = 0; i < children_.size(); ++i) {
        children_[i]->Draw();
    }
}

void GameObject::SyncRenderState() {
    if (!isActive_ || isDestroyed_) {
        return;
    }
    for (size_t i = 0; i < components_.size(); ++i) {
        components_[i]->SyncRenderState();
    }
    for (size_t i = 0; i < children_.size(); ++i) {
        if (children_[i]) {
            children_[i]->SyncRenderState();
        }
    }
}

void GameObject::DrawOutlineMask() {
    if (!isActive_) {
        return;
    }
    for (size_t i = 0; i < components_.size(); ++i) {
        components_[i]->DrawOutlineMask();
    }
    for (size_t i = 0; i < children_.size(); ++i) {
        children_[i]->DrawOutlineMask();
    }
}

void GameObject::AddChild(std::shared_ptr<GameObject> child) {
    if (!child) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(structureMutex_);

    // 既に親がいる場合は外す
    if (auto currentParent = child->GetParent()) {
        currentParent->RemoveChild(child);
    }

    child->parent_ = shared_from_this();
    children_.push_back(child);

    if (scene_) {
        if (!child->GetScene()) {
            child->SetScene(scene_);
        }
        if (auto registry = scene_->GetObjectRegistry()) {
            registry->Register(child);
        }
    }

    if (auto childTransform = child->GetComponent<TransformComponent>()) {
        childTransform->MarkWorldDirty();
    }

    // 親が非アクティブなら子も非アクティブ化
    if (!isActive_) {
        child->SetIsActive(false);
    }

    // 親のライフサイクル状態を子へ伝播
    if (lifeState_ >= GameObjectLifeState::Started) {
        if (!child->IsStarted()) {
            child->Start();
        }
    } else if (lifeState_ >= GameObjectLifeState::Spawned) {
        if (!child->IsSpawned()) {
            child->NotifySpawned();
        }
    } else if (lifeState_ >= GameObjectLifeState::Awake) {
        if (child->GetLifeState() < GameObjectLifeState::Awake) {
            child->Awake();
        }
    }
}

void GameObject::InsertChild(std::shared_ptr<GameObject> child, size_t index) {
    if (!child) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(structureMutex_);

    if (auto currentParent = child->GetParent()) {
        currentParent->RemoveChild(child);
    }

    child->parent_ = shared_from_this();
    if (index >= children_.size()) {
        children_.push_back(child);
    } else {
        children_.insert(children_.begin() + index, child);
    }

    if (scene_) {
        if (!child->GetScene()) {
            child->SetScene(scene_);
        }
        if (auto registry = scene_->GetObjectRegistry()) {
            registry->Register(child);
        }
    }

    if (auto childTransform = child->GetComponent<TransformComponent>()) {
        childTransform->MarkWorldDirty();
    }

    // 親が非アクティブなら子も非アクティブ化
    if (!isActive_) {
        child->SetIsActive(false);
    }

    // 親のライフサイクル状態を子へ伝播
    if (lifeState_ >= GameObjectLifeState::Started) {
        if (!child->IsStarted()) {
            child->Start();
        }
    } else if (lifeState_ >= GameObjectLifeState::Spawned) {
        if (!child->IsSpawned()) {
            child->NotifySpawned();
        }
    } else if (lifeState_ >= GameObjectLifeState::Awake) {
        if (child->GetLifeState() < GameObjectLifeState::Awake) {
            child->Awake();
        }
    }
}

void GameObject::RemoveChild(std::shared_ptr<GameObject> child) {
    std::lock_guard<std::recursive_mutex> lock(structureMutex_);
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        if (scene_) {
            if (auto registry = scene_->GetObjectRegistry()) {
                registry->Unregister(*it);
            }
        }

        (*it)->parent_.reset();

        if (auto childTransform = (*it)->GetComponent<TransformComponent>()) {
            childTransform->MarkWorldDirty();
        }

        children_.erase(it);
    }
}

size_t GameObject::GetChildIndex(std::shared_ptr<GameObject> child) const {
    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        return std::distance(children_.begin(), it);
    }
    return (size_t)-1;
}

void GameObject::SetParent(std::shared_ptr<GameObject> parent) {
    if (parent) {
        parent->AddChild(shared_from_this());
    } else {
        if (auto currentParent = parent_.lock()) {
            currentParent->RemoveChild(shared_from_this());
        }
    }
}

void GameObject::AddComponent(std::shared_ptr<Component> component) {
    if (!component) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(structureMutex_);

    // TransformComponent の重複追加を防止（1オブジェクトにつき常に1つのみ）
    if (dynamic_cast<TransformComponent*>(component.get())) {
        if (GetComponent<TransformComponent>()) {
            Log::OutPutLog(std::cerr, "[GameObject] Warning: TransformComponent already exists on '" + name_ +
                                          "'. Skipping duplicate addition.\n");
            return;
        }
    }
    component->SetGameObject(this);
    components_.push_back(component);
    componentMap_[typeid(*component)].push_back(component.get());

    if (auto transform = dynamic_cast<TransformComponent*>(component.get())) {
        transformCache_ = transform;
    }

    component->OnRegisterProperties();
    if (lifeState_ >= GameObjectLifeState::Awake) {
        component->OnAwake();
    }
    if (!component->IsInitialized()) {
        component->Initialize();
        component->SetInitialized(true);
    }
    if (lifeState_ >= GameObjectLifeState::Spawned) {
        component->OnSpawned();
    }
    if (isActive_) {
        component->OnEnable();
    }
    if (lifeState_ >= GameObjectLifeState::Started) {
        component->Start();
    }
}

void GameObject::RemoveComponent(Component* component) {
    if (!component) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(structureMutex_);

    // TransformComponentは基本として削除不可とする
    if (component == transformCache_) {
        return;
    }

    // 削除前にライフサイクルを適切に終了
    if (isActive_) {
        component->OnDisable();
    }
    component->OnDestroy();

    // componentMap_からの削除
    auto typeIt = componentMap_.find(typeid(*component));
    if (typeIt != componentMap_.end()) {
        auto& vec = typeIt->second;
        vec.erase(std::remove(vec.begin(), vec.end(), component), vec.end());
    }

    // components_からの削除
    components_.erase(
        std::remove_if(components_.begin(), components_.end(),
                       [component](const std::shared_ptr<Component>& ptr) { return ptr.get() == component; }),
        components_.end());
}

nlohmann::json GameObject::Serialize() const {
    nlohmann::json j;

    j["instanceId"] = instanceId_;

    // デフォルト値と異なる場合のみ出力
    if (!name_.empty()) {
        j["name"] = name_;
    }
    if (!tag_.empty()) {
        j["tag"] = tag_;
    }
    if (!isActive_) {
        j["isActive"] = isActive_; // default is true
    }
    if (isFolder_) {
        j["isFolder"] = isFolder_; // default is false
    }
    if (isLocked_) {
        j["isLocked"] = isLocked_; // default is false
    }

    if (!sourcePrefabPath_.empty()) {
        j["prefabPath"] = sourcePrefabPath_;
        // プレハブのベースデータを取得して比較し、差分（または追加分）のみ保存する
        nlohmann::json baseJ = SceneSerializer::GetPrefabJson(sourcePrefabPath_);
        nlohmann::json baseComps = baseJ.value("components", nlohmann::json::array());

        nlohmann::json comps = nlohmann::json::array();
        for (const auto& comp : components_) {
            std::string cName = comp->GetComponentName();

            nlohmann::json cdata;
            try {
                cdata = comp->Serialize();
            } catch (const std::exception& e) {
                Log::OutPutLog(std::cerr, "[GameObject] Exception during Serialize of component '" + cName +
                                              "': " + std::string(e.what()) + "\n");
                continue;
            } catch (...) {
                Log::OutPutLog(std::cerr,
                               "[GameObject] Unknown Exception during Serialize of component '" + cName + "'\n");
                continue;
            }

            if (!cdata.is_object() || cdata.empty()) {
                continue;
            }

            bool isOverridden = true; // プレハブに存在しない、または差分がある場合はtrue

            // プレハブ内の同一コンポーネントを検索
            for (const auto& baseCompJ : baseComps) {
                if (baseCompJ.value("type", "") == cName) {
                    if (baseCompJ.contains("data") && baseCompJ["data"] == cdata) {
                        isOverridden = false; // プレハブと全く同じデータ
                    }
                    break;
                }
            }

            if (isOverridden) {
                nlohmann::json cj;
                cj["type"] = cName;
                cj["data"] = cdata;
                comps.push_back(cj);
            }
        }
        if (!comps.empty()) {
            j["components"] = comps;
        }
    } else {
        if (!components_.empty()) {
            nlohmann::json comps = nlohmann::json::array();
            for (const auto& comp : components_) {
                nlohmann::json cj;
                std::string cName = comp->GetComponentName();
                cj["type"] = cName;

                nlohmann::json cdata;
                try {
                    cdata = comp->Serialize();
                } catch (const std::exception& e) {
                    Log::OutPutLog(std::cerr, "[GameObject] Exception during Serialize of component '" + cName +
                                                  "': " + std::string(e.what()) + "\n");
                    std::cerr.flush();
                } catch (...) {
                    Log::OutPutLog(std::cerr,
                                   "[GameObject] Unknown Exception during Serialize of component '" + cName + "'\n");
                    std::cerr.flush();
                }

                // コンポーネントのデータが空でなければ出力
                if (cdata.is_object() && !cdata.empty()) {
                    cj["data"] = cdata;
                }
                comps.push_back(cj);
            }
            if (!comps.empty()) {
                j["components"] = comps;
            }
        }
    }

    if (!children_.empty()) {
        nlohmann::json childrenJson = nlohmann::json::array();
        for (const auto& child : children_) {
            if (child && child->IsSerializable()) {
                childrenJson.push_back(child->Serialize());
            }
        }
        if (!childrenJson.empty()) {
            j["children"] = childrenJson;
        }
    }

    return j;
}

void GameObject::Deserialize(const nlohmann::json& j) {
    // シリアライズから復元された＝シーンに保存されている静的オブジェクトである
    SetIsSerializable(true);

    nlohmann::json baseJ = j;
    if (j.contains("prefabPath")) {
        sourcePrefabPath_ = j["prefabPath"];
        // プレハブのベースデータを取得
        baseJ = SceneSerializer::GetPrefabJson(sourcePrefabPath_);
    }

    // まずベース(またはローカル)データから基本情報を復元
    if (baseJ.contains("name")) {
        name_ = baseJ["name"];
    }
    if (baseJ.contains("instanceId")) {
        instanceId_ = baseJ["instanceId"];
    }
    if (baseJ.contains("tag")) {
        tag_ = baseJ["tag"];
    }
    if (baseJ.contains("isActive")) {
        isActive_ = baseJ["isActive"];
    }
    if (baseJ.contains("isFolder")) {
        isFolder_ = baseJ["isFolder"];
    }
    if (baseJ.contains("isLocked")) {
        isLocked_ = baseJ["isLocked"];
    }

    // ローカル上書き情報がある場合はそれで上書き
    if (j.contains("name")) {
        name_ = j["name"];
    }
    if (j.contains("instanceId")) {
        instanceId_ = j["instanceId"];
    }
    if (j.contains("tag")) {
        tag_ = j["tag"];
    }
    if (j.contains("isActive")) {
        isActive_ = j["isActive"];
    }
    if (j.contains("isFolder")) {
        isFolder_ = j["isFolder"];
    }
    if (j.contains("isLocked")) {
        isLocked_ = j["isLocked"];
    }

    if (baseJ.contains("components")) {
        std::vector<std::shared_ptr<Component>> loadedComps;
        for (const auto& cj : baseJ["components"]) {
            if (!cj.contains("type")) {
                Log::OutPutLog(std::cerr, "[GameObject] Warning: Component in base data missing 'type' field.\n");
                continue;
            }
            std::string type = cj["type"];
            std::shared_ptr<Component> newComp;
            bool isExisting = false;

            if (type == "TransformComponent") {
                // コンストラクタで既にアタッチされているTransformを再利用する
                if (auto existingTransform = GetComponent<TransformComponent>()) {
                    for (auto& comp : components_) {
                        if (comp.get() == existingTransform) {
                            newComp = comp;
                            isExisting = true;
                            break;
                        }
                    }
                }
            }
            if (!newComp) {
                newComp = ComponentFactory::Create(type);
            }

            if (newComp) {
                if (!isExisting) {
                    // AddComponentと同等の登録処理をInitializeの前に行う
                    newComp->SetGameObject(this);
                    components_.push_back(newComp);
                    componentMap_[typeid(*newComp)].push_back(newComp.get());
                    newComp->OnRegisterProperties();
                }

                // ベースデータのプロパティを復元
                if (cj.contains("data")) {
                    newComp->Deserialize(cj["data"]);
                }

                // ローカルの上書き情報があれば反映
                if (j.contains("components")) {
                    for (const auto& localCj : j["components"]) {
                        if (localCj.contains("type") && localCj["type"] == type) {
                            if (localCj.contains("data")) {
                                newComp->Deserialize(localCj["data"]);
                            }
                            break;
                        }
                    }
                }

                loadedComps.push_back(newComp);
            }
        }

        // プレハブには存在しないが、ローカルデータで追加された新規コンポーネントを復元
        if (j.contains("components")) {
            for (const auto& localCj : j["components"]) {
                if (!localCj.contains("type")) {
                    Log::OutPutLog(std::cerr, "[GameObject] Warning: Component in local data missing 'type' field.\n");
                    continue;
                }
                std::string localType = localCj["type"];

                // ベースデータに既に存在するかチェック
                bool existsInBase = false;
                if (baseJ.contains("components")) {
                    for (const auto& cj : baseJ["components"]) {
                        if (cj.contains("type") && cj["type"] == localType) {
                            existsInBase = true;
                            break;
                        }
                    }
                }

                // ベースデータに存在しない場合は新規追加
                if (!existsInBase) {
                    std::shared_ptr<Component> newComp;
                    bool isExisting = false;

                    if (localType == "TransformComponent") {
                        if (auto existingTransform = GetComponent<TransformComponent>()) {
                            for (auto& comp : components_) {
                                if (comp.get() == existingTransform) {
                                    newComp = comp;
                                    isExisting = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (!newComp) {
                        newComp = ComponentFactory::Create(localType);
                    }

                    if (newComp) {
                        if (!isExisting) {
                            newComp->SetGameObject(this);
                            components_.push_back(newComp);
                            componentMap_[typeid(*newComp)].push_back(newComp.get());
                            newComp->OnRegisterProperties();
                        }

                        if (localCj.contains("data")) {
                            newComp->Deserialize(localCj["data"]);
                        }

                        loadedComps.push_back(newComp);
                    }
                }
            }
        }

        // 全てのコンポーネントがリストに登録されてから一斉にInitializeを呼ぶ
        // これにより、Initialize内でGetComponentした際に他のコンポーネントが見つかるようになる
        lifeState_ = GameObjectLifeState::Awake;
        for (auto& comp : loadedComps) {
            comp->OnAwake();
            if (!comp->IsInitialized()) {
                comp->Initialize();
                comp->SetInitialized(true);
            }
        }
        if (isActive_) {
            for (auto& comp : loadedComps) {
                comp->OnEnable();
            }
        }
    }

    if (j.contains("children") && j["children"].is_array()) {
        for (const auto& cj : j["children"]) {
            auto child = std::make_shared<GameObject>();
            if (scene_) {
                child->SetScene(scene_);
            }
            child->Deserialize(cj);
            AddChild(child);
        }
    }
}

std::shared_ptr<GameObject> GameObject::Clone() {
    std::unordered_map<uint64_t, uint64_t> idMap;
    auto clone = CloneInternal(idMap);

    // --- 名前解決とIDの差し替え ---
    if (scene_) {
        clone->SetName(scene_->GetUniqueObjectName(this->GetName()));
    } else {
        clone->SetName(this->GetName() + " (Clone)");
    }

    clone->OnIDRemapped(idMap);

    return clone;
}

std::shared_ptr<GameObject> GameObject::CloneInternal(std::unordered_map<uint64_t, uint64_t>& idMap) {
    auto clone = std::make_shared<GameObject>();
    idMap[this->GetInstanceID()] = clone->GetInstanceID();

    clone->SetName(this->GetName());
    clone->SetTag(this->GetTag());
    clone->SetIsActive(this->GetIsActive());
    clone->SetIsFolder(this->GetIsFolder());
    clone->SetIsLocked(this->GetIsLocked());
    clone->SetIsSerializable(this->IsSerializable());
    clone->SetSourcePrefabPath(this->GetSourcePrefabPath());

    // Deep copy components
    for (const auto& comp : components_) {
        // TransformComponentはコンストラクタで既に1つ存在するため、新規追加せず値をコピーして再利用
        if (auto trans = dynamic_cast<TransformComponent*>(comp.get())) {
            if (auto cloneTrans = clone->GetComponent<TransformComponent>()) {
                cloneTrans->Deserialize(trans->Serialize());
                continue;
            }
        }

        auto clonedComp = comp->Clone();
        if (clonedComp) {
            clone->AddComponent(clonedComp);
        } else {
            Log::OutPutLog(std::cerr,
                           "[GameObject] Error: Failed to clone component: " + comp->GetComponentName() + "\n");
        }
    }

    // Deep copy children
    for (const auto& child : children_) {
        auto clonedChild = child->CloneInternal(idMap);
        clone->AddChild(clonedChild);
    }

    return clone;
}

void GameObject::SendCollisionEnter(GameObject* hitObject) {
    for (auto& comp : components_) {
        comp->OnCollisionEnter(hitObject);
    }
}

void GameObject::SendCollisionStay(GameObject* hitObject) {
    for (auto& comp : components_) {
        comp->OnCollisionStay(hitObject);
    }
}

void GameObject::SendCollisionExit(GameObject* hitObject) {
    for (auto& comp : components_) {
        comp->OnCollisionExit(hitObject);
    }
}

std::shared_ptr<GameObject> GameObject::Instantiate(const std::string& prefabPath,
                                                    const Irufemi::Vector3& position,
                                                    bool makeChild) {
    if (scene_) {
        auto obj = scene_->InstantiatePrefab(prefabPath, position);
        if (obj && makeChild) {
            AddChild(obj);
        }
        return obj;
    }
    return nullptr;
}

void GameObject::RegenerateInstanceID(bool recursive) {
    instanceId_ = Irufemi::Random::GeneratorUint64(1, ULLONG_MAX);
    if (recursive) {
        for (auto& child : children_) {
            child->RegenerateInstanceID(true);
        }
    }
}

void GameObject::OnIDRemapped(const std::unordered_map<uint64_t, uint64_t>& idMap) {
    for (auto& comp : components_) {
        comp->OnIDRemapped(idMap);
    }
    for (auto& child : children_) {
        child->OnIDRemapped(idMap);
    }
}

void GameObject::RemapJSONInstanceIDs(nlohmann::json& j, std::unordered_map<uint64_t, uint64_t>& outIdMap) {
    if (j.contains("instanceId")) {
        uint64_t oldId = j["instanceId"];
        uint64_t newId = Irufemi::Random::GeneratorUint64(1, ULLONG_MAX);
        j["instanceId"] = newId;
        outIdMap[oldId] = newId;
    }

    if (j.contains("children") && j["children"].is_array()) {
        for (auto& cj : j["children"]) {
            RemapJSONInstanceIDs(cj, outIdMap);
        }
    }
}
