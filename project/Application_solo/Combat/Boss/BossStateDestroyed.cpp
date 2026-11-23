#include "Combat/Boss/BossStateDestroyed.h"
#include "Combat/Boss/BossComponent.h"
#include "Combat/Boss/BossDamageVisualizerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "RailMechanics/RailRelativeFollowerComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/Log.h"
#include "Audio/SoundHelper.h"
#include "Effects/EffectManagerComponent.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"
#include "Renderer/Data/LightningConfig.h"
#include "RHI/DirectX12/ConstantBuffer.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Core/Math/Random/Random.h"
#include "Core/Math/MathFunction.h"
#include "Renderer/Camera/CameraManager.h"
#include "Framework/Scene/BaseScene.h"
#include "UI/LockonMarkerUIComponent.h"
#include "Player/PlayerTargetingComponent.h"
#include "Combat/EnemyBulletManagerComponent.h"
#include "Combat/BossBulletManagerComponent.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <array>

namespace {

constexpr float kDefaultFov = 0.785398f;        //!< 通常カメラFOV (45度)
constexpr float kCinematicZoomFov = 0.453786f;  //!< 撃破時シネマティックズームFOV (26度)
constexpr float kDefaultBossDistance = 80.0f;   //!< 通常時のボス追従距離 (80m)
constexpr float kCinematicBossDistance = 14.0f; //!< 撃破演出時の至近追従距離 (14m、眼前を埋め尽くす)
constexpr float kDefaultBossOffsetY = 10.0f;    //!< 通常時のボス高さオフセット (10m)
constexpr float kCinematicBossOffsetY = 2.4f;   //!< 撃破演出時のアイレベル高さオフセット (2.4m、カメラ正面に配置)

/**
 * @brief レール上のボス追従距離オフセットを動的に変更する
 */
inline void SetBossDistanceOffset(BossComponent* boss, float distance) {
    if (boss && boss->GetGameObject()) {
        if (auto follower = boss->GetGameObject()->GetComponent<RailRelativeFollowerComponent>()) {
            follower->SetDistanceOffset(distance);
        }
    }
}

/**
 * @brief レール上のボス高さローカルオフセット（Y）を動的に変更する
 */
inline void SetBossOffsetY(BossComponent* boss, float offsetY) {
    if (boss && boss->GetGameObject()) {
        if (auto follower = boss->GetGameObject()->GetComponent<RailRelativeFollowerComponent>()) {
            follower->SetLocalOffsetY(offsetY);
        }
    }
}

/**
 * @brief メインカメラの CameraComponent プロパティおよび Camera インスタンスの FOV を確実に両方駆動する
 */
inline void SetCinematicCameraFov(BossDamageVisualizerComponent* visualizer, BossComponent* boss, float fov) {
    if (visualizer) {
        if (auto camComp = visualizer->GetMainCameraComponent()) {
            camComp->SetFovAngleY(fov);
        }
    }
    if (boss && boss->GetEngine()) {
        if (auto camMgr = boss->GetEngine()->GetCameraManager()) {
            if (auto cam = camMgr->GetActiveCamera()) {
                cam->SetFovY(fov);
            }
        }
    }
}

inline Irufemi::Vector3 LerpVector3(const Irufemi::Vector3& a, const Irufemi::Vector3& b, float t) {
    return Irufemi::Vector3{std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t)};
}

/**
 * @brief 任意のワールド座標をスクリーンUV座標（0.0〜1.0）へ射影変換する
 */
inline Irufemi::Vector2 CalculateWorldScreenUV(Irufemi::IrufemiEngine* engine, const Irufemi::Vector3& worldPos,
                                               bool* outIsVisible = nullptr) {
    if (outIsVisible) {
        *outIsVisible = false;
    }
    if (!engine) {
        return Irufemi::Vector2{0.5f, 0.48f};
    }
    auto camMgr = engine->GetCameraManager();
    auto activeCam = camMgr ? camMgr->GetActiveCamera() : nullptr;
    if (!activeCam) {
        return Irufemi::Vector2{0.5f, 0.48f};
    }

    const auto& viewProj = activeCam->GetViewProjectionMatrix3D();
    float w = worldPos.x * viewProj.m[0][3] + worldPos.y * viewProj.m[1][3] + worldPos.z * viewProj.m[2][3] +
              viewProj.m[3][3];
    if (w <= 0.001f) {
        return Irufemi::Vector2{0.5f, 0.48f};
    }

    if (outIsVisible) {
        *outIsVisible = true;
    }

    Irufemi::Vector3 clipPos = Irufemi::Math::Transform(worldPos, viewProj);
    return Irufemi::Vector2{std::clamp(clipPos.x * 0.5f + 0.5f, 0.05f, 0.95f),
                            std::clamp(-clipPos.y * 0.5f + 0.5f, 0.05f, 0.95f)};
}

/**
 * @brief ボスのワールド座標をスクリーンUV座標（0.0〜1.0）へ射影変換する
 */
inline Irufemi::Vector2 CalculateBossScreenUV(BossComponent* boss) {
    if (!boss || !boss->GetGameObject()) {
        return Irufemi::Vector2{0.5f, 0.48f};
    }
    return CalculateWorldScreenUV(boss->GetEngine(), boss->GetGameObject()->GetTransform()->GetWorldPosition());
}

