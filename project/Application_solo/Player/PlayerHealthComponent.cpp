#include "Player/PlayerHealthComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/Collider/ColliderComponent.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Core/Utility/Log.h"
#include "Core/Utility/JsonUtility.h"
#include <iostream>
#include <algorithm>
#include "Combat/ScoreManagerComponent.h"

void PlayerHealthComponent::LoadStatusFromJson() {
    if (statusDataPath_.empty()) {
        return;
    }

    nlohmann::json j;
    if (!Irufemi::JsonUtility::LoadFromFile(statusDataPath_, j)) {
        Irufemi::Log::OutPutLog(std::cout, "[PlayerHealth] Failed to load status: " + statusDataPath_ + "\n");
        return;
    }

    if (j.contains("maxHp")) {
        maxHp_ = j["maxHp"].get<int>();
        hp_ = maxHp_;
    }
}

void PlayerHealthComponent::OnRegisterProperties() {
    Irufemi::Component::OnRegisterProperties();
    RegisterProperty("Status Data Path", &statusDataPath_);
    RegisterProperty("Max HP", &maxHp_);
    RegisterProperty("Current HP", &hp_);
    RegisterPropertyRange("Invincibility Time", &maxInvincibilityTime_, 0.0f, 5.0f);
    RegisterProperty("God Mode", &isGodMode_);
    RegisterPropertyRange("Death Sequence Duration", &deathSequenceDuration_, 0.5f, 10.0f);
}

void PlayerHealthComponent::Initialize() {
    LoadStatusFromJson();

    isDead_ = false;
    deathTimer_ = 0.0f;
    hasTriggeredDeathSequenceFinished_ = false;
    invincibilityTimer_ = 0.0f;
    onDamageTakenListeners_.clear();
    onPlayerDiedListeners_.clear();
}

void PlayerHealthComponent::Start() {
    if (gameObject_) {
        if (auto col = GetCollider()) {
            col->SetDebugCategory(Irufemi::DebugCategory::Combat);
            col->SetDebugCustomColor(Irufemi::Vector4{0.0f, 1.0f, 1.0f, 1.0f});
        }
        effectMask_ = gameObject_->GetComponent<Irufemi::EffectMaskComponent>();
        if (effectMask_) {
            normalOutlineColor_ = effectMask_->GetCustomParams().color1;
            hasCachedNormalOutline_ = true;
        }
    }
}

void PlayerHealthComponent::Update() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    if (engine->GetInputManager() && engine->GetInputManager()->IsKeyPressed(VK_F9)) {
        isGodMode_ = !isGodMode_;
        Irufemi::Log::OutPutLog(std::cout, std::string("[PlayerHealth] God Mode ") + (isGodMode_ ? "ON\n" : "OFF\n"));
    }
#endif

    if (auto col = GetCollider()) {
        if (IsInvincible()) {
            col->SetDebugCustomColor(Irufemi::Vector4{1.0f, 1.0f, 1.0f, 1.0f});
        } else {
            col->SetDebugCustomColor(Irufemi::Vector4{0.0f, 1.0f, 1.0f, 1.0f});
        }
    }

    if (isDead_) {
        if (!hasTriggeredDeathSequenceFinished_) {
            deathTimer_ += engine->GetRealDeltaTime();
            if (deathTimer_ >= deathSequenceDuration_) {
                hasTriggeredDeathSequenceFinished_ = true;
                NotifyDeathSequenceFinished();
            }
        }
        return;
    }

    float dt = engine->GetGameDeltaTime();
    if (dt <= 0.0f) {
        return;
    }

    // 無敵タイマーの更新 & ブリンク演出 (Juice)
    if (invincibilityTimer_ > 0.0f) {
        invincibilityTimer_ -= dt;

        if (!effectMask_ && gameObject_) {
            effectMask_ = gameObject_->GetComponent<Irufemi::EffectMaskComponent>();
        }

        if (effectMask_) {
            if (!hasCachedNormalOutline_) {
                normalOutlineColor_ = effectMask_->GetCustomParams().color1;
                hasCachedNormalOutline_ = true;
            }
            // 0.1秒周期で警告レッドと通常シアンを交互に切り替え
            bool blinkState = static_cast<int>(invincibilityTimer_ * 10.0f) % 2 == 0;
            auto params = effectMask_->GetCustomParams();
            params.color1 = blinkState ? warningOutlineColor_ : normalOutlineColor_;
            effectMask_->SetCustomParams(params);
        }

        if (invincibilityTimer_ <= 0.0f) {
            invincibilityTimer_ = 0.0f;
            // 無敵終了時に通常カラーへ完全復帰
            if (effectMask_) {
                auto params = effectMask_->GetCustomParams();
                params.color1 = normalOutlineColor_;
                effectMask_->SetCustomParams(params);
            }
        }
    }
}

void PlayerHealthComponent::TakeDamage(int damage) {
    if (isDead_) {
        return;
    }

    if (isGodMode_) {
        Irufemi::Log::OutPutLog(std::cout, "[PlayerHealth] TakeDamage ignored (God Mode)\n");
        return;
    }

    if (IsInvincible()) {
        Irufemi::Log::OutPutLog(std::cout, "[PlayerHealth] TakeDamage ignored (Invincible)\n");
        return;
    }

    hp_ -= damage;
    Irufemi::Log::OutPutLog(std::cout, "[PlayerHealth] Took Damage! HP: " + std::to_string(hp_) + "\n");

    for (const auto& listener : onHealthChangedListeners_) {
        if (listener) {
            listener((std::max)(0, hp_), maxHp_);
        }
    }

    if (hp_ <= 0) {
        hp_ = 0;
        isDead_ = true;
        deathTimer_ = 0.0f;
        hasTriggeredDeathSequenceFinished_ = false;

        NotifyPlayerDied();

        Irufemi::Log::OutPutLog(std::cout, "[PlayerHealth] Player Died!\n");
        return;
    }

    invincibilityTimer_ = maxInvincibilityTime_;

    if (auto scoreMgr = ScoreManagerComponent::GetInstance()) {
        scoreMgr->NotifyPlayerDamaged();
    }

    for (const auto& listener : onDamageTakenListeners_) {
        if (listener) {
            listener(damage);
        }
    }
}

void PlayerHealthComponent::NotifyPlayerDied() {
    auto listenersCopy = onPlayerDiedListeners_;
    for (const auto& entry : listenersCopy) {
        if (entry.callback) {
            entry.callback();
        }
    }
}

bool PlayerHealthComponent::RemoveOnPlayerDiedListener(ListenerId id) {
    auto it = std::remove_if(onPlayerDiedListeners_.begin(), onPlayerDiedListeners_.end(),
                             [id](const PlayerDiedListenerEntry& entry) { return entry.id == id; });
    if (it != onPlayerDiedListeners_.end()) {
        onPlayerDiedListeners_.erase(it, onPlayerDiedListeners_.end());
        return true;
    }
    return false;
}

void PlayerHealthComponent::NotifyDeathSequenceFinished() {
    if (onDeathSequenceFinished_) {
        onDeathSequenceFinished_();
    }
}

Irufemi::ColliderComponent* PlayerHealthComponent::GetCollider() const {
    if (gameObject_) {
        return gameObject_->GetComponentByInterface<Irufemi::ColliderComponent>();
    }
    return nullptr;
}
