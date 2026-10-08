#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <variant>
#include <functional>

class GameObject;
class BaseScene;
class IrufemiEngine;
#include "Renderer/System/Core/IRenderable.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Core/Utility/JsonUtility.h"
namespace Irufemi {
struct Ray;
}

enum class ComponentPropertyType {
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    Bool,
    String,
    Float3Array,
    Header,
    Separator,
    Enum,
    GameObjectRef
};

/**
 * @brief コンポーネントプロパティの型安全なデータポインタ保持用バリアント
 */
using ComponentPropertyData =
    std::variant<std::monostate, float*, int*, bool*, std::string*, uint64_t*, Irufemi::Vector2*, Irufemi::Vector3*,
                 Irufemi::Vector4*, std::vector<Irufemi::Vector3>*>;

struct ComponentProperty {
    std::string name;
    ComponentPropertyType type;
    ComponentPropertyData data;
    float minVal = 0.0f;
    float maxVal = 0.0f;
    std::vector<std::string> enumNames;
    std::string tooltip = "";
    nlohmann::json defaultValue;
    std::function<void()> onChanged = nullptr;
    std::string prefabFilterComponent = ""; ///< プレハブ選択時に要求する必須コンポーネント型名（空なら全プレハブ許可）

    /**
     * @brief プレハブ選択時に要求するコンポーネントフィルタを設定する（Fluent API）
     * @param[in] componentTypeName 要求するコンポーネントの型名（例: "RailShooterEnemyComponent"）
     * @return 自身の参照
     */
    ComponentProperty& SetPrefabFilter(const std::string& componentTypeName) {
        prefabFilterComponent = componentTypeName;
        return *this;
    }

    /**
     * @brief 指定した型のポインタを安全に取得する。型が一致しない場合は nullptr を返す。
     */
    template <typename T> T* GetData() const {
        if (auto* ptr = std::get_if<T*>(&data)) {
            return *ptr;
        }
        return nullptr;
    }

    /**
     * @brief 内部ポインタを void* として取得する（後方互換・UI描画用）
     */
    void* GetRawData() const {
        return std::visit(
            [](auto&& ptr) -> void* {
                using T = std::decay_t<decltype(ptr)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    return nullptr;
                } else {
                    return static_cast<void*>(ptr);
                }
            },
            data);
    }

    /**
     * @brief Tooltip を設定する。
     * @param[in] text 設定する Tooltip の値
     */
    ComponentProperty& SetTooltip(const std::string& text) {
        tooltip = text;
        return *this;
    }

    /**
     * @brief MinMax を設定する。
     * @param[in] min 設定する MinMax の値
     * @param[in] max 設定する MinMax の値
     */
    ComponentProperty& SetMinMax(float min, float max) {
        minVal = min;
        maxVal = max;
        return *this;
    }

    /**
     * @brief 値変更時の通知コールバックを設定する。
     * @param[in] callback コールバック関数
     */
    ComponentProperty& OnChanged(std::function<void()> callback) {
        onChanged = std::move(callback);
        return *this;
    }
};

/**
 * @class Component
 * @brief すべてのコンポーネントの基底クラス
 * @details GameObjectにアタッチされ、初期化・更新・描画のライフサイクルを持ちます。
 */
class Component {
public:
    virtual ~Component() = default;

    /**
     * @brief [Phase 1: Awake] 自己完結の初期化
     * @details プロパティの初期設定や、同一GameObject内のコンポーネント取得(GetComponent)を行う。
     *          ※ 他オブジェクトや外部マネージャーへのアクセスは避ける。
     */
    virtual void OnAwake() {}

    /**
     * @brief コンポーネントの初期化 (従来の初期化メソッド、後方互換用)
     */
    virtual void Initialize() {}

    /**
     * @brief [Phase 2: Spawned] シーン・座標確定時の通知
     * @details GameObjectがシーンに配置され、Transform(座標・回転)が確定した瞬間に呼ばれる。
     */
    virtual void OnSpawned() {}

    /**
     * @brief 開始処理 (最初のUpdateの直前に一度だけ呼ばれる)
     */
    virtual void Start() {}