inline float SmoothStep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/**
 * @class DeathStepOvervoltageFreeze
 * @brief 【第1幕】超電磁ヒットストップ ＆ 真空の静寂 ＆ コア臨界閃光（0.00s〜0.15s）
 */
class DeathStepOvervoltageFreeze : public IDeathSequenceStep {
public:
    void Enter(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        elapsedTime_ = 0.0f;
        isCompleted_ = false;

        if (boss && boss->GetGameObject()) {
            // トドメの撃破通知
            boss->NotifyBossDied();

            // ボス距離・高さオフセットを初期値にリセット
            SetBossDistanceOffset(boss, kDefaultBossDistance);
            SetBossOffsetY(boss, kDefaultBossOffsetY);

            // 真空の静寂を演出（BGM停止）
            SoundHelper::StopBGM();

            // コアのEffectMaskをHDR純白オーバーブライトに白熱化
            auto brightenMaskRecursive = [](auto self, Irufemi::GameObject* obj) -> void {
                if (!obj) {
                    return;
                }
                if (auto* em = obj->GetComponent<Irufemi::EffectMaskComponent>()) {
                    auto params = em->GetCustomParams();
                    params.color1 = Irufemi::Vector4{6.0f, 6.0f, 7.0f, 1.0f}; // 純白白熱
                    em->SetCustomParams(params);
                    em->SetEnableEffectMask(true);
                }
                for (const auto& child : obj->GetChildren()) {
                    if (child) {
                        self(self, child.get());
                    }
                }
            };
            brightenMaskRecursive(brightenMaskRecursive, boss->GetGameObject());

            // 0.15秒間の完全ヒットストップ（マイクロフリーズ）
            if (auto engine = boss->GetEngine()) {
                engine->SetTimeScale(0.01f);
                SoundHelper::Play(GameSE::ResultImpact);
            }
        }
    }

    void Update(BossComponent* boss, BossDamageVisualizerComponent* visualizer, float dt) override {
        elapsedTime_ += dt;
        float progress = std::clamp(elapsedTime_ / targetDuration_, 0.0f, 1.0f);

        // カメラズーム初期予兆 (45度 ➔ 38度)
        float currentFov = std::lerp(kDefaultFov, 0.65f, progress);
        SetCinematicCameraFov(visualizer, boss, currentFov);

        if (elapsedTime_ >= targetDuration_) {
            isCompleted_ = true;
        }
    }

    bool IsCompleted() const override {
        return isCompleted_;
    }

private:
    float elapsedTime_ = 0.0f;
    float targetDuration_ = 0.15f;
    bool isCompleted_ = false;
};

/**
 * @class DeathStepLightningCrescendo
 * @brief 【第2幕】電流密度クレッシェンド ＆ 物理ドリーイン接近（80m➔38m） ＆ 空間アーク放電（0.15s〜1.05s）
 */
class DeathStepLightningCrescendo : public IDeathSequenceStep {
public:
    void Enter(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        (void)visualizer;
        elapsedTime_ = 0.0f;
        burstTimer_ = 0.0f;
        burstCount_ = 0;
        isCompleted_ = false;

        if (!boss || !boss->GetGameObject() || !boss->GetEngine()) {
            isCompleted_ = true;
            return;
        }

        auto engine = boss->GetEngine();
        auto dxCommon = engine->GetDirectXCommon();

        Irufemi::Vector4 lightningColor =
            visualizer ? visualizer->GetDeathLightningColor() : Irufemi::Vector4{2.2f, 0.2f, 2.8f, 1.0f};
        Irufemi::Vector4 blastColor =
            visualizer ? visualizer->GetDeathBlastColor() : Irufemi::Vector4{3.8f, 0.3f, 4.5f, 1.0f};

        lightningBuffer_ = &boss->GetLightningBuffer();
        if (dxCommon && lightningBuffer_) {
            if (!lightningBuffer_->IsInitialized()) {
                lightningBuffer_->Initialize(dxCommon);
            }
            config_ = Irufemi::LightningConfig{};
            config_.density = 1.2f;
            config_.boltCount = 1.5f;
            config_.thickness = 3.5f;
            config_.speed = 2.5f;
            config_.intensity = 0.8f;
            config_.branchAmount = 0.6f;
            config_.coreColor = Irufemi::Vector4{1.5f, 1.3f, 1.5f, 1.0f};
            config_.haloColor = lightningColor;
            lightningBuffer_->UpdateAll(config_);

            // ボスおよび子オブジェクトのMeshRendererにBossLightning PSOとCBVアドレスを設定
            uint32_t frameIndex = dxCommon->GetCurrentBackBufferIndex();
            D3D12_GPU_VIRTUAL_ADDRESS cbvAddr = lightningBuffer_->GetGPUVirtualAddress(frameIndex);

            auto applyPSORecursive = [&](auto self, Irufemi::GameObject* obj) -> void {
                if (!obj) {
                    return;
                }
                if (auto* r = obj->GetComponent<Irufemi::MeshRendererComponent>()) {
                    r->SetCustomPSO("BossLightning");
                    r->SetCustomCBVAddress(cbvAddr);
                }
                for (const auto& child : obj->GetChildren()) {
                    if (child) {
                        self(self, child.get());
                    }
                }
            };
            applyPSORecursive(applyPSORecursive, boss->GetGameObject());
        }

        // ポストプロセス LightningCataclysm を起動
        if (auto ppm = engine->GetPostProcessManager()) {
            ppm->AddActiveMode(PostProcessMode::LightningCataclysm, PostProcessManager::Layer::PreUI);
            auto& lc = ppm->GetLightningCataclysmParams();
            lc.center = CalculateBossScreenUV(boss);
            lc.arcIntensity = 0.0f;
            lc.empWaveRadius = 0.0f;
            lc.empWaveIntensity = 0.0f;
            lc.plasmaFlashIntensity = 0.0f;
            lc.electricChromatic = 0.0f;
            lc.time = 0.0f;
            lc.flameColor = blastColor;
            lc.lightningColor = lightningColor;
        }

        // 重厚なスローモーション（0.12f）
        engine->SetTimeScale(0.12f);
    }

