#include "Combat/EnemyBeamComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Camera/CameraShakeComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "Physics/CollisionManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/System/Core/BaseModel.h"
#include "Core/Math/MathFunction.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "Renderer/Camera/CameraManager.h"
#include <cmath>
#include <algorithm>
#include <Windows.h>
#include <string>

#undef min
#undef max

EnemyBeamComponent::~EnemyBeamComponent() = default;

void EnemyBeamComponent::OnRegisterProperties() {
    RegisterProperty("Beam Length", &beamLength_);
    RegisterProperty("Beam Max Radius", &beamMaxRadius_);
    RegisterProperty("Charge Duration", &chargeDuration_);
    RegisterProperty("Fire Duration", &fireDuration_);

    RegisterProperty("Lock Lead Time", &lockLeadTime_);
    RegisterProperty("Beam Damage", &beamDamage_);
    RegisterProperty("Hit Check Margin", &hitCheckRadiusMargin_);
    RegisterProperty("Telegraph Color", &telegraphColor_);

    RegisterProperty("Charge Sphere Color", &chargeColor_);
    RegisterProperty("Beam Color", &beamColor_);
    RegisterProperty("Beam Core Color", &beamCoreColor_);
    RegisterProperty("Beam Intensity", &beamIntensity_);
    RegisterProperty("Beam Core Intensity", &beamCoreIntensity_);
    RegisterProperty("Beam Speed", &beamSpeed_);

    RegisterProperty("Aura Color", &auraColor_);
    RegisterProperty("Aura Core Color", &auraCoreColor_);
    RegisterProperty("Aura Intensity", &auraIntensity_);
    RegisterProperty("Aura Speed", &auraSpeed_);
}

void EnemyBeamComponent::Initialize() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    // --- チャージ球の初期化 ---
    chargeSphere_ = std::make_unique<Primitive3DObject>();
    chargeSphere_->Initialize(Irufemi::PrimitiveType::Sphere);
    chargeSphere_->SetColor(chargeColor_);
    chargeSphere_->SetCullingEnabled(false);
    chargeSphere_->SetCustomPSO("EnergyCore", Irufemi::BlendMode::kBlendModePremultiplied,
                                PSOManager::DepthWrite::Disable, PSOManager::CullMode::Back);
    chargeSphere_->SetIsTransparent(true);

    // --- AOE予兆危険円柱の初期化 ---
    telegraphCylinder_ = std::make_unique<Primitive3DObject>();
    telegraphCylinder_->Initialize(Irufemi::PrimitiveType::Cylinder);
    telegraphCylinder_->SetColor(telegraphColor_);
    telegraphCylinder_->SetCastShadows(false);
    telegraphCylinder_->SetCullingEnabled(false);
    telegraphCylinder_->SetIsTransparent(true);

    // --- ビーム本体の初期化 ---
    attackCylinder_ = std::make_unique<Primitive3DObject>();
    attackCylinder_->Initialize(Irufemi::PrimitiveType::Cylinder);
    attackCylinder_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    attackCylinder_->SetCastShadows(false);
    attackCylinder_->SetCullingEnabled(false);
    attackCylinder_->SetIsTransparent(true);

    attackCylinderOuter_ = std::make_unique<Primitive3DObject>();
    attackCylinderOuter_->Initialize(Irufemi::PrimitiveType::Cylinder);
    attackCylinderOuter_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
    attackCylinderOuter_->SetCastShadows(false);
    attackCylinderOuter_->SetCullingEnabled(false);
    attackCylinderOuter_->SetIsTransparent(true);

    // --- シェーダーパラメータ定数バッファの初期化 ---
    auto dxCommon = engine->GetDirectXCommon();
    if (dxCommon) {
        // AOEパラメータ用定数バッファ
        aoeParamsBuffer_.Initialize(dxCommon);
        aoeParamsData_ = AOEParams();
        aoeParamsData_.shapeType = 2; // Cylinder用
        aoeParamsData_.warningRatio = 0.0f;
        aoeParamsBuffer_.UpdateAll(aoeParamsData_);

        // 内側（極太レーザーコア）用パラメータ
        beamParamsBuffer_.Initialize(dxCommon);
        beamParamsData_ = LightningParams();
        beamParamsData_.color = {0.8f, 0.0f, 1.0f, 1.0f};     // ネオンパープルオーラ
        beamParamsData_.coreColor = {0.0f, 1.0f, 1.0f, 1.0f}; // 高エネルギーのシアンコア
        beamParamsData_.intensity = 6.0f;
        beamParamsData_.noiseThreshold = 0.35f;
        beamParamsData_.coreIntensity = 40.0f;
        beamParamsData_.coreThreshold = 0.85f;
        beamParamsData_.coreScale = 2.5f;
        beamParamsData_.speed = 3.0f;
        beamParamsData_.noiseScale = 1.2f;
        beamParamsData_.spinSpeed = 4.0f;
        beamParamsData_.twistScale = 4.0f;
        beamParamsBuffer_.UpdateAll(beamParamsData_);

        // 外側（バチバチ電撃・オーラ）用パラメータ
        auraParamsBuffer_.Initialize(dxCommon);
        auraParamsData_ = LightningParams();
        auraParamsData_.color = {0.1f, 0.0f, 0.2f, 1.0f};     // ダークパープル/黒っぽいオーラ
        auraParamsData_.coreColor = {0.8f, 0.0f, 1.0f, 1.0f}; // コアはネオンパープル
        auraParamsData_.intensity = 12.0f;
        auraParamsData_.noiseThreshold = 0.4f;
        auraParamsData_.coreIntensity = 20.0f;
        auraParamsData_.coreThreshold = 0.55f;
        auraParamsData_.coreScale = 2.5f;
        auraParamsData_.speed = 0.8f;
        auraParamsData_.noiseScale = 1.0f;
        auraParamsData_.spinSpeed = 8.0f;
        auraParamsData_.twistScale = 6.0f;
        auraParamsBuffer_.UpdateAll(auraParamsData_);
    }

    state_ = State::IDLE;
    isAimLocked_ = false;
}

