#include "Player/PlayerDamageVisualizerComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Framework/Component/Effect/ScreenEffectComponent.h"
#include "Framework/Component/Camera/CameraShakeComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/System/Core/BaseModel.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"
#include "Effects/EffectManagerComponent.h"
#include "RailMechanics/RailShooterPlayerComponent.h"
#include "Player/GravityPlayerComponent.h"
#include "Combat/EnemyBulletManagerComponent.h"
#include "Combat/BossBulletManagerComponent.h"
#include "Player/PlayerThrusterVisualizerComponent.h"
#include "Framework/Component/Effect/ParticleEmitterComponent.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Audio/AudioManager.h"
#include "Core/Math/MathFunction.h"
#include <cmath>
#include <iostream>

void PlayerDamageVisualizerComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("Flash Interval", &flashInterval_);
    RegisterProperty("Flash Duration", &flashDuration_);
    RegisterProperty("Shake Intensity", &shakeIntensity_);
}

void PlayerDamageVisualizerComponent::Initialize() {
    flashTimer_ = 0.0f;
    flashRemaining_ = 0.0f;
    isFlashing_ = false;
    colorCached_ = false;
    phase_ = DeathSequencePhase::None;
    phaseTimer_ = 0.0f;
    onDeathSequenceCompleted_ = nullptr;
}

void PlayerDamageVisualizerComponent::Start() {
    if (!gameObject_) {
        return;
    }

    healthComp_ = gameObject_->GetComponent<PlayerHealthComponent>();
    screenEffectComp_ = gameObject_->GetComponent<ScreenEffectComponent>();

    if (auto scene = gameObject_->GetScene()) {
        if (auto mainCam = scene->FindGameObject("MainCamera")) {
            mainCameraObj_ = mainCam;
            cameraShakeComp_ = mainCam->GetComponent<CameraShakeComponent>();
        }
    }

    if (healthComp_) {
        // 被弾イベントリスナーを登録
        healthComp_->AddOnDamageTakenListener([this](int damage) {
            (void)damage;
            TriggerDamageFlash();
            TriggerCameraShake();
            TriggerScreenEffect();
        });

        // 死亡イベントリスナーを登録（ステートマシン起動）
        healthComp_->AddOnPlayerDiedListener([this]() { TriggerDeathVisuals(); });
    }
}

BaseModel* PlayerDamageVisualizerComponent::GetTargetModel() {
    if (!gameObject_) {
        return nullptr;
    }

    // dynamic_cast による型安全なモデルポインタ取得
    if (auto mesh = gameObject_->GetComponent<MeshRendererComponent>()) {
        if (auto renderable = mesh->GetRenderable()) {
            return dynamic_cast<BaseModel*>(renderable);
        }
    } else if (auto skinned = gameObject_->GetComponent<SkinnedMeshRendererComponent>()) {
        if (auto renderable = skinned->GetRenderable()) {
            return dynamic_cast<BaseModel*>(renderable);
        }
    }

    return nullptr;
}

void PlayerDamageVisualizerComponent::TriggerDamageFlash() {
    if (healthComp_) {
        flashRemaining_ = healthComp_->GetMaxInvincibilityTime();
    } else {
        flashRemaining_ = flashDuration_;
    }

    isFlashing_ = true;
    flashTimer_ = 0.0f;

    BaseModel* model = GetTargetModel();
    if (model && !colorCached_) {
        originalBaseColor_ = model->GetColor();
        colorCached_ = true;
    }
}

void PlayerDamageVisualizerComponent::TriggerCameraShake() {
    if (!cameraShakeComp_ || mainCameraObj_.expired()) {
        cameraShakeComp_ = nullptr;
        if (gameObject_) {
            if (auto scene = gameObject_->GetScene()) {
                if (auto mainCam = scene->FindGameObject("MainCamera")) {
                    mainCameraObj_ = mainCam;
                    cameraShakeComp_ = mainCam->GetComponent<CameraShakeComponent>();
                }
            }
        }
    }

    if (cameraShakeComp_) {
        cameraShakeComp_->PlayShake(shakeIntensity_, shakeFrames_, shakeFrequency_);
    }
}

void PlayerDamageVisualizerComponent::TriggerScreenEffect() {
    if (!screenEffectComp_ && gameObject_) {
        screenEffectComp_ = gameObject_->GetComponent<ScreenEffectComponent>();
    }

    if (screenEffectComp_) {
        screenEffectComp_->Play();
    }
}

