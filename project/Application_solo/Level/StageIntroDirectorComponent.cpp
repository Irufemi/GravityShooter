#include "StageIntroDirectorComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/Scene/SceneTransition.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/Camera/CameraShakeComponent.h"
#include "Framework/Component/Effect/ParticleEmitterComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Renderer/Fluid/FluidSimulation2D.h"
#include "Core/System/IrufemiEngine.h"
#include "RailMechanics/RailShooterPlayerComponent.h"
#include "RailMechanics/SplineFollowerComponent.h"
#include "Player/PlayerThrusterVisualizerComponent.h"
#include "Audio/AudioManager.h"
#include "Input/GameAction.h"
#include "Platform/Input/InputManager.h"
#include "UI/UISound.h"
#include "UI/ReticleUIComponent.h"
#include <algorithm>
#include <cmath>

StageIntroDirectorComponent::~StageIntroDirectorComponent() {
    CleanupWarpVisuals();
    DestroyMembraneObject();
}

void StageIntroDirectorComponent::OnDestroy() {
    CleanupWarpVisuals();
    DestroyMembraneObject();
    if (auto cart = playerCartObj_.lock()) {
        if (auto follower = cart->GetComponent<SplineFollowerComponent>()) {
            follower->SetSpeedMultiplier(1.0f);
        }
    }
}

void StageIntroDirectorComponent::Initialize() {
    introState_ = StageIntroState::WarpIn;
    stateTimer_ = 0.0f;
    hasInitializedShip_ = false;
    hasTriggeredBreach_ = false;
    breachTimer_ = 0.0f;

    // シーン初期化時点で演出SEを先行ウォームアップ（実行時ディスクI/O根絶）
    if (auto engine = GetEngine()) {
        if (auto am = engine->GetAudioManager()) {
            if (!warpSePath_.empty()) {
                am->GetOrLoadSoundByFile(warpSePath_);
            }
            if (!fcsBootSePath_.empty()) {
                am->GetOrLoadSoundByFile(fcsBootSePath_);
            }
            if (!breachSePath_.empty()) {
                am->GetOrLoadSoundByFile(breachSePath_);
            }
        }
    }

    // 初期化時点でHUDを即座に非表示化
    SetHUDActive(false);
}

void StageIntroDirectorComponent::Start() {
    // 全コンポーネントのStartフェーズで先行して自機をワープイン開始地点へ配置し、
    // 定位置（Z=0）でのスラスター放出やHUD表示を未然に遮断する
    EnsureShipInitialized();

    // レール追従コンポーネントを持つ PlayerCart を先行キャッシュ（毎フレーム探索の根絶）
    if (auto scene = GetScene()) {
        if (auto cart = scene->FindGameObject("PlayerCart")) {
            playerCartObj_ = cart;
        }
    }

    // 空間に物理配置される重力水面膜（3Dメッシュ & EffectMask）を生成・初期化
    EnsureMembraneInitialized();
}

void StageIntroDirectorComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("WarpIn Duration", &warpInDuration_);
    RegisterProperty("Arrival Duration", &arrivalDuration_);
    RegisterProperty("Bootup Duration", &bootupDuration_);
    RegisterProperty("Start Offset Z", &startOffsetZ_);
    RegisterProperty("WarpIn FOV Boost", &warpInFovBoostDeg_);
    RegisterProperty("WarpIn Blur Width", &maxWarpBlurWidth_);
    RegisterProperty("Target Ship Name", &targetShipName_);
    RegisterProperty("Rail Speed Boost", &warpInRailSpeedBoost_);
    RegisterProperty("Warp SE Path", &warpSePath_);
    RegisterProperty("FCS Boot SE Path", &fcsBootSePath_);

    // 空間物理配置 重力エネルギー界面（Primitive + EffectMask）突破用プロパティ
    RegisterProperty("Membrane Offset Z", &membraneOffsetZ_);
    RegisterProperty("Membrane Scale X", &membraneScaleX_);
    RegisterProperty("Membrane Scale Y", &membraneScaleY_);
    RegisterProperty("Membrane Displacement", &membraneDisplacementIntensity_);
    RegisterProperty("Membrane RGB Shift", &membraneRgbShiftRatio_);
    RegisterProperty("Breach SE Path", &breachSePath_);
    RegisterProperty("Breach Progress Threshold", &breachProgressThreshold_);
    RegisterProperty("Breach Shake Amplitude", &breachShakeAmplitude_);
}

