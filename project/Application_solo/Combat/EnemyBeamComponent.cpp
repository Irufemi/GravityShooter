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

#undef min
#undef max

EnemyBeamComponent::~EnemyBeamComponent() = default;

void EnemyBeamComponent::OnRegisterProperties() {
    RegisterProperty("Beam Length", &config_.beamLength);
    RegisterProperty("Beam Max Radius", &config_.beamMaxRadius);
    RegisterProperty("Charge Duration", &config_.chargeDuration);
    RegisterProperty("Fire Duration", &config_.fireDuration);

    RegisterProperty("Lock Lead Time", &config_.lockLeadTime);
    RegisterProperty("Beam Damage", &config_.beamDamage);
    RegisterProperty("Hit Check Margin", &config_.hitCheckRadiusMargin);
    RegisterProperty("Telegraph Color", &config_.telegraphColor);

    RegisterProperty("Charge Sphere Color", &config_.chargeColor);
    RegisterProperty("Beam Color", &config_.beamColor);
    RegisterProperty("Beam Core Color", &config_.beamCoreColor);
    RegisterProperty("Beam Intensity", &config_.beamIntensity);
    RegisterProperty("Beam Core Intensity", &config_.beamCoreIntensity);
    RegisterProperty("Beam Speed", &config_.beamSpeed);

    RegisterProperty("Aura Color", &config_.auraColor);
    RegisterProperty("Aura Core Color", &config_.auraCoreColor);
    RegisterProperty("Aura Intensity", &config_.auraIntensity);
    RegisterProperty("Aura Speed", &config_.auraSpeed);
}

void EnemyBeamComponent::Initialize() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    // --- チャージ球の初期化 ---
    chargeSphere_ = std::make_unique<Primitive3DObject>();
    chargeSphere_->Initialize(Irufemi::PrimitiveType::Sphere);
    chargeSphere_->SetColor(config_.chargeColor);
    chargeSphere_->SetCullingEnabled(false);
    chargeSphere_->SetCustomPSO("EnergyCore", Irufemi::BlendMode::kBlendModePremultiplied,
                                PSOManager::DepthWrite::Disable, PSOManager::CullMode::Back);
    chargeSphere_->SetIsTransparent(true);

    // --- AOE予兆危険円柱の初期化 ---
    telegraphCylinder_ = std::make_unique<Primitive3DObject>();
    telegraphCylinder_->Initialize(Irufemi::PrimitiveType::Cylinder);
    telegraphCylinder_->SetColor(config_.telegraphColor);
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
        aoeParamsBuffer_.Initialize(dxCommon);
        aoeParamsData_ = AOEParams();
        aoeParamsData_.shapeType = 2; // Cylinder用
        aoeParamsData_.warningRatio = 0.0f;
        aoeParamsBuffer_.UpdateAll(aoeParamsData_);

        beamParamsBuffer_.Initialize(dxCommon);
        beamParamsData_ = LightningParams();
        beamParamsData_.color = config_.beamColor;
        beamParamsData_.coreColor = config_.beamCoreColor;
        beamParamsData_.intensity = config_.beamIntensity;
        beamParamsData_.coreIntensity = config_.beamCoreIntensity;
        beamParamsData_.speed = config_.beamSpeed;
        beamParamsBuffer_.UpdateAll(beamParamsData_);

        auraParamsBuffer_.Initialize(dxCommon);
        auraParamsData_ = LightningParams();
        auraParamsData_.color = config_.auraColor;
        auraParamsData_.coreColor = config_.auraCoreColor;
        auraParamsData_.intensity = config_.auraIntensity;
        auraParamsData_.speed = config_.auraSpeed;
        auraParamsBuffer_.UpdateAll(auraParamsData_);
    }
}

void EnemyBeamComponent::EnsureResources() {
    if (!chargeSphere_) {
        Initialize();
    }
}

void EnemyBeamComponent::ChangeState(std::unique_ptr<IBeamState> newState) {
    if (currentState_) {
        currentState_->OnExit(*this);
    }
    currentState_ = std::move(newState);
    if (currentState_) {
        currentState_->OnEnter(*this);
    }
}

void EnemyBeamComponent::Fire(const Irufemi::Vector3& startPos, const Irufemi::Vector3& targetPos) {
    EnsureResources();

    startPos_ = startPos;
    hasHitCurrentBeam_ = false;

    // ボスのワールド行列の逆行列を掛けて、ローカル発射口オフセットを逆算・キャッシュ
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

    // チャージステートへ遷移
    ChangeState(std::make_unique<BeamChargingState>());
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
    Irufemi::Vector3 ab = direction_ * config_.beamLength;
    float abLenSq = config_.beamLength * config_.beamLength;
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
    float hitRadius = config_.beamMaxRadius + config_.hitCheckRadiusMargin;

    if (distSq <= hitRadius * hitRadius) {
        // 障害物による射線遮蔽（Line-of-Sight）チェック
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
                        return; // 遮蔽
                    }
                }
            }
        }

        hasHitCurrentBeam_ = true;
        health->TakeDamage(config_.beamDamage);

        // 特大カメラシェイクを発火
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
        chargeSphere_->SetColor(config_.chargeColor);
    }

    beamParamsData_.color = config_.beamColor;
    beamParamsData_.coreColor = config_.beamCoreColor;
    beamParamsData_.intensity = config_.beamIntensity;
    beamParamsData_.coreIntensity = config_.beamCoreIntensity;
    beamParamsData_.speed = config_.beamSpeed;

    auraParamsData_.color = config_.auraColor;
    auraParamsData_.coreColor = config_.auraCoreColor;
    auraParamsData_.intensity = config_.auraIntensity;
    auraParamsData_.speed = config_.auraSpeed;

    auto engine = GetEngine();
    if (engine && engine->GetDirectXCommon()) {
        uint32_t frameIndex = engine->GetDirectXCommon()->GetCurrentBackBufferIndex();
        beamParamsBuffer_.Update(beamParamsData_, frameIndex);
        auraParamsBuffer_.Update(auraParamsData_, frameIndex);
    }
}

void EnemyBeamComponent::Update() {
    if (!currentState_) {
        return;
    }

    UpdateParameters();

    auto engine = GetEngine();
    float deltaTime = engine ? engine->GetGameDeltaTime() : (1.0f / 60.0f);
    if (deltaTime <= 0.0f) {
        deltaTime = 1.0f / 60.0f;
    }

    currentState_->OnUpdate(*this, deltaTime);
}

void EnemyBeamComponent::Draw() {
    if (!currentState_) {
        return;
    }

    EnsureResources();

    auto engine = GetEngine();
    if (!engine || !engine->GetDirectXCommon()) {
        return;
    }

    uint32_t frameIndex = engine->GetDirectXCommon()->GetCurrentBackBufferIndex();
    currentState_->OnDraw(*this, frameIndex);
}

std::shared_ptr<Component> EnemyBeamComponent::Clone() {
    auto clone = std::make_shared<EnemyBeamComponent>();
    clone->CopyPropertiesFrom(this);
    clone->config_ = this->config_;
    return clone;
}
