#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include <memory>
#include <vector>
#include <queue>
#include <string>

class GameObject;
class VirtualEntityManagerComponent;
class DebrisComponent;
enum class DebrisState;

// がれきのアニメーション用並行データ（Data-Oriented Parallel Array）
struct DebrisAnimData {
    float baseIdleY = 0.0f;
    float idleTimeY = 0.0f;
};

/**
 * @class DebrisManagerComponent
 * @brief ガレキの生成・プール管理・検索を行うコンポーネント
 */
class DebrisManagerComponent : public Component {
public:
    DebrisManagerComponent() = default;
    ~DebrisManagerComponent() override = default;

    void Initialize() override;
    void Update() override;
    void OnRegisterProperties() override;
    std::string GetComponentName() const override {
        return "DebrisManagerComponent";
    }

    /**
     * @brief プールからガレキを1つ取り出す
     */
    std::shared_ptr<GameObject> GetDebris();

    /**
     * @brief ガレキをプールに返却する（即時）
     */
    void ReleaseDebris(std::shared_ptr<GameObject> debris);

    /**
     * @brief ガレキをプール返却キューに積む（Update中の安全な削除用）
     */
    void MarkForRelease(std::shared_ptr<GameObject> debris);

    /**
     * @brief 仮想ガレキデータの完全削除をキューに積む（Update中の安全な削除用）
     */
    void MarkForDestroy(int virtualId, int variationIndex);

    void RegisterDebris(DebrisComponent* debris, DebrisState state);
    void UnregisterDebris(DebrisComponent* debris, DebrisState state);

    // プレイヤーからの引き寄せ処理用：指定座標から一番近い未昇格のがれきを実体化して返す
    std::shared_ptr<GameObject> ExtractNearestIdleDebris(const Irufemi::Vector3& pos, float radius);

    // 破壊通知
    void NotifyDestroyed(int virtualId, int variationIndex);

    /**
     * @brief プール枯渇時用：一番遠いIdle状態のがれきを強制降格して枠を空ける
     * @param fromPos 基準となる座標
     * @return 降格に成功した場合は true
     */
    bool DemoteFarthestIdleDebris(const Irufemi::Vector3& fromPos);

    /**
     * @brief 指定座標の周辺にガレキ（破片）をまとめてスポーンする（敵撃破時・ボス被弾時用）
     * @param centerPos スポーンの中心座標（ワールド座標）
     * @param count 生成する個数
     * @param spreadRadius 飛び散る半径
     * @param specificVariationId 特定のバリエーションIDを指定して生成する場合（空文字列の場合は通常ランダム）
     */
    void SpawnDebrisCluster(const Irufemi::Vector3& centerPos, int count = 3, float spreadRadius = 4.0f,
                            const std::string& specificVariationId = "");

    /**
     * @brief 敵撃破時用：プレイヤー方向への手前バイアスを持ってガレキを散乱スポーンする
     * @param origin 敵撃破位置（ワールド座標）
     * @param targetPlayerPos プレイヤー機体の現在位置
     * @param count 生成する個数
     * @param spreadRadius 飛び散り半径
     * @param forwardBias プレイヤー方向（手前）へ寄せる最大距離 (m)
     * @param specificVariationId 特定のバリエーションIDを指定して生成する場合（空文字列の場合は通常ランダム）
     */
    void SpawnDebrisBurst(const Irufemi::Vector3& origin, const Irufemi::Vector3& targetPlayerPos, int count = 2,
                          float spreadRadius = 3.5f, float forwardBias = 10.0f,
                          const std::string& specificVariationId = "");

    /**
     * @brief 自機の前方にガレキを一定個数スポーンする
     * @param count 生成する個数
     */
    void SpawnDebrisInFrontOfPlayer(int count);

