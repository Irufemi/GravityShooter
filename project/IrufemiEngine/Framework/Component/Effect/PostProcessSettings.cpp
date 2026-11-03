#include "PostProcessSettings.h"
#include "Renderer/PostProcess/Effects/BloomEffect.h"
#include "Renderer/PostProcess/Effects/CombinedPostEffect.h"

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
    // Add more effects here in the future
    default:
        return nullptr;
    }
}