    void Update(BossComponent* boss, BossDamageVisualizerComponent* visualizer, float dt) override {
        elapsedTime_ += dt;
        float progress = std::clamp(elapsedTime_ / targetDuration_, 0.0f, 1.0f);
        float smoothProgress = SmoothStep(progress);

        if (!boss || !boss->GetGameObject() || !boss->GetEngine()) {
            isCompleted_ = true;
            return;
        }

        auto engine = boss->GetEngine();
        auto dxCommon = engine->GetDirectXCommon();

        // 1. 【シネマティックズーム】38度 ➔ 26度（kCinematicZoomFov）
        float currentFov = std::lerp(0.65f, kCinematicZoomFov, smoothProgress);
        SetCinematicCameraFov(visualizer, boss, currentFov);

        // 2. 【物理ドリーイン ＆ アイレベル降下】レール追従距離を 80m ➔ 14m、高さを 10m ➔ 2.4m（カメラ正面）へ急接近！
        float currentDist = std::lerp(kDefaultBossDistance, kCinematicBossDistance, smoothProgress);
        SetBossDistanceOffset(boss, currentDist);

        float currentOffsetY = std::lerp(kDefaultBossOffsetY, kCinematicBossOffsetY, smoothProgress);
        SetBossOffsetY(boss, currentOffsetY);

        if (auto transform = boss->GetGameObject()->GetComponent<Irufemi::TransformComponent>()) {
            float jitter = 0.05f + progress * 0.40f;
            Irufemi::Vector3 shake{Irufemi::Random::GeneratorFloat(-jitter, jitter),
                                   Irufemi::Random::GeneratorFloat(-jitter, jitter),
                                   Irufemi::Random::GeneratorFloat(-jitter, jitter)};
            transform->SetPosition(transform->GetPosition() + shake);
        }

        // 3. パラメータのクレッシェンド動的補間 ＆ 装甲放電ディゾルブ
        config_.density = std::lerp(1.2f, 4.5f, progress);
        config_.boltCount = std::lerp(1.5f, 5.0f, progress);
        config_.thickness = std::lerp(3.5f, 7.0f, progress);
        config_.speed = std::lerp(2.5f, 10.0f, progress);
        config_.intensity = std::lerp(0.8f, 12.0f, progress * progress); // 2次加速で超発光

        // 装甲放電ディゾルブ（後半にかけて徐々に分解・消失）
        float dissolve = std::clamp((progress - 0.20f) / 0.80f, 0.0f, 1.0f);
        config_.dissolveProgress = dissolve;
        config_.dissolveEdgeWidth = 0.08f;

        if (dxCommon && lightningBuffer_) {
            uint32_t frameIndex = dxCommon->GetCurrentBackBufferIndex();
            lightningBuffer_->Update(config_, frameIndex);

            D3D12_GPU_VIRTUAL_ADDRESS cbvAddr = lightningBuffer_->GetGPUVirtualAddress(frameIndex);
            auto updateCBV = [&](auto self, Irufemi::GameObject* obj) -> void {
                if (!obj) {
                    return;
                }
                if (auto* r = obj->GetComponent<Irufemi::MeshRendererComponent>()) {
                    r->SetCustomCBVAddress(cbvAddr);
                }
                for (const auto& child : obj->GetChildren()) {
                    if (child) {
                        self(self, child.get());
                    }
                }
            };
            updateCBV(updateCBV, boss->GetGameObject());
        }

        // 4. ポストプロセス LightningCataclysm を駆動（アーク放電が空間を裂く、ドーナツ爆炎は大爆発まで出さない）
        if (auto ppm = engine->GetPostProcessManager()) {
            auto& lc = ppm->GetLightningCataclysmParams();
            lc.center = CalculateBossScreenUV(boss);
            lc.arcIntensity = std::lerp(0.0f, 3.8f, progress * progress);
            lc.electricChromatic = std::lerp(0.0f, 0.030f, progress);
            lc.empWaveRadius = 0.0f;
            lc.empWaveIntensity = 0.0f;
            lc.time += dt * 3.5f;
            if (visualizer) {
                lc.lightningColor = visualizer->GetDeathLightningColor();
                lc.flameColor = visualizer->GetDeathBlastColor();
            }
        }

        // 5. 連鎖小爆発パーティクル（0.10秒おき）
        burstTimer_ += dt;
        if (burstTimer_ >= 0.10f && burstCount_ < 9) {
            burstTimer_ = 0.0f;
            burstCount_++;

            if (auto effectMgr = EffectManagerComponent::GetInstance()) {
                Irufemi::Vector3 curPos = boss->GetGameObject()->GetTransform()->GetWorldPosition();
                Irufemi::Vector3 offset{Irufemi::Random::GeneratorFloat(-4.5f, 4.5f),
                                        Irufemi::Random::GeneratorFloat(-3.5f, 3.5f),
                                        Irufemi::Random::GeneratorFloat(-4.5f, 4.5f)};
                effectMgr->PlayEffect("debris_dust_effect", curPos + offset);
            }
            SoundHelper::Play(GameSE::DebrisShatter, 0.75f, 1.35f);
        }

        if (elapsedTime_ >= targetDuration_) {
            isCompleted_ = true;
        }
    }

