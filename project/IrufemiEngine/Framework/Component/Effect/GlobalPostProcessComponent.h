#pragma once
#include "Framework/Component/Component.h"
#include "PostProcessSettings.h"
#include <vector>
#include <memory>

/**
 * @class GlobalPostProcessComponent
 * @brief 動的な Post Process Volume (Volume Framework 互換)
 */
class GlobalPostProcessComponent : public Component {
public:
    GlobalPostProcessComponent() = default;
    ~GlobalPostProcessComponent() override = default;

    void Start() override;
    void Update() override;
    void OnDisable() override;
    void OnDestroy() override;
    void OnRegisterProperties() override;

    bool CanUpdateInEditMode() const override {
        return true;
    }
    std::string GetComponentName() const override {
        return "GlobalPostProcessComponent";
    }
    std::shared_ptr<Component> Clone() override;

    // JSON Serialization
    nlohmann::json Serialize() override;
    void Deserialize(const nlohmann::json& j) override;

    // 内部設定リストへのアクセス（エディタ用）
    const std::vector<std::shared_ptr<IPostProcessSettings>>& GetOverrides() const {
        return overrides_;
    }
    void AddOverride(std::shared_ptr<IPostProcessSettings> setting) {
        overrides_.push_back(setting);
    }
    /**
     * @brief 指定インデックスに設定を挿入する（エディタのUndo/Redo用）
     */
    void InsertOverride(size_t index, std::shared_ptr<IPostProcessSettings> setting) {
        if (index <= overrides_.size()) {
            overrides_.insert(overrides_.begin() + index, setting);
        } else {
            overrides_.push_back(setting);
        }
    }
    void RemoveOverride(size_t index) {
        if (index < overrides_.size()) {
            overrides_.erase(overrides_.begin() + index);
        }
    }

private:
    std::vector<std::shared_ptr<IPostProcessSettings>> overrides_;
};
