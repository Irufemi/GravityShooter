#include "Framework/Component/Effect/FogComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/DrawManager.h"
#include "Renderer/Data/FogParams.h"

void FogComponent::OnRegisterProperties() {
    RegisterProperty("Enabled", &enabled_).OnChanged([this]() { SyncToEngine(); });
    RegisterProperty("Color", &fogColor_).OnChanged([this]() { SyncToEngine(); });
    RegisterProperty("Start Distance", &fogStart_).SetMinMax(0.0f, 5000.0f).OnChanged([this]() { SyncToEngine(); });
    RegisterProperty("End Distance", &fogEnd_).SetMinMax(0.0f, 10000.0f).OnChanged([this]() { SyncToEngine(); });
    RegisterProperty("Density", &fogDensity_).SetMinMax(0.0f, 5.0f).OnChanged([this]() { SyncToEngine(); });
    RegisterProperty("Type (0:Lin, 1:Exp)", &fogType_).SetMinMax(0.0f, 1.0f).OnChanged([this]() { SyncToEngine(); });
}

void FogComponent::Start() {
    SyncToEngine();
}

void FogComponent::Update() {
    SyncToEngine();
}

void FogComponent::OnDisable() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    auto dm = engine->GetDrawManager();
    if (!dm) {
        return;
    }
    FogParams params = dm->GetFogParams();
    params.enabled = 0;
    dm->SetFogParams(params);
}

void FogComponent::OnDestroy() {
    OnDisable();
}

void FogComponent::SyncToEngine() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    auto dm = engine->GetDrawManager();
    if (!dm) {
        return;
    }

    FogParams params{};
    params.fogColor = fogColor_;
    params.fogStart = fogStart_;
    params.fogEnd = fogEnd_;
    params.fogDensity = fogDensity_;
    params.fogType = static_cast<uint32_t>(fogType_);
    params.enabled = enabled_ ? 1 : 0;

    dm->SetFogParams(params);
}

std::shared_ptr<Component> FogComponent::Clone() {
    auto clone = std::make_shared<FogComponent>();
    clone->fogColor_ = fogColor_;
    clone->fogStart_ = fogStart_;
    clone->fogEnd_ = fogEnd_;
    clone->fogDensity_ = fogDensity_;
    clone->fogType_ = fogType_;
    clone->enabled_ = enabled_;
    return clone;
}

nlohmann::json FogComponent::Serialize() {
    nlohmann::json j = Component::Serialize();
    j["enabled"] = enabled_;
    j["fogColor"] = {fogColor_.x, fogColor_.y, fogColor_.z};
    j["fogStart"] = fogStart_;
    j["fogEnd"] = fogEnd_;
    j["fogDensity"] = fogDensity_;
    j["fogType"] = fogType_;
    return j;
}

void FogComponent::Deserialize(const nlohmann::json& j) {
    Component::Deserialize(j);
    if (j.contains("enabled")) {
        enabled_ = j["enabled"].get<bool>();
    }
    if (j.contains("fogColor") && j["fogColor"].is_array() && j["fogColor"].size() >= 3) {
        fogColor_.x = j["fogColor"][0].get<float>();
        fogColor_.y = j["fogColor"][1].get<float>();
        fogColor_.z = j["fogColor"][2].get<float>();
    }
    if (j.contains("fogStart")) {
        fogStart_ = j["fogStart"].get<float>();
    }
    if (j.contains("fogEnd")) {
        fogEnd_ = j["fogEnd"].get<float>();
    }
    if (j.contains("fogDensity")) {
        fogDensity_ = j["fogDensity"].get<float>();
    }
    if (j.contains("fogType")) {
        fogType_ = j["fogType"].get<int>();
    }
}
