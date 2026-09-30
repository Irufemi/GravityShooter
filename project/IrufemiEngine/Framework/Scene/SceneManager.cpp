#include "Framework/Scene/SceneManager.h"
#include "Audio/AudioManager.h"
#include "Renderer/DrawManager.h"
#include "Framework/Scene/IScene.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/System/IrufemiEngine.h"
#include <Windows.h>
#include "Platform/Input/InputManager.h"
#include "Platform/Input/Mouse.h"
#include "Framework/Scene/SceneSerializer.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"
#include "Renderer/System/ParticleGPU/GPUParticleManager.h"
#include "Physics/CollisionManager.h"
#include "Renderer/Camera/CameraManager.h"
#include "Core/Utility/Log.h"
#include <iostream>

namespace {
/**
 * @class DataDrivenScene
 * @brief JSONファイルのみから構築される汎用シーン
 */
class DataDrivenScene : public BaseScene {
public:
    DataDrivenScene(const std::string& sceneFilePath) : sceneFilePath_(sceneFilePath) {}
    virtual ~DataDrivenScene() = default;

    void Initialize(IrufemiEngine* engine) override {
        BaseScene::Initialize(engine);
        SceneSerializer::Load(this, sceneFilePath_);
    }

private:
    std::string sceneFilePath_;
};
} // namespace

SceneManager::SceneManager(IrufemiEngine* engine) : engine_(engine) {}

SceneManager::~SceneManager() {
    if (initFuture_.valid()) {
        initFuture_.wait();
    }
}

// 登録順を保持しつつ登録
void SceneManager::Register(const Key& name, Factory f) {
    auto it = factories_.find(name);
    if (it == factories_.end()) {
        order_.push_back(name); // 新規登録時のみ順序リストに追加
        factories_.emplace(name, std::move(f));
    } else {
        it->second = std::move(f); // 既存は上書きのみ(順序は変えない)
    }
}

// シーン切替要求(次の Update 冒頭で反映)
void SceneManager::Request(const Key& next) {
    pending_ = next;
}

// 即時切替(初期化時など)
bool SceneManager::ChangeTo(const Key& next) {
    auto it = factories_.find(next);
    if (it == factories_.end()) {
        return false;
    }

    // シーン破棄前にGPU処理の完了を待つ (リソース解放中のアクセス違反を防ぐ)
    engine_->GetDirectXCommon()->WaitForGPU();

    // シーン破棄前に、現在のスタックの全シーンを終了させる
    for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it) {
        it->scene->OnExit();
        it->scene->Finalize();
    }
    sceneStack_.clear();

    // 次のシーンへ行く前にパーティクルの状態をクリア
    if (engine_->GetVoxelParticleManager()) {
        engine_->GetVoxelParticleManager()->Clear();
    }

    // シーン切り替え時にポストプロセスの状態とパラメータを自動リセット
    if (engine_->GetPostProcessManager()) {
        engine_->GetPostProcessManager()->Reset();
    }

    // シーン切り替え時にコリジョンマネージャーの残留コライダーと衝突履歴を安全にクリア
    if (engine_->GetCollisionManager()) {
        engine_->GetCollisionManager()->Clear();
    }

    // シーン切り替え時にカメラマネージャーの残留カメラをクリア
    if (engine_->GetCameraManager()) {
        engine_->GetCameraManager()->Clear();
    }

    // シーン切り替え時にプレハブキャッシュをクリア（古いシーンのプレハブ残留によるメモリリーク防止）
    SceneSerializer::ClearCache();

    SceneStackItem item;
    item.name = next;
    item.scene = it->second();

    // シーンにエンジンコンテキストを確実に先行バインド
    isInitializing_ = true;
    item.scene->Initialize(engine_);
    isInitializing_ = false;

    // データがあればロード
    bool hasData = SceneSerializer::Load(item.scene.get(), next);

    // データがなくてエディタモードなら、初期状態を自動生成
#ifdef EditorMode
    if (!hasData) {
        SceneSerializer::Save(item.scene.get(), next);
    }
