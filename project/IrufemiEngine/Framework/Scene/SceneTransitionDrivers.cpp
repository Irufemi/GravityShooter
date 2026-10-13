#include "Framework/Scene/SceneTransitionDrivers.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/Fluid/FluidSimulation2D.h"
#include <cmath>

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
        if (customColor_.has_value()) {
            ppm->GetFadeParams().color = *customColor_;
        } else {
            ppm->GetFadeParams().color =
                isWhite_ ? Irufemi::Vector4{1.0f, 1.0f, 1.0f, 1.0f} : Irufemi::Vector4{0.0f, 0.0f, 0.0f, 1.0f};
        }
    }
}

void RadialBlurTransitionDriver::OnUpdate(PostProcessManager* ppm, float factor) {
    if (ppm) {
        ppm->GetRadialBlurParams().blurWidth = factor * 0.05f;
        ppm->GetFadeParams().intensity = factor;
    }
}

// ============================================================================
// GravitySingularityTransitionDriver
// ============================================================================
void GravitySingularityTransitionDriver::OnStart(PostProcessManager* ppm, std::vector<PostProcessMode>& activeModes) {
    if (ppm) {
        // 重力レンズ引き込みブラー + 色収差（赤方偏移） + 事象の地平線ブラックフェード
        ppm->AddActiveMode(PostProcessMode::RadialBlur, PostProcessManager::Layer::PostUI);
        ppm->AddActiveMode(PostProcessMode::ChromaticAberration, PostProcessManager::Layer::PostUI);
        ppm->AddActiveMode(PostProcessMode::Fade, PostProcessManager::Layer::PostUI);

        activeModes.push_back(PostProcessMode::RadialBlur);
        activeModes.push_back(PostProcessMode::ChromaticAberration);
        activeModes.push_back(PostProcessMode::Fade);

        // 事象の地平線カラー（深遠な宇宙ブラック・重力シアン）
        if (customColor_.has_value()) {
            ppm->GetFadeParams().color = *customColor_;
        } else {
            ppm->GetFadeParams().color = Irufemi::Vector4{0.005f, 0.015f, 0.035f, 1.0f};
        }
    }
}

void GravitySingularityTransitionDriver::OnUpdate(PostProcessManager* ppm, float factor) {
    // 星雲の重力特異点中心 (0.50f, 0.42f) と完全に合致させ、遷移時の中心ズレ・二重渦干渉を排除
    const Irufemi::Vector2 singularityCenter{0.50f, 0.42f};

    if (ppm) {
        // 1. 空間の歪みと加速（RadialBlur: 上品で深遠な求心的ワープ吸い込み）
        float blurProgress = std::pow(factor, 2.0f);
        ppm->GetRadialBlurParams().blurWidth = blurProgress * 0.045f;
        ppm->GetRadialBlurParams().center = singularityCenter;
        ppm->GetRadialBlurParams().numSamples = 16;

        // 2. 重力色収差（ChromaticAberration: 派手な虹色ノイズを排し、繊細な重力プリズム光へ）
        float chromaProgress = std::pow(factor, 1.6f);
        ppm->GetChromaticAberrationParams().intensity = chromaProgress * 0.015f;

        // 3. 事象の地平線ブラックアウト（Fade: 滑らかに深遠な暗黒へ落ちていく）
        ppm->GetFadeParams().intensity = std::pow(factor, 2.0f);
    }
}