    void Exit(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        (void)visualizer;
        // 機体のレール追従距離を至近距離（14m）、高さをアイレベル（2.4m）に維持
        SetBossDistanceOffset(boss, kCinematicBossDistance);
        SetBossOffsetY(boss, kCinematicBossOffsetY);
    }

    bool IsCompleted() const override {
        return isCompleted_;
    }

private:
    float elapsedTime_ = 0.0f;
    float targetDuration_ = 1.10f; // 実時間1.10秒
    float burstTimer_ = 0.0f;
    int burstCount_ = 0;
    bool isCompleted_ = false;

    Irufemi::ConstantBuffer<Irufemi::LightningConfig>* lightningBuffer_ = nullptr;
    Irufemi::LightningConfig config_{};
};

/**
 * @class DeathStepImplosionAndEMP
 * @brief 【第3幕】息を呑む爆縮 ＆ 高密度Voxel相転移 ＆ EMP真空タメ（1.05s〜1.40s）
 */
class DeathStepImplosionAndEMP : public IDeathSequenceStep {
public:
    void Enter(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        elapsedTime_ = 0.0f;
        isCompleted_ = false;

        if (!boss || !boss->GetGameObject() || !boss->GetEngine()) {
            isCompleted_ = true;
            return;
        }

        // 1. カメラFOVを超望遠ズーム状態（18度）に維持
        SetCinematicCameraFov(visualizer, boss, kCinematicZoomFov);

        // 2. ボスメッシュの完全非表示化（ディゾルブが完了し、Voxelへと相転移）
        auto hideRecursive = [](auto self, Irufemi::GameObject* obj) -> void {
            if (!obj) {
                return;
            }
            if (auto* r = obj->GetComponent<Irufemi::MeshRendererComponent>()) {
                r->SetVisible(false);
            }
            if (auto* r = obj->GetComponent<Irufemi::SkinnedMeshRendererComponent>()) {
                r->SetVisible(false);
            }
            if (auto* em = obj->GetComponent<Irufemi::EffectMaskComponent>()) {
                em->SetEnableEffectMask(false);
            }
            for (const auto& child : obj->GetChildren()) {
                if (child) {
                    self(self, child.get());
                }
            }
        };
        hideRecursive(hideRecursive, boss->GetGameObject());

        // 3. 眼前の接近位置に高密度（12x12x12、千個超）ボクセルボスを実体化
        auto transform = boss->GetGameObject()->GetTransform();
        basePos_ = transform ? transform->GetWorldPosition() : Irufemi::Vector3{0.0f, 0.0f, 0.0f};
        baseRot_ = transform ? transform->GetRotation() : Irufemi::Vector3{0.0f, 0.0f, 0.0f};
        baseScale_ = transform ? transform->GetScale() : Irufemi::Vector3{1.0f, 1.0f, 1.0f};

        if (auto voxelMgr = boss->GetEngine()->GetVoxelParticleManager()) {
            staticHandle_ =
                voxelMgr->RegisterEmitter("resources/model/BossRing/BossRing.obj", Irufemi::Vector3Int{20, 20, 20});
            if (staticHandle_.IsValid()) {
                Irufemi::Vector4 voxelColor =
                    visualizer ? visualizer->GetDeathVoxelColor() : Irufemi::Vector4{9.5f, 2.0f, 8.0f, 1.0f};
                emitterParams_ = Irufemi::VoxelEmitter{};
                emitterParams_.emit = 1;
                emitterParams_.emitPosition = basePos_;
                emitterParams_.rotate = baseRot_;
                emitterParams_.scale = {0.95f, 0.95f, 0.95f}; // 超高密度分割（数千個）による緻密な造形美
                emitterParams_.lifeTime = 3.0f;
                emitterParams_.dispersion = 0.5f;
                emitterParams_.convergence = 55.0f; // 中心点へ向かって猛烈に吸い込まれる立体爆縮！
                emitterParams_.gravity = 0.0f;
                emitterParams_.startColor = Irufemi::Vector4{voxelColor.x * 0.7f, voxelColor.y * 0.7f,
                                                             voxelColor.z * 0.7f, 1.0f}; // ボス固有のヴォイドマゼンタ
                emitterParams_.endColor =
                    Irufemi::Vector4{voxelColor.x * 0.35f, voxelColor.y * 0.35f, voxelColor.z * 0.35f, 1.0f};
                voxelMgr->UpdateEmitterData(staticHandle_, emitterParams_);
            }
        }

        // 4. ポストプロセス LightningCataclysm: 大爆発ドーナツは出さず、真空の収縮放電タメを演出
        if (auto ppm = boss->GetEngine()->GetPostProcessManager()) {
            auto& lc = ppm->GetLightningCataclysmParams();
            lc.center = CalculateBossScreenUV(boss);
            lc.arcIntensity = 1.0f;
            lc.empWaveRadius = 0.0f;
            lc.empWaveIntensity = 0.0f;
            lc.electricChromatic = 0.015f;
            if (visualizer) {
                lc.lightningColor = visualizer->GetDeathLightningColor();
                lc.flameColor = visualizer->GetDeathBlastColor();
            }
        }

        SoundHelper::Play(GameSE::GravityWarp, 0.85f, 1.45f);
    }

