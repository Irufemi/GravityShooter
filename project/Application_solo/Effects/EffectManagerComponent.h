#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/MathFunction.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "Core/Utility/ObjectPool.h"

class GameObject;

/**
 * @brief 演出（エフェクト）のPrefabを一元管理し、再生（Instantiate または Poolからの取得）を行うマネージャー。
 *        シーン内に1つだけ配置されることを想定。
 */
class EffectManagerComponent : public Component {
public:
    EffectManagerComponent();
    ~EffectManagerComponent() override = default;

    void Initialize() override;
    void Start() override;
    void Update() override;
    void OnDestroy() override;
    void OnRegisterProperties() override;
    std::string GetComponentName() const override {
        return "EffectManagerComponent";
    }

    /**
     * @brief アクティブな EffectManagerComponent の静的インスタンスを取得する（O(1)）
     * @return EffectManagerComponent のポインタ（未登録時は nullptr）
     */
    static EffectManagerComponent* GetInstance() {
        return s_instance_;
    }

    /**
     * @brief 指定したキーのエフェクトを指定したワールド座標で再生する
     * @param effectKey "Hit" などのエフェクトの種類を示すキー
     * @param worldPosition 再生する座標
     */
    void PlayEffect(const std::string& effectKey, const Irufemi::Vector3& worldPosition);

    /**
     * @brief 指定したGameObjectの子としてエフェクトをアタッチし、親の移動・3軸回転に自動追従させる (Niagara SpawnAttached方式)
     * @param effectKey エフェクトプレハブの識別キー ("Thruster" 等)
     * @param parent 追従先の親GameObject (自機、敵戦闘機等)
     * @param localOffset 親ローカル空間での配置オフセット
     * @return 生成されたエフェクトGameObjectへの共有ポインタ
     */
    std::shared_ptr<GameObject> PlayAttachedEffect(
        const std::string& effectKey,
        std::shared_ptr<GameObject> parent,
        const Irufemi::Vector3& localOffset = {0.0f, 0.0f, 0.0f}
    );

private:
    static inline EffectManagerComponent* s_instance_ = nullptr; //!< 静的サービスロケータインスタンス
    // エディタから設定する、代表的なエフェクトのPrefabパス
    std::string hitEffectPath_ = "resources/prefabs/normal_attack_hit_effect.json";
    std::string dustEffectPath_ = "resources/prefabs/debris_dust_effect.json";
    std::string thrusterEffectPath_ = "resources/prefabs/player_thruster_effect.json";

    // 内部的にキーからパスを引くための辞書
    std::unordered_map<std::string, std::string> effectDictionary_;

    /**
     * @brief 指定したキーのエフェクトプールを取得またはオンデマンド生成する
     * @param effectKey エフェクトキー
     * @param prefabPath プレハブパス
     * @return エフェクトプールへの生ポインタ
     */
    ObjectPool<GameObject>* GetOrCreatePool(const std::string& effectKey, const std::string& prefabPath);

    int maxHitEffects_ = 50;
    int maxDustEffects_ = 50;
    float effectDuration_ = 2.0f; // エフェクトの生存時間
    std::unordered_map<std::string, std::unique_ptr<ObjectPool<GameObject>>> effectPools_;

    struct ActiveEffect {
        ObjectPool<GameObject>::Handle handle;
        float timer;
        std::string effectKey;
    };
    std::vector<ActiveEffect> activeEffects_;
};
