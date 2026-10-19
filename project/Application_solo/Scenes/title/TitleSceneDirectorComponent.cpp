#include "Scenes/title/TitleSceneDirectorComponent.h"
#include "Scenes/title/State/ITitleLaunchState.h"
#include "Scenes/title/State/TitleLaunchStateCharge.h"
#include "Scenes/title/State/TitleLaunchStateAccelerate.h"
#include "Scenes/title/State/TitleLaunchStateBreak.h"
#include "Scenes/title/State/TitleLaunchStateAfterglow.h"

#include "Scenes/title/TitleCosmicNebulaComponent.h"
#include "Framework/Component/Audio/AudioSourceComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/Camera/CameraShakeComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Audio/AudioManager.h"
#include "Framework/Component/Effect/ParticleEmitterComponent.h"
#include "Renderer/Object/Particle/ParticleObject.h"
#include "Renderer/Camera/Camera.h"
#include "Core/Math/MathFunction.h"
#include "Framework/Component/Effect/VoxelParticleComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/ModelBatchRendererComponent.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Input/GameAction.h"
#include "Platform/Input/InputManager.h"
#include <cmath>
#include <algorithm>
#include <random>

TitleSceneDirectorComponent::~TitleSceneDirectorComponent() {
    CleanupLaunchPostProcess();
    CleanupAsteroidBelt();
}

void TitleSceneDirectorComponent::OnDestroy() {
    CleanupLaunchPostProcess();
    CleanupAsteroidBelt();
}

void TitleSceneDirectorComponent::Initialize() {
    isLaunching_ = false;
    launchTimer_ = 0.0f;
    hasTriggeredSceneTransition_ = false;
    hasExplodedDebris_ = false;
    isMicroFreezing_ = false;
    freezeTimer_ = 0.0f;
    hasTriggeredRelease_ = false;
    isLaunchPostProcessActive_ = false;
    currentLaunchState_ = nullptr;
    currentLaunchStateType_ = LaunchState::Idle;
    stateTimer_ = 0.0f;
    idleTimer_ = 0.0f;
    debrisRepelOffsets_.clear();
    debrisBlastVelocities_.clear();
    debrisDrifts_.clear();
    initialDebrisPositions_.clear();
    asteroidInstances_.clear();
    asteroidBatchRenderer_ = nullptr;
    initialBgmVolume_ = 0.70f;
}

void TitleSceneDirectorComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("Thruster Prefab Path", &thrusterEffectPrefabPath_);
    RegisterProperty("Nozzle Offset", &nozzleOffset_);
}

TransformComponent* TitleSceneDirectorComponent::GetShipTransform() const {
    if (auto ship = shipObj_.lock()) {
        return ship->GetTransform();
    }
    return nullptr;
}

TransformComponent* TitleSceneDirectorComponent::GetCameraTransform() const {
    if (auto cam = cameraObj_.lock()) {
        return cam->GetTransform();
    }
    return nullptr;
}

CameraComponent* TitleSceneDirectorComponent::GetCameraComponent() const {
    if (auto cam = cameraObj_.lock()) {
        return cam->GetComponent<CameraComponent>();
    }
    return nullptr;
}

TitleCosmicNebulaComponent* TitleSceneDirectorComponent::GetNebulaComponent() const {
    if (auto nebula = nebulaObj_.lock()) {
        return nebula->GetComponent<TitleCosmicNebulaComponent>();
    }
    return nullptr;
}