    /**
     * @brief [Phase 4: Destroy] 離脱・後始末
     * @details 各種マネージャーからの登録解除や内部リソースの解放を行う。
     */
    virtual void OnDestroy() {}

    /**
     * @brief コンポーネントが有効化された時に呼ばれる
     */
    virtual void OnEnable() {}

    /**
     * @brief コンポーネントが無効化された時に呼ばれる
     */
    virtual void OnDisable() {}

    /**
     * @brief 所属するシーンが設定・変更された時に呼ばれる
     * @param scene 設定されたシーンのポインタ（破棄・シーン離脱時はnullptr）
     */
    virtual void OnSetScene(class BaseScene* scene) {}

    /**
     * @brief 毎フレームの更新処理
     */
    virtual void Update() {}

    /**
     * @brief Editモード時にUpdateを実行するかどうか
     * @details デフォルトはfalse（実行しない）。エディタでのプレビューが必要なコンポーネントはtrueを返す。
     */
    virtual bool CanUpdateInEditMode() const {
        return false;
    }

    /**
     * @brief ポーズ時（TimeScale == 0.0f）にUpdateを実行するかどうか
     * @details
     * デフォルトはfalse（ポーズ中は実行しない）。UIやエフェクトなど、ポーズ中でもアニメーションさせたい場合はtrueを返す。
     */
    virtual bool CanUpdateWhenPaused() const {
        return false;
    }

    /**
     * @brief 描画処理（レンダラー系コンポーネントでオーバーライド）
     */
    virtual void Draw() {}

    /**
     * @brief 描画ステート・コンピュートタスクの事前構築（ウォームアップ用）
     * @details ゲームロジックを進めずに、描画前同期やComputeTask（GPUスキニング等）の予約のみを行います。
     */
    virtual void SyncRenderState() {}

    /**
     * @brief 紐づく Renderable オブジェクトを取得する
     * @details レンダラー系コンポーネントがこれをオーバーライドすることで、アウトライン描画などを共通化します。
     */
    virtual IRenderable* GetRenderable() {
        return nullptr;
    }

    /**
     * @brief 衝突判定コールバック
     * @details 他のColliderと衝突した瞬間に呼ばれる
     */
    virtual void OnCollisionEnter(class GameObject* hitObject) {}

    /**
     * @brief 衝突判定コールバック
     * @details 他のColliderと衝突している間呼ばれ続ける
     */
    virtual void OnCollisionStay(class GameObject* hitObject) {}

    /**
     * @brief 衝突判定コールバック
     * @details 他のColliderとの衝突が終わった瞬間に呼ばれる
     */
    virtual void OnCollisionExit(class GameObject* hitObject) {}

    /**
     * @brief 選択中の輪郭マスク描画処理
     */
    virtual void DrawOutlineMask() {
        if (auto renderable = GetRenderable()) {
            renderable->DrawOutlineMask();
        }
    }

    /**
     * @brief レイキャスト判定（エディタでのオブジェクト選択時などに呼ばれる）
     * @param[in] ray 判定するレイ
     * @param[out] outDistance レイの始点から衝突点までの距離
     * @return 衝突した場合は true
     */
    virtual bool Raycast(const Irufemi::Ray& ray, float& outDistance) const {
        return false;
    }

    /**
     * @brief 自身の持つ変数をリフレクションシステムに登録する
     */
    virtual void OnRegisterProperties() {}

    /**
     * @brief コンポーネントの種類を表す文字列を返す
     */
    virtual std::string GetComponentName() const {
        return "Component";
    }

    /**
     * @brief IDが再生成された際（Clone等）に、内部で保持しているID参照を新しいIDに読み替えるためのコールバック
     * @param idMap 古いID(Key) と 新しいID(Value) の対応表
     */
    virtual void OnIDRemapped(const std::unordered_map<uint64_t, uint64_t>& idMap) {}

    /**
     * @brief 登録されたプロパティリストを取得する
     */
    const std::vector<ComponentProperty>& GetProperties() const {
        return properties_;
    }

