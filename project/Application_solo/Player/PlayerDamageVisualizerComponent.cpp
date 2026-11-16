#include "Player/PlayerDamageVisualizerComponent.h"
#include "Player/PlayerDeathStates.h"
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
#include "Audio/SoundHelper.h"
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

using namespace Irufemi;

PlayerDamageVisualizerComponent::PlayerDamageVisualizerComponent() = default;
PlayerDamageVisualizerComponent::~PlayerDamageVisualizerComponent() = default;

void PlayerDamageVisualizerComponent::OnRegisterProperties() {
    Irufemi::Component::OnRegisterProperties();
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

        // 2. 死亡シーケンスで使用する全オーディオSEのメモリ常駐化（SoundHelper経由で事前ロード保証）
        SoundHelper::PreloadAll();
    }
}

void PlayerDamageVisualizerComponent::Start() {
    if (!gameObject_) {
        return;
    }

    healthComp_ = gameObject_->GetComponent<PlayerHealthComponent>();
    screenEffectComp_ = gameObject_->GetComponent<Irufemi::ScreenEffectComponent>();
    cachedTargetModel_ = GetTargetModel();

    if (auto scene = gameObject_->GetScene()) {
        if (auto mainCam = scene->FindGameObject("MainCamera")) {
            mainCameraObj_ = mainCam;
            cameraShakeComp_ = mainCam->GetComponent<Irufemi::CameraShakeComponent>();
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
    if (cachedTargetModel_) {
        return cachedTargetModel_;
    }
    if (!gameObject_) {
        return nullptr;
    }

    // dynamic_cast による型安全なモデルポインタ取得
    if (auto mesh = gameObject_->GetComponent<Irufemi::MeshRendererComponent>()) {
        if (auto renderable = mesh->GetRenderable()) {
            cachedTargetModel_ = dynamic_cast<BaseModel*>(renderable);
            return cachedTargetModel_;
        }
    } else if (auto skinned = gameObject_->GetComponent<Irufemi::SkinnedMeshRendererComponent>()) {
        if (auto renderable = skinned->GetRenderable()) {
            cachedTargetModel_ = dynamic_cast<BaseModel*>(renderable);
            return cachedTargetModel_;
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
                    cameraShakeComp_ = mainCam->GetComponent<Irufemi::CameraShakeComponent>();
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
        screenEffectComp_ = gameObject_->GetComponent<Irufemi::ScreenEffectComponent>();
        cachedTargetModel_ = GetTargetModel();
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
    if (auto statusHud = scene->FindGameObject("PlayerStatusHUD")) {
        statusHud->SetIsActive(false);
    }
    if (auto scoreHud = scene->FindGameObject("ScoreHUD")) {
        scoreHud->SetIsActive(false);
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
            if (auto col = obj->GetComponent<Irufemi::ColliderComponent>()) {
                if ((col->GetLayer() & enemyLayerMask) != 0) {
                    isEnemy = true;
                }
            }
        }

        // 2. 敵ロジックコンポーネントによる判定（セーフティガード）
        if (!isEnemy) {
            if (obj->GetComponent<RailShooterEnemyComponent>() || obj->GetComponent<BossComponent>() ||
                obj->GetComponent<TargetableComponent>()) {
                isEnemy = true;
            }
        }

        if (isEnemy) {
            // 当該オブジェクトおよびその子階層に付与された EffectMaskComponent をすべて無効化
            auto maskComps = obj->GetComponentsInChildren<Irufemi::EffectMaskComponent>();
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
    constexpr float kMinProjectionW = 0.001f;
    constexpr float kScreenUVMarginMin = 0.05f;
    constexpr float kScreenUVMarginMax = 0.95f;

    // カメラ背後（w <= 0）への反転逆投影（海面へのすっ飛び）を数学的に完全ガード
    float w = worldPos.x * viewProj.m[0][3] + worldPos.y * viewProj.m[1][3] + worldPos.z * viewProj.m[2][3] +
              viewProj.m[3][3];
    if (w <= kMinProjectionW) {
        return deathScreenUV_;
    }

    Irufemi::Vector3 clipPos = Irufemi::Math::Transform(worldPos, viewProj);

    // クリップ座標 (-1.0〜1.0) から スクリーンUV (0.0〜1.0) へ変換
    float u = std::clamp((clipPos.x + 1.0f) * 0.5f, kScreenUVMarginMin, kScreenUVMarginMax);
    float v = std::clamp((1.0f - clipPos.y) * 0.5f, kScreenUVMarginMin, kScreenUVMarginMax);
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
    if (auto transform = gameObject_->GetComponent<Irufemi::TransformComponent>()) {
        transform->CheckAndComputeMatrix();
        deathWorldPos_ = transform->GetWorldPosition();
        deathWorldRot_ = transform->GetWorldRotation();
        originalLocalPos_ = transform->GetPosition(); // ローカル座標を確実に保存！
        originalScale_ = transform->GetWorldScale();
    }
    deathScreenUV_ = CalculateWorldScreenUV(deathWorldPos_);

    SetDeathPhase(DeathSequencePhase::Meltdown);
}

PlayerDamageVisualizerComponent::DeathSequencePhase PlayerDamageVisualizerComponent::GetDeathPhase() const {
    if (currentDeathState_) {
        return currentDeathState_->GetPhase();
    }
    return phase_;
}

void PlayerDamageVisualizerComponent::ChangeDeathState(std::unique_ptr<IPlayerDeathState> newState) {
    if (currentDeathState_) {
        currentDeathState_->Exit(this);
    }
    currentDeathState_ = std::move(newState);
    phaseTimer_ = 0.0f;
    if (currentDeathState_) {
        phase_ = currentDeathState_->GetPhase();
        currentDeathState_->Enter(this);
    } else {
        phase_ = DeathSequencePhase::None;
    }
}

void PlayerDamageVisualizerComponent::SetDeathPhase(DeathSequencePhase newPhase) {
    switch (newPhase) {
    case DeathSequencePhase::Meltdown:
        ChangeDeathState(std::make_unique<PlayerDeathStateMeltdown>());
        break;
    case DeathSequencePhase::BlackHole:
        ChangeDeathState(std::make_unique<PlayerDeathStateBlackHole>());
        break;
    case DeathSequencePhase::VacuumFreeze:
        ChangeDeathState(std::make_unique<PlayerDeathStateVacuumFreeze>());
        break;
    case DeathSequencePhase::SupernovaBurst:
        ChangeDeathState(std::make_unique<PlayerDeathStateSupernovaBurst>());
        break;
    case DeathSequencePhase::PlasmaAfterglow:
        ChangeDeathState(std::make_unique<PlayerDeathStatePlasmaAfterglow>());
        break;
    case DeathSequencePhase::WhiteoutFade:
        ChangeDeathState(std::make_unique<PlayerDeathStateWhiteoutFade>());
        break;
    case DeathSequencePhase::Completed:
        ChangeDeathState(std::make_unique<PlayerDeathStateCompleted>());
        break;
    case DeathSequencePhase::None:
    default:
        ChangeDeathState(nullptr);
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
        p.gravity = 0.0f;                         // 無重力空間を放射状に直進
        p.dispersion = 75.0f;                     // ★超高速インパルス（爆円の広がりと破片の広がりが完全同期！）
        p.scale = {0.12f, 0.12f, 0.12f};          // ★巨大ブロック化を防ぎ、繊細なクリスタル結晶
        p.startColor = {6.0f, 7.5f, 10.0f, 1.0f}; // 超高輝度HDRシアンホワイト
        p.endColor = {0.1f, 0.5f, 1.2f, 0.0f};    // 宇宙に溶ける深青プラズマ煙
        p.dissolveEdgeColor = {1.5f, 2.5f, 4.0f, 1.0f};

        // 自機シルエットから完全爆散！
        voxelManager->PlayExplosion("resources/model/PlayerCraft/PlayerCraft.obj", deathWorldPos_, deathWorldRot_,
                                    {0.0f, 0.0f, 0.0f}, originalScale_, p, {8, 8, 8});
    }

    // ② 音響レイヤリング: 空間破壊重低音 + ボクセル結晶大破砕音
    SoundHelper::Play(GameSE::ResultImpact, 1.0f, 0.75f);
    SoundHelper::Play(GameSE::DebrisShatter, 1.0f, 0.95f);

    // ④ 大インパルス・カメラ激震 (2.0秒)
    if (cameraShakeComp_) {
        cameraShakeComp_->PlayShakeSeconds(2.0f, 1.4f, 38.0f);
    }

    // ⑤ 自機モデルの完全非表示化 ＆ Transformスケール物理0化 ＆ スラスター完全消火
    if (auto mesh = gameObject_->GetComponent<Irufemi::MeshRendererComponent>()) {
        mesh->SetVisible(false);
    } else if (auto skinned = gameObject_->GetComponent<Irufemi::SkinnedMeshRendererComponent>()) {
        skinned->SetVisible(false);
    }
    if (auto transformComp = gameObject_->GetComponent<Irufemi::TransformComponent>()) {
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
    auto allEmitters = gameObject_->GetComponentsInChildren<Irufemi::ParticleEmitterComponent>();
    for (auto pe : allEmitters) {
        if (pe) {
            pe->Stop();
        }
    }
}

void PlayerDamageVisualizerComponent::UpdateDeathSequence(float dtReal) {
    if (currentDeathState_) {
        currentDeathState_->Update(this, dtReal);
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
