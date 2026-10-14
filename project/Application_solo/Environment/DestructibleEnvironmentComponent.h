#pragma once
#include "Framework/Component/Component.h"
#include "Combat/IDamageable.h"
#include <string>

class DebrisManagerComponent;
class EffectManagerComponent;

/**
 * @class DestructibleEnvironmentComponent
 * @brief 破壊可能な環境物（ビルや壁など）にアタッチされるコンポーネント。
 * @details
 * EnvironmentManagerComponentによって動的に付与され、瓦礫がヒットした際のダメージ判定と破壊イベント（瓦礫の飛散）を管理します。
 */
class DestructibleEnvironmentComponent : public Component, public IDamageable {
public:
    DestructibleEnvironmentComponent() = default;
    ~DestructibleEnvironmentComponent() override = default;

    void Initialize() override;
    void Start() override;

    std::string GetComponentName() const override {
        return "DestructibleEnvironmentComponent";
    }

    /**
     * @brief 破壊時にスポーンする破片の数を設定する
     * @param[in] count 破片の個数
     */
    void SetDebrisSpawnCount(int count) {
        debrisSpawnCount_ = count;
    }
    /**
     * @brief 破壊時にスポーンする破片の数を取得する
     * @return 破片の個数
     */
    int GetDebrisSpawnCount() const {
        return debrisSpawnCount_;
    }

    // ダメージ処理
    void TakeDamage(float damage) override {
        int intDamage = CalculateIntegerDamage(damage);
        if (intDamage > 0) {
            TakeDamage(intDamage);
        }
    }
    void TakeDamage(int damage);

    DamageableType GetDamageableType() const override {
        return DamageableType::Environment;
    }

private:
    /**
     * @brief 破片生成マネージャーを取得する（キャッシュ付き）
     */
    DebrisManagerComponent* GetDebrisManager();

    /**
     * @brief 破壊エフェクトマネージャーを取得する（キャッシュ付き）
     */
    EffectManagerComponent* GetEffectManager();

    int hp_ = 1;
    int debrisSpawnCount_ = 3;

    DebrisManagerComponent* debrisManager_ = nullptr; ///< 破片生成マネージャーへの参照
    /**
     * @brief 破壊エフェクトを再生するためのエフェクトマネージャーへの参照
     */
    EffectManagerComponent* effectManager_ = nullptr;
};
