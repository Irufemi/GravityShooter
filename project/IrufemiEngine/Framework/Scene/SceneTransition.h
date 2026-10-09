#pragma once
#include "Renderer/PostProcess/PostProcessManager.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "Core/Utility/Ease.h"

/**
 * @class SceneTransition
 * @brief シーン遷移の演出（フェード等）を管理するクラス
 */
class SceneTransition {
public:
    /**
     * @enum Type
     * @brief 画面遷移のエフェクトタイプ
     */
    enum class Type {
        Fade, ///< 【フェード】画面全体を徐々に指定の色（黒など）で塗りつぶす、最も標準的な暗転。
        Dissolve, ///< 【ディゾルブ】ノイズを利用し、画面がバラバラに崩れ去る、または再構築されるような演出。
        Slide, ///< 【スライド】画面の端からワイプ（拭き取り）のように色がスライドしてくる演出。
        RadialBlur, ///< 【放射状ブラー】中心に向かって画面が引き込まれるようなボケを伴いながら遷移する演出。
        RadialBlurWhite, ///< 【放射状ブラー(白)】ボケながら真っ白にホワイトアウトしていく演出。
    };

    SceneTransition();
    ~SceneTransition();

    /**
     * @brief 初期化
     * @param ppManager ポストプロセス管理者
     */
    void Initialize(PostProcessManager* ppManager);

    /**
     * @brief 演出を開始する
     * @param type 演出の種類
     * @param duration 演出にかける時間（秒）
     * @param isOut true:画面を隠す（フェードアウト）, false:画面を表示する（フェードイン）
     * @param easeType 演出のイージングタイプ（デフォルトは線形）
     */
    void Start(Type type, float duration, bool isOut, EaseType easeType = EaseType::Linear);

    /**
     * @brief 毎フレームの更新処理
     * @param deltaTime フレーム間経過時間
     */
    void Update(float deltaTime);

    /** @brief 演出（イン/アウト問わず）が完全に終了したか */
    bool IsFinished() const {
        return !isActive_ && timer_ >= (duration_ + kDwellTime);
    }

    /** @brief 演出が実行中か */
    bool IsActive() const {
        return isActive_;
    }

    /** @brief 現在実行中（または最後に実行された）演出タイプを取得 */
    Type GetCurrentType() const {
        return currentType_;
    }

    /** @brief フェードアウト（画面が完全に隠れきった状態）が完了したか */
    bool IsOutFinished() const {
        return !isActive_ && isOut_;
    }

    /**
     * @brief 演出完了時（イン/アウト完了問わず）のコールバックを設定
     * @param callback 呼び出される関数
     */
    void SetOnFinishedCallback(std::function<void()> callback) {
        onFinishedCallback_ = std::move(callback);
    }

    /**
     * @brief フェードアウト完了時のコールバックを設定
     * @param callback 呼び出される関数
     */
    void SetOnOutFinishedCallback(std::function<void()> callback) {
        onOutFinishedCallback_ = std::move(callback);
    }

    /** @brief 画面が完全に隠れた後の静止時間（秒） */
    static constexpr float kDwellTime = 0.15f;

private:
    PostProcessManager* ppManager_ = nullptr;

    Type currentType_ = Type::Fade;
    EaseType easeType_ = EaseType::Linear; // イージングタイプを保持
    std::unique_ptr<class ITransitionDriver> driver_;
    float timer_ = 0.0f;
    float duration_ = 1.0f;
    bool isOut_ = true;
    bool isActive_ = false;

    std::function<void()> onFinishedCallback_;
    std::function<void()> onOutFinishedCallback_;

    // トランジションが現在適用しているポストプロセスモードの追跡用
    std::vector<PostProcessMode> activeTransitionModes_;

    // 前回のポストプロセスモードを復元するための保存用（必要に応じて）
};
