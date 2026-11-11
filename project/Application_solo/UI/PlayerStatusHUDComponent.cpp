#include "UI/PlayerStatusHUDComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Player/PlayerHealthComponent.h"
#include "Player/GravityPlayerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Scene/SceneManager.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

PlayerStatusHUDComponent::PlayerStatusHUDComponent() = default;
PlayerStatusHUDComponent::~PlayerStatusHUDComponent() = default;

PlayerStatusHUDComponent* PlayerStatusHUDComponent::GetOrCreate(BaseScene* scene) {
    if (!scene) {
        return nullptr;
    }
    if (auto obj = scene->FindGameObject("PlayerStatusHUD")) {
        if (auto comp = obj->GetComponent<PlayerStatusHUDComponent>()) {
            return comp;
        }
    }

    auto hudObj = std::make_shared<GameObject>("PlayerStatusHUD");
    hudObj->SetIsSerializable(false);
    auto comp = hudObj->AddComponent<PlayerStatusHUDComponent>();
    scene->AddGameObject(hudObj);
    hudObj->Initialize();
    return comp.get();
}

void PlayerStatusHUDComponent::Initialize() {
    EnsureUIElements();
}

void PlayerStatusHUDComponent::Start() {
    EnsureUIElements();
    BindPlayerComponents();
}

void PlayerStatusHUDComponent::SetVisible(bool visible) {
    isVisible_ = visible;
    if (rootContainerObj_) {
        rootContainerObj_->SetIsActive(visible);
    }
}