bool StageIntroDirectorComponent::EnsureShipInitialized() {
    if (hasInitializedShip_) {
        return true;
    }

    auto scene = GetScene();
    if (!scene) {
        return false;
    }

    // --- 一線級エンジンのオブジェクト参照解決プラクティス ---
    // 階層構造（PlayerCartの子オブジェクト）やPrefab Linkingに対応するため多重フォールバックで探索
    std::shared_ptr<GameObject> ship = scene->FindGameObject(targetShipName_);
    if (!ship) {
        // フォールバック 1: 親オブジェクト PlayerCart の子オブジェクト群から探索
        if (auto cart = scene->FindGameObject("PlayerCart")) {
            for (const auto& child : cart->GetChildren()) {
                if (child && child->GetName() == targetShipName_) {
                    ship = child;
                    break;
                }
            }
            // フォールバック 2: RailShooterPlayerComponent を持つ子孫から型安全に逆引き
            if (!ship) {
                if (auto playerComp = cart->GetComponentInChildren<RailShooterPlayerComponent>()) {
                    ship = scene->FindGameObject(playerComp->GetGameObject());
                }
            }
        }
    }

    // フォールバック 3: シーン全体の全オブジェクトから RailShooterPlayerComponent を型安全に探索
    if (!ship) {
        for (const auto& obj : scene->GetGameObjects()) {
            if (obj) {
                if (auto playerComp = obj->GetComponentInChildren<RailShooterPlayerComponent>()) {
                    ship = scene->FindGameObject(playerComp->GetGameObject());
                    if (ship) {
                        break;
                    }
                }
            }
        }
    }

    if (!ship) {
        return false;
    }

    shipObj_ = ship;
    auto transform = ship->GetTransform();
    if (!transform) {
        return false;
    }

    initialShipLocalPos_ = transform->GetPosition();
    initialShipLocalRot_ = transform->GetRotation();

    // カメラの取得とベースFOVのキャッシュ（型安全フォールバック対応）
    if (cameraObj_.expired()) {
        std::shared_ptr<GameObject> camObj = scene->FindGameObject("MainCamera");
        if (!camObj) {
            for (const auto& obj : scene->GetGameObjects()) {
                if (obj && obj->GetComponentInChildren<CameraComponent>()) {
                    camObj = obj;
                    break;
                }
            }
        }
        if (camObj) {
            cameraObj_ = camObj;
            if (auto cam = camObj->GetComponent<CameraComponent>()) {
                baseCameraFov_ = cam->GetFovAngleY();
            }
        }
    }

    // 自機を入力無効化し、初期位置（カメラ後方）へオフセット
    if (auto playerComp = ship->GetComponent<RailShooterPlayerComponent>()) {
        playerComp->SetInputEnabled(false);
        playerComp->SetState(PlayerFlightState::WarpEntry);
    }

    Irufemi::Vector3 startPos = initialShipLocalPos_;
    startPos.z += startOffsetZ_;
    transform->SetPosition(startPos);
    transform->CheckAndComputeMatrix();

    // イントロ疾走区間として負のレール距離（-55.0m）からスタート！
    if (playerCartObj_.expired()) {
        if (auto scene = GetScene()) {
            playerCartObj_ = scene->FindGameObject("PlayerCart");
        }
    }
    if (auto cart = playerCartObj_.lock()) {
        if (auto follower = cart->GetComponent<SplineFollowerComponent>()) {
            follower->SnapToDistance(-55.0f);
            follower->SetSpeedMultiplier(warpInRailSpeedBoost_);
        }
    }

    SetHUDActive(false);
    SetShipThrusterScale(3.8f, true); // 超光速突入用の超高出力オーバードライブブースト炎（3.8倍）を展開
    SetShipThrusterActive(true); // 突入開始位置（後方）でスラスター点火

    // シーン突入開始の瞬間（フェードイン中・第1フレーム）から最大広角とラジアルブラーを先行点火！
    UpdateWarpVisuals(0.0f);

    hasInitializedShip_ = true;
    return true;
}

bool StageIntroDirectorComponent::EnsureMembraneInitialized() {
    if (membraneObj_) {
        return true;
    }

    auto scene = GetScene();
    if (!scene) {
        return false;
    }

    // レール進行親オブジェクト（PlayerCart）を探索・取得
    auto cart = playerCartObj_.lock();
    if (!cart) {
        cart = scene->FindGameObject("PlayerCart");
        if (cart) {
            playerCartObj_ = cart;
        }
    }
    if (!cart) {
        return false;
    }

    // 1. 物理的な重力エネルギー界面GameObjectを生成
    auto membrane = std::make_shared<GameObject>("GravityMembrane");
    membrane->SetIsSerializable(false); // シーン保存への混入を遮断
    membrane->SetHideInHierarchy(true);

    // 2. Transform 設定（自機ローカル空間の前方 Z = membraneOffsetZ_ に配置）
    if (auto transform = membrane->GetTransform()) {
        transform->SetPosition({0.0f, 0.0f, membraneOffsetZ_});
        transform->SetRotation({0.0f, 0.0f, 0.0f});
        transform->SetScale({membraneScaleX_, membraneScaleY_, 1.0f});
    }

    // 3. PrimitiveRendererComponent 設定（Plane板ポリゴン・Unlit・テクスチャなし・完全透過）
    auto prim = membrane->AddComponent<PrimitiveRendererComponent>();
    prim->SetShape(Irufemi::PrimitiveType::Plane);
    prim->SetTexture("");                       // UVチェッカーのロードを完全遮断
    prim->SetEnableLighting(false);             // Unlit（重力エネルギー自己発光）
    prim->SetColor({0.15f, 0.85f, 1.0f, 0.0f}); // 背景を完全透過し、EffectMaskの空間歪曲のみを投影
    prim->SetIsTransparent(true); // 深度書き込み（DepthWrite）を無効化し、背後の環境物や敵の描画遮断を完全根絶

    // 4. EffectMaskComponent 設定（膜の領域だけに局所的にSimplex Noise空間屈折・DisplacementMapを適用）
    auto maskComp = membrane->AddComponent<EffectMaskComponent>();
    maskComp->SetEnableEffectMask(true);
    maskComp->SetCustomEffectType(24); // kPostProcessMode_DisplacementMap
    PostProcessManager::CustomEffectParams cParams{};
    cParams.param1 = membraneDisplacementIntensity_; // 重力レンズ屈折歪み強度
    cParams.param2 = membraneRgbShiftRatio_;         // 色収差比率
    maskComp->SetCustomParams(cParams);

    // 5. 親（PlayerCart）の子オブジェクトとして登録
    cart->AddChild(membrane);
    membraneObj_ = membrane;

    return true;
}

