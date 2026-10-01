#pragma once
#include "Framework/Component/Component.h"
#include <vector>
#include <memory>
#include <functional>

class GameObject;
class PlayerTargetingComponent;
class PlayerHealthComponent;

/**
 * @class GravityPlayerComponent
 * @brief ガレキを引き寄せて投げる「重力スロー」アクションを制御するコンポーネント
 */
class GravityPlayerComponent : public Component {
public:
    GravityPlayerComponent() = default;
    ~GravityPlayerComponent() override = default;

    /**
     * @brief 初期化処理
     */
    void Initialize() override;

    /**
     * @brief 開始処理。ターゲティングやマネージャー等の参照を取得します。
     */
    void Start() override;

    /**
     * @brief 毎フレーム更新処理。入力受付やガレキの投擲・回転軌道を更新します。
     */
    void Update() override;

    /**
     * @brief プロパティ登録処理
     */
    void OnRegisterProperties() override;

    /**
     * @brief コンポーネント名を取得する
     * @return コンポーネント名文字列
     */
    std::string GetComponentName() const override {
        return "GravityPlayerComponent";
    }

    /**
     * @brief JSONファイルからプレイヤーステータス（保持数、引き寄せ半径等）を読み込む
     */
    void LoadStatusFromJson();

    /**
     * @brief ステータスJSONのパスを取得する
     * @return ファイルパス文字列
     */
    std::string GetStatusDataPath() const {
        return statusDataPath_;
    }

    /**
     * @brief ステータスJSONのパスを設定する
     * @param path 設定するファイルパス
     */
    void SetStatusDataPath(const std::string& path) {
        statusDataPath_ = path;
    }

private:
    /**
     * @brief ガレキの引き寄せ入力（右クリック長押し等）の処理
     */
    void HandlePullInput();

    /**
     * @brief ターゲットマーキング入力の処理
     */
    void HandleMarkInput();

    /**
     * @brief ガレキ投擲入力（左クリック等）の処理
     */
    void HandleThrowInput();

    /**
     * @brief ガレキ連続投擲シーケンスの更新
     */
    void UpdateThrowing();

private:
    std::vector<std::shared_ptr<GameObject>> orbitingDebris_; ///< 現在プレイヤーの周囲を回転しているガレキのリスト
    int maxOrbitCount_ = 5;                                   ///< 最大保持数
    float pullRadius_ = 100.0f;                               ///< 引き寄せ検知半径

    PlayerTargetingComponent* targetingComp_ = nullptr; ///< 敵ターゲティングコンポーネントの参照
    std::weak_ptr<GameObject> debrisManagerObj_;        ///< ガレキマネージャーオブジェクトの弱参照

    // Orbit parameters for pulled debris
    float orbitRadiusMin_ = 2.0f;       ///< ガレキ回転半径（最小）
    float orbitRadiusMax_ = 4.0f;       ///< ガレキ回転半径（最大）
    float orbitAngleRandomMax_ = 6.28f; ///< 回転初期角度のランダム範囲

    bool isThrowing_ = false;     ///< 投擲中フラグ
    float throwTimer_ = 0.0f;     ///< 投擲インターバルタイマー
    float hoverFrequency_ = 2.0f; ///< 浮遊の揺れ速度

    float throwInterval_ = 0.15f; ///< 投擲インターバル（秒）
    int throwRemainingCount_ = 0; ///< 今回の射撃ループで撃つ残弾数

    /// ノーロック射撃時に、レイキャストが何にも当たらなかった場合の最大飛距離
    float noLockThrowDistance_ = 1000.0f;

    std::string statusDataPath_ = "resources/GameData/PlayerStatus.json"; ///< ステータス設定ファイルパス

    PlayerHealthComponent* healthComp_ = nullptr; ///< プレイヤー体力コンポーネントの参照
};
