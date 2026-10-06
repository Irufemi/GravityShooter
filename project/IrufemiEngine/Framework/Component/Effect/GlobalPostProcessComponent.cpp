#include "Framework/Component/Effect/GlobalPostProcessComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"

void GlobalPostProcessComponent::OnRegisterProperties() {
    // GUIはGlobalPostProcessComponentEditorによって描画されるため、
    // RegisterPropertyは使用しない。
}

void GlobalPostProcessComponent::Start() {
    auto engine = GetEngine();
    if (engine) {
        if (auto pp = engine->GetPostProcessManager()) {
            pp->Reset(false); // 遷移フェード(PostUI)を維持しつつ環境エフェクトスタックをリセット
        }
    }
    Update();
}

void GlobalPostProcessComponent::Update() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    auto pp = engine->GetPostProcessManager();
    if (!pp) {
        return;
    }

    for (auto& setting : overrides_) {
        setting->ApplyToManager(pp);
    }
}

void GlobalPostProcessComponent::OnDisable() {
    auto engine = GetEngine();
    if (engine) {
        if (auto pp = engine->GetPostProcessManager()) {
            pp->Reset(false);
        }
    }
}

void GlobalPostProcessComponent::OnDestroy() {
    OnDisable();
}

std::shared_ptr<Component> GlobalPostProcessComponent::Clone() {
    auto clone = std::make_shared<GlobalPostProcessComponent>();
    for (const auto& setting : overrides_) {
        clone->AddOverride(setting->Clone());
    }
    return clone;
}

nlohmann::json GlobalPostProcessComponent::Serialize() {
    nlohmann::json j = Component::Serialize();
    nlohmann::json jOverrides = nlohmann::json::array();

    for (const auto& setting : overrides_) {
        nlohmann::json jOverride = nlohmann::json::object();
        setting->Serialize(jOverride);
        jOverrides.push_back(jOverride);
    }

    j["overrides"] = jOverrides;
    return j;
}

void GlobalPostProcessComponent::Deserialize(const nlohmann::json& j) {
    Component::Deserialize(j);

    overrides_.clear();

    if (j.contains("overrides") && j["overrides"].is_array()) {
        for (const auto& jOverride : j["overrides"]) {
            if (jOverride.contains("type")) {
                std::string type = jOverride["type"].get<std::string>();
                std::shared_ptr<IPostProcessSettings> setting = nullptr;

                if (type == "Bloom") {
                    setting = PostProcessSettingsFactory::Create(PostProcessMode::Bloom);
                } else if (type == "ColorGrading") {
                    setting = PostProcessSettingsFactory::Create(PostProcessMode::ToneMapping);
                } else if (type == "Vignette") {
                    setting = PostProcessSettingsFactory::Create(PostProcessMode::Vignette);
                }
                // 新しいエフェクトはここに追記していく

                if (setting) {
                    setting->Deserialize(jOverride);
                    overrides_.push_back(setting);
                }
            }
        }
    }
}