void StageIntroDirectorComponent::UpdateMembraneVisuals(float progress) {
    if (!membraneObj_) {
        return;
    }

    auto transform = membraneObj_->GetTransform();
    if (!transform) {
        return;
    }

    auto prim = membraneObj_->GetComponent<PrimitiveRendererComponent>();
    auto maskComp = membraneObj_->GetComponent<EffectMaskComponent>();

    if (progress < breachProgressThreshold_) {
        // 境界界面の微細な重力脈動
        float pulse = 1.0f + 0.02f * std::sin(progress * 18.0f);
        transform->SetScale({membraneScaleX_ * pulse, membraneScaleY_ * pulse, 1.0f});

        // 突破前: 自機の接近に伴い滑らかに重力界面の歪みがピークへ盛り上がる（クレッシェンド）
        if (maskComp) {
            float approach = std::clamp(progress / (std::max)(0.001f, breachProgressThreshold_), 0.0f, 1.0f);
            float intensity = membraneDisplacementIntensity_ * (0.60f + 0.40f * approach) *
                              (1.0f + 0.03f * std::sin(progress * 18.0f));
            PostProcessManager::CustomEffectParams cParams{};
            cParams.param1 = intensity;
            cParams.param2 = membraneRgbShiftRatio_;
            maskComp->SetCustomParams(cParams);
        }
        membraneObj_->SetActive(true);
        if (prim) {
            prim->SetColor({0.15f, 0.85f, 1.0f, 0.0f});
        }
    } else {
        // 突破後（Breach Moment!）:
        // 自機が重力界面を突き破り、界面が外側へ弾け飛びながら滑らかに空間へ消散（S字SmoothStep減衰）
        float t = std::clamp(
            (progress - breachProgressThreshold_) / (std::max)(0.001f, 1.0f - breachProgressThreshold_), 0.0f, 1.0f);
        float easeOut = 1.0f - t;
        float decay = easeOut * easeOut * (3.0f - 2.0f * easeOut); // C2連続 SmoothStep 減衰（末尾で傾きゼロに着地）

        // 自機の貫通穴が急拡大して外側へ引き裂かれるダイナミック拡散
        float expand = 1.0f + 1.8f * (1.0f - easeOut * easeOut);
        transform->SetScale({membraneScaleX_ * expand, membraneScaleY_ * expand, 1.0f});

        if (maskComp) {
            PostProcessManager::CustomEffectParams cParams{};
            cParams.param1 = membraneDisplacementIntensity_ * decay;
            cParams.param2 = membraneRgbShiftRatio_ * decay;
            maskComp->SetCustomParams(cParams);
        }
        if (t >= 1.0f || decay <= 0.0001f) {
            membraneObj_->SetActive(false);
        }
    }
}

void StageIntroDirectorComponent::DestroyMembraneObject() {
    if (!membraneObj_) {
        return;
    }

    membraneObj_->Destroy();

    if (auto cart = playerCartObj_.lock()) {
        cart->RemoveChild(membraneObj_);
    } else if (auto parent = membraneObj_->GetParent()) {
        parent->RemoveChild(membraneObj_);
    }

    membraneObj_->SetActive(false);
    membraneObj_.reset();
}

