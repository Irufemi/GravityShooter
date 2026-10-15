#include "Resource/Model/Animator.h"
#include "Core/System/IrufemiEngine.h"
#include "Resource/Model/AnimationManager.h"
#include "Resource/Model/Data/SkeletonData.h"
#include "Framework/UI/DebugUI.h"
#include "Core/Math/Math.h"
#include <cmath>

#if defined USE_IMGUI
#include <imgui.h>
#endif

Animator::Animator() {}
Animator::~Animator() {}

void Animator::Initialize(IrufemiEngine* engine) {
    engine_ = engine;
}

void Animator::Play(const std::string& filename, bool loop, float fadeDuration) {
    if (!engine_ || !engine_->GetAnimationManager()) {
        return;
    }

    auto newAnim = engine_->GetAnimationManager()->LoadAnimationFile(filename);
    if (!newAnim) {
        return;
    }

    if (currentAnimation_ && fadeDuration > 0.0f && currentAnimationName_ != filename) {
        previousAnimation_ = currentAnimation_;
        previousAnimationName_ = currentAnimationName_;
        previousAnimationTime_ = animationTime_;
        fadeTimer_ = 0.0f;
        fadeDuration_ = fadeDuration;
        isBlending_ = true;
    } else {
        isBlending_ = false;
        previousAnimation_.reset();
    }

    currentAnimation_ = newAnim;
    currentAnimationName_ = filename;
    animationTime_ = 0.0f;
    isLooping_ = loop;
    deltaRootTranslation_ = {0.0f, 0.0f, 0.0f};
    deltaRootRotation_ = {0.0f, 0.0f, 0.0f, 1.0f};
}

void Animator::Update(SkeletonPose& targetPose) {
    if (!currentAnimation_ || !engine_) {
        return;
    }

    float deltaTime = engine_->GetGameDeltaTime() * playbackSpeed_;
    float prevTime = animationTime_;
    animationTime_ += deltaTime;

    if (currentAnimation_->duration > 0.0f) {
        if (isLooping_) {
            if (animationTime_ >= currentAnimation_->duration) {
                // ループ周回時: [prevTime, duration] と [0.0f, newTime] の両区間の移動量を正しく合算
                float newTime = std::fmod(animationTime_, currentAnimation_->duration);

                Irufemi::Vector3 dTrans1{}, dTrans2{};
                Irufemi::Quaternion dRot1{}, dRot2{};
                ExtractRootMotion(currentAnimation_.get(), targetPose.data, prevTime, currentAnimation_->duration,
                                  dTrans1, dRot1);
                ExtractRootMotion(currentAnimation_.get(), targetPose.data, 0.0f, newTime, dTrans2, dRot2);

                deltaRootTranslation_ = dTrans1 + dTrans2;
                deltaRootRotation_ = Irufemi::Math::Multiply(dRot1, dRot2);

                animationTime_ = newTime;
            } else {
                ExtractRootMotion(currentAnimation_.get(), targetPose.data, prevTime, animationTime_,
                                  deltaRootTranslation_, deltaRootRotation_);
            }
        } else {
            animationTime_ = (std::min)(animationTime_, currentAnimation_->duration);
            ExtractRootMotion(currentAnimation_.get(), targetPose.data, prevTime, animationTime_, deltaRootTranslation_,
                              deltaRootRotation_);
        }
    } else {
        deltaRootTranslation_ = {0.0f, 0.0f, 0.0f};
        deltaRootRotation_ = {0.0f, 0.0f, 0.0f, 1.0f};
    }

    if (isBlending_ && previousAnimation_) {
        fadeTimer_ += engine_->GetGameDeltaTime(); // フェードは等速(playbackSpeedに依存しない)
        float prevDeltaTime =
            engine_->GetGameDeltaTime() * playbackSpeed_; // ブレンド元の旧アニメーションも同一の再生速度で進行
        previousAnimationTime_ += prevDeltaTime;
        if (previousAnimation_->duration > 0.0f && isLooping_) {
            previousAnimationTime_ = std::fmod(previousAnimationTime_, previousAnimation_->duration);
        }

        float weight = fadeTimer_ / fadeDuration_;
        if (weight >= 1.0f) {
            isBlending_ = false;
            previousAnimation_.reset();
            AnimationManager::ApplyAnimation(targetPose, *currentAnimation_, animationTime_,
                                             false); // Rootは適用しない(外部で移動)
        } else {
            // ブレンド適用
            AnimationManager::BlendAnimation(targetPose, *previousAnimation_, previousAnimationTime_,
                                             *currentAnimation_, animationTime_, weight, false);
        }
    } else {
        // 通常の適用
        AnimationManager::ApplyAnimation(targetPose, *currentAnimation_, animationTime_, false);
    }

    AnimationManager::SkeletonUpdate(targetPose);
}

void Animator::ExtractRootMotion(const Animation* anim, const SkeletonData* skeleton, float prevTime, float currTime,
                                 Irufemi::Vector3& outDeltaTrans, Irufemi::Quaternion& outDeltaRot) {
    outDeltaTrans = {0.0f, 0.0f, 0.0f};
    outDeltaRot = {0.0f, 0.0f, 0.0f, 1.0f};
    if (!anim || !skeleton) {
        return;
    }

    if (anim->nodeAnimations.empty()) {
        return;
    }

    // ルートノードの取得（SkeletonData::root を直接使用して O(1) アクセス）
    if (skeleton->root < 0 || static_cast<size_t>(skeleton->root) >= skeleton->joints.size()) {
        return;
    }
    const std::string& rootNodeName = skeleton->joints[skeleton->root].name;

    auto rootIt = anim->nodeAnimations.find(rootNodeName);
    if (rootIt == anim->nodeAnimations.end()) {
        return;
    }

    const NodeAnimation& rootAnim = rootIt->second;

    if (!rootAnim.translate.keyframes.empty()) {
        Irufemi::Vector3 prevPos = AnimationManager::CalculateValue(rootAnim.translate, prevTime);
        Irufemi::Vector3 currPos = AnimationManager::CalculateValue(rootAnim.translate, currTime);
        outDeltaTrans = currPos - prevPos;
    }
    if (!rootAnim.rotate.keyframes.empty()) {
        Irufemi::Quaternion prevRot = AnimationManager::CalculateValue(rootAnim.rotate, prevTime);
        Irufemi::Quaternion currRot = AnimationManager::CalculateValue(rootAnim.rotate, currTime);
        // Quaternionの差分 (prevRot^-1 * currRot)
        outDeltaRot = Irufemi::Math::Multiply(Irufemi::Math::Inverse(prevRot), currRot);
    }
}

void Animator::Debug([[maybe_unused]] const char* objName) {
#if defined USE_IMGUI
    std::string name = std::string("Animator: ") + (objName ? objName : "Unnamed");
    if (ImGui::TreeNode(name.c_str())) {
        ImGui::Text("Current Animation: %s", currentAnimationName_.c_str());
        ImGui::SliderFloat("Playback Speed", &playbackSpeed_, 0.0f, 3.0f);
        ImGui::Checkbox("Loop", &isLooping_);
        if (currentAnimation_) {
            engine_->GetDebugUI()->DebugAnimationControl(*currentAnimation_, animationTime_);
        }
        if (ImGui::Button("Reset Time")) {
            animationTime_ = 0.0f;
        }
        if (isBlending_) {
            ImGui::ProgressBar(fadeTimer_ / fadeDuration_, ImVec2(0.0f, 0.0f), "Blending...");
        }
        ImGui::TreePop();
    }
#endif
}
