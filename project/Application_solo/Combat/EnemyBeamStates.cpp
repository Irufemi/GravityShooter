#include "Combat/EnemyBeamStates.h"
#include "Combat/EnemyBeamComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Math/MathFunction.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "Renderer/Camera/CameraManager.h"
#include <algorithm>
#include <cmath>

#undef min
#undef max

// ============================================================================
// BeamChargingState
// ============================================================================

void BeamChargingState::OnEnter(EnemyBeamComponent& /*owner*/) {
    stateTimer_ = 0.0f;
    isAimLocked_ = false;
}

void BeamChargingState::OnUpdate(EnemyBeamComponent& owner, float deltaTime) {
    const auto& config = owner.GetConfig();
    auto engine = owner.GetEngine();

    stateTimer_ += deltaTime;

    // ボスの前進・旋回に合わせて発射口ワールド座標をリアルタイム同期
    Irufemi::Vector3 startPos = owner.GetCurrentMuzzlePosition();
    owner.SetStartPos(startPos);

    // --- 溜め動作のアニメーション ---
    float t = (std::min)(stateTimer_ / (std::max)(config.chargeDuration, 0.001f), 1.0f);

    // 射線追従とロック判定
    float timeLeft = config.chargeDuration - stateTimer_;
    auto player = owner.GetPlayerObject();
    if (player && player->GetComponent<TransformComponent>()) {
        if (timeLeft > config.lockLeadTime) {
            // 発射前（追尾フェーズ）: 自機座標を滑らかに追尾
            isAimLocked_ = false;
            Irufemi::Vector3 playerPos = player->GetComponent<TransformComponent>()->GetWorldPosition();
            Irufemi::Vector3 diff = playerPos - startPos;
            if (diff.x * diff.x + diff.y * diff.y + diff.z * diff.z > 1e-4f) {
                owner.SetDirection(Irufemi::Math::Normalize(diff));
            }
        } else {
            // 発射直前（射線ロックフェーズ）: 追尾停止、射線を空間に固定
            isAimLocked_ = true;
        }
    }

    // AOE パラメータの更新 (warningRatio)
    auto& aoeData = owner.GetAOEParamsData();
    aoeData.shapeType = 2; // Cylinder
    aoeData.warningRatio = t;
    if (engine && engine->GetDirectXCommon()) {
        uint32_t frameIndex = engine->GetDirectXCommon()->GetCurrentBackBufferIndex();
        owner.GetAOEParamsBuffer().Update(aoeData, frameIndex);
    }

    // 予兆シリンダーの姿勢・サイズ更新
    if (auto telegraph = owner.GetTelegraphCylinder()) {
        float currentLength = config.beamLength;
        const auto& direction = owner.GetDirection();
        Irufemi::Matrix4x4 rotMat = Irufemi::Math::DirectionToDirection({0.0f, 1.0f, 0.0f}, direction);
        Irufemi::Vector3 rotate = Irufemi::Math::ExtractEulerFromMatrix(rotMat);
        Irufemi::Vector3 center = startPos + direction * (currentLength * 0.5f);

        telegraph->SetPosition(center);
        telegraph->SetRotate(rotate);
        telegraph->SetScale({config.beamMaxRadius, currentLength, config.beamMaxRadius});

        // ロック中は激しく明滅させて危険度を最大化
        if (isAimLocked_) {
            float pulse = std::sin(stateTimer_ * 40.0f);
            Irufemi::Vector4 c =
                (pulse > 0.0f) ? Irufemi::Vector4{1.0f, 0.2f, 0.2f, 0.9f} : Irufemi::Vector4{1.0f, 1.0f, 1.0f, 0.95f};
            telegraph->SetColor(c);
        } else {
            telegraph->SetColor(config.telegraphColor);
        }
        telegraph->Update();
    }

    // チャージ球のアニメーション（収縮エネルギー凝縮表現 + 明滅）
    float easeT = t * t * t;
    float baseScale = std::lerp(0.1f, 4.0f, easeT);
    float pulse = 1.0f + 0.3f * std::sin(t * 50.0f);
    float currentScale = baseScale * pulse;

    if (auto chargeSphere = owner.GetChargeSphere()) {
        Irufemi::Transform tForm;
        tForm.scale = {currentScale, currentScale, currentScale};

        Irufemi::Vector3 cameraPos = startPos;
        if (engine && engine->GetCameraManager() && engine->GetCameraManager()->GetActiveCamera()) {
            cameraPos = engine->GetCameraManager()->GetActiveCamera()->GetTranslate();
        }

        Irufemi::Vector3 toCamera = cameraPos - startPos;
        float distSq = toCamera.x * toCamera.x + toCamera.y * toCamera.y + toCamera.z * toCamera.z;
        if (distSq > 1e-4f) {
            Irufemi::Vector3 toCameraDir = Irufemi::Math::Normalize(toCamera);
            tForm.translate = startPos + toCameraDir * (currentScale * 0.5f);
            toCamera = cameraPos - tForm.translate;
            float distXZ = std::sqrt(toCamera.x * toCamera.x + toCamera.z * toCamera.z);
            tForm.rotate.y = std::atan2(-toCamera.x, -toCamera.z);
            tForm.rotate.x = std::atan2(toCamera.y, distXZ);
            tForm.rotate.z = 0.0f;
        } else {
            tForm.translate = startPos;
            tForm.rotate = {0.0f, 0.0f, 0.0f};
        }

        chargeSphere->GetTransform().transform = tForm;
        chargeSphere->GetTransform().isDirty = true;
        chargeSphere->Update();
    }

    // 溜め完了で発射ステートへ
    if (stateTimer_ >= config.chargeDuration) {
        owner.ChangeState(std::make_unique<BeamFiringState>());
    }
}

