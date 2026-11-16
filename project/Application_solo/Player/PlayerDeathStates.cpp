#include "Player/PlayerDeathStates.h"
#include "Player/PlayerDamageVisualizerComponent.h"
#include "Player/PlayerThrusterVisualizerComponent.h"
#include "RailMechanics/RailShooterPlayerComponent.h"
#include "Player/GravityPlayerComponent.h"
#include "Combat/EnemyBulletManagerComponent.h"
#include "Combat/BossBulletManagerComponent.h"
#include "Audio/SoundHelper.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Audio/AudioManager.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Renderer/System/Core/BaseModel.h"
#include "Core/Math/MathFunction.h"
#include "Core/Math/Random/Random.h"
#include <algorithm>
#include <cmath>

#undef min
#undef max

using namespace Irufemi;

// ============================================================================
// 1. PlayerDeathStateMeltdown
// ============================================================================

void PlayerDeathStateMeltdown::Enter(PlayerDamageVisualizerComponent* visualizer) {
    if (!visualizer) {
        return;
    }
    stateTimer_ = 0.0f;

    auto go = visualizer->GetGameObject();
    if (!go) {
        return;
    }

    auto engine = visualizer->GetEngine();
    auto ppm = engine ? engine->GetPostProcessManager() : nullptr;

    // スラスターを直ちに完全消火 ＆ スラスターGameObject破棄
    if (auto thrusterComp = go->GetComponent<PlayerThrusterVisualizerComponent>()) {
        thrusterComp->SetThrusterActive(false);
    }
    for (const auto& child : go->GetChildren()) {
        if (child && child->GetName() == "PlayerThrusterRoot") {
            child->Destroy();
        }
    }

    // 自機コンポーネントを Dying ステートへ移行（入力遮断・姿勢固定）
    if (auto railPlayer = go->GetComponent<RailShooterPlayerComponent>()) {
        railPlayer->SetState(PlayerFlightState::Dying);
    }

    // 周回ガレキ ＆ 画面内敵弾の特異点一括吸引・回収
    if (auto grav = go->GetComponent<GravityPlayerComponent>()) {
        grav->AbsorbOrbitingDebrisToSingularity();
    }
    if (auto bulletMgr = EnemyBulletManagerComponent::GetInstance()) {
        bulletMgr->AbsorbAndClearAllBullets();
    }
    if (auto scene = go->GetScene()) {
        if (auto bossObj = scene->FindGameObject("Boss")) {
            if (auto bossBulletMgr = bossObj->GetComponent<BossBulletManagerComponent>()) {
                bossBulletMgr->ClearAllBullets();
            }
        }
    }

    // HUD（照準・ロックオン）の即時消去
    visualizer->HideInGameHUD();

    // 点滅解除 ＆ 自機を過熱プラズマ（バイオレット〜シアン過熱発光）
    visualizer->SetIsFlashing(false);
    if (Irufemi::BaseModel* model = visualizer->GetTargetModel()) {
        if (!visualizer->IsColorCached()) {
            visualizer->SetOriginalBaseColor(model->GetColor());
            visualizer->SetColorCached(true);
        }
        model->SetColor({0.8f, 1.2f, 3.5f, 1.0f});
    }

    // BGMを急速フェードアウト（0.25秒で完全消音）
    if (engine) {
        if (auto am = engine->GetAudioManager()) {
            am->FadeCategoryVolume(AudioCategory::BGM, 0.0f, 0.25f);
            SoundHelper::Play(GameSE::LaunchCharge);
        }
    }

    // 専用ポストプロセスモード BlackHoleSupernova を PreUI レイヤーに登録 (RAII管理)
    if (ppm) {
        visualizer->SetBlackHoleHandle(
            ppm->ActivateScoped(PostProcessMode::BlackHoleSupernova, PostProcessManager::Layer::PreUI));
        auto& bh = ppm->GetBlackHoleSupernovaParams();
        bh.center = visualizer->GetDeathScreenUV();
        bh.blackHoleRadius = 0.0f;
        bh.blackHolePullIntensity = 0.0f;
        bh.supernovaRadius = 0.0f;
        bh.supernovaIntensity = 0.0f;
        bh.supernovaChromaticRatio = 0.0f;
        bh.supernovaFlashIntensity = 0.0f;

        // 万が一古いフェードが残っていれば初期化
        ppm->RemoveActiveMode(PostProcessMode::Fade);
        ppm->GetFadeParams().intensity = 0.0f;
    }
}

