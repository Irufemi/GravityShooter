#include "Framework/Component/Effect/ScreenEffectComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/Ease.h"
#include <algorithm>

ScreenEffectComponent::ScreenEffectComponent() {}

ScreenEffectComponent::~ScreenEffectComponent() {
    OnDestroy();
}

void ScreenEffectComponent::OnDestroy() {
    if (isPlaying_) {
        auto engine = GetEngine();
        if (engine && engine->GetPostProcessManager()) {
            engine->GetPostProcessManager()->RemoveActiveMode(mode_);
        }
        isPlaying_ = false;
    }
}

void ScreenEffectComponent::Initialize() {
    // 外部（JSONやエディタ）から設定された値を維持するため、ここでハードコードの上書きは行わない
}

void ScreenEffectComponent::Play() {
    auto engine = GetEngine();
    if (!engine || !engine->GetPostProcessManager()) {
        return;
    }

    if (!isBaseCached_) {
        // 現在の値をベース（基準）としてキャッシュする
        baseGlitchParams_ = engine->GetPostProcessManager()->GetGlitchParams();
        baseVignetteParams_ = engine->GetPostProcessManager()->GetVignetteParams();
        baseChromaticAberrationParams_ = engine->GetPostProcessManager()->GetChromaticAberrationParams();
        baseRadialBlurParams_ = engine->GetPostProcessManager()->GetRadialBlurParams();
        isBaseCached_ = true;
    }

    currentWeight_ = 1.0f;
    isPlaying_ = true;

    // UIより前にかけるか後に掛けるか。今回はPostUIレイヤーにする。
    wasModeActiveBeforePlay_ = engine->GetPostProcessManager()->HasActiveMode(mode_);
    if (!wasModeActiveBeforePlay_) {
        engine->GetPostProcessManager()->AddActiveMode(mode_, PostProcessManager::Layer::PostUI);
    }
}

void ScreenEffectComponent::Update() {
    if (!isPlaying_) {
        return;
    }

    auto engine = GetEngine();
    if (!engine || !engine->GetPostProcessManager()) {
        return;
    }

    float dt = engine->GetGameDeltaTime();
    currentWeight_ -= dt / duration_;

    if (currentWeight_ <= 0.0f) {
        currentWeight_ = 0.0f;
        isPlaying_ = false;

        RestoreBaseParams(engine->GetPostProcessManager());

        if (!wasModeActiveBeforePlay_) {
            engine->GetPostProcessManager()->RemoveActiveMode(mode_);
        }
        return;
    }

    // Weightに基づいたEaseOut補間処理
    float t = EaseOutQuad(currentWeight_);
    ApplyEffectParams(engine->GetPostProcessManager(), t);
}

void ScreenEffectComponent::ApplyEffectParams(PostProcessManager* ppm, float t) {
    switch (mode_) {
    case PostProcessMode::Glitch:
        UpdateGlitchParams(ppm, t);
        break;
    case PostProcessMode::Vignette:
        UpdateVignetteParams(ppm, t);
        break;
    case PostProcessMode::ChromaticAberration:
        UpdateChromaticAberrationParams(ppm, t);
        break;
    case PostProcessMode::RadialBlur:
        UpdateRadialBlurParams(ppm, t);
        break;
    default:
        break;
    }
}

void ScreenEffectComponent::UpdateGlitchParams(PostProcessManager* ppm, float t) {
    auto& params = ppm->GetGlitchParams();
    params.intensity = Lerp(baseGlitchParams_.intensity, targetGlitchParams_.intensity, t);
    params.edgeMaskStrength = targetGlitchParams_.edgeMaskStrength;
    params.probability = targetGlitchParams_.probability;
    params.blockSizeX = targetGlitchParams_.blockSizeX;
    params.blockSizeY = targetGlitchParams_.blockSizeY;
    params.offsetBase = targetGlitchParams_.offsetBase;
    params.offsetMax = targetGlitchParams_.offsetMax;
    params.rgbShiftBase = targetGlitchParams_.rgbShiftBase;
    params.rgbShiftMax = targetGlitchParams_.rgbShiftMax;
    params.scanlineFreq = targetGlitchParams_.scanlineFreq;
    params.scanlineIntensity = targetGlitchParams_.scanlineIntensity;
    params.color = Lerp(baseGlitchParams_.color, targetGlitchParams_.color, t);
}