    void Update(BossComponent* boss, BossDamageVisualizerComponent* visualizer, float dt) override {
        (void)visualizer;
        elapsedTime_ += dt;
        float progress = std::clamp(elapsedTime_ / targetDuration_, 0.0f, 1.0f);

        if (boss && boss->GetEngine()) {
            Irufemi::Vector4 voxelColor =
                visualizer ? visualizer->GetDeathVoxelColor() : Irufemi::Vector4{9.5f, 2.0f, 8.0f, 1.0f};

            // 数千個のボクセルが convergence = 55.0 -> 110.0 によって中心に向かって立体的に吸い込まれる！
            if (staticHandle_.IsValid()) {
                if (auto voxelMgr = boss->GetEngine()->GetVoxelParticleManager()) {
                    emitterParams_.convergence = std::lerp(55.0f, 110.0f, progress);
                    float boost = std::lerp(0.7f, 1.4f, progress);
                    emitterParams_.startColor =
                        Irufemi::Vector4{voxelColor.x * boost, voxelColor.y * boost, voxelColor.z * boost, 1.0f};
                    voxelMgr->UpdateEmitterData(staticHandle_, emitterParams_);
                }
            }

            if (auto ppm = boss->GetEngine()->GetPostProcessManager()) {
                auto& lc = ppm->GetLightningCataclysmParams();
                lc.center = CalculateBossScreenUV(boss);
                lc.empWaveRadius = 0.0f;
                lc.empWaveIntensity = 0.0f;
                lc.arcIntensity = std::lerp(1.0f, 3.0f, progress);
                lc.electricChromatic = std::lerp(0.015f, 0.035f, progress);
                lc.time += dt * 2.0f;
                if (visualizer) {
                    lc.lightningColor = visualizer->GetDeathLightningColor();
                    lc.flameColor = visualizer->GetDeathBlastColor();
                }
            }
        }

        if (elapsedTime_ >= targetDuration_) {
            isCompleted_ = true;
        }
    }

    void Exit(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        (void)visualizer;
        if (boss && boss->GetEngine()) {
            if (auto voxelMgr = boss->GetEngine()->GetVoxelParticleManager()) {
                if (staticHandle_.IsValid()) {
                    voxelMgr->UnregisterEmitter(staticHandle_);
                }
            }
        }
    }

    bool IsCompleted() const override {
        return isCompleted_;
    }

private:
    float elapsedTime_ = 0.0f;
    float targetDuration_ = 0.45f; // 実時間0.45秒の息を呑むタメ
    bool isCompleted_ = false;
    Irufemi::VoxelParticleManager::EmitterHandle staticHandle_{};
    Irufemi::Vector3 basePos_{};
    Irufemi::Vector3 baseRot_{};
    Irufemi::Vector3 baseScale_{};
    Irufemi::VoxelEmitter emitterParams_{};
};

/**
 * @class DeathStepVoxelCataclysm
 * @brief 【第4幕】超高圧プラズマ超爆発 ＆ 千個超Voxelクーロン大爆散（dispersion=85.0） ＆ 全景展開（1.40s〜2.80s）
 */
class DeathStepVoxelCataclysm : public IDeathSequenceStep {
public:
    struct SubExplosionDef {
        float triggerTime = 0.0f;
        Irufemi::Vector3 offset{};
        GameSE se = GameSE::DebrisShatter;
        float seVolume = 1.0f;
        float sePitch = 1.0f;
    };

    static constexpr float kMainExplosionDelay = 0.26f;

    inline static const std::array<SubExplosionDef, 2> kSubExplosions = {{
        {0.09f, {2.8f, -1.2f, -0.6f}, GameSE::DebrisShatter, 0.95f, 1.15f},
        {0.18f, {0.6f, 2.8f, 0.8f}, GameSE::GravityWarp, 0.85f, 1.55f},
    }};

    void Enter(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        elapsedTime_ = 0.0f;
        chainStep_ = 0;
        mainExploded_ = false;
        isCompleted_ = false;

        if (!boss || !boss->GetGameObject() || !boss->GetEngine()) {
            isCompleted_ = true;
            return;
        }

        deadPos_ = boss->GetGameObject()->GetTransform()->GetWorldPosition();

        // 1. 【第1連鎖誘爆】左翼装甲で先行爆裂！
        PlaySubExplosion({-2.6f, 1.2f, 0.5f}, GameSE::DebrisShatter, 0.85f, 1.40f);
    }

