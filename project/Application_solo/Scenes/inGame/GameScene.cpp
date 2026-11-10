#include "Scenes/inGame/GameScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Irufemi.h"

// ECSコンポーネントのインクルード
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/Scene/SceneSerializer.h"
#include "Combat/ScoreManagerComponent.h"
#include "UI/ScoreHUDComponent.h"
#include "UI/ControlGuideHUDComponent.h"

GameScene::~GameScene() = default;

// 初期化
void GameScene::Initialize(IrufemiEngine* engine) {
    BaseScene::Initialize(engine);

    // スコア管理およびHUDコンポーネントを確実にプロビジョニング
    ScoreManagerComponent::GetOrCreate(this);
    ScoreHUDComponent::GetOrCreate(this);
    ControlGuideHUDComponent::GetOrCreate(this);

    // JSONからのロードは SceneManager が自動で行うため、ここでは手動で呼ばない
}

// 更新
void GameScene::Update() {
    BaseScene::Update(); // これにより GameObject 群の Update が呼ばれる
}

void GameScene::Draw() {
    BaseScene::Draw(); // これにより GameObject 群の Draw が呼ばれる
}

void GameScene::OnSuspend() {
    BaseScene::OnSuspend();
    SetHUDActive(false); // ポーズ中・オーバーレイ表示中は自機レティクル等のHUDを確実に非表示化
}

void GameScene::OnResume() {
    BaseScene::OnResume();
    SetHUDActive(true); // ゲーム復帰時にHUDを再表示
}

void GameScene::SetHUDActive(bool active) {
    // 親キャンバスが存在する場合は一括で切り替え（開放閉鎖原則 OCP）
    if (auto hudCanvas = FindGameObject("HUDCanvas")) {
        hudCanvas->SetActive(active);
        return;
    }
    if (auto hud = FindGameObject("HUD")) {
        hud->SetActive(active);
        return;
    }

    // フォールバック: 個別HUDオブジェクトの制御
    if (auto reticle = FindGameObject("Reticle")) {
        reticle->SetActive(active);
    }
    if (auto lockon = FindGameObject("LockonMarkerUI")) {
        lockon->SetActive(active);
    }
    const char* kScoreHudObjects[] = {
        "HUD_ScoreHeader", "HUD_ScoreText", "HUD_KillsText", "HUD_HitPopText", "HUD_HitPopScore",
        "HUD_ControlGuideContainer"
    };
    for (const char* name : kScoreHudObjects) {
        if (auto obj = FindGameObject(name)) {
            obj->SetActive(active);
        }
    }
}