void EnemyBeamComponent::EnsureResources() {
    if (!chargeSphere_ || !attackCylinder_ || !attackCylinderOuter_ || !telegraphCylinder_) {
        Initialize();
    }
}

void EnemyBeamComponent::Fire(const Irufemi::Vector3& startPos, const Irufemi::Vector3& targetPos) {
    EnsureResources();

    state_ = State::CHARGING;
    stateTimer_ = 0.0f;
    startPos_ = startPos;
    isAimLocked_ = false;
    hasHitCurrentBeam_ = false;

    // ボス本体に対する発射口のローカルオフセットを計算・保持（移動中の前進に追従させるため）
    if (gameObject_ && gameObject_->GetTransform()) {
        Irufemi::Matrix4x4 invWorld = Irufemi::Math::Inverse(gameObject_->GetTransform()->GetWorldMatrix());
        muzzleLocalOffset_ = Irufemi::Math::Transform(startPos, invWorld);
    } else {
        muzzleLocalOffset_ = {0.0f, 0.0f, 0.0f};
    }

    // 発射方向の初期計算
    Irufemi::Vector3 diff = targetPos - startPos_;
    float diffDistSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    if (diffDistSq > 1e-6f) {
        direction_ = Irufemi::Math::Normalize(diff);
    } else {
        direction_ = {0.0f, 0.0f, 1.0f};
    }
}

Irufemi::Vector3 EnemyBeamComponent::GetCurrentMuzzlePosition() const {
    if (gameObject_) {
        if (auto transform = gameObject_->GetTransform()) {
            return Irufemi::Math::Transform(muzzleLocalOffset_, transform->GetWorldMatrix());
        }
    }
    return startPos_;
}

GameObject* EnemyBeamComponent::GetPlayerObject() {
    auto cached = playerObj_.lock();
    if (cached && cached->GetIsActive()) {
        return cached.get();
    }
    if (gameObject_ && gameObject_->GetScene()) {
        auto player = gameObject_->GetScene()->FindGameObject("Player");
        if (player) {
            playerObj_ = player;
            return player.get();
        }
    }
    return nullptr;
}