void BeamChargingState::OnDraw(EnemyBeamComponent& owner, uint32_t frameIndex) {
    // AOE予兆円柱の描画
    if (auto telegraph = owner.GetTelegraphCylinder()) {
        telegraph->SetCustomPSO("AOEWarning", Irufemi::BlendMode::kBlendModeAdd, PSOManager::DepthWrite::Disable,
                                PSOManager::CullMode::None);
        telegraph->SetCustomCBVAddress(owner.GetAOEParamsBuffer().GetGPUVirtualAddress(frameIndex));
        telegraph->Draw();
    }

    // チャージ球の描画
    if (auto chargeSphere = owner.GetChargeSphere()) {
        if (chargeSphere->GetTransform().transform.scale.x > 0.0f) {
            chargeSphere->Draw();
        }
    }
}

void BeamChargingState::OnExit(EnemyBeamComponent& owner) {
    if (auto chargeSphere = owner.GetChargeSphere()) {
        chargeSphere->GetTransform().transform.scale = {0.0f, 0.0f, 0.0f};
        chargeSphere->GetTransform().isDirty = true;
        chargeSphere->Update();
    }
}

// ============================================================================
// BeamFiringState
// ============================================================================

void BeamFiringState::OnEnter(EnemyBeamComponent& owner) {
    stateTimer_ = 0.0f;
    owner.SetHasHitCurrentBeam(false);
}

void BeamFiringState::OnUpdate(EnemyBeamComponent& owner, float deltaTime) {
    const auto& config = owner.GetConfig();

    stateTimer_ += deltaTime;

    // ボスの前進・旋回に合わせて発射口ワールド座標をリアルタイム同期
    Irufemi::Vector3 startPos = owner.GetCurrentMuzzlePosition();
    owner.SetStartPos(startPos);

    // --- ビーム発射動作のアニメーション ---
    float t = (std::min)(stateTimer_ / (std::max)(config.fireDuration, 0.001f), 1.0f);

    float easeThickness = 1.0f - (t * t * t);
    float currentThickness = config.beamMaxRadius * easeThickness;
    float currentLength = config.beamLength;

    const auto& direction = owner.GetDirection();
    Irufemi::Matrix4x4 rotMat = Irufemi::Math::DirectionToDirection({0.0f, 1.0f, 0.0f}, direction);
    Irufemi::Vector3 rotate = Irufemi::Math::ExtractEulerFromMatrix(rotMat);
    Irufemi::Vector3 center = startPos + direction * (currentLength * 0.5f);

    if (auto attackCore = owner.GetAttackCylinder()) {
        attackCore->SetPosition(center);
        attackCore->SetRotate(rotate);
        attackCore->SetScale({currentThickness * 0.5f, currentLength, currentThickness * 0.5f});
        attackCore->Update();
    }

    if (auto attackOuter = owner.GetAttackCylinderOuter()) {
        attackOuter->SetPosition(center);
        attackOuter->SetRotate(rotate);
        attackOuter->SetScale({currentThickness, currentLength, currentThickness});
        attackOuter->Update();
    }

    // 自機への当たり判定とダメージ処理
    owner.CheckBeamCollision();

    // 終了判定（発射完了でIDLEへ戻る）
    if (stateTimer_ >= config.fireDuration) {
        owner.ChangeState(nullptr);
    }
}

void BeamFiringState::OnDraw(EnemyBeamComponent& owner, uint32_t frameIndex) {
    // 外側オーラ (LightningCrawl)
    if (auto attackOuter = owner.GetAttackCylinderOuter()) {
        attackOuter->SetCustomPSO("LightningCrawl", Irufemi::BlendMode::kBlendModeAdd, PSOManager::DepthWrite::Disable,
                                  PSOManager::CullMode::None);
        attackOuter->SetCustomCBVAddress(owner.GetAuraParamsBuffer().GetGPUVirtualAddress(frameIndex));
        attackOuter->Draw();
    }

    // 内側コア (EnergyBeam)
    if (auto attackCore = owner.GetAttackCylinder()) {
        attackCore->SetCustomPSO("EnergyBeam", Irufemi::BlendMode::kBlendModeAdd, PSOManager::DepthWrite::Disable,
                                 PSOManager::CullMode::None);
        attackCore->SetCustomCBVAddress(owner.GetBeamParamsBuffer().GetGPUVirtualAddress(frameIndex));
        attackCore->Draw();
    }
}

void BeamFiringState::OnExit(EnemyBeamComponent& /*owner*/) {}