void ScreenEffectComponent::UpdateVignetteParams(PostProcessManager* ppm, float t) {
    auto& params = ppm->GetVignetteParams();
    params.radius = Lerp(baseVignetteParams_.radius, targetVignetteParams_.radius, t);
    params.softness = Lerp(baseVignetteParams_.softness, targetVignetteParams_.softness, t);
    params.color = Lerp(baseVignetteParams_.color, targetVignetteParams_.color, t);
}

void ScreenEffectComponent::UpdateChromaticAberrationParams(PostProcessManager* ppm, float t) {
    auto& params = ppm->GetChromaticAberrationParams();
    params.intensity = Lerp(baseChromaticAberrationParams_.intensity, targetChromaticAberrationParams_.intensity, t);
}

void ScreenEffectComponent::UpdateRadialBlurParams(PostProcessManager* ppm, float t) {
    auto& params = ppm->GetRadialBlurParams();
    params.blurWidth = Lerp(baseRadialBlurParams_.blurWidth, targetRadialBlurParams_.blurWidth, t);
    params.center = Lerp(baseRadialBlurParams_.center, targetRadialBlurParams_.center, t);
    params.numSamples = targetRadialBlurParams_.numSamples;
}

void ScreenEffectComponent::RestoreBaseParams(PostProcessManager* ppm) {
    switch (mode_) {
    case PostProcessMode::Glitch:
        ppm->SetGlitchParams(baseGlitchParams_);
        break;
    case PostProcessMode::Vignette:
        ppm->SetVignetteParams(baseVignetteParams_);
        break;
    case PostProcessMode::ChromaticAberration:
        ppm->SetChromaticAberrationParams(baseChromaticAberrationParams_);
        break;
    case PostProcessMode::RadialBlur:
        ppm->SetRadialBlurParams(baseRadialBlurParams_);
        break;
    default:
        break;
    }
}

nlohmann::json ScreenEffectComponent::Serialize() {
    nlohmann::json j;
    j["mode"] = static_cast<int>(mode_);
    j["duration"] = duration_;

    if (mode_ == PostProcessMode::Glitch) {
        j["targetGlitchParams"]["intensity"] = targetGlitchParams_.intensity;
        j["targetGlitchParams"]["edgeMaskStrength"] = targetGlitchParams_.edgeMaskStrength;
        j["targetGlitchParams"]["probability"] = targetGlitchParams_.probability;
        j["targetGlitchParams"]["blockSizeX"] = targetGlitchParams_.blockSizeX;
        j["targetGlitchParams"]["blockSizeY"] = targetGlitchParams_.blockSizeY;
        j["targetGlitchParams"]["offsetBase"] = targetGlitchParams_.offsetBase;
        j["targetGlitchParams"]["offsetMax"] = targetGlitchParams_.offsetMax;
        j["targetGlitchParams"]["rgbShiftBase"] = targetGlitchParams_.rgbShiftBase;
        j["targetGlitchParams"]["rgbShiftMax"] = targetGlitchParams_.rgbShiftMax;
        j["targetGlitchParams"]["scanlineFreq"] = targetGlitchParams_.scanlineFreq;
        j["targetGlitchParams"]["scanlineIntensity"] = targetGlitchParams_.scanlineIntensity;
        j["targetGlitchParams"]["glitchColor"] = {targetGlitchParams_.color.x, targetGlitchParams_.color.y,
                                                  targetGlitchParams_.color.z, targetGlitchParams_.color.w};
    } else if (mode_ == PostProcessMode::Vignette) {
        j["targetVignetteParams"]["color"] = {targetVignetteParams_.color.x, targetVignetteParams_.color.y,
                                              targetVignetteParams_.color.z, targetVignetteParams_.color.w};
        j["targetVignetteParams"]["radius"] = targetVignetteParams_.radius;
        j["targetVignetteParams"]["softness"] = targetVignetteParams_.softness;
    } else if (mode_ == PostProcessMode::ChromaticAberration) {
        j["targetChromaticAberrationParams"]["intensity"] = targetChromaticAberrationParams_.intensity;
    } else if (mode_ == PostProcessMode::RadialBlur) {
        j["targetRadialBlurParams"]["blurWidth"] = targetRadialBlurParams_.blurWidth;
        j["targetRadialBlurParams"]["center"] = {targetRadialBlurParams_.center.x, targetRadialBlurParams_.center.y};
        j["targetRadialBlurParams"]["numSamples"] = targetRadialBlurParams_.numSamples;
    }

    return j;
}

void ScreenEffectComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("mode")) {
        mode_ = static_cast<PostProcessMode>(j["mode"]);
    }
    if (j.contains("duration")) {
        duration_ = j["duration"];
    }

    if (j.contains("targetGlitchParams")) {
        auto& gj = j["targetGlitchParams"];
        if (gj.contains("intensity")) {
            targetGlitchParams_.intensity = gj["intensity"];
        }
        if (gj.contains("edgeMaskStrength")) {
            targetGlitchParams_.edgeMaskStrength = gj["edgeMaskStrength"];
        }
        if (gj.contains("probability")) {
            targetGlitchParams_.probability = gj["probability"];
        }
        if (gj.contains("blockSizeX")) {
            targetGlitchParams_.blockSizeX = gj["blockSizeX"];
        }
        if (gj.contains("blockSizeY")) {
            targetGlitchParams_.blockSizeY = gj["blockSizeY"];
        }
        if (gj.contains("offsetBase")) {
            targetGlitchParams_.offsetBase = gj["offsetBase"];
        }
        if (gj.contains("offsetMax")) {
            targetGlitchParams_.offsetMax = gj["offsetMax"];
        }
        if (gj.contains("rgbShiftBase")) {
            targetGlitchParams_.rgbShiftBase = gj["rgbShiftBase"];
        }
        if (gj.contains("rgbShiftMax")) {
            targetGlitchParams_.rgbShiftMax = gj["rgbShiftMax"];
        }
        if (gj.contains("scanlineFreq")) {
            targetGlitchParams_.scanlineFreq = gj["scanlineFreq"];
        }
        if (gj.contains("scanlineIntensity")) {
            targetGlitchParams_.scanlineIntensity = gj["scanlineIntensity"];
        }
        if (gj.contains("glitchColor")) {
            auto& c = gj["glitchColor"];
            targetGlitchParams_.color = {c[0], c[1], c[2], c[3]};
        }
    }

    if (j.contains("targetVignetteParams")) {
        auto& vj = j["targetVignetteParams"];
        if (vj.contains("color") && vj["color"].is_array() && vj["color"].size() == 4) {
            targetVignetteParams_.color.x = vj["color"][0];
            targetVignetteParams_.color.y = vj["color"][1];
            targetVignetteParams_.color.z = vj["color"][2];
            targetVignetteParams_.color.w = vj["color"][3];
        }
        if (vj.contains("radius")) {
            targetVignetteParams_.radius = vj["radius"];
        }
        if (vj.contains("softness")) {
            targetVignetteParams_.softness = vj["softness"];
        }
    }

    if (j.contains("targetChromaticAberrationParams")) {
        auto& cj = j["targetChromaticAberrationParams"];
        if (cj.contains("intensity")) {
            targetChromaticAberrationParams_.intensity = cj["intensity"];
        }
    }

    if (j.contains("targetRadialBlurParams")) {
        auto& rj = j["targetRadialBlurParams"];
        if (rj.contains("blurWidth")) {
            targetRadialBlurParams_.blurWidth = rj["blurWidth"];
        }
        if (rj.contains("center") && rj["center"].is_array() && rj["center"].size() == 2) {
            targetRadialBlurParams_.center.x = rj["center"][0];
            targetRadialBlurParams_.center.y = rj["center"][1];
        }
        if (rj.contains("numSamples")) {
            targetRadialBlurParams_.numSamples = rj["numSamples"];
        }
    }
}

std::shared_ptr<Component> ScreenEffectComponent::Clone() {
    auto clone = std::make_shared<ScreenEffectComponent>();
    clone->CopyPropertiesFrom(this);
    clone->mode_ = this->mode_;
    clone->duration_ = this->duration_;
    clone->targetGlitchParams_ = this->targetGlitchParams_;
    clone->targetVignetteParams_ = this->targetVignetteParams_;
    clone->targetChromaticAberrationParams_ = this->targetChromaticAberrationParams_;
    clone->targetRadialBlurParams_ = this->targetRadialBlurParams_;
    return clone;
}