void TitleSceneDirectorComponent::CacheEntities() {
    auto scene = GetScene();
    if (!scene) {
        return;
    }

    // 自機の取得
    if (shipObj_.expired()) {
        if (auto shipObj = scene->FindGameObject("HeroShip")) {
            shipObj_ = shipObj;
            if (auto t = shipObj->GetTransform()) {
                initialShipPos_ = t->GetPosition();
                initialShipRot_ = t->GetRotation();
            }
        }
    }

    // カメラの取得
    if (cameraObj_.expired()) {
        if (auto camObj = scene->FindGameObject("MainCamera")) {
            cameraObj_ = camObj;
            if (auto t = camObj->GetTransform()) {
                initialCameraPos_ = t->GetPosition();
            }
            if (auto camComp = camObj->GetComponent<CameraComponent>()) {
                initialCameraFov_ = camComp->GetFovAngleY();
            }
        }
    }

    // 遠景アステロイドベルト（スペースデブリ群）のGPUインスタンシング初期化
    if (asteroidInstances_.empty()) {
        SpawnAsteroidBelt();
    }

    // 神秘的な星雲コンポーネントの取得
    if (nebulaObj_.expired()) {
        if (auto myObj = GetGameObject()) {
            if (myObj->GetComponent<TitleCosmicNebulaComponent>()) {
                nebulaObj_ = myObj->shared_from_this();
            }
        }
        if (nebulaObj_.expired()) {
            for (const auto& obj : scene->GetGameObjects()) {
                if (obj && obj->GetComponent<TitleCosmicNebulaComponent>()) {
                    nebulaObj_ = obj;
                    break;
                }
            }
        }
    }

    // 自機スラスターエフェクトのセットアップ（GameScene完全同期）
    SetupThrusterEffect();
}

