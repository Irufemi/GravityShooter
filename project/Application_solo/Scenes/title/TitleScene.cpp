#include "Scenes/title/TitleScene.h"

#include "Framework/Scene/SceneManager.h"
#include "Framework/Scene/SceneSerializer.h"
#include "Irufemi.h"

#include "Renderer/Font/FontManager.h"
#include "Platform/Input/InputManager.h"
#include "Framework/GameObject/GameObject.h"
#include "Scenes/title/TitleMenuControllerComponent.h"
#include "Audio/SoundHelper.h"

TitleScene::~TitleScene() {}

// 初期化
void TitleScene::Initialize(IrufemiEngine* engine) {
    Irufemi::BaseScene::Initialize(engine);

    // モーダル画面（HowToPlayScene, OptionsScene）およびタイトルメニューで使用される全英数字・記号を先行ウォームアップ
    // 画面遷移時のFreeType走査・GPUアップロードによるメインスレッド阻害（フレームヒッチ）を根絶
    if (engine) {
        if (auto fm = engine->GetFontManager()) {
            fm->PrecacheText("toro_glitch",
                             L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789/-%:.,_ ()[]<>+!?");
            fm->PrecacheTextSync("NotoSansJP-Bold",
                                 L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789/-%:.,_ ()[]<>+!?");
        }
        // 出撃演出で使用するSEの事前ウォームアップ（SoundHelperで安全に事前ロード）
        SoundHelper::PreloadAll();
    }

    // JSONからのロードは SceneManager が自動で行うため、ここでは手動で呼ばない
}

// 更新
void TitleScene::Update() {
    Irufemi::BaseScene::Update();
}

void TitleScene::Draw() {
    Irufemi::BaseScene::Draw();
}

void TitleScene::OnSuspend() {
    Irufemi::BaseScene::OnSuspend();

    // 他のシーン（HowToPlayやOptions）が重なった時は、タイトル文字被りを防ぐためメニューUIを非表示化
    if (auto menuMgr = FindGameObject("MenuManager")) {
        if (auto ctrl = menuMgr->GetComponent<TitleMenuControllerComponent>()) {
            ctrl->SetMenuVisible(false);
        }
    }
}

void TitleScene::OnResume() {
    Irufemi::BaseScene::OnResume();

    // スタックから復帰した時はメニューUIを再表示し、カーソル位置を現在フォーカスボタンに同期
    if (auto menuMgr = FindGameObject("MenuManager")) {
        if (auto ctrl = menuMgr->GetComponent<TitleMenuControllerComponent>()) {
            ctrl->SetMenuVisible(true);
            ctrl->RestoreFocusOnResume();
        }
    }
}