void EnemyBeamComponent::CheckBeamCollision() {
    if (hasHitCurrentBeam_) {
        return; // 今回の照射で既にヒット済みの場合は多重ダメージ＆多重シェイクを抑止
    }

    auto player = GetPlayerObject();
    if (!player) {
        return;
    }
    auto health = player->GetComponent<PlayerHealthComponent>();
    if (!health || health->IsDead() || health->IsInvincible()) {
        return;
    }
    auto playerTransform = player->GetComponent<TransformComponent>();
    if (!playerTransform) {
        return;
    }

    Irufemi::Vector3 playerPos = playerTransform->GetWorldPosition();
    Irufemi::Vector3 a = startPos_;
    Irufemi::Vector3 ab = direction_ * beamLength_;
    float abLenSq = beamLength_ * beamLength_;
    if (abLenSq <= 1e-4f) {
        return;
    }

    // 線分と自機ワールド座標との最短距離計算 (Point to Segment)
    Irufemi::Vector3 ap = playerPos - a;
    float dot = ap.x * ab.x + ap.y * ab.y + ap.z * ab.z;
    float t = std::clamp(dot / abLenSq, 0.0f, 1.0f);
    Irufemi::Vector3 closest = a + ab * t;

    Irufemi::Vector3 diff = playerPos - closest;
    float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    float hitRadius = beamMaxRadius_ + hitCheckRadiusMargin_;

    if (distSq <= hitRadius * hitRadius) {
        // 【AAA基準: 環境遮蔽（Line-of-Sight）チェック】
        // 発射口から自機までの間に環境物（Environment）の壁や柱があればビームが遮断される
        if (auto engine = GetEngine()) {
            if (auto cm = engine->GetCollisionManager()) {
                uint32_t envMask = cm->GetLayerMask("Environment");
                Irufemi::Ray ray;
                ray.origin = a;
                ray.diff = playerPos - a;
                float distToPlayer = ray.diff.Length();
                if (distToPlayer > 0.001f) {
                    RaycastHit hitInfo;
                    if (cm->Raycast(ray, hitInfo, distToPlayer, envMask, gameObject_)) {
                        // 壁に遮られているためダメージ適用をスキップ
                        return;
                    }
                }
            }
        }

        hasHitCurrentBeam_ = true;
        health->TakeDamage(beamDamage_);

        // 特大カメラシェイクを発火（キャッシュを利用してミューテックス競合を回避）
        auto cam = mainCameraObj_.lock();
        if (!cam && gameObject_ && gameObject_->GetScene()) {
            cam = gameObject_->GetScene()->FindGameObject("MainCamera");
            mainCameraObj_ = cam;
        }
        if (cam) {
            if (auto shake = cam->GetComponent<CameraShakeComponent>()) {
                shake->PlayShake(2.0f, 25);
            }
        }
    }
}

void EnemyBeamComponent::UpdateParameters() {
    if (chargeSphere_) {
        chargeSphere_->SetColor(chargeColor_);
    }

    beamParamsData_.color = beamColor_;
    beamParamsData_.coreColor = beamCoreColor_;
    beamParamsData_.intensity = beamIntensity_;
    beamParamsData_.coreIntensity = beamCoreIntensity_;
    beamParamsData_.speed = beamSpeed_;

    auraParamsData_.color = auraColor_;
    auraParamsData_.coreColor = auraCoreColor_;
    auraParamsData_.intensity = auraIntensity_;
    auraParamsData_.speed = auraSpeed_;

    auto engine = GetEngine();
    if (engine && engine->GetDirectXCommon()) {
        uint32_t frameIndex = engine->GetDirectXCommon()->GetCurrentBackBufferIndex();
        beamParamsBuffer_.Update(beamParamsData_, frameIndex);
        auraParamsBuffer_.Update(auraParamsData_, frameIndex);
    }
}