void TitleSceneDirectorComponent::SpawnAsteroidBelt() {
    if (!asteroidInstances_.empty()) {
        return;
    }

    // GPUインスタンシング用 ModelBatchRendererComponent を取得またはアタッチ
    if (auto myObj = GetGameObject()) {
        asteroidBatchRenderer_ = myObj->GetComponent<ModelBatchRendererComponent>();
        if (!asteroidBatchRenderer_) {
            auto comp = myObj->AddComponent<ModelBatchRendererComponent>();
            asteroidBatchRenderer_ = comp.get();
        }
    }
    if (asteroidBatchRenderer_) {
        // モデルを 1 回だけ安全にロード
        asteroidBatchRenderer_->LoadModel("resources/model/Debris/Generic/Debris_Generic.obj");
    }

    // シード固定の疑似乱数生成器（毎回の起動で映画的配置を確定的に再現）
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> distZ(28.0f, 215.0f);
    std::uniform_real_distribution<float> distAngle(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> distScale(0.85f, 4.2f);
    std::uniform_real_distribution<float> distRot(0.0f, 6.2831853f);
    std::uniform_real_distribution<float> distTumble(-0.16f, 0.16f);
    std::uniform_real_distribution<float> distAmp(0.04f, 0.12f);
    std::uniform_real_distribution<float> distFreq(0.18f, 0.40f);

    const int kAsteroidCount = 42;
    asteroidInstances_.reserve(kAsteroidCount);

    for (int i = 0; i < kAsteroidCount; ++i) {
        // Z座標を奥へ均等かつ自然に分散（Z=12m〜210m）
        float progress = static_cast<float>(i) / static_cast<float>(kAsteroidCount);
        float z = std::lerp(12.0f, 210.0f, progress) + (distZ(rng) - 120.0f) * 0.08f;

        // 奥行き z に応じて視野が広がる（円錐・扇状の広がり）
        float depthFactor = std::sqrt(z / 12.0f);
        float minRadius = 3.0f + depthFactor * 0.7f; // 自機周辺（中央トンネル）は必ずクリアに保つ
        float maxRadius = 5.0f + depthFactor * 5.5f;

        std::uniform_real_distribution<float> distR(minRadius, maxRadius);
        float r = distR(rng);
        float angle = distAngle(rng);

        // 楕円状の広がり（左右に広く、上下はやや引き締めてシネマティックスコープ比に合致）
        float x = std::cos(angle) * r * 1.55f;
        float y = 0.52f + std::sin(angle) * r * 0.75f;

        Irufemi::Vector3 pos{x, y, z};

        // スケール決定（手前はシャープな岩塊、奥の巨石は雄大に巨大化）
        float baseScale = distScale(rng);
        if (z < 35.0f) {
            baseScale = std::clamp(baseScale, 1.2f, 2.5f);
        } else if (z > 90.0f) {
            baseScale *= 1.5f; // 遥か遠景の小惑星は雄大に巨大化
        }
        // 各軸にわずかな非対称性を持たせてゴツゴツ感を強調
        Irufemi::Vector3 scale{baseScale * (1.0f + (distTumble(rng) * 1.5f)),
                               baseScale * (1.0f + (distTumble(rng) * 1.5f)),
                               baseScale * (1.0f + (distTumble(rng) * 1.5f))};

        // 星雲の流動速度に完全同期した角速度（1周 約20〜45秒、角速度 0.14〜0.28 rad/s）
        // 星雲の渦潮（Cosmic Nebula Swirl）に乗ってグングン流れるダイナミックなペース
        float orbitSpeed = 0.26f * std::sqrt(5.5f / (r + 1.8f));

        AsteroidInstanceData ast;
        ast.basePos = pos;
        ast.currentPos = pos;
        ast.rotation = {distRot(rng), distRot(rng), distRot(rng)};
        ast.initialScale = scale;
        ast.currentScale = scale;
        ast.tumbleSpeed = {distTumble(rng), distTumble(rng), distTumble(rng)};
        ast.driftAmp = {distAmp(rng), distAmp(rng), distAmp(rng) * 0.5f};
        ast.driftFreq = {distFreq(rng), distFreq(rng), distFreq(rng)};
        ast.blastVelocity = {0.0f, 0.0f, 0.0f};
        ast.phase = distRot(rng);
        ast.orbitRadius = r;
        ast.orbitAngle = angle;
        ast.orbitSpeed = orbitSpeed;

        asteroidInstances_.push_back(ast);
    }
}

void TitleSceneDirectorComponent::CleanupAsteroidBelt() {
    if (asteroidBatchRenderer_) {
        asteroidBatchRenderer_->ClearInstances();
    }
    asteroidInstances_.clear();
    debrisObjs_.clear();
    debrisDrifts_.clear();
    debrisRepelOffsets_.clear();
    debrisBlastVelocities_.clear();
    initialDebrisPositions_.clear();
}

void TitleSceneDirectorComponent::RenderAsteroidBelt() {
    if (!asteroidBatchRenderer_) {
        if (auto myObj = GetGameObject()) {
            asteroidBatchRenderer_ = myObj->GetComponent<ModelBatchRendererComponent>();
        }
    }
    if (!asteroidBatchRenderer_) {
        return;
    }

    asteroidBatchRenderer_->ClearInstances();
    for (const auto& ast : asteroidInstances_) {
        Irufemi::Transform t{ast.currentScale, ast.rotation, ast.currentPos};
        asteroidBatchRenderer_->AddInstance(t);
    }
}

void TitleSceneDirectorComponent::SetupThrusterEffect() {
    if (thrusterObj_.lock()) {
        return;
    }
    auto scene = GetScene();
    if (!scene) {
        return;
    }
    auto shipObj = scene->FindGameObject("HeroShip");
    if (!shipObj) {
        return;
    }

    // GameSceneと全く同じ player_thruster_effect.json をノズル位置にアタッチ生成
    auto thruster = shipObj->Instantiate(thrusterEffectPrefabPath_, nozzleOffset_, true);
    if (thruster) {
        thruster->SetIsSerializable(false); // シーン保存時の汚染防止
        thruster->SetHideInHierarchy(true);
        thrusterObj_ = thruster;
        currentThrusterScaleZ_ = 0.85f;
        targetThrusterScaleZ_ = 0.85f;

        if (auto transform = thruster->GetTransform()) {
            transform->SetScale({1.0f, 1.0f, currentThrusterScaleZ_});
        }

        // タイトル画面で確実にスラスター粒子を点火
        auto emitters = thruster->GetComponentsInChildren<ParticleEmitterComponent>();
        for (auto pe : emitters) {
            pe->Play();
        }
    }
}

void TitleSceneDirectorComponent::TriggerDebrisExplosion() {
    if (hasExplodedDebris_) {
        return;
    }
    hasExplodedDebris_ = true;

    // 手前の小惑星（Z < 45m）にスラスター点火の衝撃波による外向き速度を付与
    for (size_t i = 0; i < asteroidInstances_.size(); ++i) {
        auto& ast = asteroidInstances_[i];
        if (ast.currentPos.z < 45.0f) {
            float angle = static_cast<float>(i) * 0.785398f;
            float outwardSpeed = 3.5f + static_cast<float>(i % 3) * 1.5f;
            ast.blastVelocity =
                Irufemi::Vector3{std::cos(angle) * outwardSpeed, std::sin(angle) * outwardSpeed * 0.6f, -2.0f};
        }
    }
}

void TitleSceneDirectorComponent::OnImpactRelease() {
    if (hasTriggeredRelease_) {
        return;
    }
    hasTriggeredRelease_ = true;
    isMicroFreezing_ = false;

    // 1. ガレキのVoxel粉砕飛散を一斉発火！
    TriggerDebrisExplosion();

    // 2. 音響の多重レイヤー同時炸裂！（エンジンのPlayByFileを活用した極めてクリーンな呼び出し）
    if (auto engine = GetEngine()) {
        if (auto am = engine->GetAudioManager()) {
            // A: 爆砕重低音（se_debris_shatter）
            am->PlayByFile("resources/audio/SE/se_debris_shatter.mp3", 0.95f, 1.0f, AudioCategory::SE,
                           "resources/audio/SE/se_menu_decide.mp3");
            // B: 推進点火・ドップラー遠ざかり音（se_player_boost）
            am->PlayByFile("resources/audio/SE/se_player_boost.mp3", 1.0f, 1.0f, AudioCategory::SE);
        }
    }

    // 3. カメラの大激震シェイク発火（振幅 0.045m, 継続1.8秒, 22Hz）
    if (auto camObj = cameraObj_.lock()) {
        if (auto shake = camObj->GetComponent<CameraShakeComponent>()) {
            shake->PlayShakeSeconds(0.045f, kDurationAccelerate_, 22.0f);
        }
    }
}

void TitleSceneDirectorComponent::Update() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    float deltaTime = engine->GetDeltaTime();
    if (deltaTime <= 0.0f) {
        deltaTime = 1.0f / 60.0f;
    }

    CacheEntities();

    if (!isLaunching_) {
        UpdateIdling(deltaTime);
    } else {
        UpdateLaunchSequence(deltaTime);
    }

    // 毎フレーム GPU インスタンシングで 42基の小惑星を一括描画
    RenderAsteroidBelt();
}

