#pragma once
#include "Framework/Component/Component.h"
#include <string>

namespace Irufemi {

/**
 * @class SpawnPointComponent
 * @brief オブジェクトの生成位置（起点マーカー）を示す汎用コンポーネント。
 * @details グループ識別子（Group ID）や生成種別（Spawn
 * Type）を保持し、レベルデザインや敵・アイテムスポーン制御に利用します。
 */
class SpawnPointComponent : public Component {
public:
    SpawnPointComponent() = default;
    ~SpawnPointComponent() override = default;

    void OnRegisterProperties() override;

    std::string GetComponentName() const override {
        return "SpawnPointComponent";
    }
    void Initialize() override;

    nlohmann::json Serialize() override;
    void Deserialize(const nlohmann::json& json) override;

    // --- 汎用アクセサ ---
    const std::string& GetGroupId() const {
        return groupId_;
    }
    void SetGroupId(const std::string& id) {
        groupId_ = id;
    }

    const std::string& GetSpawnType() const {
        return spawnType_;
    }
    void SetSpawnType(const std::string& type) {
        spawnType_ = type;
    }

    // --- 後方互換性用エイリアス (Wave/Enemy仕様) ---
    const std::string& GetWaveId() const {
        return groupId_;
    }
    void SetWaveId(const std::string& id) {
        groupId_ = id;
    }

    const std::string& GetEnemyType() const {
        return spawnType_;
    }
    void SetEnemyType(const std::string& type) {
        spawnType_ = type;
    }

private:
    std::string groupId_ = "Wave1";          ///< スポーンのグループ・所属識別子（旧 waveId）
    std::string spawnType_ = "DefaultEnemy"; ///< 生成対象の種別・プレハブ名（旧 enemyType）
};

} // namespace Irufemi
