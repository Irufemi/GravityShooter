#include "Framework/Component/Effect/ScreenEffectDrivers.h"
#include "Core/Utility/Ease.h"

// ============================================================================
// GlitchEffectDriver
// ============================================================================
void GlitchEffectDriver::CacheBaseParams(PostProcessManager* ppm) {
    if (ppm) {
        baseParams_ = ppm->GetGlitchParams();
    }
}

void GlitchEffectDriver::ApplyInterpolation(PostProcessManager* ppm, float t) {
    if (!ppm) {
        return;
    }
    auto& params = ppm->GetGlitchParams();
    params.intensity = Lerp(baseParams_.intensity, targetParams_.intensity, t);
    params.edgeMaskStrength = targetParams_.edgeMaskStrength;
    params.probability = targetParams_.probability;
    params.blockSizeX = targetParams_.blockSizeX;
    params.blockSizeY = targetParams_.blockSizeY;
    params.offsetBase = targetParams_.offsetBase;
    params.offsetMax = targetParams_.offsetMax;
    params.rgbShiftBase = targetParams_.rgbShiftBase;
    params.rgbShiftMax = targetParams_.rgbShiftMax;
    params.scanlineFreq = targetParams_.scanlineFreq;
    params.scanlineIntensity = targetParams_.scanlineIntensity;
    params.color = Lerp(baseParams_.color, targetParams_.color, t);
}

void GlitchEffectDriver::RestoreBaseParams(PostProcessManager* ppm) {
    if (ppm) {
        ppm->SetGlitchParams(baseParams_);
    }
}

nlohmann::json GlitchEffectDriver::Serialize() const {
    nlohmann::json j;
    j["targetGlitchParams"]["intensity"] = targetParams_.intensity;
    j["targetGlitchParams"]["edgeMaskStrength"] = targetParams_.edgeMaskStrength;
    j["targetGlitchParams"]["probability"] = targetParams_.probability;
    j["targetGlitchParams"]["blockSizeX"] = targetParams_.blockSizeX;
    j["targetGlitchParams"]["blockSizeY"] = targetParams_.blockSizeY;
    j["targetGlitchParams"]["offsetBase"] = targetParams_.offsetBase;
    j["targetGlitchParams"]["offsetMax"] = targetParams_.offsetMax;
    j["targetGlitchParams"]["rgbShiftBase"] = targetParams_.rgbShiftBase;
    j["targetGlitchParams"]["rgbShiftMax"] = targetParams_.rgbShiftMax;
    j["targetGlitchParams"]["scanlineFreq"] = targetParams_.scanlineFreq;
    j["targetGlitchParams"]["scanlineIntensity"] = targetParams_.scanlineIntensity;
    j["targetGlitchParams"]["glitchColor"] = {targetParams_.color.x, targetParams_.color.y, targetParams_.color.z,
                                              targetParams_.color.w};
    return j;
}

void GlitchEffectDriver::Deserialize(const nlohmann::json& j) {
    if (j.contains("targetGlitchParams")) {
        const auto& gj = j["targetGlitchParams"];
        if (gj.contains("intensity")) {
            targetParams_.intensity = gj["intensity"];
        }
        if (gj.contains("edgeMaskStrength")) {
            targetParams_.edgeMaskStrength = gj["edgeMaskStrength"];
        }
        if (gj.contains("probability")) {
            targetParams_.probability = gj["probability"];
        }
        if (gj.contains("blockSizeX")) {
            targetParams_.blockSizeX = gj["blockSizeX"];
        }
        if (gj.contains("blockSizeY")) {
            targetParams_.blockSizeY = gj["blockSizeY"];
        }
        if (gj.contains("offsetBase")) {
            targetParams_.offsetBase = gj["offsetBase"];
        }
        if (gj.contains("offsetMax")) {
            targetParams_.offsetMax = gj["offsetMax"];
        }
        if (gj.contains("rgbShiftBase")) {
            targetParams_.rgbShiftBase = gj["rgbShiftBase"];
        }
        if (gj.contains("rgbShiftMax")) {
            targetParams_.rgbShiftMax = gj["rgbShiftMax"];
        }
        if (gj.contains("scanlineFreq")) {
            targetParams_.scanlineFreq = gj["scanlineFreq"];
        }
        if (gj.contains("scanlineIntensity")) {
            targetParams_.scanlineIntensity = gj["scanlineIntensity"];
        }
        if (gj.contains("glitchColor")) {
            const auto& c = gj["glitchColor"];
            targetParams_.color = {c[0], c[1], c[2], c[3]};
        }
    }
}

