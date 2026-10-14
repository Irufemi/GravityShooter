#pragma once

#include "Framework/Component/Component.h"
#include "Renderer/Object/3D/AnimationModel/AnimatedMeshObject.h"
#include <memory>
#include <string>
#include <unordered_map>
#include "Resource/Model/Data/ObjModel.h"

/**
 * @class SkinnedMeshRendererComponent
 * @brief エディタ対応のスキニングメッシュ描画コンポーネント。
 * 低レイヤーの AnimatedMeshObject をラップし、インスペクタからモデルを指定できるようにします。
 */
class SkinnedMeshRendererComponent : public Component {
public:
    SkinnedMeshRendererComponent();
    ~SkinnedMeshRendererComponent() override;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize() override;
    /**
     * @brief 生成時の自己完結初期化（AnimatedMeshObjectの生成・モデル初期化）を行います
     */
    void OnAwake() override;
    /**
     * @brief ワールド座標・Transform確定時の描画ステート同期を行います
     */
    void OnSpawned() override;
    /**
     * @brief Update を実行する。
     */
    void Update() override;
    /**
     * @brief Draw を実行する。
     */
    void Draw() override;
    void SyncRenderState() override;

    /**
     * @brief CanUpdateInEditMode かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool CanUpdateInEditMode() const override {
        return true;
    }

    /**
     * @brief OnRegisterProperties を実行する。
     */
    void OnRegisterProperties() override;

    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "SkinnedMeshRendererComponent";
    }
    /**
     * @brief Raycast を実行する。
     */
    bool Raycast(const Irufemi::Ray& ray, float& outDistance) const override;
    /**
     * @brief Renderable を取得する。
     * @return 取得された Renderable
     */
    IRenderable* GetRenderable() override {
        return animatedMesh_.get();
    }

    /**
     * @brief LoadModel を実行する。
     */
    void LoadModel(const std::string& filename);

    /**
     * @brief Serialize を実行する。
     */
    nlohmann::json Serialize() override;
    /**
     * @brief Deserialize を実行する。
     */
    void Deserialize(const nlohmann::json& j) override;

    /**
     * @brief コンポーネントのディープコピーを生成する
     * @return 複製されたコンポーネントの共有ポインタ
     */
    std::shared_ptr<Component> Clone() override;

    // アニメーターや他のロジックからポーズを流し込むための窓口
    /**
     * @brief RawObject を取得する。
     * @return 取得された RawObject
     */
    AnimatedMeshObject* GetRawObject() {
        return animatedMesh_.get();
    }

    // Animatorからのポーズ上書き用
    /**
     * @brief PoseOverride を設定する。
     * @param[in] pose 設定する PoseOverride の値
     */
    void SetPoseOverride(const struct SkeletonPose* pose) {
        poseOverride_ = pose;
    }

    /**
     * @brief Visible を設定する。
     * @param[in] visible 設定する Visible の値
     */
    void SetVisible(bool visible) {
        isVisible_ = visible;
    }
    /**
     * @brief IsVisible かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsVisible() const {
        return isVisible_;
    }

    /**
     * @brief EnableEffectMask を設定する。
     * @param[in] enable 設定する EnableEffectMask の値
     */
    void SetEnableEffectMask(bool enable);
    /**
     * @brief CustomEffectType を設定する。
     * @param[in] type 設定する CustomEffectType の値
     */
    void SetCustomEffectType(int32_t type);
    /**
     * @brief CustomEffectParam を設定する。
     * @param[in] param 設定する CustomEffectParam の値
     */
    void SetCustomEffectParam(float param);

private:
    std::unique_ptr<AnimatedMeshObject> animatedMesh_;
    std::string modelFilename_ = "";
    std::string currentLoadedFilename_ = "";

    const struct SkeletonPose* poseOverride_ = nullptr;
    bool isVisible_ = true;

    std::unordered_map<size_t, ObjMaterial> materialOverrides_;

public:
    /**
     * @brief HasMaterialOverride かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool HasMaterialOverride(size_t meshIndex) const {
        return materialOverrides_.find(meshIndex) != materialOverrides_.end();
    }
    /**
     * @brief MaterialOverride を取得する。
     * @return 取得された MaterialOverride
     */
    const ObjMaterial* GetMaterialOverride(size_t meshIndex) const {
        auto it = materialOverrides_.find(meshIndex);
        return it != materialOverrides_.end() ? &it->second : nullptr;
    }
    /**
     * @brief MaterialOverrideMutable を取得する。
     * @return 取得された MaterialOverrideMutable
     */
    ObjMaterial* GetMaterialOverrideMutable(size_t meshIndex) {
        auto it = materialOverrides_.find(meshIndex);
        return it != materialOverrides_.end() ? &it->second : nullptr;
    }
    /**
     * @brief MaterialOverride を設定する。
     * @param[in] meshIndex 設定する MaterialOverride の値
     * @param[in] material 設定する MaterialOverride の値
     */
    void SetMaterialOverride(size_t meshIndex, const ObjMaterial& material) {
        materialOverrides_[meshIndex] = material;
    }
    /**
     * @brief RemoveMaterialOverride を実行する。
     */
    void RemoveMaterialOverride(size_t meshIndex) {
        materialOverrides_.erase(meshIndex);
    }
    const std::unordered_map<size_t, ObjMaterial>& GetAllMaterialOverrides() const {
        return materialOverrides_;
    }
};
