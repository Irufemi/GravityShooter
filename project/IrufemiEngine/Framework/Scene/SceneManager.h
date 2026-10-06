#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <future>
#include <mutex>
#include <atomic>
#include "Framework/Scene/SceneTransition.h"

class IrufemiEngine;
class IScene;

/**
 * @class SceneManager
 * @brief ゲームのシーン遷移とライフサイクルを管理するクラス
 * @details IScene を継承したシーンクラスの登録、切替、更新、描画を一括して行います。
 *          シーンのスタック管理ではなく、単一の現在シーンを保持する形式です。
 *          また、ゲーム全体の一時停止（ポーズ）フラグも保持します。
 */
class SceneManager {
public:
    /** @brief シーン名（識別子）の型定義 */
    using Key = std::string;
    /** @brief シーン生成関数の型定義 */
    using Factory = std::function<std::unique_ptr<IScene>()>;

    /** @brief 遷移フェーズ */
    enum class TransitionPhase {
        None,         ///< 通常時
        Closing,      ///< フェードアウト中（現在のシーンをUpdateし続ける）
        Initializing, ///< 別スレッドでシーン破棄・生成中（LoadingScreenを描画）
        LoadingWait,  ///< ロード完了待機中（画面が暗転したままLoadingScreenのみ動く）
        Opening       ///< フェードイン中（新しいシーンをUpdateしない）
    };

    /**
     * @brief コンストラクタ
     * @param[in] engine IrufemiEngine へのポインタ（非所有）
     */
    explicit SceneManager(IrufemiEngine* engine);
    ~SceneManager();

    /** @name シーン登録・遷移 */
    ///@{
    /**
     * @brief シーンをファクトリ関数と共に登録する
     * @param[in] name シーンの識別名
     * @param[in] f シーンインスタンスを作成するラムダ式等の関数
     */
    void Register(const Key& name, Factory f);

    /**
     * @brief シーンの切替を要求する（次の Update 冒頭で反映）
     * @param[in] next 切り替え先のシーン名
     */
    void Request(const Key& next);

    /**
     * @brief 即時シーンを切り替える（初期化時などに使用）
     * @param[in] next 切り替え先のシーン名
     * @return 切り替えに成功したら true
     */
    bool ChangeTo(const Key& next);

    /**
     * @brief 演出を伴うシーン切替を開始する（スタックはクリアされます）
     * @param[in] next 次のシーン名
     * @param[in] type 演出タイプ
     * @param[in] duration 演出時間（秒）
     * @param[in] easeType 演出のイージングタイプ（デフォルトは線形）
     */
    void TransitionTo(const Key& next, SceneTransition::Type type, float duration,
                      EaseType easeType = EaseType::Linear);

    /**
     * @brief データ駆動シーン(JSON)への遷移を開始する
     * @param[in] sceneJsonName 読み込むJSON名 (拡張子なし)
     */
    void LoadScene(const std::string& sceneJsonName, SceneTransition::Type type = SceneTransition::Type::Fade,
                   float duration = 1.0f, EaseType easeType = EaseType::Linear);

    /**
     * @brief 現在のシーンの上に新しいシーンを重ねる（同期）
     * @param[in] name 重ねるシーン名
     */
    void PushScene(const Key& name);

    /**
     * @brief 一番上のシーンを破棄し、下のシーンに戻る
     */
    void PopScene();
    ///@}

    /** @name 更新・描画 */
    ///@{
    /**
     * @brief 現在のシーンの更新処理
     * @details シーン切替要求がある場合は、更新の前にシーンの差し替えを行います。
     */
    void Update();

    /**
     * @brief 現在のシーンの描画処理
     */
    void Draw();
    ///@}

    /** @name 状態取得 */
    ///@{
    /** @brief 現在のシーン名を取得 */
    const Key& GetCurrent() const;
    /** @brief 現在一番上のシーンインスタンスを取得 */
    IScene* GetCurrentScene() const;

