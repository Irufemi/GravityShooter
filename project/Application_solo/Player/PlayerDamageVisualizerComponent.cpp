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
#include "Player/PlayerTargetingComponent.h"
#include "UI/LockonMarkerUIComponent.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Framework/Component/Collider/ColliderComponent.h"
#include "Physics/CollisionManager.h"
#include "RailMechanics/RailShooterEnemyComponent.h"
#include "Combat/Boss/BossComponent.h"
#include "Player/TargetableComponent.h"
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
        if (auto markerComp = lockon->GetComponent<LockonMarkerUIComponent>()) {
            markerComp->SyncTargets({});
        }
    }

    // 自機のターゲティングコンポーネントを即時完全無効化＆ハイライト消去（業界標準フェイルセーフ）
    if (auto targeting = gameObject_->GetComponent<PlayerTargetingComponent>()) {
        targeting->SetEnabled(false);
    }

    // 演出に不要な敵オブジェクトのアウトライン（EffectMaskComponent）をピンポイント消去
    ClearEnemyOutlines();
}

void PlayerDamageVisualizerComponent::ClearEnemyOutlines() {
    if (!gameObject_) {
        return;
    }
    auto scene = gameObject_->GetScene();
    if (!scene) {
        return;
    }

    auto engine = GetEngine();
    auto cm = engine ? engine->GetCollisionManager() : nullptr;
    const uint32_t enemyLayerMask = cm ? cm->GetLayerMask("Enemy") : 0;

    for (const auto& obj : scene->GetGameObjects()) {
        if (!obj) {
            continue;
        }

        bool isEnemy = false;

        // 1. 当たり判定レイヤーによる判定（Collision Layer の "Enemy" カテゴライズ）
        if (enemyLayerMask != 0) {
            if (auto col = obj->GetComponent<ColliderComponent>()) {
                if ((col->GetLayer() & enemyLayerMask) != 0) {
                    isEnemy = true;
                }
            }
        }

        // 2. 敵ロジックコンポーネントによる判定（セーフティガード）
        if (!isEnemy) {
            if (obj->GetComponent<RailShooterEnemyComponent>() ||
                obj->GetComponent<BossComponent>() ||
                obj->GetComponent<TargetableComponent>()) {
                isEnemy = true;
            }
        }

        if (isEnemy) {
            // 当該オブジェクトおよびその子階層に付与された EffectMaskComponent をすべて無効化
            auto maskComps = obj->GetComponentsInChildren<EffectMaskComponent>();
            for (auto mask : maskComps) {
                if (mask) {
                    mask->SetEnableEffectMask(false);
                }
            }
        }
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

    case DeathSequencePhase::PlasmaAfterglow: {
        // 激震が静まり返る静寂の余韻へ移行
        break;
    }

    case DeathSequencePhase::WhiteoutFade: {
        // 静寂の余韻から、光が静かに画面を満たしてリザルト接続フェードへ移行
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
        // 【第2幕】0.35s〜1.15s (0.80秒間): 事象の地平面の「拡大」➔ 限界突破の「急速重力収縮（インプロージョン）」！
        constexpr float kDuration = 0.80f;
        float t = std::clamp(phaseTimer_ / kDuration, 0.0f, 1.0f);

        // 前半（0.00s〜0.50s / t: 0.0〜0.625）: ブラックホールが悠然と最大まで拡大！周囲の時空を猛烈吸引
        if (t <= 0.625f) {
            float tExpand = t / 0.625f;
            bh.blackHoleRadius = 0.09f * std::sin(tExpand * 3.14159265f * 0.5f);
            bh.blackHolePullIntensity = 0.30f * tExpand;

            // 自機は特異点中心へ圧縮
            if (auto transform = gameObject_->GetComponent<TransformComponent>()) {
                float scaleFactor = std::pow(1.0f - tExpand * 0.8f, 2.0f);
                transform->SetScale(originalScale_ * (std::max)(scaleFactor, 0.01f));
            }
        }
        // 後半（0.50s〜0.80s / t: 0.625〜1.0）: 限界突破！ギュウウッ…と超強烈なイージングで極小特異点へ急激に凝縮（インプロージョン / 縮小）！
        else {
            float tImplode = (t - 0.625f) / 0.375f;
            float implodeCurve = std::pow(1.0f - tImplode, 3.5f);
            bh.blackHoleRadius = 0.006f + (0.09f - 0.006f) * implodeCurve;
            bh.blackHolePullIntensity = 0.30f * implodeCurve;

            // 自機本体は完全に特異点へ吸い込まれて消滅
            if (auto transform = gameObject_->GetComponent<TransformComponent>()) {
                float scaleFactor = (1.0f - tImplode) * 0.2f;
                transform->SetScale(originalScale_ * (std::max)(scaleFactor, 0.001f));
            }
        }

        if (phaseTimer_ >= kDuration) {
            SetDeathPhase(DeathSequencePhase::VacuumFreeze);
        }
        break;
    }

    case DeathSequencePhase::VacuumFreeze: {
        // 【幕間】1.15s〜1.30s (0.15秒間): 極小特異点での完全な静寂・息を呑む一瞬のタメ（Anticipation）！
        constexpr float kDuration = 0.15f;
        bh.blackHoleRadius = 0.006f; // 極小特異点
        bh.blackHolePullIntensity = 0.0f;

        if (phaseTimer_ >= kDuration) {
            SetDeathPhase(DeathSequencePhase::SupernovaBurst);
        }
        break;
    }

    case DeathSequencePhase::SupernovaBurst: {
        // 【第3幕】1.30s〜2.05s (0.75秒間): 臨界突破！スーパーノヴァ大爆発（拡大！数千のVoxel結晶大拡散 ＆ スマートプラズマ重力波）
        constexpr float kDuration = 0.75f;
        float t = std::clamp(phaseTimer_ / kDuration, 0.0f, 1.0f);

        // 中心には崩壊せず残存する「漆黒の特異点ボイドコア」とフォトンリングを維持！
        bh.blackHoleRadius = 0.018f * (1.0f - t * 0.2f);
        bh.blackHolePullIntensity = 0.04f;
        bh.supernovaFlashIntensity = 0.0f;

        // 爆発開始の最初の 0.08 秒間に急峻な立ち上がりインパルスを持たせ、テクスチャの唐突なポップを根絶！
        float popInCurve = (t < 0.08f) ? (t / 0.08f) : 1.0f;

        // 多層重力波＆スマートプラズマ流体の全方位展開（超高速・高精細）
        bh.supernovaRadius = t * 2.8f;
        bh.supernovaIntensity = 1.8f * popInCurve * (1.0f - t * 0.20f);
        bh.supernovaChromaticRatio = 0.12f * popInCurve * (1.0f - t * 0.40f);

        // 爆発の広がりとVoxel結晶の飛散を100%美しく魅せるため、ここでは白フェードを入れない！
        if (phaseTimer_ >= kDuration) {
            SetDeathPhase(DeathSequencePhase::PlasmaAfterglow);
        }
        break;
    }

    case DeathSequencePhase::PlasmaAfterglow: {
        // 【第4幕】2.05s〜2.95s (0.90秒間): 激震が静まり返り、漂うプラズマとVoxel結晶を見届ける「静寂の余韻」
        constexpr float kDuration = 0.90f;
        float t = std::clamp(phaseTimer_ / kDuration, 0.0f, 1.0f);

        // ★静止フリーズを完全根絶！衝撃波面は画面外の彼方へ突き抜けさせ、物理的な拡散を継続
        bh.supernovaRadius = 2.8f + t * 2.7f; // 2.8 -> 5.5（画面外へ完全に抜け去る）

        // 空間歪みとプラズマ残光は「スゥーッ…」と滑らかにゼロへ戻り、元の静かな海面が戻ってくる！
        float decayCurve = (1.0f - t) * (1.0f - t);
        bh.supernovaIntensity = 1.44f * decayCurve;
        bh.supernovaChromaticRatio = 0.072f * decayCurve;
        bh.blackHoleRadius = 0.014f * (1.0f - t * 0.8f);

        // 余韻の後半（t >= 0.70）から、GAME OVER接続用のシネマティックフェードが静かに立ち上がる
        if (t >= 0.70f) {
            float fadeT = (t - 0.70f) / 0.30f;
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

    case DeathSequencePhase::WhiteoutFade: {
        // 【第5幕】2.95s〜3.45s (0.50秒間): 光が穏やかに満ちてリザルト画面（GAME OVER）へシームレス接続
        constexpr float kDuration = 0.50f;
        float t = std::clamp(phaseTimer_ / kDuration, 0.0f, 1.0f);
        if (ppm) {
            ppm->GetFadeParams().intensity = 0.5f + t * 0.5f; // 0.5 -> 1.0
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