void EnemyBeamComponent::UpdateCharging(float deltaTime) {
    auto engine = GetEngine();

    // ボスの前進・旋回に合わせて発射口ワールド座標をリアルタイム同期
    startPos_ = GetCurrentMuzzlePosition();

    // --- 溜め動作のアニメーション ---
    float t = std::min(stateTimer_ / chargeDuration_, 1.0f);

    // 射線追従とロック判定
    float timeLeft = chargeDuration_ - stateTimer_;
    auto player = GetPlayerObject();
    if (player && player->GetComponent<TransformComponent>()) {
        if (timeLeft > lockLeadTime_) {
            // 発射前（追尾フェーズ）: 自機座標を滑らかに追尾
            isAimLocked_ = false;
            Irufemi::Vector3 playerPos = player->GetComponent<TransformComponent>()->GetWorldPosition();
            Irufemi::Vector3 diff = playerPos - startPos_;
            if (diff.x * diff.x + diff.y * diff.y + diff.z * diff.z > 1e-4f) {
                direction_ = Irufemi::Math::Normalize(diff);
            }
        } else {
            // 発射直前（射線ロックフェーズ）: 追尾停止、射線を空間に固定
            isAimLocked_ = true;
        }
    }

    // AOE パラメータの更新 (warningRatio)
    aoeParamsData_.shapeType = 2; // Cylinder
    aoeParamsData_.warningRatio = t;
    if (engine && engine->GetDirectXCommon()) {
        uint32_t frameIndex = engine->GetDirectXCommon()->GetCurrentBackBufferIndex();
        aoeParamsBuffer_.Update(aoeParamsData_, frameIndex);
    }

    // 予兆シリンダーの姿勢・サイズ更新
    if (telegraphCylinder_) {
        float currentLength = beamLength_;
        Irufemi::Matrix4x4 rotMat = Irufemi::Math::DirectionToDirection({0.0f, 1.0f, 0.0f}, direction_);
        Irufemi::Vector3 rotate = Irufemi::Math::ExtractEulerFromMatrix(rotMat);
        Irufemi::Vector3 center = startPos_ + direction_ * (currentLength * 0.5f);

        telegraphCylinder_->SetPosition(center);
        telegraphCylinder_->SetRotate(rotate);
        telegraphCylinder_->SetScale({beamMaxRadius_, currentLength, beamMaxRadius_});

        // ロック中は激しく明滅させて危険度を最大化
        if (isAimLocked_) {
            float pulse = std::sin(stateTimer_ * 40.0f);
            Irufemi::Vector4 c =
                (pulse > 0.0f) ? Irufemi::Vector4{1.0f, 0.2f, 0.2f, 0.9f} : Irufemi::Vector4{1.0f, 1.0f, 1.0f, 0.95f};
            telegraphCylinder_->SetColor(c);
        } else {
            telegraphCylinder_->SetColor(telegraphColor_);
        }
        telegraphCylinder_->Update();
    }

    // イーズイン (急激に収縮してエネルギーが凝縮される表現) + 明滅
    float easeT = t * t * t;
    float baseScale = std::lerp(0.1f, 4.0f, easeT);
    float pulse = 1.0f + 0.3f * std::sin(t * 50.0f);
    float currentScale = baseScale * pulse;

    if (chargeSphere_) {
        Irufemi::Transform tForm;
        tForm.scale = {currentScale, currentScale, currentScale};

        Irufemi::Vector3 cameraPos = startPos_;
        if (engine && engine->GetCameraManager() && engine->GetCameraManager()->GetActiveCamera()) {
            cameraPos = engine->GetCameraManager()->GetActiveCamera()->GetTranslate();
        }

        Irufemi::Vector3 toCamera = cameraPos - startPos_;
        float distSq = toCamera.x * toCamera.x + toCamera.y * toCamera.y + toCamera.z * toCamera.z;
        if (distSq > 1e-4f) {
            Irufemi::Vector3 toCameraDir = Irufemi::Math::Normalize(toCamera);
            tForm.translate = startPos_ + toCameraDir * (currentScale * 0.5f);
            toCamera = cameraPos - tForm.translate;
            float distXZ = std::sqrt(toCamera.x * toCamera.x + toCamera.z * toCamera.z);
            tForm.rotate.y = std::atan2(-toCamera.x, -toCamera.z);
            tForm.rotate.x = std::atan2(toCamera.y, distXZ);
            tForm.rotate.z = 0.0f;
        } else {
            tForm.translate = startPos_;
            tForm.rotate = {0.0f, 0.0f, 0.0f};
        }

        chargeSphere_->GetTransform().transform = tForm;
        chargeSphere_->GetTransform().isDirty = true;
        chargeSphere_->Update();
    }

    // 溜め完了で発射ステートへ
    if (stateTimer_ >= chargeDuration_) {
        state_ = State::FIRING;
        stateTimer_ = 0.0f;

        if (chargeSphere_) {
            chargeSphere_->GetTransform().transform.scale = {0.0f, 0.0f, 0.0f};
            chargeSphere_->GetTransform().isDirty = true;
            chargeSphere_->Update();
        }
    }
}

void EnemyBeamComponent::UpdateFiring(float deltaTime) {
    // ボスの前進・旋回に合わせて発射口ワールド座標をリアルタイム同期
    startPos_ = GetCurrentMuzzlePosition();

    // --- ビーム発射動作のアニメーション ---
    float t = std::min(stateTimer_ / fireDuration_, 1.0f);

    float easeThickness = 1.0f - (t * t * t);
    float currentThickness = beamMaxRadius_ * easeThickness;
    float currentLength = beamLength_;

    Irufemi::Matrix4x4 rotMat = Irufemi::Math::DirectionToDirection({0.0f, 1.0f, 0.0f}, direction_);
    Irufemi::Vector3 rotate = Irufemi::Math::ExtractEulerFromMatrix(rotMat);
    Irufemi::Vector3 center = startPos_ + direction_ * (currentLength * 0.5f);

    if (attackCylinder_) {
        attackCylinder_->SetPosition(center);
        attackCylinder_->SetRotate(rotate);
        attackCylinder_->SetScale({currentThickness * 0.5f, currentLength, currentThickness * 0.5f});
        attackCylinder_->Update();
    }

    if (attackCylinderOuter_) {
        attackCylinderOuter_->SetPosition(center);
        attackCylinderOuter_->SetRotate(rotate);
        attackCylinderOuter_->SetScale({currentThickness, currentLength, currentThickness});
        attackCylinderOuter_->Update();
    }

    // 自機への当たり判定とダメージ処理
    CheckBeamCollision();

    // 終了判定
    if (stateTimer_ >= fireDuration_) {
        state_ = State::IDLE;
    }
}