void StageIntroDirectorComponent::SetIntroState(StageIntroState newState) {
    introState_ = newState;
    stateTimer_ = 0.0f;

    struct PhaseConfig {
        float StageIntroDirectorComponent::*durationMember;
        float initialThrusterScale;
        float targetThrusterScale;
        bool isPositionInterpolated;
        bool isSnapPositionOnEnter;
        bool isHudActive;
        bool isInputEnabled;
        void (*onEnterHook)(StageIntroDirectorComponent*);
        void (*onUpdateHook)(StageIntroDirectorComponent*, float progress);
    };

    static const std::unordered_map<StageIntroState, PhaseConfig> kPhaseConfigs = {
        {StageIntroState::WarpIn,
         {&StageIntroDirectorComponent::warpInDuration_, 3.8f, 3.8f, true, false, false, false, nullptr, nullptr}},
        {StageIntroState::Arrival,
         {&StageIntroDirectorComponent::arrivalDuration_, 3.8f, 0.8f, false, true, false, false, nullptr,
          [](StageIntroDirectorComponent* self, float progress) { self->ApplyArrivalInertia(progress); }}},
        {StageIntroState::Bootup,
         {&StageIntroDirectorComponent::bootupDuration_, 0.8f, 0.8f, false, true, true, false,
          [](StageIntroDirectorComponent* self) { self->TriggerFCSBootupSequence(); }, nullptr}},
        {StageIntroState::Active, {nullptr, 0.8f, 0.8f, false, false, true, true, nullptr, nullptr}},
    };

    auto it = kPhaseConfigs.find(introState_);
    if (it == kPhaseConfigs.end()) {
        return;
    }
    const auto& config = it->second;

    SetHUDActive(config.isHudActive);
    SetShipThrusterActive(true);
    SetShipThrusterScale(config.initialThrusterScale, true);

    if (config.isSnapPositionOnEnter) {
        if (auto ship = shipObj_.lock()) {
            if (auto transform = ship->GetTransform()) {
                transform->SetPosition(initialShipLocalPos_);
            }
        }
    }

    if (auto ship = shipObj_.lock()) {
        if (auto playerComp = ship->GetComponent<RailShooterPlayerComponent>()) {
            playerComp->SetInputEnabled(config.isInputEnabled);
            if (config.isInputEnabled) {
                playerComp->SetState(PlayerFlightState::Idle); // イントロ完了時に通常操縦へ移行
            }
        }
    }

    if (config.onEnterHook) {
        config.onEnterHook(this);
    }

    // 通常戦闘（Active）へ移行した瞬間に、厳密に 0.0m 地点からレール進行を開始
    if (introState_ == StageIntroState::Active) {
        if (auto cart = playerCartObj_.lock()) {
            if (auto follower = cart->GetComponent<SplineFollowerComponent>()) {
                follower->SnapToDistance(0.0f);
                follower->SetSpeedMultiplier(1.0f);
            }
        }
    }
}