void PlayerDeathStateMeltdown::Update(PlayerDamageVisualizerComponent* visualizer, float dt) {
    if (!visualizer) {
        return;
    }
    stateTimer_ += dt;

    // 【第1幕】0.00s〜0.40s: 装甲の過熱振動と微小Voxel剥離
    constexpr float kDuration = 0.40f;
    float t = std::clamp(stateTimer_ / kDuration, 0.0f, 1.0f);

    auto go = visualizer->GetGameObject();
    if (go) {
        if (auto transform = go->GetComponent<Irufemi::TransformComponent>()) {
            float jitter = (1.0f - t) * 0.08f;
            Irufemi::Vector3 shake{Irufemi::Random::GeneratorFloat(-jitter, jitter),
                                   Irufemi::Random::GeneratorFloat(-jitter, jitter),
                                   Irufemi::Random::GeneratorFloat(-jitter, jitter)};
            transform->SetPosition(visualizer->GetOriginalLocalPos() + shake);
        }
    }

    if (stateTimer_ >= kDuration) {
        visualizer->ChangeDeathState(std::make_unique<PlayerDeathStateBlackHole>());
    }
}

// ============================================================================
// 2. PlayerDeathStateBlackHole
// ============================================================================

void PlayerDeathStateBlackHole::Enter(PlayerDamageVisualizerComponent* /*visualizer*/) {
    stateTimer_ = 0.0f;
}

void PlayerDeathStateBlackHole::Update(PlayerDamageVisualizerComponent* visualizer, float dt) {
    if (!visualizer) {
        return;
    }
    stateTimer_ += dt;

    auto engine = visualizer->GetEngine();
    auto ppm = engine ? engine->GetPostProcessManager() : nullptr;
    if (!ppm) {
        return;
    }

    auto& bh = ppm->GetBlackHoleSupernovaParams();
    bh.center = visualizer->GetDeathScreenUV();

    // 【第2幕】0.35s〜1.15s (0.80秒間): 事象の地平面拡大 ➔ 急速重力収縮（インプロージョン）
    constexpr float kDuration = 0.80f;
    float t = std::clamp(stateTimer_ / kDuration, 0.0f, 1.0f);

    auto go = visualizer->GetGameObject();

    // 前半（0.00s〜0.50s / t: 0.0〜0.625）: ブラックホール拡大
    if (t <= 0.625f) {
        float tExpand = t / 0.625f;
        bh.blackHoleRadius = 0.09f * std::sin(tExpand * Irufemi::Math::PIDiv2);
        bh.blackHolePullIntensity = 0.30f * tExpand;

        if (go) {
            if (auto transform = go->GetComponent<Irufemi::TransformComponent>()) {
                float scaleFactor = std::pow(1.0f - tExpand * 0.8f, 2.0f);
                transform->SetScale(visualizer->GetOriginalScale() * (std::max)(scaleFactor, 0.01f));
            }
        }
    }
    // 後半（0.50s〜0.80s / t: 0.625〜1.0）: 極小特異点へ急激に凝縮（インプロージョン）
    else {
        float tImplode = (t - 0.625f) / 0.375f;
        float implodeCurve = std::pow(1.0f - tImplode, 3.5f);
        bh.blackHoleRadius = 0.006f + (0.09f - 0.006f) * implodeCurve;
        bh.blackHolePullIntensity = 0.30f * implodeCurve;

        if (go) {
            if (auto transform = go->GetComponent<Irufemi::TransformComponent>()) {
                float scaleFactor = (1.0f - tImplode) * 0.2f;
                transform->SetScale(visualizer->GetOriginalScale() * (std::max)(scaleFactor, 0.001f));
            }
        }
    }

    if (stateTimer_ >= kDuration) {
        visualizer->ChangeDeathState(std::make_unique<PlayerDeathStateVacuumFreeze>());
    }
}