void PlayerDamageVisualizerComponent::HideInGameHUD() {
    if (!gameObject_ || !gameObject_->GetScene()) {
        return;
    }
    auto scene = gameObject_->GetScene();
    if (auto reticle = scene->FindGameObject("Reticle")) {
        reticle->SetIsActive(false);
    }
    if (auto lockon = scene->FindGameObject("LockonMarkerUI")) {
        lockon->SetIsActive(false);
    }
}

void PlayerDamageVisualizerComponent::TriggerDeathVisuals() {
    if (phase_ != DeathSequencePhase::None) {
        return; // 二重発火を完全にガード
    }
    SetDeathPhase(DeathSequencePhase::Implosion);
}

void PlayerDamageVisualizerComponent::SetDeathPhase(DeathSequencePhase newPhase) {
    phase_ = newPhase;
    phaseTimer_ = 0.0f;

    auto engine = GetEngine();
    auto ppm = engine ? engine->GetPostProcessManager() : nullptr;

    switch (phase_) {
    case DeathSequencePhase::Implosion: {
        // 被弾の瞬間（0.00s）に推進スラスターを直ちに完全消火 ＆ スラスターGameObjectを破棄
        if (auto thrusterComp = gameObject_->GetComponent<PlayerThrusterVisualizerComponent>()) {
            thrusterComp->SetThrusterActive(false);
        }
        for (const auto& child : gameObject_->GetChildren()) {
            if (child && child->GetName() == "PlayerThrusterRoot") {
                child->Destroy();
            }
        }

        // 自機コンポーネントを Dying ステートへ移行（入力遮断・姿勢固定）
        if (auto railPlayer = gameObject_->GetComponent<RailShooterPlayerComponent>()) {
            railPlayer->SetState(PlayerFlightState::Dying);
        }

        // 周回ガレキ ＆ 画面内敵弾の特異点一括吸引・回収
        if (auto grav = gameObject_->GetComponent<GravityPlayerComponent>()) {
            grav->AbsorbOrbitingDebrisToSingularity();
        }
        if (auto bulletMgr = EnemyBulletManagerComponent::GetInstance()) {
            bulletMgr->AbsorbAndClearAllBullets();
        }
        if (auto scene = gameObject_->GetScene()) {
            if (auto bossObj = scene->FindGameObject("Boss")) {
                if (auto bossBulletMgr = bossObj->GetComponent<BossBulletManagerComponent>()) {
                    bossBulletMgr->ClearAllBullets();
                }
            }
        }

        // HUD（照準・ロックオン）の即時消去
        HideInGameHUD();

        // 点滅解除 ＆ 自機を青白い重力プラズマ光彩で発光
        if (isFlashing_) {
            isFlashing_ = false;
        }
        if (BaseModel* model = GetTargetModel()) {
            if (!colorCached_) {
                originalBaseColor_ = model->GetColor();
                colorCached_ = true;
            }
            // 重力凝縮による青白い過熱発光
            model->SetColor({0.4f, 1.6f, 2.8f, 1.0f});
        }

        // 基準スケールを記録
        auto transform = gameObject_->GetComponent<TransformComponent>();
        originalScale_ = transform ? transform->GetWorldScale() : Irufemi::Vector3{1.0f, 1.0f, 1.0f};

        // BGMを急速フェードアウト（0.30秒で音量0へ）
        if (engine) {
            if (auto am = engine->GetAudioManager()) {
                am->FadeCategoryVolume(AudioCategory::BGM, 0.0f, 0.30f);
                // 重力エネルギー凝縮チャージ音
                am->PlayByFile("resources/audio/SE/se_launch_charge.mp3", 0.90f, 0.90f, AudioCategory::SE);
            }
        }

        // 万が一フェードが残っていれば確実に初期化
        if (ppm) {
            ppm->RemoveActiveMode(PostProcessMode::Fade);
            ppm->GetFadeParams().intensity = 0.0f;
        }
        break;
    }

    case DeathSequencePhase::VacuumFreeze: {
        // 極小スケールで完全固定（息を呑む完全な静寂）
        if (auto transform = gameObject_->GetComponent<TransformComponent>()) {
            transform->SetScale(originalScale_ * 0.02f);
        }
        break;
    }

    case DeathSequencePhase::SupernovaBurst: {
        // 臨界突破！スーパーノヴァ大爆発
        TriggerSupernovaExplosion();
        break;
    }

    case DeathSequencePhase::WhiteoutFade: {
        // 全画面ホワイトアウトを開始（0.0 から滑らかに侵食）
        if (ppm) {
            ppm->AddActiveMode(PostProcessMode::Fade, PostProcessManager::Layer::PreUI);
            ppm->GetFadeParams().color = {1.0f, 1.0f, 1.0f, 1.0f};
            ppm->GetFadeParams().intensity = 0.0f;
        }
        break;
    }

    case DeathSequencePhase::Completed: {
        // 全画面白1.0を担保した上で、シーケンス完了コールバック（GameLoopManagerへの通知）を発火
        if (ppm) {
            ppm->GetFadeParams().intensity = 1.0f;
        }
        if (onDeathSequenceCompleted_) {
            onDeathSequenceCompleted_();
        }
        break;
    }

    case DeathSequencePhase::None:
    default:
        break;
    }
}

