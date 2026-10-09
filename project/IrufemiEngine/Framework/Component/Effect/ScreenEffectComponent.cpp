#include "Framework/Component/Effect/ScreenEffectComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/Ease.h"
#include <algorithm>

namespace {
// 互換性フォールバック用ダミー静的インスタンス
PostProcessManager::GlitchParams sDummyGlitch{};
PostProcessManager::VignetteParams sDummyVignette{};
PostProcessManager::ChromaticAberrationParams sDummyChromatic{};
PostProcessManager::RadialBlurParams sDummyRadialBlur{};
} // namespace

ScreenEffectComponent::ScreenEffectComponent() {}

ScreenEffectComponent::~ScreenEffectComponent() {
    OnDestroy();
}

void ScreenEffectComponent::OnDestroy() {
    if (isPlaying_) {
        auto engine = GetEngine();
        if (engine && engine->GetPostProcessManager()) {
            if (driver_) {
                driver_->RestoreBaseParams(engine->GetPostProcessManager());
            }
            engine->GetPostProcessManager()->RemoveActiveMode(mode_);
        }
        isPlaying_ = false;
    }
}

void ScreenEffectComponent::Initialize() {
    EnsureDriver();
}

void ScreenEffectComponent::EnsureDriver() {
    if (!driver_ && mode_ != PostProcessMode::None) {
        driver_ = ScreenEffectDriverFactory::CreateDriver(mode_);
    }
}

void ScreenEffectComponent::SetMode(PostProcessMode mode) {
    if (mode_ != mode || !driver_) {
        mode_ = mode;
        driver_ = ScreenEffectDriverFactory::CreateDriver(mode_);
    }
}

void ScreenEffectComponent::Play() {
    auto engine = GetEngine();
    if (!engine || !engine->GetPostProcessManager()) {
        return;
    }

    EnsureDriver();
    auto* ppm = engine->GetPostProcessManager();

    if (driver_) {
        driver_->CacheBaseParams(ppm);
    }

    currentWeight_ = 1.0f;
    isPlaying_ = true;

    wasModeActiveBeforePlay_ = ppm->HasActiveMode(mode_);
    if (!wasModeActiveBeforePlay_ && mode_ != PostProcessMode::None) {
        ppm->AddActiveMode(mode_, PostProcessManager::Layer::PostUI);
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

    auto* ppm = engine->GetPostProcessManager();

    if (currentWeight_ <= 0.0f) {
        currentWeight_ = 0.0f;
        isPlaying_ = false;

        if (driver_) {
            driver_->RestoreBaseParams(ppm);
        }

        if (!wasModeActiveBeforePlay_ && mode_ != PostProcessMode::None) {
            ppm->RemoveActiveMode(mode_);
        }
        return;
    }

    // Weightに基づいたEaseOut補間処理
    float t = EaseOutQuad(currentWeight_);
    if (driver_) {
        driver_->ApplyInterpolation(ppm, t); // 完全なゼロ分岐実行！
    }
}

nlohmann::json ScreenEffectComponent::Serialize() {
    nlohmann::json j;
    j["mode"] = static_cast<int>(mode_);
    j["duration"] = duration_;

    if (driver_) {
        nlohmann::json driverJson = driver_->Serialize();
        j.update(driverJson);
    }

    return j;
}

void ScreenEffectComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("mode")) {
        SetMode(static_cast<PostProcessMode>(j["mode"]));
    }
    if (j.contains("duration")) {
        duration_ = j["duration"];
    }

    EnsureDriver();
    if (driver_) {
        driver_->Deserialize(j);
    }
}

std::shared_ptr<Component> ScreenEffectComponent::Clone() {
    auto clone = std::make_shared<ScreenEffectComponent>();
    clone->CopyPropertiesFrom(this);
    clone->mode_ = this->mode_;
    clone->duration_ = this->duration_;
    if (this->driver_) {
        clone->driver_ = this->driver_->Clone();
    }
    return clone;
}

// ============================================================================
// 既存 API 互換レイヤー
// ============================================================================
void ScreenEffectComponent::SetTargetGlitchParams(const PostProcessManager::GlitchParams& params) {
    if (!driver_ || mode_ != PostProcessMode::Glitch) {
        SetMode(PostProcessMode::Glitch);
    }
    if (auto* d = dynamic_cast<GlitchEffectDriver*>(driver_.get())) {
        d->SetTargetParams(params);
    }
}

