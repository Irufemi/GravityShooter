#pragma once
#include "Framework/Component/Component.h"
#include <string>
#include "Renderer/PostProcess/PostProcessManager.h"

class MeshRendererComponent;
class SkinnedMeshRendererComponent;

/**
 * @class EffectMaskComponent
 * @brief オブジェクト固有のポストプロセスエフェクト（アニメ塗り、グリッチ等）を指定するためのコンポーネント
 * @details このコンポーネントをアタッチすると、同じGameObjectにあるRendererのカスタムエフェクト値を上書きします。
 */
class EffectMaskComponent : public Component {
public:
    EffectMaskComponent();
    ~EffectMaskComponent() override;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize() override;
    /**
     * @brief 生成時の同一オブジェクト内MeshRenderer取得を行います
     */
    void OnAwake() override;
    /**
     * @brief シーン参加時のPostProcessManagerキャッシュを行います
     */
    void OnSpawned() override;
    /**
     * @brief Update を実行する。
     */
    void Update() override;

    /**
     * @brief インスペクターおよびシリアライズ用のプロパティ登録を行う
     */
    void OnRegisterProperties() override;

    /**
     * @brief CanUpdateInEditMode かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool CanUpdateInEditMode() const override {
        return true;
    }

    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "EffectMaskComponent";
    }
    /**
     * @brief Serialize を実行する。
     */
    nlohmann::json Serialize() override;
    /**
     * @brief Deserialize を実行する。
     */
    void Deserialize(const nlohmann::json& j) override;

    /**
     * @brief クローンを作成する
     */
    std::shared_ptr<Component> Clone() override;

    /**
     * @brief EnableEffectMask を設定する。
     * @param[in] enable 設定する EnableEffectMask の値
     */
    void SetEnableEffectMask(bool enable) {
        enableEffectMask_ = enable;
    }
    /**
     * @brief EnableEffectMask を取得する。
     * @return 取得された EnableEffectMask
     */
    bool GetEnableEffectMask() const {
        return enableEffectMask_;
    }

    /**
     * @brief CustomEffectType を設定する。
     * @param[in] type 設定する CustomEffectType の値
     */
    void SetCustomEffectType(int32_t type) {
        customEffectType_ = type;
    }
    /**
     * @brief CustomEffectType を取得する。
     * @return 取得された CustomEffectType
     */
    int32_t GetCustomEffectType() const {
        return customEffectType_;
    }

    /**
     * @brief CachedEffectParam を取得する。
     * @return 取得された CachedEffectParam
     */
    float GetCachedEffectParam() const {
        return cachedEffectParam_;
    }

    /**
     * @brief CustomParams を取得する。
     * @return 取得された CustomParams
     */
    PostProcessManager::CustomEffectParams& GetCustomParams() {
        return customParams_;
    }
    /**
     * @brief CustomParams を取得する。
     * @return 取得された CustomParams
     */
    const PostProcessManager::CustomEffectParams& GetCustomParams() const {
        return customParams_;
    }

private:
    bool enableEffectMask_ = true;
    int32_t customEffectType_ = 0;
    float cachedEffectParam_ = 0.0f;
    PostProcessManager::CustomEffectParams customParams_;

    bool lastEnable_ = false;
    int32_t lastType_ = -1;
    float lastParam_ = -1.0f;
    PostProcessManager* cachedPostProcessManager_ = nullptr;
};
