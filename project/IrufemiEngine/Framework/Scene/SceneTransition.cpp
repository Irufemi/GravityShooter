#include "Framework/Scene/SceneTransition.h"
#include "Framework/Scene/SceneTransitionDrivers.h"
#include <algorithm>
#include <unordered_map>
#include <functional>

SceneTransition::SceneTransition()
    : fadeDriver_(std::make_unique<FadeTransitionDriver>()),
      dissolveDriver_(std::make_unique<DissolveTransitionDriver>()),
      slideDriver_(std::make_unique<SlideTransitionDriver>()),
      radialBlurDriver_(std::make_unique<RadialBlurTransitionDriver>(false)),
      radialBlurWhiteDriver_(std::make_unique<RadialBlurTransitionDriver>(true)),
      gravitySingularityDriver_(std::make_unique<GravitySingularityTransitionDriver>()) {}

SceneTransition::~SceneTransition() = default;

void SceneTransition::Initialize(PostProcessManager* ppManager) {
    ppManager_ = ppManager;
}

void SceneTransition::Start(Type type, float duration, bool isOut, EaseType easeType,
                            const std::optional<Irufemi::Vector4>& customColor) {
    if (!ppManager_) {
        return;
    }

    currentType_ = type;
    easeType_ = easeType;
    duration_ = (std::max)(0.001f, duration); // 0除算防止
    isOut_ = isOut;
    timer_ = 0.0f;
    isActive_ = true;

    if (customColor.has_value()) {
        customColor_ = customColor;
    } else if (isOut) {
        customColor_ = std::nullopt; // フェードアウト開始時に新規指定が無ければリセット
    }

    // 前回のトランジション演出で使ったエフェクトだけを確実に取り除く
    for (auto mode : activeTransitionModes_) {
        ppManager_->RemoveActiveMode(mode);
    }
    activeTransitionModes_.clear();

    // 基本は黒フェードにリセットしておく（白フェード等で上書きされた色が残るのを防ぐため）
    ppManager_->GetFadeParams().color = {0.0f, 0.0f, 0.0f, 1.0f};

    switch (currentType_) {
    case Type::Fade:
        activeDriver_ = fadeDriver_.get();
        break;
    case Type::Dissolve:
        activeDriver_ = dissolveDriver_.get();
        break;
    case Type::Slide:
        activeDriver_ = slideDriver_.get();
        break;
    case Type::RadialBlur:
        radialBlurDriver_->SetCustomColor(customColor_);
        activeDriver_ = radialBlurDriver_.get();
        break;
    case Type::RadialBlurWhite:
        radialBlurWhiteDriver_->SetCustomColor(customColor_);
        activeDriver_ = radialBlurWhiteDriver_.get();
        break;
    case Type::GravitySingularity:
        gravitySingularityDriver_->SetCustomColor(customColor_);
        activeDriver_ = gravitySingularityDriver_.get();
        break;
    default:
        activeDriver_ = fadeDriver_.get();
        break;
    }

    if (activeDriver_) {
        activeDriver_->OnStart(ppManager_, activeTransitionModes_);
    }

    if (onStartCallback_) {
        onStartCallback_(currentType_, isOut_);
    }
}

void SceneTransition::Update(float deltaTime) {
    if (!isActive_ || !ppManager_) {
        return;
    }

    timer_ += deltaTime;
    float totalDuration = duration_ + kDwellTime;

    if (timer_ >= totalDuration) {
        timer_ = totalDuration;
        isActive_ = false;

        // フェードイン（画面が表示される方）が完了した場合はトランジションエフェクトのみをクリア
        if (!isOut_) {
            for (auto mode : activeTransitionModes_) {
                ppManager_->RemoveActiveMode(mode);
            }
            activeTransitionModes_.clear();
            ppManager_->ResetPostUI();
        }

        if (isOut_ && onOutFinishedCallback_) {
            auto cb = std::move(onOutFinishedCallback_);
            cb();
        }
        if (onFinishedCallback_) {
            auto cb = std::move(onFinishedCallback_);
            cb();
        }
    }

    // 演出自体の進行度 (0.0 ~ 1.0)
    // 溜め時間 (kDwellTime) 中は 1.0 固定にする
    float progress = (std::min)(1.0f, timer_ / duration_);

    // イージング関数の適用
    float easedProgress = EvaluateEase(easeType_, progress);

    // 実際にエフェクトに適用する係数
    float factor = isOut_ ? easedProgress : (1.0f - easedProgress);

    // 各モードのパラメータ更新をDriverへ委譲
    if (activeDriver_) {
        activeDriver_->OnUpdate(ppManager_, factor);
    }
}