void StageIntroDirectorComponent::Update() {
    // 演出完了済みの場合は処理をスキップ
    if (introState_ == StageIntroState::Active) {
        return;
    }

    if (!hasInitializedShip_) {
        if (!EnsureShipInitialized()) {
            return;
        }
    }

    auto engine = GetEngine();
    // シーン遷移（暗転・フェードイン中・ロード中・ロード画面表示中）は演出タイマーを完全停止して初期状態で待機
    bool isTransitionActive = false;
    if (engine) {
        if (auto st = engine->GetSceneTransition()) {
            if (st->IsActive()) {
                isTransitionActive = true;
            }
        }
        if (auto sm = engine->GetSceneManager()) {
            if (sm->IsTransitioning() || sm->IsLoading() || sm->ShouldDrawLoadingScreen()) {
                isTransitionActive = true;
            }
        }
    }

    if (isTransitionActive) {
        stateTimer_ = 0.0f;
        UpdateWarpVisuals(0.0f);

        // 暗転・ロード・遷移中は自機を初期突入位置（カメラ前方の手前ドアップ）に確実に固定
        if (auto ship = shipObj_.lock()) {
            if (auto transform = ship->GetTransform()) {
                Irufemi::Vector3 p = initialShipLocalPos_;
                p.z += startOffsetZ_;
                transform->SetPosition(p);
                transform->CheckAndComputeMatrix();
            }
        }
        // 膜オブジェクトも初期位置・スケール・表示状態に確実に固定
        if (membraneObj_) {
            membraneObj_->SetActive(true);
            if (auto transform = membraneObj_->GetTransform()) {
                transform->SetPosition({0.0f, 0.0f, membraneOffsetZ_});
                transform->SetScale({membraneScaleX_, membraneScaleY_, 1.0f});
                transform->CheckAndComputeMatrix();
            }
            if (auto prim = membraneObj_->GetComponent<PrimitiveRendererComponent>()) {
                prim->SetTexture("");
                prim->SetColor({0.15f, 0.85f, 1.0f, 0.0f});
                prim->SetIsTransparent(true);
            }
        }
        return;
    }

    float deltaTime = engine ? engine->GetDeltaTime() : (1.0f / 60.0f);
    if (deltaTime <= 0.0f) {
        deltaTime = 1.0f / 60.0f;
    }

    // プレイヤーによるクイックスキップ判定 (出撃クリックやチャタリングの即死誤爆防止)
    // 1. 演出開始から最低0.60秒間（水面膜への突進・突破完了前）はスキップ入力を一切受け付けない
    // 2. ゲームプレイ中のマウスクリックでの暴発を防ぐため、明示的なSPACEキー/ESCAPEキー/STARTボタンのみに限定
    if (engine && stateTimer_ > 0.60f) {
        if (auto input = engine->GetInputManager()) {
            bool isExplicitSkip = input->IsKeyPressed(VK_SPACE) || input->IsKeyPressed(VK_ESCAPE);
            if (auto pad = input->GetGamePad()) {
                if (pad->IsButtonPressed(XINPUT_GAMEPAD_START) || pad->IsButtonPressed(XINPUT_GAMEPAD_BACK)) {
                    isExplicitSkip = true;
                }
            }
            if (isExplicitSkip) {
                SkipIntroSequence();
                return;
            }
        }
    }

    // 初回フレームの巨大なdeltaTimeスパイク（ロード直後のヒッチ）を安全に防ぐ
    float safeDeltaTime = (std::min)(deltaTime, 1.0f / 30.0f);
    stateTimer_ += safeDeltaTime;

    struct PhaseConfig {
        float StageIntroDirectorComponent::*durationMember;
        float initialThrusterScale;
        float targetThrusterScale;
        bool isPositionInterpolated;
        bool isSnapPositionOnEnter;
        bool isHudActive;
        bool isInputEnabled;
        void (*onEnterHook)(StageIntroDirectorComponent*);
        void (*onUpdateHook)(StageIntroDirectorComponent*, float progress);
    };

    static const std::unordered_map<StageIntroState, PhaseConfig> kPhaseConfigs = {
        {StageIntroState::WarpIn,
         {&StageIntroDirectorComponent::warpInDuration_, 2.5f, 2.5f, true, false, false, false, nullptr, nullptr}},
        {StageIntroState::Arrival,
         {&StageIntroDirectorComponent::arrivalDuration_, 2.5f, 0.8f, false, true, false, false, nullptr,
          [](StageIntroDirectorComponent* self, float progress) { self->ApplyArrivalInertia(progress); }}},
        {StageIntroState::Bootup,
         {&StageIntroDirectorComponent::bootupDuration_, 0.8f, 0.8f, false, true, true, false,
          [](StageIntroDirectorComponent* self) { self->TriggerFCSBootupSequence(); }, nullptr}},
        {StageIntroState::Active, {nullptr, 0.8f, 0.8f, false, false, true, true, nullptr, nullptr}},
    };

    auto it = kPhaseConfigs.find(introState_);
    if (it == kPhaseConfigs.end()) {
        return;
    }
    const auto& config = it->second;

    // インスペクターのプロパティから所要時間を取得（WarpInフェーズは最低1.40秒を保証して確実に水面膜の揺らぎと突破を魅せる）
    float duration = 1.0f;
    if (config.durationMember) {
        float rawDuration = this->*(config.durationMember);
        if (config.durationMember == &StageIntroDirectorComponent::warpInDuration_) {
            rawDuration = (std::max)(1.40f, rawDuration);
        }
        duration = (std::max)(0.001f, rawDuration);
    }

    float progress = std::clamp(stateTimer_ / duration, 0.0f, 1.0f);

    // スラスター炎の補間
    float thrusterScale = std::lerp(config.initialThrusterScale, config.targetThrusterScale, progress);
    SetShipThrusterScale(thrusterScale, true);

    // 位置補間（WarpInフェーズの3次急減速）および視覚ワーピング連動
    if (config.isPositionInterpolated) {
        // カメラ（Z=-6.5m）の視錐台手前ドアップから前方の重力水面膜（Z=0.0m）へ向けて確実に突進させる安全クランプ
        float effectiveOffsetZ = std::clamp(startOffsetZ_, -5.5f, -3.0f);
        float t = 1.0f - std::pow(1.0f - progress, 3.0f);
        Irufemi::Vector3 curPos = initialShipLocalPos_;
        curPos.z = initialShipLocalPos_.z + effectiveOffsetZ * (1.0f - t);
        if (auto ship = shipObj_.lock()) {
            if (auto transform = ship->GetTransform()) {
                transform->SetPosition(curPos);
            }
        }

        // カメラFOVワーピングとPreUIラジアルブラーの動的駆動
        UpdateWarpVisuals(progress);

        // 空間配置された重力水面膜のアニメーション（前進中の静的波打ち -> 突入時の爆発的破裂拡大）
        UpdateMembraneVisuals(progress);

        // レール進行速度の動的ブースト連動（2.2倍から1.0倍へ指数関数的に滑らかに減速）
        if (auto cart = playerCartObj_.lock()) {
            if (auto follower = cart->GetComponent<SplineFollowerComponent>()) {
                float mult = std::lerp(warpInRailSpeedBoost_, 1.0f, t);
                follower->SetSpeedMultiplier(mult);
            }
        }

        // 接近中の自機前進に伴う重力場押し分け気流（Fluid Downwash）の微量注入
        if (progress < breachProgressThreshold_) {
            if (engine) {
                if (auto fluid = engine->GetFluidSimulation()) {
                    fluid->AddForce({0.50f, 0.50f}, {0.0f, 0.8f}, 0.15f);
                }
            }
        }

        // 自機が重力エネルギー界面を貫通した瞬間に突破インパルスを発火！（進行度または自機物理Z到達で完全同期）
        bool isShipBreachingMembrane = (curPos.z >= membraneOffsetZ_);
        if ((progress >= breachProgressThreshold_ || isShipBreachingMembrane) && !hasTriggeredBreach_) {
            TriggerBreachImpulse();
        }
    }

    // 固有演出フック
    if (config.onUpdateHook) {
        config.onUpdateHook(this, progress);
    }

    // 次のフェーズへ自動遷移
    if (progress >= 1.0f) {
        if (introState_ == StageIntroState::WarpIn) {
            CleanupWarpVisuals();    // 到着時に確実に等倍復帰
            DestroyMembraneObject(); // 到着時に物理膜オブジェクトを完全に解放
            // レール速度を確実に通常速度へリセット
            if (auto cart = playerCartObj_.lock()) {
                if (auto follower = cart->GetComponent<SplineFollowerComponent>()) {
                    follower->SetSpeedMultiplier(1.0f);
                }
            }
            SetIntroState(StageIntroState::Arrival);
        } else if (introState_ == StageIntroState::Arrival) {
            SetIntroState(StageIntroState::Bootup);
        } else if (introState_ == StageIntroState::Bootup) {
            SetIntroState(StageIntroState::Active);
        }
    }
}

