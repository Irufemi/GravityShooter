#include "Scenes/inGame/GameScene.h"
#include "Framework/Scene/SceneManager.h"
#include "Irufemi.h"

// ECSコンポーネントのインクルード
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/Scene/SceneSerializer.h"
#include "Combat/ScoreManagerComponent.h"
#include "Combat/EnemyBulletManagerComponent.h"
#include "UI/ScoreHUDComponent.h"
#include "UI/ControlGuideHUDComponent.h"
#include "Renderer/Font/FontManager.h"
#include "Core/System/IrufemiEngine.h"

GameScene::~GameScene() = default;

// 初期化
void GameScene::Initialize(IrufemiEngine* engine) {
    Irufemi::BaseScene::Initialize(engine);

    // スコア管理、敵弾プール管理、およびHUDコンポーネントを確実にプロビジョニング
    ScoreManagerComponent::GetOrCreate(this);
    EnemyBulletManagerComponent::GetOrCreate(this);
    ScoreHUDComponent::GetOrCreate(this);
    ControlGuideHUDComponent::GetOrCreate(this);

    // インゲーム・HUD・リザルトで使われる全フォントグリフをロード画面中に同期先行ウォームアップ
    // （初出現文字によるMSDF生成＆GPUアトラス転送のヒッチを完全根絶）
    if (engine) {
        if (auto fm = engine->GetFontManager()) {
            const std::string hudFontId = "NotoSansJP-Bold";
            const std::string resultFontId = "toro_glitch";

            // 1. 操作ガイドHUD（日本語・記号）- NotoSansJP
            fm->PrecacheTextSync(hudFontId, L"// FCS CONTROLS: 操作システム");
            fm->PrecacheTextSync(hudFontId,
                                 L"[E / LT] ガレキ引寄    [右クリック / RB] ロックオン    [左クリック / RT] 一斉射撃");
            fm->PrecacheTextSync(hudFontId,
                                 L"[R / B]  ロック解除    [WASD / Lスティック] 機体移動    [ESC / START] ポーズ");

            // 2. スコアHUD & ポップアップ & プレイヤーHUD - NotoSansJP
            fm->PrecacheTextSync(hudFontId, L"// DATA: SCORE 0123456789 KILLS: +pts CHAIN x HIT!!");
            fm->PrecacheTextSync(hudFontId, L"// HULL INTEGRITY DEBRIS % (0123456789/10)");

            // 3. リザルト画面 & ゲームオーバー - toro_glitch
            fm->PrecacheTextSync(resultFontId,
                                 L"STAGE CLEAR GAME OVER Press SPACE to Return MISSION RESULT FINAL EVALUATION");
            fm->PrecacheTextSync(resultFontId, L"RANK: S A B C D [SYS: ONLINE]");
            fm->PrecacheTextSync(
                resultFontId,
                L"[01] TOTAL SCORE [02] ENEMIES DESTROYED [03] DEBRIS SHATTERED [04] MAX COMBO [05] CLEAR TIME");
            fm->PrecacheTextSync(resultFontId, L"0123456789:.,/%+-");
        }
    }

    // JSONからのロードは SceneManager が自動で行うため、ここでは手動で呼ばない
}

// 更新
void GameScene::Update() {
    Irufemi::BaseScene::Update(); // これにより GameObject 群の Update が呼ばれる
}

void GameScene::Draw() {
    Irufemi::BaseScene::Draw(); // これにより GameObject 群の Draw が呼ばれる
}

void GameScene::OnSuspend() {
    Irufemi::BaseScene::OnSuspend();
    SetHUDActive(false); // ポーズ中・オーバーレイ表示中は自機レティクル等のHUDを確実に非表示化
}

void GameScene::OnResume() {
    Irufemi::BaseScene::OnResume();
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
    const char* kScoreHudObjects[] = {"HUD_ScoreHeader", "HUD_ScoreText",   "HUD_KillsText",
                                      "HUD_HitPopText",  "HUD_HitPopScore", "HUD_ControlGuideContainer"};
    for (const char* name : kScoreHudObjects) {
        if (auto obj = FindGameObject(name)) {
            obj->SetActive(active);
        }
    }
}