std::unique_ptr<IScreenEffectDriver> GlitchEffectDriver::Clone() const {
    auto clone = std::make_unique<GlitchEffectDriver>();
    clone->baseParams_ = this->baseParams_;
    clone->targetParams_ = this->targetParams_;
    return clone;
}

// ============================================================================
// VignetteEffectDriver
// ============================================================================
void VignetteEffectDriver::CacheBaseParams(PostProcessManager* ppm) {
    if (ppm) {
        baseParams_ = ppm->GetVignetteParams();
    }
}

void VignetteEffectDriver::ApplyInterpolation(PostProcessManager* ppm, float t) {
    if (!ppm) {
        return;
    }
    auto& params = ppm->GetVignetteParams();
    params.radius = Lerp(baseParams_.radius, targetParams_.radius, t);
    params.softness = Lerp(baseParams_.softness, targetParams_.softness, t);
    params.color = Lerp(baseParams_.color, targetParams_.color, t);
}

void VignetteEffectDriver::RestoreBaseParams(PostProcessManager* ppm) {
    if (ppm) {
        ppm->SetVignetteParams(baseParams_);
    }
}

nlohmann::json VignetteEffectDriver::Serialize() const {
    nlohmann::json j;
    j["targetVignetteParams"]["color"] = {targetParams_.color.x, targetParams_.color.y, targetParams_.color.z,
                                          targetParams_.color.w};
    j["targetVignetteParams"]["radius"] = targetParams_.radius;
    j["targetVignetteParams"]["softness"] = targetParams_.softness;
    return j;
}

void VignetteEffectDriver::Deserialize(const nlohmann::json& j) {
    if (j.contains("targetVignetteParams")) {
        const auto& vj = j["targetVignetteParams"];
        if (vj.contains("color") && vj["color"].is_array() && vj["color"].size() == 4) {
            targetParams_.color.x = vj["color"][0];
            targetParams_.color.y = vj["color"][1];
            targetParams_.color.z = vj["color"][2];
            targetParams_.color.w = vj["color"][3];
        }
        if (vj.contains("radius")) {
            targetParams_.radius = vj["radius"];
        }
        if (vj.contains("softness")) {
            targetParams_.softness = vj["softness"];
        }
    }
}

std::unique_ptr<IScreenEffectDriver> VignetteEffectDriver::Clone() const {
    auto clone = std::make_unique<VignetteEffectDriver>();
    clone->baseParams_ = this->baseParams_;
    clone->targetParams_ = this->targetParams_;
    return clone;
}

// ============================================================================
// ChromaticAberrationEffectDriver
// ============================================================================
void ChromaticAberrationEffectDriver::CacheBaseParams(PostProcessManager* ppm) {
    if (ppm) {
        baseParams_ = ppm->GetChromaticAberrationParams();
    }
}

void ChromaticAberrationEffectDriver::ApplyInterpolation(PostProcessManager* ppm, float t) {
    if (!ppm) {
        return;
    }
    auto& params = ppm->GetChromaticAberrationParams();
    params.intensity = Lerp(baseParams_.intensity, targetParams_.intensity, t);
}

void ChromaticAberrationEffectDriver::RestoreBaseParams(PostProcessManager* ppm) {
    if (ppm) {
        ppm->SetChromaticAberrationParams(baseParams_);
    }
}

nlohmann::json ChromaticAberrationEffectDriver::Serialize() const {
    nlohmann::json j;
    j["targetChromaticAberrationParams"]["intensity"] = targetParams_.intensity;
    return j;
}

