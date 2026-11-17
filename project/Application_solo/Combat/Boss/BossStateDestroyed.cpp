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
#include <iostream>
#include <cmath>
#include <algorithm>

namespace {

constexpr float kDefaultFov = 0.785398f;         //!< 通常カメラFOV (45度)
constexpr float kCinematicZoomFov = 0.453786f;    //!< 撃破時シネマティックズームFOV (26度)
constexpr float kDefaultBossDistance = 80.0f;    //!< 通常時のボス追従距離 (80m)
constexpr float kCinematicBossDistance = 14.0f;  //!< 撃破演出時の至近追従距離 (14m、眼前を埋め尽くす)
constexpr float kDefaultBossOffsetY = 10.0f;     //!< 通常時のボス高さオフセット (10m)
constexpr float kCinematicBossOffsetY = 2.4f;    //!< 撃破演出時のアイレベル高さオフセット (2.4m、カメラ正面に配置)

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
    return Irufemi::Vector3{
        std::lerp(a.x, b.x, t),
        std::lerp(a.y, b.y, t),
        std::lerp(a.z, b.z, t)
    };
}

/**
 * @brief ボスのワールド座標をスクリーンUV座標（0.0〜1.0）へ射影変換する
 */
inline Irufemi::Vector2 CalculateBossScreenUV(BossComponent* boss) {
    if (!boss || !boss->GetGameObject() || !boss->GetEngine()) {
        return Irufemi::Vector2{0.5f, 0.48f};
    }
    auto engine = boss->GetEngine();
    auto camMgr = engine->GetCameraManager();
    auto activeCam = camMgr ? camMgr->GetActiveCamera() : nullptr;
    if (!activeCam) {
        return Irufemi::Vector2{0.5f, 0.48f};
    }

    Irufemi::Vector3 worldPos = boss->GetGameObject()->GetTransform()->GetWorldPosition();
    const auto& viewProj = activeCam->GetViewProjectionMatrix3D();

    float w = worldPos.x * viewProj.m[0][3] + worldPos.y * viewProj.m[1][3] + worldPos.z * viewProj.m[2][3] + viewProj.m[3][3];
    if (w <= 0.001f) {
        return Irufemi::Vector2{0.5f, 0.48f};
    }

    Irufemi::Vector3 clipPos = Irufemi::Math::Transform(worldPos, viewProj);
    return Irufemi::Vector2{
        std::clamp(clipPos.x * 0.5f + 0.5f, 0.05f, 0.95f),
        std::clamp(-clipPos.y * 0.5f + 0.5f, 0.05f, 0.95f)
    };
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
        if (dxCommon) {
            lightningBuffer_.Initialize(dxCommon);
            config_ = Irufemi::LightningConfig{};
            config_.density = 1.2f;
            config_.boltCount = 1.5f;
            config_.thickness = 3.5f;
            config_.speed = 2.5f;
            config_.intensity = 0.8f;
            config_.branchAmount = 0.6f;
            config_.coreColor = Irufemi::Vector4{1.2f, 1.2f, 1.5f, 1.0f};
            config_.haloColor = Irufemi::Vector4{0.3f, 0.8f, 2.5f, 1.0f};
            lightningBuffer_.UpdateAll(config_);

            // ボスおよび子オブジェクトのMeshRendererにBossLightning PSOとCBVアドレスを設定
            uint32_t frameIndex = dxCommon->GetCurrentBackBufferIndex();
            D3D12_GPU_VIRTUAL_ADDRESS cbvAddr = lightningBuffer_.GetGPUVirtualAddress(frameIndex);

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
            Irufemi::Vector3 shake{
                Irufemi::Random::GeneratorFloat(-jitter, jitter),
                Irufemi::Random::GeneratorFloat(-jitter, jitter),
                Irufemi::Random::GeneratorFloat(-jitter, jitter)
            };
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

        if (dxCommon) {
            uint32_t frameIndex = dxCommon->GetCurrentBackBufferIndex();
            lightningBuffer_.Update(config_, frameIndex);

            D3D12_GPU_VIRTUAL_ADDRESS cbvAddr = lightningBuffer_.GetGPUVirtualAddress(frameIndex);
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

        // 4. ポストプロセス LightningCataclysm を駆動（アーク放電が空間を裂く）
        if (auto ppm = engine->GetPostProcessManager()) {
            auto& lc = ppm->GetLightningCataclysmParams();
            lc.center = CalculateBossScreenUV(boss);
            lc.arcIntensity = std::lerp(0.0f, 3.8f, progress * progress);
            lc.electricChromatic = std::lerp(0.0f, 0.030f, progress);
            lc.empWaveRadius = progress * 0.45f;
            lc.empWaveIntensity = progress * 0.85f;
            lc.time += dt * 3.5f;
        }

        // 5. 連鎖小爆発パーティクル（0.10秒おき）
        burstTimer_ += dt;
        if (burstTimer_ >= 0.10f && burstCount_ < 9) {
            burstTimer_ = 0.0f;
            burstCount_++;

            if (auto effectMgr = EffectManagerComponent::GetInstance()) {
                Irufemi::Vector3 curPos = boss->GetGameObject()->GetTransform()->GetWorldPosition();
                Irufemi::Vector3 offset{
                    Irufemi::Random::GeneratorFloat(-4.5f, 4.5f),
                    Irufemi::Random::GeneratorFloat(-3.5f, 3.5f),
                    Irufemi::Random::GeneratorFloat(-4.5f, 4.5f)
                };
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

    Irufemi::ConstantBuffer<Irufemi::LightningConfig> lightningBuffer_;
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
            if (!obj) return;
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
                if (child) self(self, child.get());
            }
        };
        hideRecursive(hideRecursive, boss->GetGameObject());

        // 3. 眼前の接近位置に高密度（12x12x12、千個超）ボクセルボスを静止実体化（共振タメ）
        auto transform = boss->GetGameObject()->GetTransform();
        Irufemi::Vector3 deadPos = transform ? transform->GetWorldPosition() : Irufemi::Vector3{0.0f, 0.0f, 0.0f};
        Irufemi::Vector3 deadRot = transform ? transform->GetRotation() : Irufemi::Vector3{0.0f, 0.0f, 0.0f};
        Irufemi::Vector3 deadScale = transform ? transform->GetScale() : Irufemi::Vector3{1.0f, 1.0f, 1.0f};

        if (auto voxelMgr = boss->GetEngine()->GetVoxelParticleManager()) {
            staticHandle_ = voxelMgr->RegisterEmitter("resources/model/BossRing/BossRing.obj", Irufemi::Vector3Int{12, 12, 12});
            if (staticHandle_.IsValid()) {
                Irufemi::VoxelEmitter p{};
                p.emit = 1;
                p.emitPosition = deadPos;
                p.rotate = deadRot;
                p.scale = deadScale;
                p.lifeTime = 3.0f;
                p.dispersion = 0.35f; // その場で微小振動（電磁共振）
                p.gravity = 0.0f;
                p.startColor = {3.5f, 4.5f, 7.0f, 1.0f}; // 超高圧青白プラズマ発光
                p.endColor = {0.3f, 0.7f, 1.8f, 1.0f};
                voxelMgr->UpdateEmitterData(staticHandle_, p);
            }
        }

        // 4. ポストプロセス LightningCataclysm: 放電を抑えて真空の収縮タメを演出
        if (auto ppm = boss->GetEngine()->GetPostProcessManager()) {
            auto& lc = ppm->GetLightningCataclysmParams();
            lc.center = CalculateBossScreenUV(boss);
            lc.arcIntensity = 0.6f;
            lc.empWaveRadius = 0.35f;
            lc.empWaveIntensity = 1.2f;
        }

        SoundHelper::Play(GameSE::GravityWarp, 0.85f, 1.45f);
    }

    void Update(BossComponent* boss, BossDamageVisualizerComponent* visualizer, float dt) override {
        (void)visualizer;
        elapsedTime_ += dt;
        float progress = std::clamp(elapsedTime_ / targetDuration_, 0.0f, 1.0f);

        if (boss && boss->GetEngine()) {
            if (auto ppm = boss->GetEngine()->GetPostProcessManager()) {
                auto& lc = ppm->GetLightningCataclysmParams();
                lc.center = CalculateBossScreenUV(boss);
                // EMP波面の急速爆縮・収縮
                lc.empWaveRadius = std::lerp(0.35f, 0.02f, progress);
                lc.empWaveIntensity = std::lerp(1.2f, 0.1f, progress);
                lc.electricChromatic = std::lerp(0.030f, 0.010f, progress);
                lc.time += dt * 1.5f;
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
};

/**
 * @class DeathStepVoxelCataclysm
 * @brief 【第4幕】超高圧プラズマ超爆発 ＆ 千個超Voxelクーロン大爆散（dispersion=85.0） ＆ 全景展開（1.40s〜2.80s）
 */
class DeathStepVoxelCataclysm : public IDeathSequenceStep {
public:
    void Enter(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        elapsedTime_ = 0.0f;
        isCompleted_ = false;

        if (!boss || !boss->GetGameObject() || !boss->GetEngine()) {
            isCompleted_ = true;
            return;
        }

        Irufemi::Vector3 deadPos = boss->GetGameObject()->GetTransform()->GetWorldPosition();

        // 1. 特大カメラシェイク発火
        if (visualizer) {
            visualizer->TriggerDeathShake();
        }

        // 2. 高密度（12x12x12、千個超）ボクセルの一斉超高速飛散爆発（dispersion = 85.0f）！
        if (auto voxelMgr = boss->GetEngine()->GetVoxelParticleManager()) {
            Irufemi::VoxelEmitter p{};
            p.lifeTime = 3.2f;
            p.gravity = 2.8f;
            p.dispersion = 85.0f; // 画面全体（カメラ方向にも）超高速クーロン爆散！
            p.particleType = 5;   // DebrisExplosive
            p.scale = {1.2f, 1.2f, 1.2f};
            p.startColor = {4.5f, 5.5f, 9.0f, 1.0f}; // 超発光
            p.endColor = {0.05f, 0.15f, 0.35f, 1.0f};

            auto transform = boss->GetGameObject()->GetTransform();
            Irufemi::Vector3 deadRot = transform ? transform->GetRotation() : Irufemi::Vector3{0.0f, 0.0f, 0.0f};
            Irufemi::Vector3 deadScale = transform ? transform->GetScale() : Irufemi::Vector3{1.0f, 1.0f, 1.0f};

            voxelMgr->PlayExplosion("resources/model/BossRing/BossRing.obj", deadPos,
                                    Irufemi::Vector3{0.0f, 0.0f, 0.0f}, deadRot, deadScale,
                                    p, Irufemi::Vector3Int{12, 12, 12});
        }

        // 3. ポストプロセス LightningCataclysm: 全画面純白プラズマフラッシュ ＆ EMP衝撃波大爆発
        if (auto ppm = boss->GetEngine()->GetPostProcessManager()) {
            auto& lc = ppm->GetLightningCataclysmParams();
            lc.center = CalculateBossScreenUV(boss);
            lc.plasmaFlashIntensity = 4.5f; // 純白フラッシュ
            lc.arcIntensity = 4.2f;
            lc.empWaveRadius = 0.0f;
            lc.empWaveIntensity = 3.5f;
            lc.electricChromatic = 0.065f;
        }

        // 4. GPU衝撃波パーティクル再生
        if (auto effectMgr = EffectManagerComponent::GetInstance()) {
            effectMgr->PlayEffect("debris_dust_effect", deadPos);
        }

        SoundHelper::Play(GameSE::DebrisShatter, 1.0f, 0.85f);
    }

    void Update(BossComponent* boss, BossDamageVisualizerComponent* visualizer, float dt) override {
        elapsedTime_ += dt;
        float progress = std::clamp(elapsedTime_ / targetDuration_, 0.0f, 1.0f);

        if (!boss || !boss->GetEngine()) {
            return;
        }

        auto ppm = boss->GetEngine()->GetPostProcessManager();
        if (ppm) {
            auto& lc = ppm->GetLightningCataclysmParams();
            lc.center = CalculateBossScreenUV(boss);
            // プラズマ白熱フラッシュの急速減衰（0.30秒で減衰）
            lc.plasmaFlashIntensity = (std::max)(0.0f, 4.5f * (1.0f - progress / 0.25f));
            // EMP波面の全画面大爆進
            lc.empWaveRadius = progress * 2.8f;
            lc.empWaveIntensity = 3.5f * (1.0f - progress);
            lc.arcIntensity = 4.2f * (1.0f - progress);
            lc.electricChromatic = 0.065f * (1.0f - progress);
            lc.time += dt * 4.0f;
        }

        // 爆散中盤（0.45s以降）にかけてカメラFOVを26度から通常角45度へスムーズにズームアウト、およびボス追従距離・高さを復帰
        if (progress > 0.45f) {
            float zoomOutProgress = (progress - 0.45f) / 0.55f;
            float smoothZoom = SmoothStep(zoomOutProgress);

            float currentFov = std::lerp(kCinematicZoomFov, kDefaultFov, smoothZoom);
            SetCinematicCameraFov(visualizer, boss, currentFov);

            float currentDist = std::lerp(kCinematicBossDistance, kDefaultBossDistance, smoothZoom);
            SetBossDistanceOffset(boss, currentDist);

            float currentOffsetY = std::lerp(kCinematicBossOffsetY, kDefaultBossOffsetY, smoothZoom);
            SetBossOffsetY(boss, currentOffsetY);
        }

        if (elapsedTime_ >= targetDuration_) {
            isCompleted_ = true;
        }
    }

    void Exit(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        // カメラFOVとボス追従距離・高さを通常状態に確定
        SetCinematicCameraFov(visualizer, boss, kDefaultFov);
        SetBossDistanceOffset(boss, kDefaultBossDistance);
        SetBossOffsetY(boss, kDefaultBossOffsetY);
    }

    bool IsCompleted() const override {
        return isCompleted_;
    }

private:
    float elapsedTime_ = 0.0f;
    float targetDuration_ = 1.60f; // 実時間1.60秒
    bool isCompleted_ = false;
};

/**
 * @class DeathStepFinishSequence
 * @brief 【第5幕】ポストプロセス解除 ＆ リザルト即時通知 ＆ ボス非アクティブ化
 */
class DeathStepFinishSequence : public IDeathSequenceStep {
public:
    void Enter(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        isCompleted_ = true;

        // 1. ポストプロセス LightningCataclysm を解除
        if (boss && boss->GetEngine()) {
            if (auto ppm = boss->GetEngine()->GetPostProcessManager()) {
                ppm->RemoveActiveMode(PostProcessMode::LightningCataclysm);
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

} // namespace

void BossStateDestroyed::Enter(BossComponent* boss) {
    Irufemi::Log::OutPutLog(std::cout, "Boss Destroyed! Starting lightning cataclysm death sequence...\n");

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
    steps_.push_back(std::make_unique<DeathStepOvervoltageFreeze>());  // 第1幕: ヒットストップ & 静寂 & コア白熱
    steps_.push_back(std::make_unique<DeathStepLightningCrescendo>()); // 第2幕: 物理ドリーイン & 超望遠ズーム & 放電クレッシェンド
    steps_.push_back(std::make_unique<DeathStepImplosionAndEMP>());     // 第3幕: 息を呑む爆縮 & 高密度Voxel相転移 & EMPタメ
    steps_.push_back(std::make_unique<DeathStepVoxelCataclysm>());     // 第4幕: 千個超Voxelクーロン大爆散 & プラズマ超閃光
    steps_.push_back(std::make_unique<DeathStepFinishSequence>());     // 第5幕: 完了通知 & ResultScene即時接続

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
    (void)boss;
}

void BossStateDestroyed::OnTakeDamage(BossComponent* boss, float damage) {
    (void)boss;
    (void)damage;
}