    // Debris パラメータのゲッター
    float GetDebrisPullSpeed() const {
        return debrisPullSpeed_;
    }
    float GetDebrisThrowSpeed() const {
        return debrisThrowSpeed_;
    }
    float GetDebrisOrbitSpeed() const {
        return debrisOrbitSpeed_;
    }
    float GetDebrisOrbitRadius() const {
        return debrisOrbitRadius_;
    }
    float GetDebrisDamage() const {
        return debrisDamage_;
    }
    float GetDebrisEnemyDamage() const {
        return debrisEnemyDamage_;
    }
    float GetDebrisPullYOffset() const {
        return debrisPullYOffset_;
    }
    float GetCameraShakeIntensity() const {
        return cameraShakeIntensity_;
    }
    int GetCameraShakeDurationFrames() const {
        return cameraShakeDurationFrames_;
    }
    Irufemi::Vector4 GetPlayerAuraColor() const {
        return playerAuraColor_;
    }
    Irufemi::Vector4 GetBossAuraColor() const {
        return bossAuraColor_;
    }
    Irufemi::Vector4 GetIdleAuraColor() const {
        return idleAuraColor_;
    }
    float GetCatchDistanceSq() const {
        return catchDistanceSq_;
    }
    float GetBossShieldRadius() const {
        return bossShieldRadius_;
    }

    Irufemi::Vector3 GetDebrisBaseScale() const {
        return debrisBaseScale_;
    }
    float GetColliderRadius() const {
        return colliderRadius_;
    }
    Irufemi::Vector3 GetAuraScale() const {
        return auraScale_;
    }
    float GetMaxThrowDistanceSq() const {
        return maxThrowDistance_ * maxThrowDistance_;
    }

private:
    // --- Data-Driven Variations ---
    struct DebrisVariation {
        std::string id;
        std::string modelPath;
        int maxVirtualCount = 0;
        int maxPoolSize = 0;
        int spawnWeight = 1;
        VirtualEntityManagerComponent* virtualManager = nullptr;
        std::vector<DebrisAnimData> animDataList;
        std::queue<int> activeIds;
        std::shared_ptr<GameObject> poolObject;
    };
    std::vector<DebrisVariation> variations_;

    // --- Debris Settings ---
    float debrisPullSpeed_ = 10.0f;
    float debrisThrowSpeed_ = 50.0f;
    float debrisOrbitSpeed_ = 2.0f;
    float debrisOrbitRadius_ = 2.0f;
    float debrisDamage_ = 10.0f;
    float debrisEnemyDamage_ = 100.0f;
    float debrisPullYOffset_ = 2.0f;
    float cameraShakeIntensity_ = 0.5f;
    int cameraShakeDurationFrames_ = 10;
    Irufemi::Vector4 playerAuraColor_ = {0.0f, 0.8f, 1.0f, 0.4f};
    Irufemi::Vector4 bossAuraColor_ = {0.8f, 0.0f, 0.6f, 0.4f};
    Irufemi::Vector4 idleAuraColor_ = {0.6f, 0.2f, 1.0f, 0.4f};
    float catchDistanceSq_ = 2.0f;
    float bossShieldRadius_ = 8.0f;

    Irufemi::Vector3 debrisBaseScale_ = {0.5f, 0.5f, 0.5f};
    float colliderRadius_ = 0.5f;
    Irufemi::Vector3 auraScale_ = {2.2f, 2.2f, 2.2f};
    float maxThrowDistance_ = 1500.0f;

    // --- Auto Supply Settings ---
    bool autoSupplyEnabled_ = true;       ///< 前方ガレキの自動補充を有効にするか
    int targetFieldCount_ = 30;           ///< 自機前方に維持する目標ガレキ数
    float supplyCheckInterval_ = 0.5f;    ///< 補充チェック間隔（秒）
    float supplyTimer_ = 0.0f;            ///< 補充タイマー
    float recycleBehindDistance_ = 25.0f; ///< 自機後方何メートルで回収するか

    std::vector<std::shared_ptr<GameObject>> pendingReleases_;
    std::vector<std::pair<int, int>> pendingDestroys_;

    void UpdatePulledDebris(float deltaTime);
    void UpdateOrbitingDebris(float deltaTime);
    void UpdateBossOrbitingDebris(float deltaTime);
    void UpdateThrownDebris(float deltaTime);

    std::vector<DebrisComponent*> pulledDebris_;
    std::vector<DebrisComponent*> orbitingDebris_;
    std::vector<DebrisComponent*> bossOrbitingDebris_;
    std::vector<DebrisComponent*> thrownDebris_;
    std::vector<DebrisComponent*> activeIdleDebris_; ///< 実体化されているIdle状態のがれきリスト
};