void PlayerStatusHUDComponent::EnsureUIElements() {
    if (isElementsCreated_ || !gameObject_) {
        return;
    }
    auto scene = gameObject_->GetScene();
    if (!scene) {
        return;
    }

    constexpr float kBaseX = 45.0f;
    constexpr float kBaseY = 32.0f;

    // 1. ルートコンテナ（透過フレームレスデザイン）
    rootContainerObj_ = std::make_shared<GameObject>("HUD_PlayerStatusRoot");
    rootContainerObj_->SetIsSerializable(false);
    scene->AddGameObject(rootContainerObj_);
    if (auto t = rootContainerObj_->GetTransform()) {
        t->SetPosition({kBaseX, kBaseY, 0.0f});
    }

    // 2. HPヘッダーテキスト
    hpHeaderObj_ = std::make_shared<GameObject>("HUD_HPHeader");
    hpHeaderObj_->SetIsSerializable(false);
    scene->AddGameObject(hpHeaderObj_);
    rootContainerObj_->AddChild(hpHeaderObj_);
    if (auto t = hpHeaderObj_->GetTransform()) {
        t->SetPosition({0.0f, 0.0f, 1.5f});
    }
    hpHeaderText_ = hpHeaderObj_->AddComponent<TextRendererComponent>().get();
    hpHeaderText_->SetFontId("toro_glitch");
    hpHeaderText_->SetAlignment(TextAlignment::Left);
    hpHeaderText_->SetBaseScale(11.0f);
    hpHeaderText_->SetTopMost(true);
    hpHeaderText_->SetColor({0.35f, 0.80f, 1.0f, 0.75f});
    hpHeaderText_->SetText(L"// HULL INTEGRITY");
    hpHeaderObj_->Initialize();

    // 3. HPゲージ枠（スリット構造）
    constexpr float kBarY = 20.0f;
    constexpr float kBarW = 180.0f;
    constexpr float kBarH = 10.0f;

    hpBarBgObj_ = std::make_shared<GameObject>("HUD_HPBarBg");
    hpBarBgObj_->SetIsSerializable(false);
    scene->AddGameObject(hpBarBgObj_);
    rootContainerObj_->AddChild(hpBarBgObj_);
    if (auto t = hpBarBgObj_->GetTransform()) {
        t->SetPosition({0.0f, kBarY, 1.6f});
    }
    hpBarBgSprite_ = hpBarBgObj_->AddComponent<SpriteRendererComponent>().get();
    hpBarBgSprite_->SetBaseSize({kBarW, kBarH});
    hpBarBgSprite_->SetAnchor({0.0f, 0.5f});
    hpBarBgSprite_->SetColor({0.03f, 0.07f, 0.12f, 0.75f});
    hpBarBgSprite_->SetTopMost(true);
    hpBarBgObj_->Initialize();

    // 4. HP Ghost Bar（後追い遅延バー）
    hpGhostBarObj_ = std::make_shared<GameObject>("HUD_HPGhostBar");
    hpGhostBarObj_->SetIsSerializable(false);
    scene->AddGameObject(hpGhostBarObj_);
    rootContainerObj_->AddChild(hpGhostBarObj_);
    if (auto t = hpGhostBarObj_->GetTransform()) {
        t->SetPosition({0.0f, kBarY, 1.4f});
    }
    hpGhostBarSprite_ = hpGhostBarObj_->AddComponent<SpriteRendererComponent>().get();
    hpGhostBarSprite_->SetBaseSize({kBarW, kBarH});
    hpGhostBarSprite_->SetAnchor({0.0f, 0.5f});
    hpGhostBarSprite_->SetColor({1.0f, 0.40f, 0.15f, 0.85f}); // オレンジ/白熱赤
    hpGhostBarSprite_->SetTopMost(true);
    hpGhostBarObj_->Initialize();

    // 5. HP Front Bar（即時実HPバー）
    hpFrontBarObj_ = std::make_shared<GameObject>("HUD_HPFrontBar");
    hpFrontBarObj_->SetIsSerializable(false);
    scene->AddGameObject(hpFrontBarObj_);
    rootContainerObj_->AddChild(hpFrontBarObj_);
    if (auto t = hpFrontBarObj_->GetTransform()) {
        t->SetPosition({0.0f, kBarY, 1.2f});
    }
    hpFrontBarSprite_ = hpFrontBarObj_->AddComponent<SpriteRendererComponent>().get();
    hpFrontBarSprite_->SetBaseSize({kBarW, kBarH});
    hpFrontBarSprite_->SetAnchor({0.0f, 0.5f});
    hpFrontBarSprite_->SetColor({0.15f, 0.95f, 0.85f, 0.95f}); // ネオンエメラルドシアン
    hpFrontBarSprite_->SetTopMost(true);
    hpFrontBarObj_->Initialize();

    // 6. HPパーセントテキスト
    hpTextObj_ = std::make_shared<GameObject>("HUD_HPText");
    hpTextObj_->SetIsSerializable(false);
    scene->AddGameObject(hpTextObj_);
    rootContainerObj_->AddChild(hpTextObj_);
    if (auto t = hpTextObj_->GetTransform()) {
        t->SetPosition({kBarW + 10.0f, kBarY - 6.0f, 1.2f});
    }
    hpText_ = hpTextObj_->AddComponent<TextRendererComponent>().get();
    hpText_->SetFontId("toro_glitch");
    hpText_->SetAlignment(TextAlignment::Left);
    hpText_->SetBaseScale(11.0f);
    hpText_->SetTopMost(true);
    hpText_->SetColor({1.0f, 1.0f, 1.0f, 0.90f});
    hpText_->SetText(L"100%");
    hpTextObj_->Initialize();

    // 7. デブリPipスロット（HPバー直下、Y: 38px）
    constexpr float kDebrisY = 38.0f;
    constexpr float kPipStartX = 52.0f;
    constexpr float kPipW = 18.0f;
    constexpr float kPipH = 8.0f;
    constexpr float kPipSpacing = 5.0f;

    debrisHeaderObj_ = std::make_shared<GameObject>("HUD_DebrisHeader");
    debrisHeaderObj_->SetIsSerializable(false);
    scene->AddGameObject(debrisHeaderObj_);
    rootContainerObj_->AddChild(debrisHeaderObj_);
    if (auto t = debrisHeaderObj_->GetTransform()) {
        t->SetPosition({0.0f, kDebrisY - 5.0f, 1.5f});
    }
    debrisHeaderText_ = debrisHeaderObj_->AddComponent<TextRendererComponent>().get();
    debrisHeaderText_->SetFontId("toro_glitch");
    debrisHeaderText_->SetAlignment(TextAlignment::Left);
    debrisHeaderText_->SetBaseScale(10.0f);
    debrisHeaderText_->SetTopMost(true);
    debrisHeaderText_->SetColor({0.30f, 0.75f, 0.95f, 0.70f});
    debrisHeaderText_->SetText(L"DEBRIS");
    debrisHeaderObj_->Initialize();

    for (int i = 0; i < kMaxPipCount; ++i) {
        float pipX = kPipStartX + i * (kPipW + kPipSpacing);

        // Pip背景枠
        auto bgObj = std::make_shared<GameObject>("HUD_PipBg_" + std::to_string(i));
        bgObj->SetIsSerializable(false);
        scene->AddGameObject(bgObj);
        rootContainerObj_->AddChild(bgObj);
        if (auto t = bgObj->GetTransform()) {
            t->SetPosition({pipX + kPipW * 0.5f, kDebrisY, 1.5f});
        }
        auto bgSprite = bgObj->AddComponent<SpriteRendererComponent>();
        bgSprite->SetBaseSize({kPipW, kPipH});
        bgSprite->SetAnchor({0.5f, 0.5f});
        bgSprite->SetColor({0.03f, 0.07f, 0.12f, 0.75f});
        bgSprite->SetTopMost(true);
        bgObj->Initialize();
        debrisPipBgObjs_[i] = bgObj;

        // Pip充填バー
        auto fillObj = std::make_shared<GameObject>("HUD_PipFill_" + std::to_string(i));
        fillObj->SetIsSerializable(false);
        scene->AddGameObject(fillObj);
        rootContainerObj_->AddChild(fillObj);
        if (auto t = fillObj->GetTransform()) {
            t->SetPosition({pipX + kPipW * 0.5f, kDebrisY, 1.3f});
        }
        auto fillSprite = fillObj->AddComponent<SpriteRendererComponent>();
        fillSprite->SetBaseSize({kPipW - 2.0f, kPipH - 2.0f});
        fillSprite->SetAnchor({0.5f, 0.5f});
        fillSprite->SetColor({0.06f, 0.10f, 0.15f, 0.25f}); // 初期は空スロット
        fillSprite->SetTopMost(true);
        fillObj->Initialize();
        debrisPipFillObjs_[i] = fillObj;
        debrisPipFillSprites_[i] = fillSprite.get();
        pipPopScales_[i] = 1.0f;
    }

    // 8. デブリカウントテキスト
    constexpr float kCountX = kPipStartX + (kPipW + kPipSpacing) * 5.0f + 6.0f;
    debrisCountObj_ = std::make_shared<GameObject>("HUD_DebrisCount");
    debrisCountObj_->SetIsSerializable(false);
    scene->AddGameObject(debrisCountObj_);
    rootContainerObj_->AddChild(debrisCountObj_);
    if (auto t = debrisCountObj_->GetTransform()) {
        t->SetPosition({kCountX, kDebrisY - 5.0f, 1.5f});
    }
    debrisCountText_ = debrisCountObj_->AddComponent<TextRendererComponent>().get();
    debrisCountText_->SetFontId("toro_glitch");
    debrisCountText_->SetAlignment(TextAlignment::Left);
    debrisCountText_->SetBaseScale(10.0f);
    debrisCountText_->SetTopMost(true);
    debrisCountText_->SetColor({0.60f, 0.85f, 1.0f, 0.75f});
    debrisCountText_->SetText(L"(0/5)");
    debrisCountObj_->Initialize();

    rootContainerObj_->Initialize();
    isElementsCreated_ = true;
}