void TitleSceneDirectorComponent::UpdateIdling(float deltaTime) {
    idleTimer_ += deltaTime;

    auto shipTransform = GetShipTransform();
    auto cameraTransform = GetCameraTransform();

    // 1. 自機のホバリング浮遊（上下微動 + ロール揺らぎ）
    if (shipTransform) {
        float hoverY = initialShipPos_.y + std::sin(idleTimer_ * 1.8f) * 0.07f;
        float rollZ = initialShipRot_.z + std::sin(idleTimer_ * 1.2f) * 0.035f;
        float pitchX = initialShipRot_.x + std::cos(idleTimer_ * 1.5f) * 0.020f;

        shipTransform->SetPosition({initialShipPos_.x, hoverY, initialShipPos_.z});
        shipTransform->SetRotation({pitchX, initialShipRot_.y, rollZ});
    }

    // 2. カメラの呼吸揺れ
    if (cameraTransform) {
        float swayX = initialCameraPos_.x + std::sin(idleTimer_ * 0.8f) * 0.03f;
        float swayY = initialCameraPos_.y + std::cos(idleTimer_ * 0.9f) * 0.025f;
        cameraTransform->SetPosition({swayX, swayY, initialCameraPos_.z});
    }

    // 3. アステロイドベルト（スペースデブリ群）の星雲同期スワール公転 ＆ 3軸自転
    for (auto& ast : asteroidInstances_) {
        // 差動公転角の更新（星雲の反時計回り渦流に同期して緩やかに公転）
        ast.orbitAngle += ast.orbitSpeed * deltaTime;
        if (ast.orbitAngle > 6.2831853f * 100.0f) {
            ast.orbitAngle -= 6.2831853f * 100.0f;
        }

        // 楕円軌道（シネマティックスコープ比: X=1.55, Y=0.75, 星雲重力特異点中心 Y=0.52m）
        float orbitX = std::cos(ast.orbitAngle) * ast.orbitRadius * 1.55f;
        float orbitY = 0.52f + std::sin(ast.orbitAngle) * ast.orbitRadius * 0.75f;

        // 微小な宇宙波打ち（深宇宙の揺らぎ）
        float timeVal = idleTimer_ + ast.phase;
        float driftX = std::sin(timeVal * ast.driftFreq.x) * ast.driftAmp.x +
                       std::cos(timeVal * ast.driftFreq.x * 0.45f) * (ast.driftAmp.x * 0.35f);
        float driftY = std::cos(timeVal * ast.driftFreq.y) * ast.driftAmp.y +
                       std::sin(timeVal * ast.driftFreq.y * 0.55f) * (ast.driftAmp.y * 0.30f);
        float driftZ = std::sin(timeVal * ast.driftFreq.z) * ast.driftAmp.z;

        ast.currentPos = Irufemi::Vector3{orbitX + driftX, orbitY + driftY, ast.basePos.z + driftZ};

        // 斜め3軸の重厚な自転（タンブリング）
        ast.rotation.x += deltaTime * ast.tumbleSpeed.x;
        ast.rotation.y += deltaTime * ast.tumbleSpeed.y;
        ast.rotation.z += deltaTime * ast.tumbleSpeed.z;
    }

    // 4. 自機スラスターの呼吸脈動（GameSceneアイドル時と完全準拠）
    targetThrusterScaleZ_ = 0.85f + std::sin(idleTimer_ * 2.2f) * 0.10f;
    float lerpFactor = 1.0f - std::exp(-10.0f * deltaTime);
    currentThrusterScaleZ_ = std::lerp(currentThrusterScaleZ_, targetThrusterScaleZ_, lerpFactor);

    if (auto thruster = thrusterObj_.lock()) {
        if (auto t = thruster->GetTransform()) {
            t->SetScale({1.0f, 1.0f, currentThrusterScaleZ_});
        }
    }
}