#endif

    item.scene->OnEnter();           // シーン開始
    item.scene->WarmUpRenderState(); // 初回描画ステートのウォームアップ

    sceneStack_.push_back(std::move(item));
    wasLoading_ = false;

    engine_->SetTimeScale(1.0f); // 時間の進みもリセット
    engine_->SetCursorLocked(!sceneStack_.back().scene->IsCursorVisible());
    return true;
}

void SceneManager::PushScene(const Key& name) {
    auto it = factories_.find(name);
    if (it == factories_.end()) {
        return;
    }

    // 重ねるだけのシーン遷移では既存リソースを破棄しないため、GPUの完了待機は不要（FPS低下・スパイクの原因となるため削除）
    // engine_->GetDirectXCommon()->WaitForGPU();

    if (!sceneStack_.empty()) {
        // 現在の最前面シーンをサスペンド状態にする（バックグラウンドへ）
        sceneStack_.back().scene->OnSuspend();
    }

    SceneStackItem item;
    item.name = name;
    item.scene = it->second();

    // ★上に重なるシーンがAudioをブロックする場合、SEだけをポーズする（BGMとUIはそのまま）
    if (!sceneStack_.empty() && item.scene->IsAudioBlocking()) {
        engine_->GetAudioManager()->PauseCategory(AudioCategory::SE);
    }

    // シーンにエンジンコンテキストを確実に先行バインド
    item.scene->Initialize(engine_);

    // データがあればロード
    bool hasData = SceneSerializer::Load(item.scene.get(), name);

    // データがなくてエディタモードなら、初期状態を自動生成
#ifdef EditorMode
    if (!hasData) {
        SceneSerializer::Save(item.scene.get(), name);
    }
#endif

    item.scene->OnEnter();           // 新しいシーン開始
    item.scene->WarmUpRenderState(); // 初回描画ステートのウォームアップ

    sceneStack_.push_back(std::move(item));

    engine_->SetCursorLocked(!sceneStack_.back().scene->IsCursorVisible());
}

void SceneManager::PopScene() {
    if (sceneStack_.empty()) {
        return;
    }

    engine_->GetDirectXCommon()->WaitForGPU();

    // 最前面のシーンの終了処理
    sceneStack_.back().scene->OnExit();
    sceneStack_.back().scene->Finalize();
    sceneStack_.pop_back();

    // パーティクルの状態をクリア（Pop前のシーンから残ったパーティクルを消去）
    if (engine_->GetVoxelParticleManager()) {
        engine_->GetVoxelParticleManager()->Clear();
    }
    if (engine_->GetGPUParticleManager()) {
        engine_->GetGPUParticleManager()->ClearAllParticles();
    }

    if (!sceneStack_.empty()) {
        // 次のシーンが最前面に復帰するためレジューム処理を行う
        sceneStack_.back().scene->OnResume();
        // ★ポーズしていたSEカテゴリを再開する
        engine_->GetAudioManager()->ResumeCategory(AudioCategory::SE);
        engine_->SetCursorLocked(!sceneStack_.back().scene->IsCursorVisible());
    } else {
        engine_->SetCursorLocked(false);
    }
}

void SceneManager::TransitionTo(const Key& next, SceneTransition::Type type, float duration, EaseType easeType) {
    if (transitionPhase_ != TransitionPhase::None) {
        return; // 二重遷移防止
    }

    pendingTransition_ = next;
    pendingType_ = type;
    pendingDuration_ = duration;
    pendingEaseType_ = easeType;
    transitionPhase_ = TransitionPhase::Closing;

    engine_->GetSceneTransition()->Start(type, duration, true, easeType);
}

void SceneManager::LoadScene(const std::string& sceneJsonName, SceneTransition::Type type, float duration,
                             EaseType easeType) {
    // 未登録の場合、DataDrivenScene を生成するファクトリを自動登録
    if (factories_.find(sceneJsonName) == factories_.end()) {
        Register(sceneJsonName, [sceneJsonName]() { return std::make_unique<DataDrivenScene>(sceneJsonName); });
    }

    // 通常の遷移処理に回す
    TransitionTo(sceneJsonName, type, duration, easeType);
}

