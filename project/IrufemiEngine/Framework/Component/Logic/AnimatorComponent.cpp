#include "Framework/Component/Logic/AnimatorComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/Math/MathFunction.h"

AnimatorComponent::AnimatorComponent() {
    animator_ = std::make_unique<Animator>();
}

AnimatorComponent::~AnimatorComponent() {}

void AnimatorComponent::Initialize() {
    OnSpawned();
}

void AnimatorComponent::OnSpawned() {
    if (GetGameObject() && GetGameObject()->GetScene()) {
        animator_->Initialize(GetGameObject()->GetScene()->GetEngine());
    }
}

void AnimatorComponent::Start() {
    if (GetGameObject() && GetGameObject()->GetScene()) {
        animator_->Initialize(GetGameObject()->GetScene()->GetEngine());
    }

    if (!defaultAnimation_.empty()) {
        Play(defaultAnimation_, true);
    }
}

void AnimatorComponent::Play(const std::string& animationName, bool loop, float fadeDuration) {
    defaultAnimation_ = animationName;
    currentLoadedAnimation_ = animationName;
    animator_->Play(animationName, loop, fadeDuration);
}

void AnimatorComponent::Update() {
    // エディタ等で文字列が変更された場合の動的ロード検知
    if (defaultAnimation_ != currentLoadedAnimation_) {
        Play(defaultAnimation_, true); // デフォルトループで再ロード
    }

    animator_->SetPlaybackSpeed(playbackSpeed_);

    // 同じGameObjectについている SkinnedMeshRenderer を探す（キャッシュ化）
    if (!cachedRenderer_ && GetGameObject()) {
        cachedRenderer_ = GetGameObject()->GetComponent<SkinnedMeshRendererComponent>();
    }

    if (cachedRenderer_ && cachedRenderer_->GetRawObject()) {
        SkeletonPose* pose = cachedRenderer_->GetRawObject()->GetInternalSkeletonPose();
        if (pose && pose->data) {
            animator_->Update(*pose);

            // ルートモーションをGameObjectのTransformに適用する
            if (applyRootMotion_ && GetGameObject()) {
                auto transform = GetTransform();
                if (transform) {
                    Irufemi::Vector3 deltaTrans = animator_->GetDeltaRootTranslation();
                    Irufemi::Quaternion deltaRot = animator_->GetDeltaRootRotation();

                    // キャラクターの現在の回転を考慮してワールド空間へベクトル変換
                    const Irufemi::Quaternion& currentQuat = transform->GetRotationQuat();
                    Irufemi::Vector3 worldDelta = Irufemi::Math::RotateVector(deltaTrans, currentQuat);
                    transform->SetPosition(Irufemi::Math::Add(transform->GetPosition(), worldDelta));

                    // 回転ルートモーションの合成（クォータニオン乗算）
                    Irufemi::Quaternion newQuat = Irufemi::Math::Multiply(currentQuat, deltaRot);
                    transform->SetRotationQuat(newQuat);
                }
            }

            // SkinnedMeshRenderer に対して計算済みのポーズを描画に使うよう指示
            cachedRenderer_->SetPoseOverride(pose);
        }
    }
}

void AnimatorComponent::OnRegisterProperties() {
    RegisterProperty("Default Animation", &defaultAnimation_).SetTooltip("The name of the initial animation to play");

    RegisterProperty("Playback Speed", &playbackSpeed_).SetTooltip("Animation playback speed multiplier");

    RegisterProperty("Apply Root Motion", &applyRootMotion_)
        .SetTooltip("Apply animation root motion to GameObject transform");
}