void TitleSceneDirectorComponent::StartLaunchSequence() {
    if (isLaunching_) {
        return;
    }

    isLaunching_ = true;
    launchTimer_ = 0.0f;
    hasTriggeredSceneTransition_ = false;
    hasExplodedDebris_ = false;

    // State パターン: [Phase 1: 蓄勢 (Charge)] へ突入
    ChangeState(std::make_unique<TitleLaunchStateCharge>());
}

void TitleSceneDirectorComponent::SkipLaunchSequence() {
    if (hasTriggeredSceneTransition_) {
        return;
    }

    CleanupLaunchPostProcess();
    hasTriggeredSceneTransition_ = true;
    if (auto engine = GetEngine()) {
        if (auto am = engine->GetAudioManager()) {
            am->StopCategory(AudioCategory::SE);
        }
        if (auto sm = engine->GetSceneManager()) {
            // 世界観に合致した重力シアンブラックで空間を吸い込み暗転
            sm->TransitionTo(nextSceneName_, SceneTransition::Type::RadialBlur, 0.35f, EaseType::EaseInQuad,
                             Irufemi::Vector4{0.01f, 0.03f, 0.06f, 1.0f});
        }
    }
}

void TitleSceneDirectorComponent::ChangeState(std::unique_ptr<ITitleLaunchState> newState) {
    if (currentLaunchState_) {
        currentLaunchState_->Exit(this);
    }
    currentLaunchState_ = std::move(newState);
    stateTimer_ = 0.0f;
    if (currentLaunchState_) {
        currentLaunchStateType_ = static_cast<LaunchState>(currentLaunchState_->GetStateType());
        currentLaunchState_->Enter(this);
    } else {
        currentLaunchStateType_ = LaunchState::Idle;
    }
}