    void PlaySubExplosion(const Irufemi::Vector3& offset, GameSE se, float volume, float pitch) {
        Irufemi::Vector3 subPos = deadPos_ + offset;
        if (auto effectMgr = EffectManagerComponent::GetInstance()) {
            effectMgr->PlayEffect("Hit", subPos);
        }
        SoundHelper::Play(se, volume, pitch);
    }

    void TriggerMainExplosion(BossComponent* boss, BossDamageVisualizerComponent* visualizer) {
        if (!boss || !boss->GetGameObject() || !boss->GetEngine()) {
            return;
        }

        // 0. 爆縮タメから一気に通常速度（1.0f）へ解放！起爆の瞬発力とカタルシスを最大化！
        boss->GetEngine()->SetTimeScale(1.0f);

        // 1. 特大カメラシェイク発火
        if (visualizer) {
            visualizer->TriggerDeathShake();
        }

        // 2. 3D空間多層爆発エフェクト（大火球・高密度スパーク・立ち昇る噴煙・3Dリング）を爆心地に炸裂！
        if (auto effectMgr = EffectManagerComponent::GetInstance()) {
            effectMgr->PlayEffect("BossExplosion", deadPos_);
        }

        // 3. 超高密度（20x20x20、数千個）ボクセルの3D全方位大爆散（白熱マゼンタから金属煤へと冷えながら飛散）！
        if (auto voxelMgr = boss->GetEngine()->GetVoxelParticleManager()) {
            Irufemi::Vector4 voxelColor =
                visualizer ? visualizer->GetDeathVoxelColor() : Irufemi::Vector4{9.5f, 2.0f, 8.0f, 1.0f};

            Irufemi::VoxelEmitter p{};
            p.lifeTime = 3.2f;
            p.gravity = 1.2f;
            p.dispersion = 75.0f; // カメラ手前・奥・全方位へ圧倒的な3D深度で超高速大爆散！
            p.particleType = 5;   // DebrisExplosive
            p.scale = {1.1f, 1.1f, 1.1f};
            p.startColor = voxelColor;                // ボス固有の暴走マゼンタプラズマ
            p.endColor = {0.05f, 0.05f, 0.08f, 1.0f}; // 飛散して冷え切った機体の金属煤
            p.dissolveEdgeColor = {voxelColor.x * 0.8f, voxelColor.y * 0.4f, voxelColor.z * 0.5f, 1.0f};

            auto transform = boss->GetGameObject()->GetTransform();
            Irufemi::Vector3 deadRot = transform ? transform->GetRotation() : Irufemi::Vector3{0.0f, 0.0f, 0.0f};
            Irufemi::Vector3 deadScale = transform ? transform->GetScale() : Irufemi::Vector3{1.0f, 1.0f, 1.0f};

            voxelMgr->PlayExplosion("resources/model/BossRing/BossRing.obj", deadPos_,
                                    Irufemi::Vector3{0.0f, 0.0f, 0.0f}, deadRot, deadScale, p,
                                    Irufemi::Vector3Int{20, 20, 20});
        }

        // 4. ポストプロセス LightningCataclysm: 瞬間閃光 ＆ 局所屈折衝撃波リング ＆ 連動アーク放電
        // （※全画面を歪ませず、ボスの周囲0.5以内の局所リングのみを上品に歪ませ、全画面色収差は0に維持）
        if (auto ppm = boss->GetEngine()->GetPostProcessManager()) {
            auto& lc = ppm->GetLightningCataclysmParams();
            lc.center = CalculateWorldScreenUV(boss->GetEngine(), deadPos_);
            lc.plasmaFlashIntensity = 0.50f; // ドカンと炸裂する瞬間閃光
            lc.arcIntensity = 6.5f;
            lc.empWaveRadius = 0.05f;    // 局所衝撃波リングの起点
            lc.empWaveIntensity = 0.40f; // 全画面ではなくボスの周囲のみを歪ませる適度な強度
            lc.electricChromatic = 0.0f; // 全画面色収差は完全オフ（視界・UIを明瞭に維持）
            if (visualizer) {
                lc.flameColor = visualizer->GetDeathBlastColor();
                lc.lightningColor = visualizer->GetDeathLightningColor();
            }
        }

        // 重低音インパクト ＆ デブリ破砕音の多重再生によるド迫力爆発音
        SoundHelper::Play(GameSE::ResultImpact, 1.0f, 0.80f);
        SoundHelper::Play(GameSE::DebrisShatter, 1.0f, 0.85f);
    }

