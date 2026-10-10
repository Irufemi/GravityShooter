#pragma once
#include "Framework/Component/Component.h"
#include <string>
#include <memory>

class ResultManagerComponent : public Component {
public:
    ResultManagerComponent() = default;
    ~ResultManagerComponent() override = default;

    void Initialize() override;
    void Update() override;
    void OnRegisterProperties() override;
    std::string GetComponentName() const override {
        return "ResultManagerComponent";
    }

private:
    float timer_ = 0.0f;
    float resultDelayTime_ = 2.0f;
    float timeScaleResetDelayTime_ = 1.5f; // スローモーションを解除するまでの時間
    float timeScaleRecoveryDuration_ = 0.5f; // スローモーションから等倍速に戻るまでの時間(イージング時間)
    bool canReturnToTitle_ = false;
    bool hasResetTimeScale_ = false;
    bool hasPlayedSnapSE_ = false;

    // イージング用状態変数
    bool isRecoveringTimeScale_ = false;
    float timeScaleRecoveryTimer_ = 0.0f;
    float startTimeScale_ = 0.0f;

    // プロパティ化してエディタで設定可能にする
    std::string nextSceneName_ = "Title";
    float baseTitleScale_ = 64.0f;

    std::weak_ptr<GameObject> darkBackdropObj_;
    std::weak_ptr<GameObject> pressSpaceObj_;
};