void StageIntroDirectorComponent::ApplyArrivalInertia(float progress) {
    if (auto ship = shipObj_.lock()) {
        if (auto transform = ship->GetTransform()) {
            // 急制動に伴う物理整流挙動（減衰振動方程式）
            // θ_pitch(t) = θ_max * exp(-λ * t) * cos(ω * t)
            // θ_max = -0.07 rad (約 -4.0度、急制動時の前傾ノーズダイブ)
            // λ = 7.0 (減衰係数)
            // ω = 22.0 rad/s (固有角振動数)
            // t = progress * arrivalDuration_ (秒)
            float t = progress * arrivalDuration_;
            const float thetaMax = -0.07f;
            const float lambda = 7.0f;
            const float omega = 22.0f;
            float pitchOffset = thetaMax * std::exp(-lambda * t) * std::cos(omega * t);

            auto rot = initialShipLocalRot_;
            rot.x += pitchOffset;
            transform->SetRotation(rot);
        }
    }
}

void StageIntroDirectorComponent::TriggerBreachImpulse() {
    if (hasTriggeredBreach_) {
        return;
    }
    hasTriggeredBreach_ = true;
    breachTimer_ = 0.001f; // プラズマ閃光パルス発火

    // 1. 重力膜を突き破る空間インパルス（正面大激震）
    if (auto camObj = cameraObj_.lock()) {
        auto shake = camObj->GetComponent<CameraShakeComponent>();
        if (!shake) {
            auto newShake = camObj->AddComponent<CameraShakeComponent>();
            shake = newShake.get();
        }
        if (shake) {
            shake->PlayShakeSeconds(breachShakeAmplitude_, 0.45f, 28.0f);
        }
    }

    // 2. 音響多重化（重低音空間音 ＋ 重力界面破断音の完全同期）
    if (auto engine = GetEngine()) {
        if (auto am = engine->GetAudioManager()) {
            if (!warpSePath_.empty()) {
                am->PlayByFile(warpSePath_, 0.80f, 1.15f, AudioCategory::SE);
            }
            if (!breachSePath_.empty()) {
                am->PlayByFile(breachSePath_, 0.85f, 1.0f, AudioCategory::SE);
            }
        }
    }

    // 3. GPGPU流体場（FluidSimulation2D）へ超強度重力渦＋8方向放射状外力＋シアン重力プラズマを一斉バースト注入！
    if (auto engine = GetEngine()) {
        if (auto fluid = engine->GetFluidSimulation()) {
            const Irufemi::Vector2 centerUV{0.50f, 0.50f};
            // A: 重力特異点スワール（事象の地平線突破の激しい渦動）
            fluid->AddGravityVortex(centerUV, 45.0f, 65.0f, 0.55f);

            // B: 8方向放射状インパルス（空間破断による全方位重力衝撃波）
            constexpr int kRadialDirections = 8;
            constexpr float kRadialForceMagnitude = 18.0f;
            constexpr float kRadialRadius = 0.22f;
            for (int i = 0; i < kRadialDirections; ++i) {
                float angle = static_cast<float>(i) * (2.0f * 3.14159265f / kRadialDirections);
                Irufemi::Vector2 dir{std::cos(angle), std::sin(angle)};
                Irufemi::Vector2 injectPos = centerUV + dir * 0.05f;
                fluid->AddForce(injectPos, dir * kRadialForceMagnitude, kRadialRadius);
            }

            // C: 高密度シアン重力プラズマ染料注入（星雲・特異点エネルギーの爆発的飛散）
            fluid->AddDensity(centerUV, Irufemi::Vector4{0.15f, 0.90f, 1.0f, 1.0f} * 2.0f, 0.35f);
        }
    }

    // 4. 重力波ディストーション（空間波紋＋色収差）を一斉発火！（超広域・高強度・極太ショックウェーブ）
    if (auto engine = GetEngine()) {
        if (auto ppm = engine->GetPostProcessManager()) {
            ppm->TriggerGravityDistortion(Irufemi::Vector2(0.5f, 0.5f), 0.28f, 0.65f, 1.6f, 0.045f, 0.22f);
        }
    }
}