    void Update(BossComponent* boss, BossDamageVisualizerComponent* visualizer, float dt) override {
        elapsedTime_ += dt;

        if (!boss || !boss->GetEngine()) {
            return;
        }

        // --- 多段連鎖爆発シークエンス（ドカン！ ➔ ドカン！ ➔ ドカン！ ➔ ドッカァァァン！！） ---
        while (chainStep_ < static_cast<int>(kSubExplosions.size()) &&
               elapsedTime_ >= kSubExplosions[chainStep_].triggerTime) {
            const auto& step = kSubExplosions[chainStep_];
            PlaySubExplosion(step.offset, step.se, step.seVolume, step.sePitch);
            ++chainStep_;
        }

        if (elapsedTime_ >= kMainExplosionDelay && !mainExploded_) {
            mainExploded_ = true;
            TriggerMainExplosion(boss, visualizer);
        }

        // 本爆発後の経過時間
        float postExplodeTime = elapsedTime_ - kMainExplosionDelay;

        if (mainExploded_) {
            auto ppm = boss->GetEngine()->GetPostProcessManager();
            if (ppm) {
                auto& lc = ppm->GetLightningCataclysmParams();
                bool isVisible = false;
                lc.center = CalculateWorldScreenUV(boss->GetEngine(), deadPos_, &isVisible);

                if (!isVisible) {
                    lc.empWaveIntensity = 0.0f;
                    lc.arcIntensity = 0.0f;
                    lc.plasmaFlashIntensity = 0.0f;
                } else {
                    // 1. 瞬間閃光：0.08秒で素早く引き、大爆発の全貌を露出
                    lc.plasmaFlashIntensity = (std::max)(0.0f, 0.50f * (1.0f - postExplodeTime / 0.08f));

                    // 2. ボス周囲の局所衝撃波リング：最初の0.35秒でボスの周囲（半径0.65）まで広がり0.38秒でフェード
                    // （※画面外へ無限拡大させず、全画面色収差は完全オフにして視界とUIを明瞭に維持）
                    float waveTime = (std::min)(postExplodeTime / 0.35f, 1.0f);
                    float easeOutWave = 1.0f - std::pow(1.0f - waveTime, 3.0f);
                    lc.empWaveRadius = std::lerp(0.05f, 0.65f, easeOutWave);
                    float waveFade = (std::max)(0.0f, 1.0f - postExplodeTime / 0.38f);
                    lc.empWaveIntensity = waveFade * waveFade * 0.40f;
                    lc.electricChromatic = 0.0f;

                    // 3. 連動アーク放電：0.40秒で収束
                    float arcFade = (std::max)(0.0f, 1.0f - postExplodeTime / 0.40f);
                    lc.arcIntensity = arcFade * arcFade * 6.5f;
                    lc.time += dt * 5.0f;
                    if (visualizer) {
                        lc.flameColor = visualizer->GetDeathBlastColor();
                        lc.lightningColor = visualizer->GetDeathLightningColor();
                    }
                }
            }

            // 4. カメラFOVを通常角へスムーズにズームアウトしながら余韻を見送る
            if (postExplodeTime > 0.30f) {
                float zoomOutProgress = (std::min)((postExplodeTime - 0.30f) / 1.30f, 1.0f);
                float smoothZoom = SmoothStep(zoomOutProgress);

                float currentFov = std::lerp(kCinematicZoomFov, kDefaultFov, smoothZoom);
                SetCinematicCameraFov(visualizer, boss, currentFov);
            }
        }

        if (elapsedTime_ >= targetDuration_) {
            isCompleted_ = true;
        }
    }

    void Exit(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        SetCinematicCameraFov(visualizer, boss, kDefaultFov);
    }

    bool IsCompleted() const override {
        return isCompleted_;
    }

private:
    float elapsedTime_ = 0.0f;
    float targetDuration_ = 2.40f; // 多段連鎖爆発＋本爆発＋雄大な余韻を見守る2.4秒
    int chainStep_ = 0;
    bool mainExploded_ = false;
    bool isCompleted_ = false;
    Irufemi::Vector3 deadPos_{};
};

/**
 * @class DeathStepFinishSequence
 * @brief 【第5幕】ポストプロセス解除 ＆ リザルト即時通知 ＆ ボス非アクティブ化
 */
class DeathStepFinishSequence : public IDeathSequenceStep {
public:
    void Enter(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        isCompleted_ = true;

        // 1. ポストプロセス LightningCataclysm を解除 & アウトラインミュート解除
        if (boss && boss->GetEngine()) {
            if (auto ppm = boss->GetEngine()->GetPostProcessManager()) {
                ppm->RemoveActiveMode(PostProcessMode::LightningCataclysm);
                ppm->SetOutlineMuted(false);
            }
        }

        // 2. カメラFOVとボス追従距離・高さを通常状態に確実に復元
        SetCinematicCameraFov(visualizer, boss, kDefaultFov);
        SetBossDistanceOffset(boss, kDefaultBossDistance);
        SetBossOffsetY(boss, kDefaultBossOffsetY);

        // 3. タイムスケールを完全復帰 (1.0f)
        if (boss && boss->GetEngine()) {
            boss->GetEngine()->SetTimeScale(1.0f);
        }

        // 4. リザルト画面遷移を即座に通知
        if (boss) {
            boss->NotifyDeathSequenceFinished();
        }

        // 5. 通知完了後にボス本体を非アクティブ化
        if (boss && boss->GetGameObject()) {
            boss->GetGameObject()->SetIsActive(false);
        }
    }

    void Update(BossComponent* boss, BossDamageVisualizerComponent* visualizer, float dt) override {
        (void)boss;
        (void)visualizer;
        (void)dt;
    }

    bool IsCompleted() const override {
        return isCompleted_;
    }

private:
    bool isCompleted_ = false;
};