    /**
     * @brief プロパティの登録ヘルパー
     */
    ComponentProperty& RegisterProperty(const std::string& name, float* ptr) {
        properties_.push_back({name, ComponentPropertyType::Float, ptr, 0.0f, 0.0f, {}, "", *ptr});
        return properties_.back();
    }
    /**
     * @brief RegisterPropertyRange を実行する。
     */
    ComponentProperty& RegisterPropertyRange(const std::string& name, float* ptr, float min, float max) {
        properties_.push_back({name, ComponentPropertyType::Float, ptr, min, max, {}, "", *ptr});
        return properties_.back();
    }
    /**
     * @brief RegisterProperty を実行する。
     */
    ComponentProperty& RegisterProperty(const std::string& name, int* ptr) {
        properties_.push_back({name, ComponentPropertyType::Int, ptr, 0.0f, 0.0f, {}, "", *ptr});
        return properties_.back();
    }
    /**
     * @brief RegisterPropertyRange を実行する。
     */
    ComponentProperty& RegisterPropertyRange(const std::string& name, int* ptr, int min, int max) {
        properties_.push_back(
            {name, ComponentPropertyType::Int, ptr, static_cast<float>(min), static_cast<float>(max), {}, "", *ptr});
        return properties_.back();
    }
    /**
     * @brief RegisterEnum を実行する。
     */
    ComponentProperty& RegisterEnum(const std::string& name, int* ptr, const std::vector<std::string>& enumNames) {
        properties_.push_back({name, ComponentPropertyType::Enum, ptr, 0.0f, 0.0f, enumNames, "", *ptr});
        return properties_.back();
    }
    /**
     * @brief RegisterProperty を実行する。
     */
    ComponentProperty& RegisterProperty(const std::string& name, bool* ptr) {
        properties_.push_back({name, ComponentPropertyType::Bool, ptr, 0.0f, 0.0f, {}, "", *ptr});
        return properties_.back();
    }
    /**
     * @brief RegisterProperty を実行する。
     */
    ComponentProperty& RegisterProperty(const std::string& name, std::string* ptr) {
        properties_.push_back({name, ComponentPropertyType::String, ptr, 0.0f, 0.0f, {}, "", *ptr});
        return properties_.back();
    }
    /**
     * @brief RegisterGameObjectRef を実行する。
     */
    ComponentProperty& RegisterGameObjectRef(const std::string& name, uint64_t* ptr) {
        properties_.push_back({name, ComponentPropertyType::GameObjectRef, ptr, 0.0f, 0.0f, {}, "", *ptr});
        return properties_.back();
    }
    /**
     * @brief RegisterProperty を実行する。
     */
    ComponentProperty& RegisterProperty(const std::string& name, Irufemi::Vector2* ptr) {
        properties_.push_back(
            {name, ComponentPropertyType::Float2, ptr, 0.0f, 0.0f, {}, "", Irufemi::JsonUtility::ToJson(*ptr)});
        return properties_.back();
    }
    /**
     * @brief RegisterProperty を実行する。
     */
    ComponentProperty& RegisterProperty(const std::string& name, Irufemi::Vector3* ptr) {
        properties_.push_back(
            {name, ComponentPropertyType::Float3, ptr, 0.0f, 0.0f, {}, "", Irufemi::JsonUtility::ToJson(*ptr)});
        return properties_.back();
    }
    /**
     * @brief RegisterProperty を実行する。
     */
    ComponentProperty& RegisterProperty(const std::string& name, Irufemi::Vector4* ptr) {
        properties_.push_back(
            {name, ComponentPropertyType::Float4, ptr, 0.0f, 0.0f, {}, "", Irufemi::JsonUtility::ToJson(*ptr)});
        return properties_.back();
    }
    /**
     * @brief RegisterProperty を実行する。
     */
    ComponentProperty& RegisterProperty(const std::string& name, std::vector<Irufemi::Vector3>* ptr) {
        nlohmann::json jArray = nlohmann::json::array();
        for (const auto& v : *ptr) {
            jArray.push_back(Irufemi::JsonUtility::ToJson(v));
        }
        properties_.push_back({name, ComponentPropertyType::Float3Array, ptr, 0.0f, 0.0f, {}, "", jArray});
        return properties_.back();
    }
    /**
     * @brief RegisterHeader を実行する。
     */
    ComponentProperty& RegisterHeader(const std::string& name) {
        properties_.push_back(
            {name, ComponentPropertyType::Header, ComponentPropertyData{}, 0.0f, 0.0f, {}, "", nullptr});
        return properties_.back();
    }
    /**
     * @brief RegisterSeparator を実行する。
     */
    ComponentProperty& RegisterSeparator() {
        properties_.push_back(
            {"", ComponentPropertyType::Separator, ComponentPropertyData{}, 0.0f, 0.0f, {}, "", nullptr});
        return properties_.back();
    }