void StageIntroDirectorComponent::TriggerFCSBootupSequence() {
    // 1. 重力FCSオンライン起動チャイム再生（プロパティ参照）
    if (auto engine = GetEngine()) {
        if (auto am = engine->GetAudioManager()) {
            if (!fcsBootSePath_.empty()) {
                am->PlayByFile(fcsBootSePath_, 0.85f, 1.0f, AudioCategory::SE);
            }
        }
    }

    // 2. 自機のピッチ角を基準姿勢へ安定化
    if (auto ship = shipObj_.lock()) {
        if (auto transform = ship->GetTransform()) {
            transform->SetRotation(initialShipLocalRot_);
        }
    }

    // 3. レティクルHUDのFCSブートアップスプリング展開アニメーションを発火
    if (auto scene = GetScene()) {
        if (auto reticleObj = scene->FindGameObject("Reticle")) {
            if (auto reticleUI = reticleObj->GetComponent<ReticleUIComponent>()) {
                reticleUI->TriggerBootupAnimation();
            }
        }
    }
}

void StageIntroDirectorComponent::SkipIntroSequence() {
    if (introState_ == StageIntroState::Active) {
        return;
    }

    // 視覚ワーピング（FOV/ラジアルブラー）を確実に基準状態へリセット
    CleanupWarpVisuals();
    DestroyMembraneObject();

    // レール距離を厳密に 0.0m にスナップし、通常速度へ復帰
    if (auto cart = playerCartObj_.lock()) {
        if (auto follower = cart->GetComponent<SplineFollowerComponent>()) {
            follower->SnapToDistance(0.0f);
            follower->SetSpeedMultiplier(1.0f);
        }
    }

    // 自機座標と回転を定位置・基準姿勢へ即座にスナップ
    if (auto ship = shipObj_.lock()) {
        if (auto transform = ship->GetTransform()) {
            transform->SetPosition(initialShipLocalPos_);
            transform->SetRotation(initialShipLocalRot_);
        }
    }

    // スラスター炎を通常戦闘スケールへスナップ
    SetShipThrusterScale(0.8f, true);

    // 通常戦闘ステートへ即時移行 (HUD点灯・操作権委譲)
    SetIntroState(StageIntroState::Active);
}

void StageIntroDirectorComponent::SetHUDActive(bool active) {
    auto scene = GetScene();
    if (!scene) {
        return;
    }

    if (auto reticle = scene->FindGameObject("Reticle")) {
        reticle->SetActive(active);
    }
    if (auto lockon = scene->FindGameObject("LockonMarkerUI")) {
        lockon->SetActive(active);
    }
}

void StageIntroDirectorComponent::SetShipThrusterActive(bool active) {
    if (auto ship = shipObj_.lock()) {
        if (auto thrusterComp = ship->GetComponent<PlayerThrusterVisualizerComponent>()) {
            thrusterComp->SetThrusterActive(active);
        }
    }
}

void StageIntroDirectorComponent::SetShipThrusterScale(float targetScaleZ, bool snap) {
    if (auto ship = shipObj_.lock()) {
        if (auto thrusterComp = ship->GetComponent<PlayerThrusterVisualizerComponent>()) {
            if (snap) {
                thrusterComp->SetCurrentScaleZ(targetScaleZ);
            } else {
                thrusterComp->SetTargetScaleZ(targetScaleZ);
            }
        }
    }
}

CameraComponent* StageIntroDirectorComponent::GetCameraComponent() const {
    if (auto cam = cameraObj_.lock()) {
        return cam->GetComponent<CameraComponent>();
    }
    return nullptr;
}