PostProcessManager::GlitchParams& ScreenEffectComponent::GetTargetGlitchParams() {
    if (!driver_ || mode_ != PostProcessMode::Glitch) {
        SetMode(PostProcessMode::Glitch);
    }
    if (auto* d = dynamic_cast<GlitchEffectDriver*>(driver_.get())) {
        return d->GetTargetParams();
    }
    return sDummyGlitch;
}

const PostProcessManager::GlitchParams& ScreenEffectComponent::GetTargetGlitchParams() const {
    if (auto* d = dynamic_cast<GlitchEffectDriver*>(driver_.get())) {
        return d->GetTargetParams();
    }
    return sDummyGlitch;
}

void ScreenEffectComponent::SetTargetVignetteParams(const PostProcessManager::VignetteParams& params) {
    if (!driver_ || mode_ != PostProcessMode::Vignette) {
        SetMode(PostProcessMode::Vignette);
    }
    if (auto* d = dynamic_cast<VignetteEffectDriver*>(driver_.get())) {
        d->SetTargetParams(params);
    }
}

PostProcessManager::VignetteParams& ScreenEffectComponent::GetTargetVignetteParams() {
    if (!driver_ || mode_ != PostProcessMode::Vignette) {
        SetMode(PostProcessMode::Vignette);
    }
    if (auto* d = dynamic_cast<VignetteEffectDriver*>(driver_.get())) {
        return d->GetTargetParams();
    }
    return sDummyVignette;
}

const PostProcessManager::VignetteParams& ScreenEffectComponent::GetTargetVignetteParams() const {
    if (auto* d = dynamic_cast<VignetteEffectDriver*>(driver_.get())) {
        return d->GetTargetParams();
    }
    return sDummyVignette;
}

void ScreenEffectComponent::SetTargetChromaticAberrationParams(const PostProcessManager::ChromaticAberrationParams& params) {
    if (!driver_ || mode_ != PostProcessMode::ChromaticAberration) {
        SetMode(PostProcessMode::ChromaticAberration);
    }
    if (auto* d = dynamic_cast<ChromaticAberrationEffectDriver*>(driver_.get())) {
        d->SetTargetParams(params);
    }
}

PostProcessManager::ChromaticAberrationParams& ScreenEffectComponent::GetTargetChromaticAberrationParams() {
    if (!driver_ || mode_ != PostProcessMode::ChromaticAberration) {
        SetMode(PostProcessMode::ChromaticAberration);
    }
    if (auto* d = dynamic_cast<ChromaticAberrationEffectDriver*>(driver_.get())) {
        return d->GetTargetParams();
    }
    return sDummyChromatic;
}

const PostProcessManager::ChromaticAberrationParams& ScreenEffectComponent::GetTargetChromaticAberrationParams() const {
    if (auto* d = dynamic_cast<ChromaticAberrationEffectDriver*>(driver_.get())) {
        return d->GetTargetParams();
    }
    return sDummyChromatic;
}

void ScreenEffectComponent::SetTargetRadialBlurParams(const PostProcessManager::RadialBlurParams& params) {
    if (!driver_ || mode_ != PostProcessMode::RadialBlur) {
        SetMode(PostProcessMode::RadialBlur);
    }
    if (auto* d = dynamic_cast<RadialBlurEffectDriver*>(driver_.get())) {
        d->SetTargetParams(params);
    }
}

PostProcessManager::RadialBlurParams& ScreenEffectComponent::GetTargetRadialBlurParams() {
    if (!driver_ || mode_ != PostProcessMode::RadialBlur) {
        SetMode(PostProcessMode::RadialBlur);
    }
    if (auto* d = dynamic_cast<RadialBlurEffectDriver*>(driver_.get())) {
        return d->GetTargetParams();
    }
    return sDummyRadialBlur;
}

const PostProcessManager::RadialBlurParams& ScreenEffectComponent::GetTargetRadialBlurParams() const {
    if (auto* d = dynamic_cast<RadialBlurEffectDriver*>(driver_.get())) {
        return d->GetTargetParams();
    }
    return sDummyRadialBlur;
}
