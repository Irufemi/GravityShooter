#include "Framework/Component/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraManager.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"

CameraComponent::CameraComponent() = default;
CameraComponent::~CameraComponent() {
    OnDestroy();
}

void CameraComponent::OnDestroy() {
    if (gameObject_) {
        auto* engine = GetEngine();
        if (engine && engine->GetCameraManager()) {
            if (camera_) {
                engine->GetCameraManager()->RemoveCamera(camera_);
            }
        }
    }
}

void CameraComponent::OnRegisterProperties() {
    RegisterProperty("FOV", &fovAngleY_);
    RegisterProperty("NearZ", &nearZ_);
    RegisterProperty("FarZ", &farZ_);
    RegisterProperty("MakeActive", &makeActive_);
}

void CameraComponent::Initialize() {
    OnAwake();
}

void CameraComponent::OnAwake() {
    if (!camera_) {
        camera_ = std::make_shared<Camera>();

        auto* engine = GetEngine();
        int clientWidth = engine ? engine->GetGameResolutionWidth() : 1280;
        int clientHeight = engine ? engine->GetGameResolutionHeight() : 720;
        camera_->Initialize(clientWidth, clientHeight);

        // プロパティ値を適用（3D透視投影パラメータのみを設定し、2D正射影空間を保護）
        camera_->SetFovY(fovAngleY_);
        camera_->SetNearZ(nearZ_);
        camera_->SetFarZ(farZ_);
    }
}

void CameraComponent::Start() {
    if (gameObject_) {
        auto* engine = GetEngine();
        if (engine && engine->GetCameraManager() && camera_) {
            engine->GetCameraManager()->AddCamera(gameObject_->GetName(), camera_);
            if (makeActive_) {
                engine->GetCameraManager()->SetActiveCamera(gameObject_->GetName());
            }

            // 初期位置・角度を即座にCameraオブジェクトへ反映（第0フレーム同期）
            if (auto transform = GetTransform()) {
                camera_->SetTranslate(Irufemi::Math::Add(transform->GetWorldPosition(), positionOffset_));
                camera_->SetRotate(Irufemi::Math::Add(transform->GetWorldRotation(), rotationOffset_));
                camera_->SetFovY(fovAngleY_);
                camera_->SetNearZ(nearZ_);
                camera_->SetFarZ(farZ_);
                camera_->UpdateMatrix();
            }
        }
    }
}

void CameraComponent::Update() {
    if (!gameObject_ || !camera_) {
        return;
    }

    auto transform = GetTransform();
    if (!transform) {
        return;
    }

    // GameObjectのTransformとCameraの座標・角度を同期（ワールド座標系）＋ 演出オフセット
    camera_->SetTranslate(Irufemi::Math::Add(transform->GetWorldPosition(), positionOffset_));
    camera_->SetRotate(Irufemi::Math::Add(transform->GetWorldRotation(), rotationOffset_));

    // パラメータの動的更新反映（3D透視投影パラメータのみを設定）
    camera_->SetFovY(fovAngleY_);
    camera_->SetNearZ(nearZ_);
    camera_->SetFarZ(farZ_);

    camera_->UpdateMatrix();
}
