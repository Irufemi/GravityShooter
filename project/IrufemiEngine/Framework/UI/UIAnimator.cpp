#include "Framework/UI/UIAnimator.h"
#include "Core/Utility/Ease.h"
#include <cmath>

namespace Irufemi {

void UIAnimator::Update(float deltaTime) {
    time_ += deltaTime;
}

void UIAnimator::Reset() {
    time_ = 0.0f;
}

float UIAnimator::GetPulseAlpha(float base, float amplitude, float speed) const {
    float normSin = 0.5f * (1.0f + std::sin(time_ * speed));
    return Lerp(base, base + amplitude, normSin);
}

bool UIAnimator::GetFlashVisibility(float speed) const {
    return std::sin(time_ * speed) > 0.0f;
}

float UIAnimator::GetFloatOffset(float amplitude, float speed, float phaseOffset) const {
    return std::sin(time_ * speed + phaseOffset) * amplitude;
}

} // namespace Irufemi