/**
 * @brief ボス撃破演出に伴うシネマティック・アイソレーション
 * @details 戦闘HUD（レティクル・ロックオン等）や画面内の敵弾を一掃し、ボスの劇的な崩壊演出にフォーカスする
 * @note アウトライン（自機・雑魚敵等）の非表示化は、個別オブジェクト操作ではなく
 * PostProcessManager::SetOutlineMuted(true) で元栓一括遮断
 */
void IsolateCinematicScene(BossComponent* boss) {
    if (!boss || !boss->GetGameObject()) {
        return;
    }
    auto scene = boss->GetGameObject()->GetScene();
    if (!scene) {
        return;
    }

    // 1. 戦闘用HUD（レティクル、ロックオン枠、HPバー、スコアHUD）の非表示化
    if (auto reticle = scene->FindGameObject("Reticle")) {
        reticle->SetIsActive(false);
    }
    if (auto lockon = scene->FindGameObject("LockonMarkerUI")) {
        lockon->SetIsActive(false);
        if (auto markerComp = lockon->GetComponent<LockonMarkerUIComponent>()) {
            markerComp->SyncTargets({});
        }
    }
    if (auto statusHud = scene->FindGameObject("PlayerStatusHUD")) {
        statusHud->SetIsActive(false);
    }
    if (auto scoreHud = scene->FindGameObject("ScoreHUD")) {
        scoreHud->SetIsActive(false);
    }

    // 2. 自機ターゲティングの無効化
    if (auto player = scene->FindGameObject("Player")) {
        if (auto targeting = player->GetComponent<PlayerTargetingComponent>()) {
            targeting->SetEnabled(false);
        }
    }

    // 3. 画面内の敵弾の一括クリア
    if (auto enemyBulletMgr = scene->FindGameObject("EnemyBulletManager")) {
        if (auto ebm = enemyBulletMgr->GetComponent<EnemyBulletManagerComponent>()) {
            ebm->AbsorbAndClearAllBullets();
        }
    }
    if (auto bossBulletMgr = scene->FindGameObject("BossBulletManager")) {
        if (auto bbm = bossBulletMgr->GetComponent<BossBulletManagerComponent>()) {
            bbm->ClearAllBullets();
        }
    }
}

} // namespace

void BossStateDestroyed::Enter(BossComponent* boss) {
    Irufemi::Log::OutPutLog(std::cout, "Boss Destroyed! Starting lightning cataclysm death sequence...\n");

    // ポストプロセス層のアウトラインを一括ミュート（自機・敵機等の輪郭線貫通をパイプライン元栓で完全遮断）
    if (boss && boss->GetEngine()) {
        if (auto ppm = boss->GetEngine()->GetPostProcessManager()) {
            ppm->SetOutlineMuted(true);
        }
    }

    // 戦闘HUD非表示・敵弾一掃（シネマティック・アイソレーション）
    IsolateCinematicScene(boss);

    // 付随オブジェクト（ドローン群・シールド・弾幕）を一括回収・停止
    if (boss) {
        boss->CleanupSubEntities();
    }

    visualizer_ = nullptr;
    if (boss && boss->GetGameObject()) {
        visualizer_ = boss->GetGameObject()->GetComponent<BossDamageVisualizerComponent>();
    }

    steps_.clear();
    currentStepIndex_ = 0;

    // 5幕構成のシネマティック撃破シーケンス
    steps_.push_back(std::make_unique<DeathStepOvervoltageFreeze>()); // 第1幕: ヒットストップ & 静寂 & コア白熱
    steps_.push_back(
        std::make_unique<DeathStepLightningCrescendo>()); // 第2幕: 物理ドリーイン & 超望遠ズーム & 放電クレッシェンド
    steps_.push_back(std::make_unique<DeathStepImplosionAndEMP>()); // 第3幕: 息を呑む爆縮 & 高密度Voxel相転移 & EMPタメ
    steps_.push_back(std::make_unique<DeathStepVoxelCataclysm>());  // 第4幕: 千個超Voxelクーロン大爆散 & プラズマ超閃光
    steps_.push_back(std::make_unique<DeathStepFinishSequence>());  // 第5幕: 完了通知 & ResultScene即時接続

    if (!steps_.empty()) {
        steps_[currentStepIndex_]->Enter(boss, visualizer_);
    }
}

void BossStateDestroyed::Update(BossComponent* boss) {
    if (currentStepIndex_ >= steps_.size()) {
        return;
    }

    if (!boss || !boss->GetEngine()) {
        return;
    }

    float dt = boss->GetEngine()->GetRealDeltaTime();
    auto& currentStep = steps_[currentStepIndex_];
    currentStep->Update(boss, visualizer_, dt);

    if (currentStep->IsCompleted()) {
        currentStep->Exit(boss, visualizer_);
        currentStepIndex_++;
        if (currentStepIndex_ < steps_.size()) {
            steps_[currentStepIndex_]->Enter(boss, visualizer_);
        }
    }
}

void BossStateDestroyed::Exit(BossComponent* boss) {
    // 撃破ステート離脱時、確実にアウトラインミュートを解除
    if (boss && boss->GetEngine()) {
        if (auto ppm = boss->GetEngine()->GetPostProcessManager()) {
            ppm->SetOutlineMuted(false);
        }
    }
}

void BossStateDestroyed::OnTakeDamage(BossComponent* boss, float damage) {
    (void)boss;
    (void)damage;
}
