#include "Combat/Boss/BossStateDestroyed.h"
#include "Combat/Boss/BossComponent.h"
#include "Combat/Boss/BossDamageVisualizerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/Log.h"
#include "Audio/SoundHelper.h"
#include "Effects/EffectManagerComponent.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"
#include "Renderer/Data/LightningConfig.h"
#include "RHI/DirectX12/ConstantBuffer.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Core/Math/Random/Random.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace {

/**
 * @class DeathStepOvervoltageFreeze
 * @brief 【第1幕】超電磁ヒットストップ ＆ コア臨界閃光（0.00s〜0.10s）
 */
class DeathStepOvervoltageFreeze : public IDeathSequenceStep {
public:
    void Enter(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        (void)visualizer;
        elapsedTime_ = 0.0f;
        isCompleted_ = false;

        if (boss && boss->GetGameObject()) {
            // トドメの撃破通知
            boss->NotifyBossDied();

            // コアのEffectMaskをHDR純白オーバーブライトに白熱化
            auto brightenMaskRecursive = [](auto self, Irufemi::GameObject* obj) -> void {
                if (!obj) {
                    return;
                }
                if (auto* em = obj->GetComponent<Irufemi::EffectMaskComponent>()) {
                    auto params = em->GetCustomParams();
                    params.color1 = Irufemi::Vector4{5.0f, 5.0f, 5.5f, 1.0f}; // 純白白熱
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

            // 0.10秒間の完全ヒットストップ（マイクロフリーズ）
            if (auto engine = boss->GetEngine()) {
                engine->SetTimeScale(0.01f);
                SoundHelper::Play(GameSE::ResultImpact);
            }
        }
    }

    void Update(BossComponent* boss, BossDamageVisualizerComponent* visualizer, float dt) override {
        (void)boss;
        (void)visualizer;
        elapsedTime_ += dt;
        if (elapsedTime_ >= targetDuration_) {
            isCompleted_ = true;
        }
    }

    bool IsCompleted() const override {
        return isCompleted_;
    }

private:
    float elapsedTime_ = 0.0f;
    float targetDuration_ = 0.10f; // 実時間0.10秒
    bool isCompleted_ = false;
};

/**
 * @class DeathStepLightningCrescendo
 * @brief 【第2幕】電流密度クレッシェンド ＆ 漏電暴走 ＆ 電磁ジッター（0.10s〜0.90s）
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
            config_.density = 1.0f;
            config_.boltCount = 1.0f;
            config_.thickness = 3.0f;
            config_.speed = 2.0f;
            config_.intensity = 0.5f;
            config_.branchAmount = 0.5f;
            config_.coreColor = Irufemi::Vector4{1.0f, 1.0f, 1.2f, 1.0f};
            config_.haloColor = Irufemi::Vector4{0.2f, 0.7f, 2.0f, 1.0f};
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

        if (auto transform = boss->GetGameObject()->GetComponent<Irufemi::TransformComponent>()) {
            originalLocalPos_ = transform->GetPosition();
        }

        // スローモーション（0.12f）を開始
        engine->SetTimeScale(0.12f);
    }

    void Update(BossComponent* boss, BossDamageVisualizerComponent* visualizer, float dt) override {
        (void)visualizer;
        elapsedTime_ += dt;
        float progress = std::clamp(elapsedTime_ / targetDuration_, 0.0f, 1.0f);

        if (!boss || !boss->GetGameObject() || !boss->GetEngine()) {
            isCompleted_ = true;
            return;
        }

        auto engine = boss->GetEngine();
        auto dxCommon = engine->GetDirectXCommon();

        // 1. パラメータのクレッシェンド動的補間
        config_.density = std::lerp(1.0f, 3.8f, progress);
        config_.boltCount = std::lerp(1.0f, 4.5f, progress);
        config_.thickness = std::lerp(3.0f, 6.0f, progress);
        config_.speed = std::lerp(2.0f, 8.0f, progress);
        config_.intensity = std::lerp(0.5f, 7.0f, progress * progress); // 2次加速

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

        // 2. 電磁共振ジッター（装甲の熱暴走振動）
        if (auto transform = boss->GetGameObject()->GetComponent<Irufemi::TransformComponent>()) {
            float jitter = 0.05f + progress * 0.25f;
            Irufemi::Vector3 shake{
                Irufemi::Random::GeneratorFloat(-jitter, jitter),
                Irufemi::Random::GeneratorFloat(-jitter, jitter),
                Irufemi::Random::GeneratorFloat(-jitter, jitter)
            };
            transform->SetPosition(originalLocalPos_ + shake);
        }

        // 3. 連鎖小爆発パーティクル（0.10秒おき）
        burstTimer_ += dt;
        if (burstTimer_ >= 0.10f && burstCount_ < 8) {
            burstTimer_ = 0.0f;
            burstCount_++;

            if (auto effectMgr = EffectManagerComponent::GetInstance()) {
                Irufemi::Vector3 basePos = boss->GetGameObject()->GetTransform()->GetWorldPosition();
                Irufemi::Vector3 offset{
                    Irufemi::Random::GeneratorFloat(-4.0f, 4.0f),
                    Irufemi::Random::GeneratorFloat(-3.0f, 3.0f),
                    Irufemi::Random::GeneratorFloat(-4.0f, 4.0f)
                };
                effectMgr->PlayEffect("debris_dust_effect", basePos + offset);
            }
            SoundHelper::Play(GameSE::DebrisShatter, 0.7f, 1.3f);
        }

        if (elapsedTime_ >= targetDuration_) {
            isCompleted_ = true;
        }
    }

    void Exit(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        (void)visualizer;
        // 機体ローカル座標を復元
        if (boss && boss->GetGameObject()) {
            if (auto transform = boss->GetGameObject()->GetComponent<Irufemi::TransformComponent>()) {
                transform->SetPosition(originalLocalPos_);
            }
        }
    }

    bool IsCompleted() const override {
        return isCompleted_;
    }

private:
    float elapsedTime_ = 0.0f;
    float targetDuration_ = 0.80f; // 実時間0.80秒
    float burstTimer_ = 0.0f;
    int burstCount_ = 0;
    bool isCompleted_ = false;

    Irufemi::Vector3 originalLocalPos_{0.0f, 0.0f, 0.0f};
    Irufemi::ConstantBuffer<Irufemi::LightningConfig> lightningBuffer_;
    Irufemi::LightningConfig config_{};
};

/**
 * @class DeathStepImplosionAndEMP
 * @brief 【第3幕】爆縮 ＆ EMP電磁パルス（0.90s〜1.20s）
 */
class DeathStepImplosionAndEMP : public IDeathSequenceStep {
public:
    void Enter(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        (void)visualizer;
        elapsedTime_ = 0.0f;
        isCompleted_ = false;

        // ポストプロセスに電磁パルス色収差を急激に注入
        if (boss && boss->GetEngine()) {
            if (auto ppm = boss->GetEngine()->GetPostProcessManager()) {
                ppm->AddActiveMode(PostProcessMode::ChromaticAberration, PostProcessManager::Layer::PreUI);
                auto& ca = ppm->GetChromaticAberrationParams();
                ca.intensity = 0.050f;
            }
        }
    }

    void Update(BossComponent* boss, BossDamageVisualizerComponent* visualizer, float dt) override {
        (void)visualizer;
        elapsedTime_ += dt;
        float progress = std::clamp(elapsedTime_ / targetDuration_, 0.0f, 1.0f);

        // 色収差のフェードアウト
        if (boss && boss->GetEngine()) {
            if (auto ppm = boss->GetEngine()->GetPostProcessManager()) {
                auto& ca = ppm->GetChromaticAberrationParams();
                ca.intensity = 0.050f * (1.0f - progress);
            }
        }

        if (elapsedTime_ >= targetDuration_) {
            isCompleted_ = true;
        }
    }

    void Exit(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        (void)visualizer;
        if (boss && boss->GetEngine()) {
            if (auto ppm = boss->GetEngine()->GetPostProcessManager()) {
                ppm->RemoveActiveMode(PostProcessMode::ChromaticAberration);
            }
        }
    }

    bool IsCompleted() const override {
        return isCompleted_;
    }

private:
    float elapsedTime_ = 0.0f;
    float targetDuration_ = 0.30f; // 実時間0.30秒
    bool isCompleted_ = false;
};

/**
 * @class DeathStepVoxelCataclysm
 * @brief 【第4幕】超高圧プラズマ爆発 ＆ 電磁Voxel超粉砕（1.20s〜2.50s）
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

        // 2. 自作エンジンの強み: VoxelParticleManager によるモデル超粉砕
        if (auto voxelMgr = boss->GetEngine()->GetVoxelParticleManager()) {
            Irufemi::VoxelEmitter p{};
            p.lifeTime = 2.5f;
            p.gravity = 2.0f;
            p.dispersion = 32.0f; // 巨大な電磁飛散
            p.scale = {0.8f, 0.8f, 0.8f};
            p.startColor = {1.8f, 2.2f, 3.5f, 1.0f}; // 超高圧青白プラズマ発光
            p.endColor = {0.05f, 0.1f, 0.2f, 1.0f};

            auto transform = boss->GetGameObject()->GetTransform();
            Irufemi::Vector3 deadRot = transform ? transform->GetRotation() : Irufemi::Vector3{0.0f, 0.0f, 0.0f};
            Irufemi::Vector3 deadScale = transform ? transform->GetScale() : Irufemi::Vector3{1.0f, 1.0f, 1.0f};

            // ボスリングモデルを物理ボクセル化して四散
            voxelMgr->PlayExplosion("resources/model/BossRing/BossRing.obj", deadPos,
                                    Irufemi::Vector3{0.0f, 0.0f, 0.0f}, deadRot, deadScale,
                                    p, Irufemi::Vector3Int{3, 3, 3});
        }

        // 3. GPU衝撃波パーティクル再生
        if (auto effectMgr = EffectManagerComponent::GetInstance()) {
            effectMgr->PlayEffect("debris_dust_effect", deadPos);
        }

        // 4. ボスメッシュおよびアウトラインの完全非表示化（Voxelに置き換わったように見せる）
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
    }

    void Update(BossComponent* boss, BossDamageVisualizerComponent* visualizer, float dt) override {
        (void)boss;
        (void)visualizer;
        elapsedTime_ += dt;
        if (elapsedTime_ >= targetDuration_) {
            isCompleted_ = true;
        }
    }

    bool IsCompleted() const override {
        return isCompleted_;
    }

private:
    float elapsedTime_ = 0.0f;
    float targetDuration_ = 1.30f; // 実時間1.30秒
    bool isCompleted_ = false;
};

/**
 * @class DeathStepFinishSequence
 * @brief 【第5幕】ボス非アクティブ化と撃破シーケンス完了通知（2.50s〜2.70s）
 */
class DeathStepFinishSequence : public IDeathSequenceStep {
public:
    void Enter(BossComponent* boss, BossDamageVisualizerComponent* visualizer) override {
        (void)visualizer;
        if (boss && boss->GetGameObject()) {
            boss->GetGameObject()->SetIsActive(false);
        }
        if (boss) {
            boss->NotifyDeathSequenceFinished();
        }
        isCompleted_ = true;
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
    steps_.push_back(std::make_unique<DeathStepOvervoltageFreeze>());  // 第1幕: ヒットストップ & コア白熱
    steps_.push_back(std::make_unique<DeathStepLightningCrescendo>()); // 第2幕: 電流密度クレッシェンド & 漏電暴走
    steps_.push_back(std::make_unique<DeathStepImplosionAndEMP>());     // 第3幕: 爆縮 & EMP電磁パルス
    steps_.push_back(std::make_unique<DeathStepVoxelCataclysm>());     // 第4幕: 帯電Voxel超粉砕 & 衝撃波
    steps_.push_back(std::make_unique<DeathStepFinishSequence>());     // 第5幕: 完了通知 & ResultScene接続

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
