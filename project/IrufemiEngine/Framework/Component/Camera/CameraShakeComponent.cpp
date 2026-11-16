#include "Framework/Component/Camera/CameraShakeComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/PerlinNoise.h"
#include "Core/Utility/Ease.h"
#include "Core/Math/Random/Random.h"
#include <algorithm>
#include <climits>

using namespace Irufemi;

CameraShakeComponent::CameraShakeComponent() {
    perlinNoise_ = std::make_shared<Irufemi::PerlinNoise>(
        static_cast<unsigned int>(Irufemi::Random::GeneratorUint64(0, UINT_MAX)));
}

CameraShakeComponent::~CameraShakeComponent() = default;

void CameraShakeComponent::OnRegisterProperties() {
    RegisterProperty("Global Intensity Multiplier", &globalIntensityMultiplier_);
}

void CameraShakeComponent::Initialize() {
    OnAwake();
}

void CameraShakeComponent::OnAwake() {
    if (gameObject_) {
        cameraComp_ = gameObject_->GetComponent<CameraComponent>();
    }
}

void CameraShakeComponent::PlayShake(float intensity, int durationFrames, float frequency) {
    // 60FPS基準で秒に変換
    float durationSeconds = durationFrames / 60.0f;
    PlayShakeSeconds(intensity, durationSeconds, frequency);
}

void CameraShakeComponent::PlayShakeSeconds(float intensity, float durationSeconds, float frequency) {
    ShakeEvent ev;
    ev.maxIntensity = intensity;
    ev.duration = durationSeconds;
    ev.currentTime = 0.0f;
    ev.frequency = frequency;
    // X, Y, Z の揺れやすさにバラツキを持たせる
    ev.axisIntensity = {1.0f, 1.0f, 0.5f};
    ev.seed = static_cast<unsigned int>(Irufemi::Random::GeneratorUint64(0, UINT_MAX));

    activeShakes_.push_back(ev);
}

void CameraShakeComponent::Update() {
    if (!cameraComp_) {
        if (gameObject_) {
            cameraComp_ = gameObject_->GetComponent<CameraComponent>();
        }
        if (!cameraComp_) {
            return;
        }
    }

    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    float dt = engine->GetGameDeltaTime();

    Irufemi::Vector3 totalOffset = {0.0f, 0.0f, 0.0f};

    for (auto& ev : activeShakes_) {
        ev.currentTime += dt;
        if (ev.currentTime > ev.duration) {
            continue; // 終了したイベント
        }

        // 0.0(開始) から 1.0(終了) へ
        float t = ev.currentTime / ev.duration;

        // 減衰計算 (EaseOutQuadでフェードアウト)
        // tが1.0に近づくほど、1.0から0.0へ減衰させる
        float decay = 1.0f - EaseOutQuad(t);

        // パーリンノイズによる滑らかな揺れ (X, Y, Zでシードをずらす)
        float nx = perlinNoise_->Noise1D(ev.currentTime * ev.frequency + ev.seed);
        float ny = perlinNoise_->Noise1D(ev.currentTime * ev.frequency + ev.seed + 100.0f);
        float nz = perlinNoise_->Noise1D(ev.currentTime * ev.frequency + ev.seed + 200.0f);

        float currentIntensity = ev.maxIntensity * decay * globalIntensityMultiplier_;

        totalOffset.x += nx * currentIntensity * ev.axisIntensity.x;
        totalOffset.y += ny * currentIntensity * ev.axisIntensity.y;
        totalOffset.z += nz * currentIntensity * ev.axisIntensity.z;
    }

    // 終了したイベントを削除 (C++20 std::erase_if)
    std::erase_if(activeShakes_, [](const ShakeEvent& e) { return e.currentTime >= e.duration; });

    cameraComp_->SetPositionOffset(totalOffset);
}