    /**
     * @brief コンポーネントの状態をJSONにシリアライズする
     */
    virtual nlohmann::json Serialize() {
        nlohmann::json j = nlohmann::json::object();
        for (const auto& prop : properties_) {
            std::visit(
                [&](auto&& ptr) {
                    using T = std::decay_t<decltype(ptr)>;
                    if constexpr (std::is_same_v<T, float*>) {
                        if (ptr) {
                            j[prop.name] = *ptr;
                        }
                    } else if constexpr (std::is_same_v<T, int*>) {
                        if (ptr) {
                            j[prop.name] = *ptr;
                        }
                    } else if constexpr (std::is_same_v<T, bool*>) {
                        if (ptr) {
                            j[prop.name] = *ptr;
                        }
                    } else if constexpr (std::is_same_v<T, std::string*>) {
                        if (ptr) {
                            j[prop.name] = *ptr;
                        }
                    } else if constexpr (std::is_same_v<T, uint64_t*>) {
                        if (ptr) {
                            j[prop.name] = *ptr;
                        }
                    } else if constexpr (std::is_same_v<T, Irufemi::Vector2*>) {
                        if (ptr) {
                            j[prop.name] = Irufemi::JsonUtility::ToJson(*ptr);
                        }
                    } else if constexpr (std::is_same_v<T, Irufemi::Vector3*>) {
                        if (ptr) {
                            j[prop.name] = Irufemi::JsonUtility::ToJson(*ptr);
                        }
                    } else if constexpr (std::is_same_v<T, Irufemi::Vector4*>) {
                        if (ptr) {
                            j[prop.name] = Irufemi::JsonUtility::ToJson(*ptr);
                        }
                    } else if constexpr (std::is_same_v<T, std::vector<Irufemi::Vector3>*>) {
                        if (ptr) {
                            nlohmann::json jArray = nlohmann::json::array();
                            for (const auto& v : *ptr) {
                                jArray.push_back(Irufemi::JsonUtility::ToJson(v));
                            }
                            j[prop.name] = jArray;
                        }
                    }
                },
                prop.data);
        }
        return j;
    }

    /**
     * @brief JSONからコンポーネントの状態を復元する
     */
    virtual void Deserialize(const nlohmann::json& j) {
        for (const auto& prop : properties_) {
            if (!j.contains(prop.name)) {
                continue;
            }
            std::visit(
                [&](auto&& ptr) {
                    using T = std::decay_t<decltype(ptr)>;
                    if constexpr (std::is_same_v<T, float*>) {
                        if (ptr) {
                            *ptr = j[prop.name].get<float>();
                        }
                    } else if constexpr (std::is_same_v<T, int*>) {
                        if (ptr) {
                            *ptr = j[prop.name].get<int>();
                        }
                    } else if constexpr (std::is_same_v<T, bool*>) {
                        if (ptr) {
                            *ptr = j[prop.name].get<bool>();
                        }
                    } else if constexpr (std::is_same_v<T, std::string*>) {
                        if (ptr) {
                            *ptr = j[prop.name].get<std::string>();
                        }
                    } else if constexpr (std::is_same_v<T, uint64_t*>) {
                        if (ptr) {
                            *ptr = j[prop.name].get<uint64_t>();
                        }
                    } else if constexpr (std::is_same_v<T, Irufemi::Vector2*>) {
                        if (ptr) {
                            *ptr = Irufemi::JsonUtility::ToVector2(j[prop.name], *ptr);
                        }
                    } else if constexpr (std::is_same_v<T, Irufemi::Vector3*>) {
                        if (ptr) {
                            *ptr = Irufemi::JsonUtility::ToVector3(j[prop.name], *ptr);
                        }
                    } else if constexpr (std::is_same_v<T, Irufemi::Vector4*>) {
                        if (ptr) {
                            *ptr = Irufemi::JsonUtility::ToVector4(j[prop.name], *ptr);
                        }
                    } else if constexpr (std::is_same_v<T, std::vector<Irufemi::Vector3>*>) {
                        if (ptr) {
                            auto arr = j[prop.name];
                            if (arr.is_array()) {
                                ptr->clear();
                                for (const auto& item : arr) {
                                    ptr->push_back(Irufemi::JsonUtility::ToVector3(item));
                                }
                            }
                        }
                    }
                },
                prop.data);
        }
    }