// ============================================================================
// 3. PlayerDeathStateVacuumFreeze
// ============================================================================

void PlayerDeathStateVacuumFreeze::Enter(PlayerDamageVisualizerComponent* visualizer) {
    stateTimer_ = 0.0f;
    if (!visualizer) {
        return;
    }

    // 極小スケールで完全固定（完全な静寂・真空）
    auto go = visualizer->GetGameObject();
    if (go) {
        if (auto transform = go->GetComponent<Irufemi::TransformComponent>()) {
            transform->SetScale(visualizer->GetOriginalScale() * 0.01f);
        }
    }
}

void PlayerDeathStateVacuumFreeze::Update(PlayerDamageVisualizerComponent* visualizer, float dt) {
    if (!visualizer) {
        return;
    }
    stateTimer_ += dt;

    auto engine = visualizer->GetEngine();
    auto ppm = engine ? engine->GetPostProcessManager() : nullptr;
    if (ppm) {
        auto& bh = ppm->GetBlackHoleSupernovaParams();
        bh.center = visualizer->GetDeathScreenUV();
        bh.blackHoleRadius = 0.006f;
        bh.blackHolePullIntensity = 0.0f;
    }

    // 【幕間】0.15秒間: 極小特異点での完全な静寂・タメ
    constexpr float kDuration = 0.15f;
    if (stateTimer_ >= kDuration) {
        visualizer->ChangeDeathState(std::make_unique<PlayerDeathStateSupernovaBurst>());
    }
}

// ============================================================================
// 4. PlayerDeathStateSupernovaBurst
// ============================================================================

void PlayerDeathStateSupernovaBurst::Enter(PlayerDamageVisualizerComponent* visualizer) {
    stateTimer_ = 0.0f;
    if (visualizer) {
        visualizer->TriggerSupernovaExplosion();
    }
}

void PlayerDeathStateSupernovaBurst::Update(PlayerDamageVisualizerComponent* visualizer, float dt) {
    if (!visualizer) {
        return;
    }
    stateTimer_ += dt;

    auto engine = visualizer->GetEngine();
    auto ppm = engine ? engine->GetPostProcessManager() : nullptr;
    if (!ppm) {
        return;
    }

    auto& bh = ppm->GetBlackHoleSupernovaParams();
    bh.center = visualizer->GetDeathScreenUV();

    // 【第3幕】0.75秒間: 臨界突破！スーパーノヴァ大爆発
    constexpr float kDuration = 0.75f;
    float t = std::clamp(stateTimer_ / kDuration, 0.0f, 1.0f);

    bh.blackHoleRadius = 0.018f * (1.0f - t * 0.2f);
    bh.blackHolePullIntensity = 0.04f;

    // ホワイトフラッシュ
    bh.supernovaFlashIntensity = (t < 0.10f) ? (1.0f - t / 0.10f) : 0.0f;
    float popInCurve = (t < 0.08f) ? (t / 0.08f) : 1.0f;

    bh.supernovaRadius = t * 2.8f;
    bh.supernovaIntensity = 1.8f * popInCurve * (1.0f - t * 0.20f);
    bh.supernovaChromaticRatio = 0.12f * popInCurve * (1.0f - t * 0.40f);

    if (stateTimer_ >= kDuration) {
        visualizer->ChangeDeathState(std::make_unique<PlayerDeathStatePlasmaAfterglow>());
    }
}

// ============================================================================
// 5. PlayerDeathStatePlasmaAfterglow
// ============================================================================

void PlayerDeathStatePlasmaAfterglow::Enter(PlayerDamageVisualizerComponent* /*visualizer*/) {
    stateTimer_ = 0.0f;
}