void TitleSceneDirectorComponent::ChangeState(LaunchState newState) {
    using LaunchStateFactory = std::unique_ptr<ITitleLaunchState> (*)();
    static const std::unordered_map<LaunchState, LaunchStateFactory> kLaunchStateFactories = {
        {LaunchState::Idle, []() -> std::unique_ptr<ITitleLaunchState> { return nullptr; }},
        {LaunchState::Charge,
         []() -> std::unique_ptr<ITitleLaunchState> { return std::make_unique<TitleLaunchStateCharge>(); }},
        {LaunchState::Accelerate,
         []() -> std::unique_ptr<ITitleLaunchState> { return std::make_unique<TitleLaunchStateAccelerate>(); }},
        {LaunchState::Break,
         []() -> std::unique_ptr<ITitleLaunchState> { return std::make_unique<TitleLaunchStateBreak>(); }},
        {LaunchState::Afterglow,
         []() -> std::unique_ptr<ITitleLaunchState> { return std::make_unique<TitleLaunchStateAfterglow>(); }},
    };

    if (auto it = kLaunchStateFactories.find(newState); it != kLaunchStateFactories.end()) {
        ChangeState(it->second());
    }
}

TitleSceneDirectorComponent::LaunchState TitleSceneDirectorComponent::GetLaunchState() const {
    if (currentLaunchState_) {
        return static_cast<LaunchState>(currentLaunchState_->GetStateType());
    }
    return currentLaunchStateType_;
}

void TitleSceneDirectorComponent::TriggerSceneTransition(float fadeDuration) {
    if (hasTriggeredSceneTransition_) {
        return;
    }
    hasTriggeredSceneTransition_ = true;
    if (auto engine = GetEngine()) {
        if (auto sm = engine->GetSceneManager()) {
            // 重力特異点跳躍（事象の地平線への空間吸い込み・重力色収差・流体渦巻き連動）
            sm->TransitionTo(nextSceneName_, SceneTransition::Type::GravitySingularity, fadeDuration,
                             EaseType::EaseInCubic, Irufemi::Vector4{0.005f, 0.015f, 0.035f, 1.0f});
        }
    }
}

void TitleSceneDirectorComponent::UpdateLaunchSequence(float deltaTime) {
    launchTimer_ += deltaTime;

    // プレイヤーによるクイック・スキップ判定（出撃開始から0.25秒後以降、決定キーまたはクリックで即フェード突入）
    if (launchTimer_ > 0.25f && !hasTriggeredSceneTransition_) {
        if (auto engine = GetEngine()) {
            if (auto input = engine->GetInputManager()) {
                if (InputHelper::IsActionPressed(input, GameAction::UI_Submit) || input->IsCursorActionPressed()) {
                    SkipLaunchSequence();
                    return;
                }
            }
        }
    }

    // Stateパターン更新（多態性呼び出し）
    stateTimer_ += deltaTime;
    if (currentLaunchState_) {
        currentLaunchState_->Update(this, deltaTime);
    }

    // 出撃加速・離脱時の動的シネマティック・ポストプロセス更新
    UpdateLaunchPostProcess(deltaTime);

    // スラスターのスケールを急峻に追従
    float lerpFactor = 1.0f - std::exp(-18.0f * deltaTime);
    currentThrusterScaleZ_ = std::lerp(currentThrusterScaleZ_, targetThrusterScaleZ_, lerpFactor);

    if (auto thruster = thrusterObj_.lock()) {
        if (auto transform = thruster->GetTransform()) {
            transform->SetScale({1.0f, 1.0f, currentThrusterScaleZ_});
        }
    }
}