void StageIntroDirectorComponent::UpdateWarpVisuals(float progress) {
    // 突入初動(progress < breachProgressThreshold_)は重力水面膜の流体揺らぎ・最大ブラーを維持し、
    // 自機が膜を突き破った瞬間からS字SmoothStep減衰（界面が外側へ弾けながら滑らかに通常空間へ溶け込む）
    float curve = 1.0f;
    float blurCurve = 1.0f;
    if (progress >= breachProgressThreshold_) {
        float t = std::clamp(
            (progress - breachProgressThreshold_) / (std::max)(0.001f, 1.0f - breachProgressThreshold_), 0.0f, 1.0f);
        float easeOut = 1.0f - t;
        curve = easeOut * easeOut * (3.0f - 2.0f * easeOut); // C2連続 SmoothStep
        // ラジアルブラーは突破の瞬間に急減衰させ、重力波紋のシャープな色収差を際立たせる
        blurCurve = easeOut * easeOut * easeOut * easeOut;
    }

    // 1. カメラFOV動的ブレンド (最大広角から等倍へ滑らかに復帰)
    if (auto cam = GetCameraComponent()) {
        float fovBoostRad = curve * (warpInFovBoostDeg_ * 3.14159265f / 180.0f);
        cam->SetFovAngleY(baseCameraFov_ + fovBoostRad);
    }

    // 2. ポストプロセス重層駆動
    if (auto engine = GetEngine()) {
        if (auto ppm = engine->GetPostProcessManager()) {
            float deltaTime = engine->GetDeltaTime();
            if (deltaTime <= 0.0f) {
                deltaTime = 1.0f / 60.0f;
            }

            // ラジアルブラー ＆ 色収差（超高速突入の速度線）
            float blurWidth = maxWarpBlurWidth_ * blurCurve;
            if (blurWidth > 0.0005f) {
                if (!isRadialBlurActive_) {
                    ppm->AddActiveMode(PostProcessMode::RadialBlur, PostProcessManager::Layer::PreUI);
                    isRadialBlurActive_ = true;
                }
                auto& rb = ppm->GetRadialBlurParams();
                rb.center = {0.5f, 0.5f};
                rb.blurWidth = blurWidth;
                rb.numSamples = 16;

                if (!isChromaticActive_) {
                    ppm->AddActiveMode(PostProcessMode::ChromaticAberration, PostProcessManager::Layer::PreUI);
                    isChromaticActive_ = true;
                }
                auto& ca = ppm->GetChromaticAberrationParams();
                ca.intensity = 0.045f * curve;
            } else {
                if (isRadialBlurActive_) {
                    ppm->RemoveActiveMode(PostProcessMode::RadialBlur);
                    ppm->GetRadialBlurParams().blurWidth = 0.0f;
                    isRadialBlurActive_ = false;
                }
                if (isChromaticActive_) {
                    ppm->RemoveActiveMode(PostProcessMode::ChromaticAberration);
                    ppm->GetChromaticAberrationParams().intensity = 0.0f;
                    isChromaticActive_ = false;
                }
            }

            // 3. 特異点暗黒空間からの接続 ＆ 突破破断閃光（Fade / Flash）
            Irufemi::Vector4 fadeColor{0.0f, 0.0f, 0.0f, 1.0f};
            float fadeIntensity = 0.0f;

            if (breachTimer_ > 0.0f) {
                // 突破瞬間: まばゆい重力破断閃光（シアンホワイト閃光がパァン！と弾けて滑らかに減衰）
                breachTimer_ += deltaTime;
                float flash = breachFlashIntensity_ * std::exp(-breachTimer_ * 5.5f);
                if (flash > 0.005f) {
                    fadeColor = {0.65f, 0.95f, 1.0f, 1.0f}; // シアン・ホワイト閃光
                    fadeIntensity = flash;
                } else {
                    breachTimer_ = -1.0f; // 閃光終了
                }
            } else if (progress < breachProgressThreshold_) {
                // 突入初動: 特異点（ブラックホール）の闇の余韻から前方の重力界面へ突進
                float approach = progress / (std::max)(0.001f, breachProgressThreshold_);
                fadeColor = {0.0f, 0.0f, 0.0f, 1.0f};      // 特異点暗黒
                fadeIntensity = 0.45f * (1.0f - approach); // 0.45 -> 0.0 へ滑らかに透けていく
            }

            if (fadeIntensity > 0.01f) {
                if (!isFadeActive_) {
                    ppm->AddActiveMode(PostProcessMode::Fade, PostProcessManager::Layer::PreUI);
                    isFadeActive_ = true;
                }
                auto& fp = ppm->GetFadeParams();
                fp.color = fadeColor;
                fp.intensity = fadeIntensity;
            } else if (isFadeActive_) {
                ppm->RemoveActiveMode(PostProcessMode::Fade);
                ppm->GetFadeParams().intensity = 0.0f;
                isFadeActive_ = false;
            }
        }
    }
}

void StageIntroDirectorComponent::CleanupWarpVisuals() {
    // カメラFOVを等倍に安全復帰
    if (auto cam = GetCameraComponent()) {
        cam->SetFovAngleY(baseCameraFov_);
    }

    // ラジアルブラー・色収差・フェード閃光の確実な解除
    if (auto engine = GetEngine()) {
        if (auto ppm = engine->GetPostProcessManager()) {
            if (isRadialBlurActive_) {
                ppm->RemoveActiveMode(PostProcessMode::RadialBlur);
                ppm->GetRadialBlurParams().blurWidth = 0.0f;
                isRadialBlurActive_ = false;
            }
            if (isChromaticActive_) {
                ppm->RemoveActiveMode(PostProcessMode::ChromaticAberration);
                ppm->GetChromaticAberrationParams().intensity = 0.0f;
                isChromaticActive_ = false;
            }
            if (isFadeActive_) {
                ppm->RemoveActiveMode(PostProcessMode::Fade);
                ppm->GetFadeParams().intensity = 0.0f;
                isFadeActive_ = false;
            }
        }
    }
}
