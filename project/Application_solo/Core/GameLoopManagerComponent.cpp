#include "Core/GameLoopManagerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Renderer/Font/FontManager.h"
#include "Framework/Scene/BaseScene.h"
#include "Engine/Irufemi.h"
#include "Scenes/Result/ResultScene.h"

#include "Player/PlayerHealthComponent.h"
#include "Combat/Boss/BossComponent.h"
#include "Platform/Input/InputManager.h"
#include "Input/GameAction.h"
#include "Core/Utility/Log.h"
#include <iostream>

GameLoopManagerComponent::~GameLoopManagerComponent() {
    if (auto player = playerObj_.lock()) {
        if (auto health = player->GetComponent<PlayerHealthComponent>()) {
            health->SetOnPlayerDied(nullptr);
            health->SetOnDeathSequenceFinished(nullptr);
        }
    }
    if (auto boss = bossObj_.lock()) {
        if (auto b = boss->GetComponent<BossComponent>()) {
            b->SetOnBossDied(nullptr);
            b->SetOnDeathSequenceFinished(nullptr);
        }
    }
}

void GameLoopManagerComponent::Initialize() {
    state_ = State::Playing;
    playerObj_.reset();
    bossObj_.reset();

    // 事前キャッシュ: ResultScene で使用するテキストのSDF生成をバックグラウンドで事前に行う
    if (auto engine = GetEngine()) {
        if (auto fm = engine->GetFontManager()) {
            fm->PrecacheText("toro_glitch", L"STAGE CLEAR");
            fm->PrecacheText("toro_glitch", L"GAME OVER");
            fm->PrecacheText("toro_glitch", L"Press SPACE to Return");
        }
    }
}

void GameLoopManagerComponent::Start() {
    BindTargets();
}

bool GameLoopManagerComponent::BindTargets() {
    if (!gameObject_ || !gameObject_->GetScene()) {
        return false;
    }

    auto scene = gameObject_->GetScene();

    if (playerObj_.expired() && !targetPlayerName_.empty()) {
        if (auto playerObj = scene->FindGameObject(targetPlayerName_)) {
            if (auto playerHealth = playerObj->GetComponent<PlayerHealthComponent>()) {
                playerObj_ = playerObj;
                playerHealth->SetOnPlayerDied([this]() { OnPlayerDied(); });
                playerHealth->SetOnDeathSequenceFinished([this]() { OnDeathSequenceFinished(); });
            }
        }
    }

    if (bossObj_.expired() && !targetBossName_.empty()) {
        if (auto bossObj = scene->FindGameObject(targetBossName_)) {
            if (auto boss = bossObj->GetComponent<BossComponent>()) {
                bossObj_ = bossObj;
                boss->SetOnBossDied([this]() { OnBossDied(); });
                boss->SetOnDeathSequenceFinished([this]() { OnDeathSequenceFinished(); });
            }
        }
    }

    return !playerObj_.expired() && !bossObj_.expired();
}

void GameLoopManagerComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("Result Time Scale", &timeScaleAtResult_);
    RegisterProperty("Target Player Name", &targetPlayerName_);
    RegisterProperty("Target Boss Name", &targetBossName_);
}

void GameLoopManagerComponent::Update() {
    if (state_ == State::Playing) {
        if (playerObj_.expired() || bossObj_.expired()) {
            BindTargets();
        }

        // ポーズ画面呼び出しトリガー（ESCキー または ゲームパッド STARTボタン）
        if (auto engine = GetEngine()) {
            if (auto input = engine->GetInputManager()) {
                bool triggerPause = InputHelper::IsActionPressed(input, GameAction::Pause);
                if (triggerPause) {
                    if (auto sm = engine->GetSceneManager()) {
                        sm->PushScene("Pause");
                    }
                }
            }
        }
    }
}

void GameLoopManagerComponent::OnBossDied() {
    if (state_ != State::Playing) {
        return;
    }
    state_ = State::Finished;
    isClear_ = true;
    if (auto player = playerObj_.lock()) {
        if (auto health = player->GetComponent<PlayerHealthComponent>()) {
            health->SetGodMode(true); // ゲームクリア時に被弾しないようにする
        }
    }
    if (auto engine = GetEngine()) {
        engine->SetTimeScale(timeScaleAtResult_);
    }
}

void GameLoopManagerComponent::OnPlayerDied() {
    if (state_ != State::Playing) {
        return;
    }
    state_ = State::Finished;
    isClear_ = false;
    if (auto engine = GetEngine()) {
        engine->SetTimeScale(timeScaleAtResult_);
    }
}

void GameLoopManagerComponent::OnDeathSequenceFinished() {
    ResultScene::SetIsClear(isClear_);
    if (auto engine = GetEngine()) {
        if (auto sceneManager = engine->GetSceneManager()) {
            sceneManager->PushScene("Result");
        }
    }
}