void PlayerDamageVisualizerComponent::TriggerSupernovaExplosion() {
    if (!gameObject_) {
        return;
    }
    auto engine = GetEngine();
    auto transform = gameObject_->GetComponent<TransformComponent>();
    Irufemi::Vector3 deathPos = transform ? transform->GetWorldPosition() : Irufemi::Vector3{0, 0, 0};
    Irufemi::Vector3 deathRot = transform ? transform->GetWorldRotation() : Irufemi::Vector3{0, 0, 0};

    // ① 自作エンジンの看板機能: 自機モデルの VoxelParticle 超新星大破砕（超高輝度シアンホワイト）
    if (auto voxelManager = engine ? engine->GetVoxelParticleManager() : nullptr) {
        VoxelEmitter p{};
        p.particleType = 5; // Explosive
        p.lifeTime = 3.5f;
        p.gravity = 0.0f;     // 無重力で猛烈に四散
        p.dispersion = 32.0f; // 高速インパルス
        p.scale = {0.20f, 0.20f, 0.20f}; // 巨大ブロック化を防ぎ、繊細なクリスタル粒子に設定
        p.startColor = {4.0f, 5.0f, 6.0f, 1.0f}; // 超高輝度HDRシアンホワイト
        p.endColor = {0.1f, 0.5f, 1.2f, 0.0f};   // 宇宙空間に溶けるシアンプラズマ煙
        p.dissolveEdgeColor = {0.8f, 1.5f, 2.5f, 1.0f};

        // 解像度 {8, 8, 8} で適切なボクセルサイズを即時生成し、自機シルエットから美しく炸裂！
        voxelManager->PlayExplosion("resources/model/PlayerCraft/PlayerCraft.obj", deathPos, deathRot,
                                    {0.0f, 0.0f, 0.0f}, originalScale_, p, {8, 8, 8});
    }

    // ② 空間を切り裂く重力波ショックウェーブ（Gravity Distortion）を自機スクリーン座標を中心に発火
    if (auto ppm = engine ? engine->GetPostProcessManager() : nullptr) {
        ppm->TriggerGravityDistortion({0.5f, 0.65f}, 0.50f, 1.20f, 2.5f, 0.09f, 0.35f);
    }

    // ③ 二次爆発エフェクトの重畳（粉塵クラウド ＆ 閃光スパーク）
    if (auto fxMgr = EffectManagerComponent::GetInstance()) {
        fxMgr->PlayEffect("debris_dust_effect", deathPos);
        fxMgr->PlayEffect("Hit", deathPos);
    }

    // ④ 重厚な大爆発音響レイヤリング（重低音インパクト ＋ ボクセル大破砕音）
    if (engine) {
        if (auto am = engine->GetAudioManager()) {
            // 重低音インパクト（ズゥゥンという空間破壊コア）
            am->PlayByFile("resources/audio/SE/se_result_impact.mp3", 1.0f, 0.80f, AudioCategory::SE);
            // ボクセル大破砕音（ガシャァンという結晶・機体粉砕音）
            am->PlayByFile("resources/audio/SE/se_debris_shatter.mp3", 0.95f, 0.95f, AudioCategory::SE);
        }
    }

    // ⑤ 大インパルス・カメラシェイク（強震: 2.0秒持続）を発火
    if (cameraShakeComp_) {
        cameraShakeComp_->PlayShakeSeconds(2.0f, 1.2f, 35.0f);
    }

    // ⑥ 自機モデルを完全非表示化 ＆ Transformスケール物理0化 ＆ スラスター完全消火
    if (auto mesh = gameObject_->GetComponent<MeshRendererComponent>()) {
        mesh->SetVisible(false);
    } else if (auto skinned = gameObject_->GetComponent<SkinnedMeshRendererComponent>()) {
        skinned->SetVisible(false);
    }
    if (auto transformComp = gameObject_->GetComponent<TransformComponent>()) {
        transformComp->SetScale({0.0f, 0.0f, 0.0f});
        transformComp->CheckAndComputeMatrix();
    }
    if (auto thrusterComp = gameObject_->GetComponent<PlayerThrusterVisualizerComponent>()) {
        thrusterComp->SetThrusterActive(false);
    }
    for (const auto& child : gameObject_->GetChildren()) {
        if (child && child->GetName() == "PlayerThrusterRoot") {
            child->Destroy();
        }
    }
    auto allEmitters = gameObject_->GetComponentsInChildren<ParticleEmitterComponent>();
    for (auto pe : allEmitters) {
        if (pe) {
            pe->Stop();
        }
    }
}

