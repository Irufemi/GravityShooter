#include "UI/ReticleUIComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/Primitive2DRendererComponent.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Player/PlayerTargetingComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"
#include <cmath>
#include <algorithm>

void ReticleUIComponent::Initialize() {
    currentScale_ = 1.0f;
    pulseTimer_ = 0.0f;
    wasHovering_ = false;

    if (gameObject_) {
        primitiveRenderer_ = gameObject_->GetComponent<Primitive2DRendererComponent>();
        if (primitiveRenderer_) {
            primitiveRenderer_->SetShape(Irufemi::Primitive2DType::Ring);
            primitiveRenderer_->SetSize({36.0f, 36.0f});
            primitiveRenderer_->SetThickness(2.0f);
            primitiveRenderer_->SetTopMost(true);
        }
    }

    // ゲーム開始時は画面中央（640, 360）から照準を開始
    if (auto engine = GetEngine()) {
        if (auto inputManager = engine->GetInputManager()) {
            inputManager->SetVirtualCursorPosition({640.0f, 360.0f});
        }
    }
}

void ReticleUIComponent::Update() {
    if (!gameObject_) {
        return;
    }

    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    auto transform = GetTransform();
    if (!transform) {
        return;
    }

    auto inputManager = engine->GetInputManager();
    auto cameraManager = engine->GetCameraManager();
    if (!inputManager || !cameraManager || !cameraManager->GetActiveCamera()) {
        return;
    }
    auto camera = cameraManager->GetActiveCamera();
    float deltaTime = engine->GetGameDeltaTime();
    if (deltaTime <= 0.0f) {
        deltaTime = engine->GetDeltaTime();
    }

    if (!primitiveRenderer_) {
        primitiveRenderer_ = gameObject_->GetComponent<Primitive2DRendererComponent>();
        if (primitiveRenderer_) {
            primitiveRenderer_->SetShape(Irufemi::Primitive2DType::Ring);
            primitiveRenderer_->SetSize({36.0f, 36.0f});
            primitiveRenderer_->SetThickness(2.0f);
            primitiveRenderer_->SetTopMost(true);
        }
    }

    // ターゲットコンポーネントの参照を取得（キャッシュ）
    if (!targetingComp_) {
        if (auto scene = gameObject_->GetScene()) {
            if (auto playerObj = scene->FindGameObject("Player")) {
                targetingComp_ = playerObj->GetComponent<PlayerTargetingComponent>();
            }
        }
    }

    // 1. 敵ホバー状態の判定
    bool isHovering = false;
    if (targetingComp_) {
        isHovering = (targetingComp_->GetHoverTarget() != nullptr);
    }

    // ホバー開始時の瞬間パルス
    if (isHovering && !wasHovering_) {
        pulseTimer_ = 0.2f; // パルス発生
    }
    wasHovering_ = isHovering;

    // 2. 仮想カーソル更新（ホバーマグネット摩擦を適用）
    float speedMultiplier = isHovering ? kMagnetFriction_ : 1.0f;
    inputManager->UpdateVirtualCursor(deltaTime, speedMultiplier);

    // 3. レティクル座標の更新（仮想カーソルと同期）
    const auto& cursorPos = inputManager->GetVirtualCursorPosition();
    Irufemi::Vector2 uiPos = camera->ScreenToUIPosition(cursorPos);
    transform->SetPosition(Irufemi::Vector3{uiPos.x, uiPos.y, 0.0f});

    // 4. Game Juice 演出（パルス拡縮と発光カラー）
    float targetScale = 1.0f;
    Irufemi::Vector4 targetColor = {0.1f, 0.85f, 1.0f, 0.65f}; // 通常時: 控えめなネオンシアンリング

    if (isHovering) {
        targetColor = {1.0f, 0.35f, 0.2f, 0.85f}; // ホバー時: ロック警告レッド/オレンジ
        if (pulseTimer_ > 0.0f) {
            pulseTimer_ -= deltaTime;
            targetScale = 1.18f; // パルス瞬間拡大
        } else {
            targetScale = 1.06f; // ホバー維持時
        }
    }

    // スケールのスムーズ補間
    currentScale_ = std::lerp(currentScale_, targetScale, (std::min)(1.0f, deltaTime * 16.0f));
    transform->SetScale({currentScale_, currentScale_, 1.0f});

    if (primitiveRenderer_) {
        primitiveRenderer_->SetColor(targetColor);
    }
}
