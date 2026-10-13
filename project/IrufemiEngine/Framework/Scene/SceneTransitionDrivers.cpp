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
    if (ppm) {
        // 1. 空間の歪みと加速（RadialBlur: 指数関数的にブースト）
        float blurProgress = std::pow(factor, 2.2f);
        ppm->GetRadialBlurParams().blurWidth = blurProgress * 0.12f;
        ppm->GetRadialBlurParams().center = {0.5f, 0.5f};
        ppm->GetRadialBlurParams().numSamples = 16;

        // 2. 重力色収差（ChromaticAberration: 臨界点でピークに達する赤方偏移）
        float chromaProgress = std::pow(factor, 1.5f);
        ppm->GetChromaticAberrationParams().intensity = chromaProgress * 0.06f;

        // 3. 事象の地平線ブラックアウト（Fade: 終盤で一気に吸い込まれ暗転）
        ppm->GetFadeParams().intensity = std::pow(factor, 2.5f);
    }

    // 4. 流体星雲シミュレーションとの物理連動（画面中心へ強力な重力特異点を注入）
    if (ppm) {
        if (auto engine = ppm->GetEngine()) {
            if (auto fluid = engine->GetFluidSimulation()) {
                // factorの上昇に合わせて吸引力と回転渦を指数関数的にブースト
                float pullStrength = factor * 45.0f;
                float swirlStrength = factor * 60.0f;
                float radius = 0.5f + factor * 0.3f;
                fluid->AddGravityVortex(Irufemi::Vector2{0.5f, 0.5f}, pullStrength, swirlStrength, radius);
            }
        }
    }
}