void TitleSceneDirectorComponent::UpdateLaunchPostProcess(float deltaTime) {
    (void)deltaTime;
    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    auto ppm = engine->GetPostProcessManager();
    if (!ppm) {
        return;
    }

    // Phase 1 (Charge) または Idle 中、出撃未開始時は適用しない
    if (!isLaunching_ || currentLaunchStateType_ == LaunchState::Idle ||
        currentLaunchStateType_ == LaunchState::Charge) {
        CleanupLaunchPostProcess();
        return;
    }

    // 大手ゲームエンジンのプラクティス：2DメニューUI（フェード消滅中）を汚染せず、3D空間と星雲にのみ適用
    // Layer::PreUI でエフェクトスタックに安全に登録（初回フレームでのみ登録）
    if (!isLaunchPostProcessActive_) {
        ppm->AddActiveMode(PostProcessMode::Vignette, PostProcessManager::Layer::PreUI);
        ppm->AddActiveMode(PostProcessMode::RadialBlur, PostProcessManager::Layer::PreUI);
        ppm->AddActiveMode(PostProcessMode::ChromaticAberration, PostProcessManager::Layer::PreUI);
        isLaunchPostProcessActive_ = true;
    }

    // タイムライン連動アニメーションの目標値
    float targetBlurWidth = 0.0f;
    float targetVignetteRadius = 1.0f;
    float targetChroma = 0.0f;

    // 星雲シェーダー（CosmicNebula.PS.hlsl）および重力特異点跳躍（GravitySingularity）と完全合致した焦点
    const Irufemi::Vector2 kSingularityCenter{0.50f, 0.42f};

    if (currentLaunchStateType_ == LaunchState::Accelerate) {
        // [Phase 2: 咆哮・点火急加速]
        // 臨界蓄圧（マイクロフリーズ 0.045s）中は身震いのみとし、光学的歪みは保留
        float effectiveTimer = (std::max)(0.0f, stateTimer_ - kDurationFreeze_);
        float duration = (std::max)(0.001f, kDurationAccelerate_ - kDurationFreeze_);
        float p = std::clamp(effectiveTimer / duration, 0.0f, 1.0f);

        // 自機の超推力急加速曲線（4次急加速）に自然に呼応する3次イーズイン
        float curve = p * p * p;

        // トンネル視野狭窄: 外周から徐々に重力シアンブラックの暗がりが絞り込まれる (1.00 -> 0.70)
        targetVignetteRadius = std::lerp(1.00f, 0.70f, curve);

        // 特異点ラジアルブラー: 自機の推進力に合わせて外周の星雲がじわじわと流れる (0.00 -> 0.016)
        targetBlurWidth = std::lerp(0.0f, 0.016f, curve);

        // 速度ドップラー色収差: わずかなレンズ周辺赤方偏移 (0.00 -> 0.006)
        targetChroma = std::lerp(0.0f, 0.006f, curve);

    } else if (currentLaunchStateType_ == LaunchState::Break) {
        // [Phase 3: 突破・超光速ダイブ]
        float p = std::clamp(stateTimer_ / (std::max)(0.001f, kDurationBreak_), 0.0f, 1.0f);

        // 超光速の極点へ向けてサインカーブで滑らかに飽和
        float curve = std::sin(p * 1.5707963f);

        // トンネル最深部: 事象の地平線トンネルに飛び込む (0.70 -> 0.52)
        targetVignetteRadius = std::lerp(0.70f, 0.52f, curve);

        // ラジアルブラー最大ピーク (0.016 -> 0.032)
        targetBlurWidth = std::lerp(0.016f, 0.032f, curve);

        // レンズ色収差ピーク (0.006 -> 0.014)
        targetChroma = std::lerp(0.006f, 0.014f, curve);

    } else if (currentLaunchStateType_ == LaunchState::Afterglow) {
        // [Phase 4: 余韻・自機消失後]
        // 自機が特異点へ消滅した直後、ピークのワープ歪みを維持し、
        // 1.25秒後に発火する GravitySingularity シーン遷移（暗転）へシームレスにバトンタッチ
        targetVignetteRadius = 0.52f;
        targetBlurWidth = 0.032f;
        targetChroma = 0.014f;
    }

    // --- 各ポストプロセスパラメータの安全な適用 ---
    // 1. トンネル視野狭窄 (Vignette): 深遠な重力シアンブラックで外周を包み込み、中央の特異点へ視線を誘導
    auto& vig = ppm->GetVignetteParams();
    vig.color = Irufemi::Vector4{0.005f, 0.015f, 0.035f, 1.0f}; // 世界観に合致した重力シアンブラック
    vig.radius = targetVignetteRadius;
    vig.softness = 0.45f;

    // 2. 特異点ラジアルブラー (RadialBlur): 中心(0.50, 0.42)の特異点に向けて周辺星雲を吸い込み歪曲
    auto& rb = ppm->GetRadialBlurParams();
    rb.center = kSingularityCenter;
    rb.blurWidth = targetBlurWidth;
    rb.numSamples = 24;

    // 3. 光学的赤方偏移 (ChromaticAberration): 超光速突破に伴う繊細な波長ズレ
    auto& ca = ppm->GetChromaticAberrationParams();
    ca.intensity = targetChroma;
}

