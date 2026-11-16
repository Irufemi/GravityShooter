#include "Renderer/PostProcess/PostProcessDefinitions.h"
#include "PostProcessSettings.h"
#include "Renderer/PostProcess/Effects/BloomEffect.h"
#include "Renderer/PostProcess/Effects/CombinedPostEffect.h"

namespace Irufemi {

// ---------------------------------------------------------
// Bloom Settings
// ---------------------------------------------------------
void BloomSettings::ApplyToManager(PostProcessManager* manager) {
    if (!manager) {
        return;
    }
    if (enabled) {
        if (!manager->HasActiveMode(PostProcessMode::Bloom)) {
            manager->AddActiveMode(PostProcessMode::Bloom);
        }
        if (auto* bloom = manager->GetEffect<Irufemi::BloomEffect>()) {
            bloom->GetParams().intensity = intensity;
            bloom->GetParams().threshold = threshold;
            bloom->GetParams().sigma = sigma;
            bloom->SetParams(bloom->GetParams());
        }
    } else {
        manager->RemoveActiveMode(PostProcessMode::Bloom);
    }
}

void BloomSettings::Serialize(nlohmann::json& j) const {
    j["type"] = "Bloom";
    j["enabled"] = enabled;
    j["intensity"] = intensity;
    j["threshold"] = threshold;
    j["sigma"] = sigma;
}

void BloomSettings::Deserialize(const nlohmann::json& j) {
    if (j.contains("enabled")) {
        enabled = j["enabled"];
    }
    if (j.contains("intensity")) {
        intensity = j["intensity"];
    }
    if (j.contains("threshold")) {
        threshold = j["threshold"];
    }
    if (j.contains("sigma")) {
        sigma = j["sigma"];
    }
}

std::shared_ptr<IPostProcessSettings> BloomSettings::Clone() const {
    return std::make_shared<BloomSettings>(*this);
}

// ---------------------------------------------------------
// Color Grading Settings
// ---------------------------------------------------------
void ColorGradingSettings::ApplyToManager(PostProcessManager* manager) {
    if (!manager) {
        return;
    }
    if (enabled) {
        if (!manager->HasActiveMode(PostProcessMode::ToneMapping)) {
            manager->AddActiveMode(PostProcessMode::ToneMapping);
        }
        if (!manager->HasActiveMode(PostProcessMode::HSV)) {
            manager->AddActiveMode(PostProcessMode::HSV);
        }

        if (auto* combined = manager->GetEffect<Irufemi::CombinedPostEffect>()) {
            combined->GetToneMappingParams().exposure = exposure;
            combined->GetHSVParams().hue = hue;
            combined->GetHSVParams().saturation = saturation;
            combined->GetHSVParams().value = value;
        }
    } else {
        manager->RemoveActiveMode(PostProcessMode::ToneMapping);
        manager->RemoveActiveMode(PostProcessMode::HSV);
    }
}

void ColorGradingSettings::Serialize(nlohmann::json& j) const {
    j["type"] = "ColorGrading";
    j["enabled"] = enabled;
    j["exposure"] = exposure;
    j["hue"] = hue;
    j["saturation"] = saturation;
    j["value"] = value;
}

void ColorGradingSettings::Deserialize(const nlohmann::json& j) {
    if (j.contains("enabled")) {
        enabled = j["enabled"];
    }
    if (j.contains("exposure")) {
        exposure = j["exposure"];
    }
    if (j.contains("hue")) {
        hue = j["hue"];
    }
    if (j.contains("saturation")) {
        saturation = j["saturation"];
    }
    if (j.contains("value")) {
        value = j["value"];
    }
}

std::shared_ptr<IPostProcessSettings> ColorGradingSettings::Clone() const {
    return std::make_shared<ColorGradingSettings>(*this);
}

// ---------------------------------------------------------
// Vignette Settings
// ---------------------------------------------------------
void VignetteSettings::ApplyToManager(PostProcessManager* manager) {
    if (!manager) {
        return;
    }
    if (enabled) {
        if (!manager->HasActiveMode(PostProcessMode::Vignette)) {
            manager->AddActiveMode(PostProcessMode::Vignette);
        }
        if (auto* combined = manager->GetEffect<Irufemi::CombinedPostEffect>()) {
            combined->GetVignetteParams().radius = radius;
            combined->GetVignetteParams().softness = softness;
        }
    } else {
        manager->RemoveActiveMode(PostProcessMode::Vignette);
    }
}

void VignetteSettings::Serialize(nlohmann::json& j) const {
    j["type"] = "Vignette";
    j["enabled"] = enabled;
    j["radius"] = radius;
    j["softness"] = softness;
}

void VignetteSettings::Deserialize(const nlohmann::json& j) {
    if (j.contains("enabled")) {
        enabled = j["enabled"];
    }
    if (j.contains("radius")) {
        radius = j["radius"];
    }
    if (j.contains("softness")) {
        softness = j["softness"];
    }
}

std::shared_ptr<IPostProcessSettings> VignetteSettings::Clone() const {
    return std::make_shared<VignetteSettings>(*this);
}

// ---------------------------------------------------------
// Outline Settings
// ---------------------------------------------------------
void OutlineSettings::ApplyToManager(PostProcessManager* manager) {
    if (!manager) {
        return;
    }
    if (enabled) {
        if (!manager->HasActiveMode(PostProcessMode::DepthBasedOutline)) {
            manager->AddActiveMode(PostProcessMode::DepthBasedOutline);
        }
        if (auto* combined = manager->GetEffect<Irufemi::CombinedPostEffect>()) {
            combined->GetOutlineParams().intensity = intensity;
            combined->GetOutlineParams().maskMaxRadius = maskMaxRadius;
        }
    } else {
        manager->RemoveActiveMode(PostProcessMode::DepthBasedOutline);
    }
}

void OutlineSettings::Serialize(nlohmann::json& j) const {
    j["type"] = "Outline";
    j["enabled"] = enabled;
    j["intensity"] = intensity;
    j["maskMaxRadius"] = maskMaxRadius;
}

void OutlineSettings::Deserialize(const nlohmann::json& j) {
    if (j.contains("enabled")) {
        enabled = j["enabled"];
    }
    if (j.contains("intensity")) {
        intensity = j["intensity"];
    }
    if (j.contains("maskMaxRadius")) {
        maskMaxRadius = j["maskMaxRadius"];
    }
}

std::shared_ptr<IPostProcessSettings> OutlineSettings::Clone() const {
    return std::make_shared<OutlineSettings>(*this);
}

// ---------------------------------------------------------
// BandeDessineeOutlineSettings Implementation
// ---------------------------------------------------------
void BandeDessineeOutlineSettings::ApplyToManager(PostProcessManager* manager) {
    if (!manager) {
        return;
    }
    if (enabled) {
        if (!manager->HasActiveMode(PostProcessMode::BandeDessineeOutline)) {
            manager->AddActiveMode(PostProcessMode::BandeDessineeOutline);
        }
        if (auto* combined = manager->GetEffect<Irufemi::CombinedPostEffect>()) {
            auto& p = combined->GetBandeDessineeOutlineParams();
            p.inkColor = inkColor;
            p.intensity = intensity;
            p.depthWeight = depthWeight;
            p.normalWeight = normalWeight;
            p.distanceFadeStart = distanceFadeStart;
            p.distanceFadeEnd = distanceFadeEnd;
        }
    } else {
        manager->RemoveActiveMode(PostProcessMode::BandeDessineeOutline);
    }
}

void BandeDessineeOutlineSettings::Serialize(nlohmann::json& j) const {
    j["type"] = "BandeDessineeOutline";
    j["enabled"] = enabled;
    j["inkColor"] = {inkColor.x, inkColor.y, inkColor.z, inkColor.w};
    j["intensity"] = intensity;
    j["depthWeight"] = depthWeight;
    j["normalWeight"] = normalWeight;
    j["distanceFadeStart"] = distanceFadeStart;
    j["distanceFadeEnd"] = distanceFadeEnd;
}

void BandeDessineeOutlineSettings::Deserialize(const nlohmann::json& j) {
    if (j.contains("enabled")) {
        enabled = j["enabled"];
    }
    if (j.contains("inkColor") && j["inkColor"].is_array() && j["inkColor"].size() >= 4) {
        inkColor = {j["inkColor"][0], j["inkColor"][1], j["inkColor"][2], j["inkColor"][3]};
    }
    if (j.contains("intensity")) {
        intensity = j["intensity"];
    }
    if (j.contains("depthWeight")) {
        depthWeight = j["depthWeight"];
    }
    if (j.contains("normalWeight")) {
        normalWeight = j["normalWeight"];
    }
    if (j.contains("distanceFadeStart")) {
        distanceFadeStart = j["distanceFadeStart"];
    }
    if (j.contains("distanceFadeEnd")) {
        distanceFadeEnd = j["distanceFadeEnd"];
    }
}

std::shared_ptr<IPostProcessSettings> BandeDessineeOutlineSettings::Clone() const {
    return std::make_shared<BandeDessineeOutlineSettings>(*this);
}

// ---------------------------------------------------------
// Factory
// ---------------------------------------------------------
std::shared_ptr<IPostProcessSettings> PostProcessSettingsFactory::Create(PostProcessMode mode) {
    switch (mode) {
    case PostProcessMode::Bloom:
        return std::make_shared<BloomSettings>();
    case PostProcessMode::ToneMapping:
        return std::make_shared<ColorGradingSettings>(); // ColorGrading covers ToneMapping and HSV
    case PostProcessMode::Vignette:
        return std::make_shared<VignetteSettings>();
    case PostProcessMode::DepthBasedOutline:
        return std::make_shared<OutlineSettings>();
    case PostProcessMode::BandeDessineeOutline:
        return std::make_shared<BandeDessineeOutlineSettings>();
    // Add more effects here in the future
    default:
        return nullptr;
    }
}

std::shared_ptr<IPostProcessSettings> PostProcessSettingsFactory::Create(const std::string& typeName) {
    if (typeName == "Bloom") {
        return Create(PostProcessMode::Bloom);
    }
    if (typeName == "ColorGrading" || typeName == "ToneMapping") {
        return Create(PostProcessMode::ToneMapping);
    }
    if (typeName == "Vignette") {
        return Create(PostProcessMode::Vignette);
    }
    if (typeName == "Outline" || typeName == "DepthBasedOutline") {
        return Create(PostProcessMode::DepthBasedOutline);
    }
    if (typeName == "BandeDessineeOutline") {
        return Create(PostProcessMode::BandeDessineeOutline);
    }

    // SSOT マクロによるエフェクト名・DisplayName からの動的逆引き
#define IRUFEMI_MATCH_SETTINGS_MODE(ModeName, StructType, PassType, DefaultLayer, DisplayName)                         \
    if (typeName == #ModeName || typeName == DisplayName) {                                                            \
        return Create(PostProcessMode::ModeName);                                                                      \
    }
    IRUFEMI_POSTPROCESS_EFFECT_LIST(IRUFEMI_MATCH_SETTINGS_MODE)
#undef IRUFEMI_MATCH_SETTINGS_MODE

    return nullptr;
}

} // namespace Irufemi