void PlayerStatusHUDComponent::BindPlayerComponents() {
    auto scene = gameObject_ ? gameObject_->GetScene() : nullptr;
    if (!scene) {
        return;
    }

    auto player = scene->FindGameObject("Player");
    if (!player) {
        return;
    }
    playerObj_ = player;

    cachedHealthComp_ = player->GetComponent<PlayerHealthComponent>();
    cachedGravityComp_ = player->GetComponent<GravityPlayerComponent>();

    if (cachedHealthComp_) {
        int maxHp = cachedHealthComp_->GetMaxHp();
        int hp = cachedHealthComp_->GetHp();
        currentHpRatio_ = (maxHp > 0) ? static_cast<float>(hp) / maxHp : 1.0f;
        targetHpRatio_ = currentHpRatio_;
        ghostHpRatio_ = currentHpRatio_;

        cachedHealthComp_->AddOnHealthChangedListener([this](int currentHp, int maxHp) {
            targetHpRatio_ = (maxHp > 0) ? static_cast<float>(currentHp) / maxHp : 0.0f;
            targetHpRatio_ = std::clamp(targetHpRatio_, 0.0f, 1.0f);

            // 被弾時はGhost Barの遅延タイマーおよび白熱フラッシュを発火
            if (targetHpRatio_ < currentHpRatio_) {
                ghostDelayTimer_ = kGhostDelayDuration;
                hitFlashTimer_ = 0.12f;
            }
        });
    }
}

