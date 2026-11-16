#include "Framework/Component/Logic/SpawnPointComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Core/Utility/Log.h"

namespace Irufemi {

void SpawnPointComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    // プロパティとしてエディタに公開（汎用名で登録）
    RegisterProperty("Group ID", &groupId_);
    RegisterProperty("Spawn Type", &spawnType_);
}

void SpawnPointComponent::Initialize() {
    // 依存関係をクリーンに保つため、マネージャー等への直接登録は行わず
    // 各システムが初期化時にシーンからこのコンポーネントを集めてキャッシュする手法をとる
}

nlohmann::json SpawnPointComponent::Serialize() {
    nlohmann::json j = Component::Serialize();
    // 新旧両方のフォーマットに対応できるようにキーを出力
    j["data"]["groupId"] = groupId_;
    j["data"]["spawnType"] = spawnType_;
    j["data"]["waveId"] = groupId_;      // 互換性維持
    j["data"]["enemyType"] = spawnType_; // 互換性維持
    return j;
}

void SpawnPointComponent::Deserialize(const nlohmann::json& json) {
    Component::Deserialize(json);
    if (json.contains("data")) {
        const auto& data = json["data"];
        // groupId (フォールバック: waveId)
        if (data.contains("groupId")) {
            groupId_ = data["groupId"].get<std::string>();
        } else if (data.contains("waveId")) {
            groupId_ = data["waveId"].get<std::string>();
        }
        // spawnType (フォールバック: enemyType)
        if (data.contains("spawnType")) {
            spawnType_ = data["spawnType"].get<std::string>();
        } else if (data.contains("enemyType")) {
            spawnType_ = data["enemyType"].get<std::string>();
        }
    }
}

} // namespace Irufemi