bool SceneManager::UpdateLoadStatus() {
    bool isLoading = IsLoading();

    if (isLoading) {
        // ロード画面のアニメーションなどのための時間加算は実時間で行う
        loadingTimer_ += engine_->GetRealDeltaTime();
    } else {
        loadingTimer_ = 0.0f;
    }

    if (isLoading != wasLoading_) {
        if (isLoading) {
            // ロード中はマウスカーソルを表示させる
            engine_->SetCursorLocked(false);
        } else {
            // ロード完了時に、現在のシーン本来の設定に戻す
            if (!sceneStack_.empty()) {
                engine_->SetCursorLocked(!sceneStack_.back().scene->IsCursorVisible());
            }
        }
        wasLoading_ = isLoading;
    }
    return isLoading;
}

void SceneManager::ProcessImmediateTransition() {
    if (!pending_.empty()) {
        pendingTransition_ = pending_;
        StartAsyncInitialize(pending_); // 即時切替の場合も非同期初期化を開始する
        transitionPhase_ = TransitionPhase::Initializing;
        pending_.clear();
    }
}

void SceneManager::ProcessTransitionPhase(bool& isLoading) {
    if (transitionPhase_ == TransitionPhase::Closing) {
        // フェードアウト完了待ち
        if (engine_->GetSceneTransition()->IsOutFinished()) {
            // 新しいシーンに非同期で切り替え、裏ロード開始
            StartAsyncInitialize(pendingTransition_);
            transitionPhase_ = TransitionPhase::Initializing; // 初期化フェーズへ
        }
    } else if (transitionPhase_ == TransitionPhase::Initializing) {
        // バックグラウンドでのシーン破棄・初期化完了待ち
        if (initFuture_.valid() && initFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                initFuture_.get(); // ワーカースレッドの完了待機および例外の再スロー
            } catch (const std::exception& e) {
                Log::OutPutLog(std::cerr,
                               std::string("[SceneManager] Scene async initialization failed: ") + e.what() + "\n");
            } catch (...) {
                Log::OutPutLog(std::cerr, "[SceneManager] Scene async initialization failed with unknown exception\n");
            }

            {
                std::lock_guard<std::mutex> lock(nextSceneMutex_);
                if (nextScene_) {
                    SceneStackItem item;
                    item.name = pendingTransition_;
                    item.scene = std::move(nextScene_);
                    // ここではまだ呼ばない（ロード完了後に呼ぶ）
                    sceneStack_.push_back(std::move(item));
                }
            }

            pendingTransition_.clear(); // ロード完了後にクリアする
            isInitializing_ = false;
            isAsyncInitializing_.store(false);

            // ポーズ可能なシーンかどうかに応じてマウスをロック
            if (!sceneStack_.empty()) {
                engine_->SetCursorLocked(!sceneStack_.back().scene->IsCursorVisible());
            }

            transitionPhase_ = TransitionPhase::LoadingWait; // ロード待機フェーズへ
        }
    } else if (transitionPhase_ == TransitionPhase::LoadingWait) {
        // 全てのアセットのロード完了を待つ (画面は真っ暗なまま、上にLoadingScreenが描画される)
        if (!engine_->IsAssetLoading()) {
            transitionPhase_ = TransitionPhase::Opening;

            // ---------------------------------------------------------
            // 【重要】フェードイン中 (Opening) は Update() がスキップされるため、
            // そのまま Draw() が呼ばれると、未初期化の定数バッファ（ゼロ行列）や
            // 前シーンのカメラデータが使われてしまい、深刻な点滅・描画崩れが発生する。
            // これを防ぐため、ゲームロジック（Update）は動かさずに、描画ステート・
            // GPUスキニング予約・初回フレームデータ提出（WarmUpRenderState）のみを安全に済ませる。
            // ---------------------------------------------------------
            if (!sceneStack_.empty()) {
                sceneStack_.back().scene->OnEnter(); // ロード完了直後にBGM再生等を開始
                sceneStack_.back().scene->WarmUpRenderState();
            }

            // ロードが完了した瞬間に、フェードインを開始する
            engine_->GetSceneTransition()->Start(pendingType_, pendingDuration_, false, pendingEaseType_);
        }
    } else if (transitionPhase_ == TransitionPhase::Opening) {
        // フェードイン完了待ち
        if (engine_->GetSceneTransition()->IsFinished()) {
            transitionPhase_ = TransitionPhase::None;
        }
    }

    // ロード中かどうかを最新の状態で再評価する（非同期初期化が開始された直後を考慮）
    isLoading = IsLoading();
}

