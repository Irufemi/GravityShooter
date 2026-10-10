#include "Framework/Scene/SceneTransition.h"
#include "Framework/Scene/SceneTransitionDrivers.h"
#include <algorithm>
#include <unordered_map>
#include <functional>

namespace {
std::unique_ptr<ITransitionDriver> CreateDriver(SceneTransition::Type type) {
    static const std::unordered_map<SceneTransition::Type, std::function<std::unique_ptr<ITransitionDriver>()>>
        kFactory = {
            {SceneTransition::Type::Fade, []() { return std::make_unique<FadeTransitionDriver>(); }},
            {SceneTransition::Type::Dissolve, []() { return std::make_unique<DissolveTransitionDriver>(); }},
            {SceneTransition::Type::Slide, []() { return std::make_unique<SlideTransitionDriver>(); }},
            {SceneTransition::Type::RadialBlur, []() { return std::make_unique<RadialBlurTransitionDriver>(false); }},
            {SceneTransition::Type::RadialBlurWhite,
             []() { return std::make_unique<RadialBlurTransitionDriver>(true); }},
        };
    auto it = kFactory.find(type);
    if (it != kFactory.end()) {
        return it->second();
    }
    return std::make_unique<FadeTransitionDriver>();
}
} // namespace

SceneTransition::SceneTransition() = default;
SceneTransition::~SceneTransition() = default;

void SceneTransition::Initialize(PostProcessManager* ppManager) {
    ppManager_ = ppManager;
}

void SceneTransition::Start(Type type, float duration, bool isOut, EaseType easeType) {
    if (!ppManager_) {
        return;
    }

    currentType_ = type;
    easeType_ = easeType;
    duration_ = (std::max)(0.001f, duration); // 0除算防止
    isOut_ = isOut;
    timer_ = 0.0f;
    isActive_ = true;

    // 前回のトランジション演出で使ったエフェクトだけを確実に取り除く
    for (auto mode : activeTransitionModes_) {
        ppManager_->RemoveActiveMode(mode);
    }
    activeTransitionModes_.clear();

    // 基本は黒フェードにリセットしておく（白フェード等で上書きされた色が残るのを防ぐため）
    ppManager_->GetFadeParams().color = {0.0f, 0.0f, 0.0f, 1.0f};

    driver_ = CreateDriver(currentType_);
    if (driver_) {
        driver_->OnStart(ppManager_, activeTransitionModes_);
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
    if (driver_) {
        driver_->OnUpdate(ppManager_, factor);
    }
}
