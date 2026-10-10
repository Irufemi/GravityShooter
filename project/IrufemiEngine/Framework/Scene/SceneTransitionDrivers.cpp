#include "Framework/Scene/SceneTransitionDrivers.h"

// ============================================================================
// FadeTransitionDriver
// ============================================================================
void FadeTransitionDriver::OnStart(PostProcessManager* ppm, std::vector<PostProcessMode>& activeModes) {
    if (ppm) {
        ppm->AddActiveMode(PostProcessMode::Fade, PostProcessManager::Layer::PostUI);
        activeModes.push_back(PostProcessMode::Fade);
    }
}

void FadeTransitionDriver::OnUpdate(PostProcessManager* ppm, float factor) {
    if (ppm) {
        ppm->GetFadeParams().intensity = factor;
    }
}

// ============================================================================
// DissolveTransitionDriver
// ============================================================================
void DissolveTransitionDriver::OnStart(PostProcessManager* ppm, std::vector<PostProcessMode>& activeModes) {
    if (ppm) {
        ppm->AddActiveMode(PostProcessMode::Dissolve, PostProcessManager::Layer::PostUI);
        activeModes.push_back(PostProcessMode::Dissolve);
        ppm->GetDissolveParams().edgeColor = {1.0f, 0.4f, 0.3f, 1.0f}; // 炎のようなオレンジ色
    }
}

void DissolveTransitionDriver::OnUpdate(PostProcessManager* ppm, float factor) {
    if (ppm) {
        ppm->GetDissolveParams().threshold = factor * 1.1f;
    }
}

// ============================================================================
// SlideTransitionDriver
// ============================================================================
void SlideTransitionDriver::OnStart(PostProcessManager* ppm, std::vector<PostProcessMode>& activeModes) {
    if (ppm) {
        ppm->AddActiveMode(PostProcessMode::Slide, PostProcessManager::Layer::PostUI);
        activeModes.push_back(PostProcessMode::Slide);
    }
}

void SlideTransitionDriver::OnUpdate(PostProcessManager* ppm, float factor) {
    if (ppm) {
        ppm->GetSlideParams().threshold = factor * 1.05f;
    }
}

// ============================================================================
// RadialBlurTransitionDriver
// ============================================================================
void RadialBlurTransitionDriver::OnStart(PostProcessManager* ppm, std::vector<PostProcessMode>& activeModes) {
    if (ppm) {
        ppm->AddActiveMode(PostProcessMode::RadialBlur, PostProcessManager::Layer::PostUI);
        ppm->AddActiveMode(PostProcessMode::Fade, PostProcessManager::Layer::PostUI);
        activeModes.push_back(PostProcessMode::RadialBlur);
        activeModes.push_back(PostProcessMode::Fade);
        ppm->GetFadeParams().color =
            isWhite_ ? Irufemi::Vector4{1.0f, 1.0f, 1.0f, 1.0f} : Irufemi::Vector4{0.0f, 0.0f, 0.0f, 1.0f};
    }
}

void RadialBlurTransitionDriver::OnUpdate(PostProcessManager* ppm, float factor) {
    if (ppm) {
        ppm->GetRadialBlurParams().blurWidth = factor * 0.05f;
        ppm->GetFadeParams().intensity = factor;
    }
}
