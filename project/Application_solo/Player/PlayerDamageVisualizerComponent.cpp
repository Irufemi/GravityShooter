#include "Player/PlayerDamageVisualizerComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Framework/Component/Effect/ScreenEffectComponent.h"
#include "Framework/Component/Camera/CameraShakeComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"
#include "Core/Math/Random/Random.h"
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
#include "RailMechanics/SplineFollowerComponent.h"
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
    deathScreenUV_ = {0.5f, 0.5f};
    deathWorldPos_ = {0.0f, 0.0f, 0.0f};
    deathWorldRot_ = {0.0f, 0.0f, 0.0f};

    // --- シーン初期化時の完全事前ロード（ゼロヒッチ保証） ---
    if (auto engine = GetEngine()) {
        // 1. 自機モデルの VoxelParticle プール事前確保＆パースキャッシュ（2,000粒子）
        if (auto voxelManager = engine->GetVoxelParticleManager()) {
            voxelManager->ReservePool("resources/model/PlayerCraft/PlayerCraft.obj", {8, 8, 8}, 2000);
        }

        // 2. 死亡シーケンスで使用する全オーディオSEのメモリ常駐化（音量0でキャッシュ）
        if (auto audioManager = engine->GetAudioManager()) {
            audioManager->PlayByFile("resources/audio/SE/se_launch_charge.mp3", 0.0f);
            audioManager->PlayByFile("resources/audio/SE/se_result_impact.mp3", 0.0f);
            audioManager->PlayByFile("resources/audio/SE/se_debris_shatter.mp3", 0.0f);
            audioManager->PlayByFile("resources/audio/SE/se_hud_snap.mp3", 0.0f);
        }
    }
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

Irufemi::Vector2 PlayerDamageVisualizerComponent::CalculateWorldScreenUV(const Irufemi::Vector3& worldPos) {
    auto engine = GetEngine();
    if (!engine || !gameObject_) {
        return {0.5f, 0.5f};
    }

    auto cameraManager = engine->GetCameraManager();
    auto camera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
    if (!camera) {
        return {0.5f, 0.5f};
    }

    Irufemi::Matrix4x4 viewProj = camera->GetViewProjectionMatrix3D();
    // カメラ背後（w <= 0）への反転逆投影（海面へのすっ飛び）を数学的に完全ガード
    float w = worldPos.x * viewProj.m[0][3] + worldPos.y * viewProj.m[1][3] + worldPos.z * viewProj.m[2][3] + viewProj.m[3][3];
    if (w <= 0.001f) {
        return deathScreenUV_;
    }

    Irufemi::Vector3 clipPos = Irufemi::Math::Transform(worldPos, viewProj);

    // クリップ座標 (-1.0〜1.0) から スクリーンUV (0.0〜1.0) へ変換
    float u = std::clamp((clipPos.x + 1.0f) * 0.5f, 0.05f, 0.95f);
    float v = std::clamp((1.0f - clipPos.y) * 0.5f, 0.05f, 0.95f);
    return {u, v};
}

void PlayerDamageVisualizerComponent::TriggerDeathVisuals() {
    if (phase_ != DeathSequencePhase::None) {
        return; // 二重発火を完全にガード
    }

    // 親台車（PlayerCart）の前進を即時完全停止し、カメラによる自機の置き去り・追い抜きを根絶
    if (auto scene = gameObject_->GetScene()) {
        if (auto cart = scene->FindGameObject("PlayerCart")) {
            if (auto follower = cart->GetComponent<SplineFollowerComponent>()) {
                follower->SetSpeedMultiplier(0.0f);
            }
        }
    }
    if (auto parent = gameObject_->GetParent()) {
        if (auto follower = parent->GetComponent<SplineFollowerComponent>()) {
            follower->SetSpeedMultiplier(0.0f);
        }
    }

    // 撃墜時のワールド座標・回転・ローカル座標・スクリーンUVを確定保存（以降の爆発中心として完全固定）
    if (auto transform = gameObject_->GetComponent<TransformComponent>()) {
        transform->CheckAndComputeMatrix();
        deathWorldPos_ = transform->GetWorldPosition();
        deathWorldRot_ = transform->GetWorldRotation();
        originalLocalPos_ = transform->GetPosition(); // ローカル座標を確実に保存！
        originalScale_ = transform->GetWorldScale();
    }
    deathScreenUV_ = CalculateWorldScreenUV(deathWorldPos_);

    SetDeathPhase(DeathSequencePhase::Meltdown);
}