void PlayerStatusHUDComponent::Update() {
    if (!isVisible_) {
        return;
    }

    EnsureUIElements();

    // ポーズ画面（PauseScene）など別シーンが前面にある場合はHUDを自動非表示
    bool isCurrentSceneActive = true;
    if (auto engine = GetEngine()) {
        if (auto sm = engine->GetSceneManager()) {
            if (gameObject_ && gameObject_->GetScene()) {
                isCurrentSceneActive = (sm->GetCurrentScene() == gameObject_->GetScene());
            }
        }
    }

    if (!isCurrentSceneActive) {
        if (rootContainerObj_ && rootContainerObj_->GetIsActive()) {
            rootContainerObj_->SetIsActive(false);
        }
        return;
    } else {
        if (isVisible_ && rootContainerObj_ && !rootContainerObj_->GetIsActive()) {
            rootContainerObj_->SetIsActive(true);
        }
    }

    if (!cachedHealthComp_ || !cachedGravityComp_ || playerObj_.expired()) {
        BindPlayerComponents();
    }

    float dt = 0.0166f;
    if (auto engine = GetEngine()) {
        dt = engine->GetDeltaTime();
    }

    UpdateHealthAnimation(dt);
    UpdateDebrisPips(dt);
    UpdateCriticalPulse(dt);
}

void PlayerStatusHUDComponent::UpdateHealthAnimation(float dt) {
    constexpr float kBarW = 180.0f;
    constexpr float kBarH = 10.0f;

    // 1. 実HPバーの即時・滑らかな追従
    currentHpRatio_ = std::lerp(currentHpRatio_, targetHpRatio_, (std::min)(1.0f, dt * 25.0f));

    // 2. Ghost Bar（後追いバー）の遅延＆減衰アニメーション
    if (ghostDelayTimer_ > 0.0f) {
        ghostDelayTimer_ -= dt;
    } else {
        ghostHpRatio_ = std::lerp(ghostHpRatio_, currentHpRatio_, (std::min)(1.0f, dt * 5.5f));
    }
    if (ghostHpRatio_ < currentHpRatio_) {
        ghostHpRatio_ = currentHpRatio_;
    }

    // 3. スプライトサイズの更新（左端アンカー基準）
    if (hpFrontBarSprite_) {
        float frontW = (std::max)(0.0f, kBarW * currentHpRatio_);
        hpFrontBarSprite_->SetBaseSize({frontW, kBarH});

        // 被弾フラッシュまたはHP残量によるカラー変化
        if (hitFlashTimer_ > 0.0f) {
            hitFlashTimer_ -= dt;
            hpFrontBarSprite_->SetColor({2.2f, 2.2f, 2.2f, 1.0f}); // 白熱閃光
        } else if (currentHpRatio_ > 0.50f) {
            hpFrontBarSprite_->SetColor({0.15f, 0.95f, 0.85f, 0.95f}); // ネオンエメラルドシアン
        } else if (currentHpRatio_ > 0.25f) {
            hpFrontBarSprite_->SetColor({1.0f, 0.85f, 0.20f, 0.95f}); // イエロー警告
        } else {
            // 危険域：赤色
            hpFrontBarSprite_->SetColor({1.0f, 0.15f, 0.20f, 0.95f});
        }
    }

    if (hpGhostBarSprite_) {
        float ghostW = (std::max)(0.0f, kBarW * ghostHpRatio_);
        hpGhostBarSprite_->SetBaseSize({ghostW, kBarH});
    }

    // 4. HPパーセント数値テキストの更新
    if (hpText_ && cachedHealthComp_) {
        int percent = static_cast<int>(std::round(std::clamp(currentHpRatio_, 0.0f, 1.0f) * 100.0f));
        std::wstring text = std::to_wstring(percent) + L"%";
        hpText_->SetText(text);

        if (currentHpRatio_ <= 0.25f) {
            hpText_->SetColor({1.0f, 0.25f, 0.25f, 1.0f});
        } else {
            hpText_->SetColor({1.0f, 1.0f, 1.0f, 0.90f});
        }
    }
}