    /**
     * @brief コンポーネントのプロパティを別のコンポーネントからコピーする (ディープコピー用)
     */
    virtual void CopyPropertiesFrom(const Component* other) {
        if (!other || properties_.size() != other->properties_.size()) {
            return;
        }
        for (size_t i = 0; i < properties_.size(); ++i) {
            auto& dst = properties_[i];
            const auto& src = other->properties_[i];
            if (dst.data.index() != src.data.index()) {
                continue;
            }
            std::visit(
                [](auto&& dstPtr, auto&& srcPtr) {
                    using DstT = std::decay_t<decltype(dstPtr)>;
                    using SrcT = std::decay_t<decltype(srcPtr)>;
                    if constexpr (std::is_same_v<DstT, SrcT> && !std::is_same_v<DstT, std::monostate>) {
                        if (dstPtr && srcPtr) {
                            *dstPtr = *srcPtr;
                        }
                    }
                },
                dst.data, src.data);
        }
    }

    /**
     * @brief クローンを作成する
     */
    virtual std::shared_ptr<Component> Clone();

    /**
     * @brief エディタ（インスペクター）用UIの描画処理について
     * @details エディタ側のカスタムUIを実装したい場合は、OnRegisterProperties()
     * をオーバーライドしてプロパティを登録するか、 Engine/Editor/Core/ComponentEditorRegistry
     * にカスタムエディタ(IComponentEditor)を登録してください。
     */

    /**
     * @brief 所属するGameObjectのセット
     * @param gameObject 親となるGameObjectのポインタ
     */
    void SetGameObject(GameObject* gameObject) {
        gameObject_ = gameObject;
    }

    /**
     * @brief 所属するGameObjectの取得
     * @return GameObject* 親のポインタ
     */
    GameObject* GetGameObject() const {
        return gameObject_;
    }

    /**
     * @brief 所属するGameObjectのアタッチされた TransformComponent を取得するショートカット
     * @return TransformComponent*
     */
    class TransformComponent* GetTransform() const;

    /**
     * @brief 所属するシーンを取得するショートカット
     * @return BaseScene* 所属シーンへのポインタ（未所属ならnullptr）
     */
    BaseScene* GetScene() const;

    /**
     * @brief エンジンコアインスタンスを取得するショートカット
     * @return IrufemiEngine* エンジンへのポインタ（未所属ならnullptr）
     */
    IrufemiEngine* GetEngine() const;

    /**
     * @brief 初期化済みかどうかを取得する
     */
    bool IsInitialized() const {
        return isInitialized_;
    }

    /**
     * @brief 初期化済みフラグを設定する
     */
    void SetInitialized(bool initialized) {
        isInitialized_ = initialized;
    }

protected:
    GameObject* gameObject_ = nullptr;          ///< 親GameObjectへのポインタ
    std::vector<ComponentProperty> properties_; ///< 自動シリアライズ・UI化用のプロパティリスト

private:
    bool isInitialized_ = false; ///< 初期化済みフラグ
};