void SceneManager::UpdateActiveScenes() {
    if (!sceneStack_.empty()) {
        // ※フェードイン中 (Opening) は Update を呼ばないよう制限
        if (transitionPhase_ != TransitionPhase::Opening) {
            // 上層（末尾）から順に、UpdateBlocking が true のシーンを見つける
            int updateStartIndex = static_cast<int>(sceneStack_.size()) - 1;
            for (int i = static_cast<int>(sceneStack_.size()) - 1; i >= 0; --i) {
                updateStartIndex = i;
                if (sceneStack_[i].scene->IsUpdateBlocking()) {
                    break;
                }
            }
            // 見つけたシーンから上層へ順番に Update を実行
            size_t initialSize = sceneStack_.size();
            for (int i = updateStartIndex; i < static_cast<int>(sceneStack_.size()); ++i) {
                sceneStack_[i].scene->Update();
                // Update中にPush/Popが行われた場合は、誤作動（同じ入力での即座の反応等）を防ぐためループを抜ける
                if (sceneStack_.size() != initialSize) {
                    break;
                }
            }
        }
    }
}

void SceneManager::Update() {
    // 1. ロード状況の確認とカーソル制御
    bool isLoading = UpdateLoadStatus();

#if defined(_DEBUG) || defined(EditorMode) || defined(DEVELOPMENT)
    // デバッグ用の即時リロード機能 (一線級エンジンのConsole Command / CheatManagerに相当)
    if (engine_->GetInputManager()->IsKeyPressed('R')) {
        const Key& currentScene = GetCurrent();
        // ロード中や遷移中でなければリロード要求を発行
        if (!currentScene.empty() && !isLoading && pending_.empty() && transitionPhase_ == TransitionPhase::None) {
            Request(currentScene);
        }
    }
#endif

    // 2. 即時シーン切り替え要求の処理
    ProcessImmediateTransition();

    // 3. 遷移プロセスの更新
    ProcessTransitionPhase(isLoading);

    // ロード中であれば、ここで更新を止めてLoadingUIだけアニメーションさせる
    if (isLoading) {
        return;
    }

    // 4. ロード完了時のみ、シーン自体のUpdateを回す
    UpdateActiveScenes();
}

void SceneManager::Draw() {
    // ロード中（非同期初期化やアセット読み込み中）は新シーンの Update がまだ呼ばれておらず、
    // 未初期化の行列で描画されてしまう（しろ飛び等の原因になる）ため、背景の描画をスキップする。
    // （画面はクリアカラーで塗りつぶされ、維持されたトランジションエフェクトで覆われる）
    if (IsLoading()) {
        return;
    }

    if (!sceneStack_.empty()) {
        // 上層（末尾）から順に、DrawBlocking が true のシーンを見つける
        int drawStartIndex = static_cast<int>(sceneStack_.size()) - 1;
        for (int i = static_cast<int>(sceneStack_.size()) - 1; i >= 0; --i) {
            drawStartIndex = i;
            if (sceneStack_[i].scene->IsDrawBlocking()) {
                break;
            }
        }
        // 見つけたシーンから上層へ順番に Draw を実行
        for (int i = drawStartIndex; i < static_cast<int>(sceneStack_.size()); ++i) {
            sceneStack_[i].scene->Draw();
        }
    }
}