void EnemyBeamComponent::Update() {
    if (state_ == State::IDLE) {
        return;
    }

    UpdateParameters();

    auto engine = GetEngine();
    float deltaTime = engine ? engine->GetGameDeltaTime() : (1.0f / 60.0f);
    if (deltaTime <= 0.0f) {
        deltaTime = 1.0f / 60.0f;
    }

    stateTimer_ += deltaTime;

    switch (state_) {
    case State::CHARGING:
        UpdateCharging(deltaTime);
        break;
    case State::FIRING:
        UpdateFiring(deltaTime);
        break;
    default:
        break;
    }
}

void EnemyBeamComponent::Draw() {
    EnsureResources();

    auto engine = GetEngine();
    if (!engine || !engine->GetDirectXCommon()) {
        return;
    }

    uint32_t frameIndex = engine->GetDirectXCommon()->GetCurrentBackBufferIndex();

    if (state_ == State::CHARGING) {
        // AOE予兆円柱の描画
        if (telegraphCylinder_) {
            telegraphCylinder_->SetCustomPSO("AOEWarning", Irufemi::BlendMode::kBlendModeAdd,
                                             PSOManager::DepthWrite::Disable, PSOManager::CullMode::None);
            telegraphCylinder_->SetCustomCBVAddress(aoeParamsBuffer_.GetGPUVirtualAddress(frameIndex));
            telegraphCylinder_->Draw();
        }

        // チャージ球の描画
        if (chargeSphere_ && chargeSphere_->GetTransform().transform.scale.x > 0.0f) {
            chargeSphere_->Draw();
        }
    } else if (state_ == State::FIRING) {
        // 外側オーラ (LightningCrawl)
        if (attackCylinderOuter_) {
            attackCylinderOuter_->SetCustomPSO("LightningCrawl", Irufemi::BlendMode::kBlendModeAdd,
                                               PSOManager::DepthWrite::Disable, PSOManager::CullMode::None);
            attackCylinderOuter_->SetCustomCBVAddress(auraParamsBuffer_.GetGPUVirtualAddress(frameIndex));
            attackCylinderOuter_->Draw();
        }

        // 内側コア (EnergyBeam)
        if (attackCylinder_) {
            attackCylinder_->SetCustomPSO("EnergyBeam", Irufemi::BlendMode::kBlendModeAdd,
                                          PSOManager::DepthWrite::Disable, PSOManager::CullMode::None);
            attackCylinder_->SetCustomCBVAddress(beamParamsBuffer_.GetGPUVirtualAddress(frameIndex));
            attackCylinder_->Draw();
        }
    }
}

std::shared_ptr<Component> EnemyBeamComponent::Clone() {
    auto clone = std::make_shared<EnemyBeamComponent>();
    clone->CopyPropertiesFrom(this);
    clone->beamLength_ = this->beamLength_;
    clone->beamMaxRadius_ = this->beamMaxRadius_;
    clone->chargeDuration_ = this->chargeDuration_;
    clone->fireDuration_ = this->fireDuration_;
    clone->lockLeadTime_ = this->lockLeadTime_;
    clone->beamDamage_ = this->beamDamage_;
    clone->hitCheckRadiusMargin_ = this->hitCheckRadiusMargin_;
    clone->telegraphColor_ = this->telegraphColor_;
    clone->chargeColor_ = this->chargeColor_;
    clone->beamColor_ = this->beamColor_;
    clone->beamCoreColor_ = this->beamCoreColor_;
    clone->beamIntensity_ = this->beamIntensity_;
    clone->beamCoreIntensity_ = this->beamCoreIntensity_;
    clone->beamSpeed_ = this->beamSpeed_;
    clone->auraColor_ = this->auraColor_;
    clone->auraCoreColor_ = this->auraCoreColor_;
    clone->auraIntensity_ = this->auraIntensity_;
    clone->auraSpeed_ = this->auraSpeed_;
    return clone;
}