void PlayerStatusHUDComponent::UpdateDebrisPips(float dt) {
    if (!cachedGravityComp_) {
        return;
    }

    int currentCount = cachedGravityComp_->GetOrbitingDebrisCount();
    currentCount = std::clamp(currentCount, 0, kMaxPipCount);

    // 新たにガレキをキャッチしたスロットに Pop-in アニメーションを付与
    if (currentCount > lastDebrisCount_) {
        for (int i = lastDebrisCount_; i < currentCount; ++i) {
            if (i < kMaxPipCount) {
                pipPopScales_[i] = 1.35f; // 1.35倍からスナップ
            }
        }
    }
    lastDebrisCount_ = currentCount;

    constexpr float kPipW = 18.0f;
    constexpr float kPipH = 8.0f;

    for (int i = 0; i < kMaxPipCount; ++i) {
        // Pop スケールの減衰
        if (pipPopScales_[i] > 1.0f) {
            pipPopScales_[i] = std::lerp(pipPopScales_[i], 1.0f, (std::min)(1.0f, dt * 14.0f));
            if (pipPopScales_[i] < 1.005f) {
                pipPopScales_[i] = 1.0f;
            }
        }

        if (debrisPipFillSprites_[i]) {
            float s = pipPopScales_[i];
            debrisPipFillSprites_[i]->SetBaseSize({(kPipW - 2.0f) * s, (kPipH - 2.0f) * s});

            if (i < currentCount) {
                // 保持中：発光ネオンシアン
                debrisPipFillSprites_[i]->SetColor({0.15f, 0.90f, 1.0f, 0.95f});
            } else {
                // 空スロット：暗色半透明
                debrisPipFillSprites_[i]->SetColor({0.06f, 0.10f, 0.15f, 0.25f});
            }
        }
    }

    if (debrisCountText_) {
        std::wstring countStr = L"(" + std::to_wstring(currentCount) + L"/" + std::to_wstring(kMaxPipCount) + L")";
        debrisCountText_->SetText(countStr);
    }
}

void PlayerStatusHUDComponent::UpdateCriticalPulse(float dt) {
    if (currentHpRatio_ <= 0.25f) {
        criticalPulseTimer_ += dt * 7.0f;
        float pulse = (std::sin(criticalPulseTimer_) * 0.5f + 0.5f); // 0.0 〜 1.0

        if (hpHeaderText_) {
            hpHeaderText_->SetColor({1.0f, 0.25f * pulse, 0.25f * pulse, 0.95f});
            hpHeaderText_->SetText(L"// CRITICAL WARNING");
        }
        if (hpBarBgSprite_) {
            hpBarBgSprite_->SetColor({0.15f * pulse, 0.03f, 0.05f, 0.85f});
        }
    } else {
        criticalPulseTimer_ = 0.0f;
        if (hpHeaderText_) {
            hpHeaderText_->SetColor({0.35f, 0.80f, 1.0f, 0.75f});
            hpHeaderText_->SetText(L"// HULL INTEGRITY");
        }
        if (hpBarBgSprite_) {
            hpBarBgSprite_->SetColor({0.03f, 0.07f, 0.12f, 0.75f});
        }
    }
}