IScene* SceneManager::GetCurrentScene() const {
    return sceneStack_.empty() ? nullptr : sceneStack_.back().scene.get();
}

const SceneManager::Key& SceneManager::GetCurrent() const {
    static const Key emptyKey = "";
    return sceneStack_.empty() ? emptyKey : sceneStack_.back().name;
}

// 並び順は登録順
std::vector<SceneManager::Key> SceneManager::GetRegisteredKeys() const {
    return order_;
}

bool SceneManager::IsLoading() const {
    // シーン遷移フェーズ中（非同期初期化中またはアセット完了待機中）のみロード状態とみなす
    // ゲームプレイ進行中（Opening / None）にバックグラウンドタスク等で誤ってロード画面が割り込むのを防止
    if (transitionPhase_ == TransitionPhase::Initializing) {
        return true;
    }
    if (transitionPhase_ == TransitionPhase::LoadingWait) {
        return engine_->IsAssetLoading();
    }
    return false;
}

bool SceneManager::ShouldDrawLoadingScreen() const {
    // チラつき防止：ロード状態になってから0.1秒以上経過した場合のみ描画する
    return IsLoading() && (loadingTimer_ >= 0.1f);
}

void SceneManager::StartAsyncInitialize(const Key& next) {
    auto it = factories_.find(next);
    if (it == factories_.end()) {
        return;
    }

    Factory factory = it->second;
    isAsyncInitializing_.store(true);
    isInitializing_ = true;
    wasLoading_ = false;
    engine_->SetTimeScale(1.0f); // 時間の進みをリセット

    // --- 【重要】---
    // シーン破棄前に、現在溜まっている描画・コンピュートタスクを破棄する。
    engine_->GetDrawManager()->ClearAllQueues();

    // GPU処理の完了を待つ (リソース解放中のアクセス違反を防ぐ)
    engine_->GetDirectXCommon()->WaitForGPU();

    // 現在のシーンスタックの全シーンを終了させて破棄（メインスレッドで実行）
    for (auto it = sceneStack_.rbegin(); it != sceneStack_.rend(); ++it) {
        it->scene->OnExit();
        it->scene->Finalize();
    }
    sceneStack_.clear();

    // 次のシーンへ行く前にパーティクルの状態をクリア
    if (engine_->GetVoxelParticleManager()) {
        engine_->GetVoxelParticleManager()->Clear();
    }
    if (engine_->GetGPUParticleManager()) {
        engine_->GetGPUParticleManager()->ClearAllParticles();
    }

    // シーン切り替え時にポストプロセスの状態とパラメータを自動リセット
    if (engine_->GetPostProcessManager()) {
        engine_->GetPostProcessManager()->Reset(false);
    }

    // シーン切り替え時にコリジョンマネージャーの残留コライダーと衝突履歴を安全にクリア
    if (engine_->GetCollisionManager()) {
        engine_->GetCollisionManager()->Clear();
    }

    // シーン切り替え時にカメラマネージャーの残留カメラをクリア
    if (engine_->GetCameraManager()) {
        engine_->GetCameraManager()->Clear();
    }

    // シーン切り替え時にプレハブキャッシュをクリア（古いシーンのプレハブ残留によるメモリリーク防止）
    SceneSerializer::ClearCache();

    initFuture_ = std::async(std::launch::async, [this, factory, next]() {
        // 新しいシーンを生成
        auto newScene = factory();

        // 初期化（裏ロード: シーンにエンジンコンテキストを先行バインド）
        newScene->Initialize(engine_);

        // データがあればロード
        bool hasData = SceneSerializer::Load(newScene.get(), next);

        // データがなくてエディタモードなら、初期状態を自動生成
#ifdef EditorMode
        if (!hasData) {
            SceneSerializer::Save(newScene.get(), next);
        }
#endif

        // メインスレッドへ渡すための準備
        {
            std::lock_guard<std::mutex> lock(nextSceneMutex_);
            nextScene_ = std::move(newScene);
        }

        isAsyncInitializing_.store(false);
    });
}