void PlayerDeathStatePlasmaAfterglow::Update(PlayerDamageVisualizerComponent* visualizer, float dt) {
    if (!visualizer) {
        return;
    }
    stateTimer_ += dt;

    auto engine = visualizer->GetEngine();
    auto ppm = engine ? engine->GetPostProcessManager() : nullptr;
    if (!ppm) {
        return;
    }

    auto& bh = ppm->GetBlackHoleSupernovaParams();
    bh.center = visualizer->GetDeathScreenUV();

    // 【第4幕】0.80秒間: 白熱流体が画面を飲み込みホワイトアウトへ加速
    constexpr float kDuration = 0.80f;
    float t = std::clamp(stateTimer_ / kDuration, 0.0f, 1.0f);

    bh.supernovaRadius = 2.8f + t * 2.7f;
    bh.supernovaIntensity = 1.8f;
    bh.supernovaChromaticRatio = 0.08f * (1.0f - t * 0.5f);
    bh.blackHoleRadius = 0.014f * (1.0f - t);
    bh.supernovaFlashIntensity = 0.0f;

    ppm->AddActiveMode(PostProcessMode::Fade, PostProcessManager::Layer::PreUI);
    ppm->GetFadeParams().color = {1.0f, 1.0f, 1.0f, 1.0f};
    ppm->GetFadeParams().intensity = t;

    if (stateTimer_ >= kDuration) {
        visualizer->ChangeDeathState(std::make_unique<PlayerDeathStateWhiteoutFade>());
    }
}

// ============================================================================
// 6. PlayerDeathStateWhiteoutFade
// ============================================================================

void PlayerDeathStateWhiteoutFade::Enter(PlayerDamageVisualizerComponent* visualizer) {
    stateTimer_ = 0.0f;
    if (!visualizer) {
        return;
    }

    auto engine = visualizer->GetEngine();
    auto ppm = engine ? engine->GetPostProcessManager() : nullptr;
    if (ppm) {
        visualizer->SetDeathFadeHandle(ppm->ActivateScoped(PostProcessMode::Fade, PostProcessManager::Layer::PreUI));
        ppm->GetFadeParams().color = {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

void PlayerDeathStateWhiteoutFade::Update(PlayerDamageVisualizerComponent* visualizer, float dt) {
    if (!visualizer) {
        return;
    }
    stateTimer_ += dt;

    auto engine = visualizer->GetEngine();
    auto ppm = engine ? engine->GetPostProcessManager() : nullptr;
    if (ppm) {
        ppm->AddActiveMode(PostProcessMode::Fade, PostProcessManager::Layer::PreUI);
        ppm->GetFadeParams().color = {1.0f, 1.0f, 1.0f, 1.0f};
        ppm->GetFadeParams().intensity = 1.0f;

        auto& bh = ppm->GetBlackHoleSupernovaParams();
        bh.center = visualizer->GetDeathScreenUV();
        bh.supernovaIntensity = 0.0f;
        bh.blackHoleRadius = 0.0f;
        bh.supernovaFlashIntensity = 0.0f;
    }

    constexpr float kDuration = 0.30f;
    if (stateTimer_ >= kDuration) {
        visualizer->ChangeDeathState(std::make_unique<PlayerDeathStateCompleted>());
    }
}

// ============================================================================
// 7. PlayerDeathStateCompleted
// ============================================================================

void PlayerDeathStateCompleted::Enter(PlayerDamageVisualizerComponent* visualizer) {
    if (!visualizer) {
        return;
    }

    auto engine = visualizer->GetEngine();
    auto ppm = engine ? engine->GetPostProcessManager() : nullptr;
    if (ppm) {
        ppm->GetFadeParams().intensity = 1.0f;
        visualizer->ReleaseBlackHoleHandle();
    }

    visualizer->InvokeDeathCompletedCallback();
}

void PlayerDeathStateCompleted::Update(PlayerDamageVisualizerComponent* /*visualizer*/, float /*dt*/) {
    // 完了後は静止
}