void PlayerDamageVisualizerComponent::UpdateDeathSequence(float dtReal) {
    if (phase_ == DeathSequencePhase::None || phase_ == DeathSequencePhase::Completed) {
        return;
    }

    phaseTimer_ += dtReal;
    auto engine = GetEngine();
    auto ppm = engine ? engine->GetPostProcessManager() : nullptr;

    switch (phase_) {
    case DeathSequencePhase::Implosion: {
        // 【Phase 1】 0.00s 〜 0.65s (0.65秒) : 3次イーズインで加速度的に中心へ極小収縮
        constexpr float kDuration = 0.65f;
        float progress = std::clamp(phaseTimer_ / kDuration, 0.0f, 1.0f);
        float scaleFactor = std::pow(1.0f - progress, 3.0f);
        if (auto transform = gameObject_->GetComponent<TransformComponent>()) {
            transform->SetScale(originalScale_ * (std::max)(scaleFactor, 0.02f));
        }

        if (phaseTimer_ >= kDuration) {
            SetDeathPhase(DeathSequencePhase::VacuumFreeze);
        }
        break;
    }

    case DeathSequencePhase::VacuumFreeze: {
        // 【Phase 2】 0.65s 〜 0.85s (0.20秒) : 真空の完全静寂（息を呑む極小点フリーズ）
        constexpr float kDuration = 0.20f;
        if (phaseTimer_ >= kDuration) {
            SetDeathPhase(DeathSequencePhase::SupernovaBurst);
        }
        break;
    }

    case DeathSequencePhase::SupernovaBurst: {
        // 【Phase 3】 0.85s 〜 2.10s (1.25秒！) : Voxel大破片と重力波紋が宇宙に広がる光景をしっかり魅せる！
        constexpr float kDuration = 1.25f;
        if (phaseTimer_ >= kDuration) {
            SetDeathPhase(DeathSequencePhase::WhiteoutFade);
        }
        break;
    }

    case DeathSequencePhase::WhiteoutFade: {
        // 【Phase 4】 2.10s 〜 2.70s (0.60秒) : 爆発を見届けた後、閃光がカメラを覆い尽くしホワイトアウト（0.0 -> 1.0）
        constexpr float kDuration = 0.60f;
        float progress = std::clamp(phaseTimer_ / kDuration, 0.0f, 1.0f);
        if (ppm) {
            ppm->GetFadeParams().intensity = progress;
        }

        if (phaseTimer_ >= kDuration) {
            SetDeathPhase(DeathSequencePhase::Completed);
        }
        break;
    }

    default:
        break;
    }
}

void PlayerDamageVisualizerComponent::Update() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    float dtGame = engine->GetGameDeltaTime();
    float dtReal = engine->GetRealDeltaTime();

    // 被弾点滅処理の更新（ゲーム内時間依存）
    if (isFlashing_ && dtGame > 0.0f) {
        flashRemaining_ -= dtGame;
        flashTimer_ += dtGame;

        BaseModel* model = GetTargetModel();
        if (model) {
            if (!colorCached_) {
                originalBaseColor_ = model->GetColor();
                colorCached_ = true;
            }

            if (fmod(flashTimer_, flashInterval_ * 2.0f) < flashInterval_) {
                model->SetColor(flashColor_);
            } else {
                model->SetColor(originalBaseColor_);
            }
        }

        if (flashRemaining_ <= 0.0f) {
            isFlashing_ = false;
            flashRemaining_ = 0.0f;
            if (model && colorCached_) {
                model->SetColor(originalBaseColor_);
            }
        }
    }

    // 死亡演出ステートマシンの更新（実時間 dtReal 依存）
    UpdateDeathSequence(dtReal);
}