void PlayerDamageVisualizerComponent::SetDeathPhase(DeathSequencePhase newPhase) {
    phase_ = newPhase;
    phaseTimer_ = 0.0f;

    auto engine = GetEngine();
    auto ppm = engine ? engine->GetPostProcessManager() : nullptr;

    switch (phase_) {
    case DeathSequencePhase::Meltdown: {
        // スラスターを直ちに完全消火 ＆ スラスターGameObject破棄
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

        // 点滅解除 ＆ 自機を過熱プラズマ（バイオレット〜シアン過熱発光）
        if (isFlashing_) {
            isFlashing_ = false;
        }
        if (BaseModel* model = GetTargetModel()) {
            if (!colorCached_) {
                originalBaseColor_ = model->GetColor();
                colorCached_ = true;
            }
            model->SetColor({0.8f, 1.2f, 3.5f, 1.0f});
        }

        // BGMを急速フェードアウト（0.25秒で完全消音）
        if (engine) {
            if (auto am = engine->GetAudioManager()) {
                am->FadeCategoryVolume(AudioCategory::BGM, 0.0f, 0.25f);
                // 重力エネルギー凝縮チャージ音
                am->PlayByFile("resources/audio/SE/se_launch_charge.mp3", 0.95f, 0.85f, AudioCategory::SE);
            }
        }

        // 専用ポストプロセスモード BlackHoleSupernova を PreUI レイヤーに登録
        if (ppm) {
            ppm->AddActiveMode(PostProcessMode::BlackHoleSupernova, PostProcessManager::Layer::PreUI);
            auto& bh = ppm->GetBlackHoleSupernovaParams();
            bh.center = deathScreenUV_;
            bh.blackHoleRadius = 0.0f;
            bh.blackHolePullIntensity = 0.0f;
            bh.supernovaRadius = 0.0f;
            bh.supernovaIntensity = 0.0f;
            bh.supernovaChromaticRatio = 0.0f;
            bh.supernovaFlashIntensity = 0.0f;

            // 万が一古いフェードが残っていれば初期化
            ppm->RemoveActiveMode(PostProcessMode::Fade);
            ppm->GetFadeParams().intensity = 0.0f;
        }
        break;
    }

    case DeathSequencePhase::BlackHole: {
        break;
    }

    case DeathSequencePhase::VacuumFreeze: {
        // 極小スケールで完全固定（息を呑む完全な静寂・真空）
        if (auto transform = gameObject_->GetComponent<TransformComponent>()) {
            transform->SetScale(originalScale_ * 0.01f);
        }
        break;
    }

    case DeathSequencePhase::SupernovaBurst: {
        // 臨界突破！スーパーノヴァ大爆裂
        TriggerSupernovaExplosion();
        break;
    }

    case DeathSequencePhase::PlasmaAfterglow:
    case DeathSequencePhase::WhiteoutFade: {
        // 全画面ホワイトアウトを開始（白1.0へ）
        if (ppm) {
            ppm->AddActiveMode(PostProcessMode::Fade, PostProcessManager::Layer::PreUI);
            ppm->GetFadeParams().color = {1.0f, 1.0f, 1.0f, 1.0f};
        }
        break;
    }

    case DeathSequencePhase::Completed: {
        // 全画面白1.0を完全に担保し、リザルト画面へコールバック通知（白抜け戻りを完全防止）
        if (ppm) {
            ppm->GetFadeParams().intensity = 1.0f;
            ppm->RemoveActiveMode(PostProcessMode::BlackHoleSupernova);
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

    // ① 自機モデルの VoxelParticle 超新星大爆散（数千のクリスタル結晶が全方位へ超高速飛散）
    if (auto voxelManager = engine ? engine->GetVoxelParticleManager() : nullptr) {
        VoxelEmitter p{};
        p.particleType = 5; // Explosive
        p.lifeTime = 2.5f;
        p.gravity = 0.0f;     // 無重力空間を放射状に直進
        p.dispersion = 75.0f; // ★超高速インパルス（爆円の広がりと破片の広がりが完全同期！）
        p.scale = {0.12f, 0.12f, 0.12f}; // ★巨大ブロック化を防ぎ、繊細なクリスタル結晶
        p.startColor = {6.0f, 7.5f, 10.0f, 1.0f}; // 超高輝度HDRシアンホワイト
        p.endColor = {0.1f, 0.5f, 1.2f, 0.0f};   // 宇宙に溶ける深青プラズマ煙
        p.dissolveEdgeColor = {1.5f, 2.5f, 4.0f, 1.0f};

        // 自機シルエットから完全爆散！
        voxelManager->PlayExplosion("resources/model/PlayerCraft/PlayerCraft.obj", deathWorldPos_, deathWorldRot_,
                                    {0.0f, 0.0f, 0.0f}, originalScale_, p, {8, 8, 8});
    }

    // ② 音響レイヤリング: 空間破壊重低音 + ボクセル結晶大破砕音
    if (engine) {
        if (auto am = engine->GetAudioManager()) {
            am->PlayByFile("resources/audio/SE/se_result_impact.mp3", 1.0f, 0.75f, AudioCategory::SE);
            am->PlayByFile("resources/audio/SE/se_debris_shatter.mp3", 1.0f, 0.95f, AudioCategory::SE);
        }
    }

    // ④ 大インパルス・カメラ激震 (2.0秒)
    if (cameraShakeComp_) {
        cameraShakeComp_->PlayShakeSeconds(2.0f, 1.4f, 38.0f);
    }

    // ⑤ 自機モデルの完全非表示化 ＆ Transformスケール物理0化 ＆ スラスター完全消火
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
    if (!ppm) {
        return;
    }

    // 撃墜時に確定・ロックされたスクリーン座標を中心に常に固定（ズレ0%保証）
    auto& bh = ppm->GetBlackHoleSupernovaParams();
    bh.center = deathScreenUV_;

    switch (phase_) {
    case DeathSequencePhase::Meltdown: {
        // 【第1幕】0.00s〜0.40s: 装甲の過熱振動と微小Voxel剥離
        constexpr float kDuration = 0.40f;
        float t = std::clamp(phaseTimer_ / kDuration, 0.0f, 1.0f);
        if (auto transform = gameObject_->GetComponent<TransformComponent>()) {
            float jitter = (1.0f - t) * 0.08f;
            Irufemi::Vector3 shake{
                Irufemi::Random::GeneratorFloat(-jitter, jitter),
                Irufemi::Random::GeneratorFloat(-jitter, jitter),
                Irufemi::Random::GeneratorFloat(-jitter, jitter)
            };
            transform->SetPosition(originalLocalPos_ + shake); // ローカル座標に加算してワープを完全防止！
        }

        if (phaseTimer_ >= kDuration) {
            SetDeathPhase(DeathSequencePhase::BlackHole);
        }
        break;
    }

    case DeathSequencePhase::BlackHole: {
        // 【第2幕】0.40s〜1.00s (0.60秒間): 漆黒の事象の地平面が拡大し、周囲を猛烈に吸引
        constexpr float kDuration = 0.60f;
        float t = std::clamp(phaseTimer_ / kDuration, 0.0f, 1.0f);

        // 事象の地平面半径: 0.0 -> 0.08 (確固たる漆黒コア)
        bh.blackHoleRadius = 0.08f * std::sin(t * 3.14159265f * 0.5f);
        bh.blackHolePullIntensity = 0.25f * t;

        // 自機本体はブラックホールの中心へ急速圧縮 (1.0 -> 0.0)
        if (auto transform = gameObject_->GetComponent<TransformComponent>()) {
            float scaleFactor = std::pow(1.0f - t, 3.0f);
            transform->SetScale(originalScale_ * (std::max)(scaleFactor, 0.01f));
        }

        if (phaseTimer_ >= kDuration) {
            SetDeathPhase(DeathSequencePhase::VacuumFreeze);
        }
        break;
    }

    case DeathSequencePhase::VacuumFreeze: {
        // 【幕間】1.00s〜1.06s (0.06秒間): 臨界極小点での完全静寂フリーズ
        constexpr float kDuration = 0.06f;
        bh.blackHoleRadius = 0.015f; // 極小特異点
        bh.blackHolePullIntensity = 0.0f;

        if (phaseTimer_ >= kDuration) {
            SetDeathPhase(DeathSequencePhase::SupernovaBurst);
        }
        break;
    }

    case DeathSequencePhase::SupernovaBurst: {
        // 【第3幕】1.06s〜1.80s (0.74秒間): スーパーノヴァ大爆発！（波紋・火球・Voxel結晶の全方位拡散）
        constexpr float kDuration = 0.74f;
        float t = std::clamp(phaseTimer_ / kDuration, 0.0f, 1.0f);
        bh.blackHoleRadius = 0.0f; // 特異点崩壊
        bh.supernovaFlashIntensity = 0.0f; // ★偽の先行ホワイトアウトを廃止！爆発の広がりを明瞭に見せる

        // 超高速重力波衝撃波 ＆ GRAVITY DAZE 色収差の全方位展開（超高密度火球）
        bh.supernovaRadius = t * 2.5f;
        bh.supernovaIntensity = 2.5f * (1.0f - t * 0.4f); // 爆炎と過熱プラズマを画面端まで維持
        bh.supernovaChromaticRatio = 0.16f * (1.0f - t);  // 鮮烈なプリズム分光

        // 爆発が画面端に達する後半（t >= 0.65）から、臨界光のホワイトアウトが立ち上がり始める！
        if (t >= 0.65f) {
            float fadeT = (t - 0.65f) / 0.35f;
            if (ppm) {
                ppm->AddActiveMode(PostProcessMode::Fade, PostProcessManager::Layer::PreUI);
                ppm->GetFadeParams().color = {1.0f, 1.0f, 1.0f, 1.0f};
                ppm->GetFadeParams().intensity = fadeT * 0.5f; // 0.0 -> 0.5
            }
        }

        if (phaseTimer_ >= kDuration) {
            SetDeathPhase(DeathSequencePhase::WhiteoutFade);
        }
        break;
    }

    case DeathSequencePhase::PlasmaAfterglow:
    case DeathSequencePhase::WhiteoutFade: {
        // 【第4幕】1.80s〜2.40s (0.60秒間): 爆発エネルギー臨界飽和による完全ホワイトアウト
        constexpr float kDuration = 0.60f;
        float t = std::clamp(phaseTimer_ / kDuration, 0.0f, 1.0f);
        if (ppm) {
            ppm->GetFadeParams().intensity = 0.5f + t * 0.5f; // 0.5 -> 1.0 完全ホワイトアウト！
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

