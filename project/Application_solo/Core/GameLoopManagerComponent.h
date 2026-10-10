#pragma once
#include "Framework/Component/Component.h"
#include <string>
#include <memory>

class GameObject;
class PlayerHealthComponent;
class BossComponent;

/**
 * @class GameLoopManagerComponent
 * @brief ゲームの進行状態（ゲームオーバー・ステージクリア・リザルト遷移）を一元管理するコンポーネント
 * @details
 * プレイヤーおよびボスの死亡イベントを購読し、演出用のタイムスケール変更およびリザルトシーンの呼び出しを制御する。
 */
class GameLoopManagerComponent : public Component {
public:
    /**
     * @enum State
     * @brief ゲームループの進行状態
     */
    enum class State {
        Playing, ///< 通常プレイ中
        Finished ///< クリアまたはゲームオーバー演出終了後
    };

    GameLoopManagerComponent() = default;
    ~GameLoopManagerComponent() override;

    /**
     * @brief コンポーネントの初期化
     */
    void Initialize() override;

    /**
     * @brief 初回更新直前の開始処理（対象オブジェクトの先行バインドを行う）
     */
    void Start() override;

    /**
     * @brief 毎フレーム更新
     */
    void Update() override;

    /**
     * @brief インスペクター用プロパティの登録
     */
    void OnRegisterProperties() override;

    std::string GetComponentName() const override {
        return "GameLoopManagerComponent";
    }

private:
    /**
     * @brief 対象となるプレイヤーおよびボスの GameObject を検索してイベントをバインドする
     * @return 必要な対象がすべてバインドされた場合は true
     */
    bool BindTargets();

    /**
     * @brief ボス撃破時のコールバック
     */
    void OnBossDied();

    /**
     * @brief プレイヤー死亡時のコールバック
     */
    void OnPlayerDied();

    /**
     * @brief 死亡演出終了時のコールバック（リザルトシーンへ遷移）
     */
    void OnDeathSequenceFinished();

    State state_ = State::Playing;
    float timeScaleAtResult_ = 0.05f; ///< 撃破・決着瞬間の超スローモーション (0.05倍速)
    bool isClear_ = false;

    std::string targetPlayerName_ = "Player";
    std::string targetBossName_ = "Boss";

    // 生ポインタを排除し、安全な参照追跡のために weak_ptr で管理
    std::weak_ptr<GameObject> playerObj_;
    std::weak_ptr<GameObject> bossObj_;
};