    /**
     * @brief 一つ下のシーン名（スタックの末尾から2番目）を取得
     * @details PauseSceneなどの上に重なるシーンから、呼び出し元のシーンを判定するために使用します。
     * @return std::string シーンの識別名。スタックが2つ未満の場合は空文字を返します。
     */
    std::string GetPreviousSceneName() const {
        if (sceneStack_.size() >= 2) {
            return sceneStack_[sceneStack_.size() - 2].name;
        }
        return "";
    }

    /** @brief 一つ下のシーン（スタックの末尾から2番目）のポインタを取得 */
    IScene* GetPreviousScene() const {
        if (sceneStack_.size() >= 2) {
            return sceneStack_[sceneStack_.size() - 2].scene.get();
        }
        return nullptr;
    }

    /** @brief 登録済みの全シーン名を取得（登録順） */
    std::vector<Key> GetRegisteredKeys() const;
    ///@}

    /** @brief シーンの初期化（Initialize）実行中かどうかを取得 */
    bool IsInitializing() const {
        return isInitializing_;
    }

    /** @brief ロード画面を表示すべきロード中（または遷移中）かどうかを取得 */
    bool IsLoading() const;

    /** @brief シーン遷移演出（フェードアウト/イン）中かどうかを取得 */
    bool IsTransitioning() const {
        return transitionPhase_ != TransitionPhase::None;
    }

    /** @brief チラつき防止（アンチフリッカー）機能付きで、ローディング画面を描画すべきかを取得 */
    bool ShouldDrawLoadingScreen() const;
    ///@}

private:
    IrufemiEngine* engine_ = nullptr; ///< エンジン本体への参照

    std::unordered_map<Key, Factory> factories_; ///< シーン識別名と生成関数のマップ
    std::vector<Key> order_;                     ///< 登録されたシーン名のリスト（順序保持用）

    struct SceneStackItem {
        Key name;
        std::unique_ptr<IScene> scene;
    };
    std::vector<SceneStackItem> sceneStack_; ///< 現在実行中のシーンスタック

    Key pending_{}; ///< 次フレームで切り替え予定のシーン名

    bool isInitializing_ = false; ///< シーンの初期化（Initialize）実行中フラグ

    // --- 遷移管理 ---
    TransitionPhase transitionPhase_ = TransitionPhase::None;
    Key pendingTransition_{};
    SceneTransition::Type pendingType_ = SceneTransition::Type::Fade;
    float pendingDuration_ = 1.0f;
    EaseType pendingEaseType_ = EaseType::Linear;

    bool wasLoading_ = false;   ///< 前フレームがロード中だったか
    float loadingTimer_ = 0.0f; ///< ロード状態の継続時間を計測するタイマー

    // --- 非同期読み込み用 ---
    std::future<void> initFuture_;
    std::mutex nextSceneMutex_;
    std::unique_ptr<IScene> nextScene_{};
    std::atomic<bool> isAsyncInitializing_{false};

    /**
     * @brief 非同期でのシーン破棄・初期化を開始する
     */
    void StartAsyncInitialize(const Key& next);

    // --- 内部処理用メソッド ---
    /**
     * @brief アセットのロード状況をチェックし、マウスカーソルの表示状態を更新する
     * @return ロード中であれば true を返す
     */
    bool UpdateLoadStatus();

    /**
     * @brief 即時シーン切替要求がある場合の処理を行う
     */
    void ProcessImmediateTransition();

    /**
     * @brief フェード遷移および非同期ロードのステートマシンを進行させる
     * @param[in,out] isLoading ロード状態フラグ（遷移によってロードが始まった場合は更新される）
     */
    void ProcessTransitionPhase(bool& isLoading);

    /**
     * @brief シーンスタック内のアクティブなシーンの Update を呼び出す
     */
    void UpdateActiveScenes();
};