void ChromaticAberrationEffectDriver::Deserialize(const nlohmann::json& j) {
    if (j.contains("targetChromaticAberrationParams")) {
        const auto& cj = j["targetChromaticAberrationParams"];
        if (cj.contains("intensity")) {
            targetParams_.intensity = cj["intensity"];
        }
    }
}

std::unique_ptr<IScreenEffectDriver> ChromaticAberrationEffectDriver::Clone() const {
    auto clone = std::make_unique<ChromaticAberrationEffectDriver>();
    clone->baseParams_ = this->baseParams_;
    clone->targetParams_ = this->targetParams_;
    return clone;
}

// ============================================================================
// RadialBlurEffectDriver
// ============================================================================
void RadialBlurEffectDriver::CacheBaseParams(PostProcessManager* ppm) {
    if (ppm) {
        baseParams_ = ppm->GetRadialBlurParams();
    }
}

void RadialBlurEffectDriver::ApplyInterpolation(PostProcessManager* ppm, float t) {
    if (!ppm) {
        return;
    }
    auto& params = ppm->GetRadialBlurParams();
    params.blurWidth = Lerp(baseParams_.blurWidth, targetParams_.blurWidth, t);
    params.center = Lerp(baseParams_.center, targetParams_.center, t);
    params.numSamples = targetParams_.numSamples;
}

void RadialBlurEffectDriver::RestoreBaseParams(PostProcessManager* ppm) {
    if (ppm) {
        ppm->SetRadialBlurParams(baseParams_);
    }
}

nlohmann::json RadialBlurEffectDriver::Serialize() const {
    nlohmann::json j;
    j["targetRadialBlurParams"]["blurWidth"] = targetParams_.blurWidth;
    j["targetRadialBlurParams"]["center"] = {targetParams_.center.x, targetParams_.center.y};
    j["targetRadialBlurParams"]["numSamples"] = targetParams_.numSamples;
    return j;
}

void RadialBlurEffectDriver::Deserialize(const nlohmann::json& j) {
    if (j.contains("targetRadialBlurParams")) {
        const auto& rj = j["targetRadialBlurParams"];
        if (rj.contains("blurWidth")) {
            targetParams_.blurWidth = rj["blurWidth"];
        }
        if (rj.contains("center") && rj["center"].is_array() && rj["center"].size() == 2) {
            targetParams_.center.x = rj["center"][0];
            targetParams_.center.y = rj["center"][1];
        }
        if (rj.contains("numSamples")) {
            targetParams_.numSamples = rj["numSamples"];
        }
    }
}

std::unique_ptr<IScreenEffectDriver> RadialBlurEffectDriver::Clone() const {
    auto clone = std::make_unique<RadialBlurEffectDriver>();
    clone->baseParams_ = this->baseParams_;
    clone->targetParams_ = this->targetParams_;
    return clone;
}

// ============================================================================
// ScreenEffectDriverFactory (Registration-based Factory)
// ============================================================================
std::unique_ptr<IScreenEffectDriver> ScreenEffectDriverFactory::CreateDriver(PostProcessMode mode) {
    using DriverCreator = std::unique_ptr<IScreenEffectDriver> (*)();
    static const std::unordered_map<PostProcessMode, DriverCreator> kFactoryRegistry = {
        {PostProcessMode::Glitch,
         []() -> std::unique_ptr<IScreenEffectDriver> { return std::make_unique<GlitchEffectDriver>(); }},
        {PostProcessMode::Vignette,
         []() -> std::unique_ptr<IScreenEffectDriver> { return std::make_unique<VignetteEffectDriver>(); }},
        {PostProcessMode::ChromaticAberration,
         []() -> std::unique_ptr<IScreenEffectDriver> { return std::make_unique<ChromaticAberrationEffectDriver>(); }},
        {PostProcessMode::RadialBlur,
         []() -> std::unique_ptr<IScreenEffectDriver> { return std::make_unique<RadialBlurEffectDriver>(); }},
    };

    if (auto it = kFactoryRegistry.find(mode); it != kFactoryRegistry.end()) {
        return it->second();
    }
    return nullptr;
}