void TitleSceneDirectorComponent::CleanupLaunchPostProcess() {
    if (auto nebulaComp = GetNebulaComponent()) {
        nebulaComp->SetCollapseProgress(0.0f);
        nebulaComp->SetSwirlMultiplier(1.0f);
    }

    if (isLaunchPostProcessActive_) {
        if (auto engine = GetEngine()) {
            if (auto ppm = engine->GetPostProcessManager()) {
                ppm->RemoveActiveMode(PostProcessMode::Vignette);
                ppm->RemoveActiveMode(PostProcessMode::RadialBlur);
                ppm->RemoveActiveMode(PostProcessMode::ChromaticAberration);

                // パラメータを安全にデフォルト初期化
                auto& rb = ppm->GetRadialBlurParams();
                rb.blurWidth = 0.0f;
                auto& vig = ppm->GetVignetteParams();
                vig.radius = 1.0f;
                auto& ca = ppm->GetChromaticAberrationParams();
                ca.intensity = 0.0f;
            }
        }
        isLaunchPostProcessActive_ = false;
    }
}

bool TitleSceneDirectorComponent::GetClosestFrontDebrisScreenPos(Irufemi::Vector2& outUIPos) const {
    auto cameraComp = GetCameraComponent();
    if (!cameraComp) {
        return false;
    }
    auto camera = cameraComp->GetCamera();
    if (!camera) {
        return false;
    }

    Irufemi::Matrix4x4 viewProj = camera->GetViewProjectionMatrix3D();
    float bestZ = 9999.0f;
    bool found = false;

    for (const auto& ast : asteroidInstances_) {
        Irufemi::Vector3 clipPos = Irufemi::Math::Transform(ast.currentPos, viewProj);

        // 画面手前にあり、カメラ視野内
        if (clipPos.z > 0.05f && clipPos.z < 1.0f) {
            // より手前（カメラに近いもの）を優先
            if (clipPos.z < bestZ) {
                bestZ = clipPos.z;
                float screenX = (clipPos.x + 1.0f) * 0.5f * camera->GetViewportWidth();
                float screenY = (1.0f - clipPos.y) * 0.5f * camera->GetViewportHeight();
                outUIPos = camera->ScreenToUIPosition({screenX, screenY});
                found = true;
            }
        }
    }
    return found;
}

void TitleSceneDirectorComponent::TriggerGravitationalShockwave(float power) {
    shockwaveIntensity_ = (std::max)(shockwaveIntensity_, power);
}
