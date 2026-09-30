#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Transform.h"
#include "Renderer/Object/3D/Primitive/Primitive3DObject.h"
#include "Renderer/Data/LightningParams.h"
#include "Renderer/Data/AOEParams.h"
#include "RHI/DirectX12/ConstantBuffer.h"
#include <memory>

class GameObject;

/**
 * @class EnemyBeamComponent
 * @brief 敵が発射するビーム演出および当たり判定を管理するコンポーネント（AOE予兆対応）
 */
class EnemyBeamComponent : public Component {
public:
    EnemyBeamComponent() = default;
    ~EnemyBeamComponent() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void OnRegisterProperties() override;

    std::string GetComponentName() const override {
        return "EnemyBeamComponent";
    }

    /**
     * @brief クローンを作成する
     */
    std::shared_ptr<Component> Clone() override;

    /**
     * @brief ビーム発射シーケンスを開始する
     * @param startPos 発射元の座標
     * @param targetPos ターゲットの座標
     */
    void Fire(const Irufemi::Vector3& startPos, const Irufemi::Vector3& targetPos);

    bool IsActive() const {
        return state_ != State::IDLE;
    }

private:
    enum class State { IDLE, CHARGING, FIRING };

    State state_ = State::IDLE;

    // パラメータ
    Irufemi::Vector3 startPos_;
    Irufemi::Vector3 direction_;
    float beamLength_ = 200.0f;   // ビームの最大長
    float beamMaxRadius_ = 1.0f;  // カメラに収まる程度の細さに調整（元は太すぎた）
    float chargeDuration_ = 1.5f; // 溜め時間
    float fireDuration_ = 0.8f;   // 発射時間

    // AOE予兆・ロックオンパラメータ
    float lockLeadTime_ = 0.4f; //!< 発射前何秒で射線を固定（ロック）するか
    bool isAimLocked_ = false;  //!< 射線が固定されたかどうか
    Irufemi::Vector4 telegraphColor_ = {1.0f, 0.1f, 0.1f, 0.7f}; //!< 予兆円柱の基本色

    // 当たり判定パラメータ
    int beamDamage_ = 30;               //!< ビーム直撃ダメージ
    float hitCheckRadiusMargin_ = 0.5f; //!< 当たり判定マージン（機体半径考慮）

    Irufemi::Vector4 chargeColor_ = {0.7f, 0.0f, 0.9f, 1.0f};

    Irufemi::Vector4 beamColor_ = {0.8f, 0.0f, 1.0f, 1.0f};
    Irufemi::Vector4 beamCoreColor_ = {0.0f, 1.0f, 1.0f, 1.0f};
    float beamIntensity_ = 6.0f;
    float beamCoreIntensity_ = 40.0f;
    float beamSpeed_ = 3.0f;

    Irufemi::Vector4 auraColor_ = {0.1f, 0.0f, 0.2f, 1.0f};
    Irufemi::Vector4 auraCoreColor_ = {0.8f, 0.0f, 1.0f, 1.0f};
    float auraIntensity_ = 12.0f;
    float auraSpeed_ = 0.8f;

    float stateTimer_ = 0.0f;

    // 描画オブジェクト
    std::unique_ptr<Primitive3DObject> chargeSphere_ = nullptr;
    std::unique_ptr<Primitive3DObject> telegraphCylinder_ = nullptr;   // AOE予兆危険円柱
    std::unique_ptr<Primitive3DObject> attackCylinder_ = nullptr;      // 内側の極太レーザーコア
    std::unique_ptr<Primitive3DObject> attackCylinderOuter_ = nullptr; // 外側の電撃オーラ

    // シェーダーパラメータ定数バッファ
    ConstantBuffer<AOEParams> aoeParamsBuffer_;
    AOEParams aoeParamsData_{};

    ConstantBuffer<LightningParams> beamParamsBuffer_;
    LightningParams beamParamsData_{};

    ConstantBuffer<LightningParams> auraParamsBuffer_;
    LightningParams auraParamsData_{};

    // プレイヤーオブジェクトのキャッシュ
    std::weak_ptr<GameObject> playerObj_;

    /**
     * @brief 描画リソースが未生成の場合に遅延初期化する
     */
    void EnsureResources();

    /**
     * @brief インスペクターやプロパティの変更を定数バッファデータへ同期する
     */
    void UpdateParameters();

    /**
     * @brief チャージ状態のアニメーション・ビルボード更新（予兆追尾 ➔ 射線固定）
     * @param deltaTime 経過時間
     */
    void UpdateCharging(float deltaTime);

    /**
     * @brief 発射状態のビーム伸長・回転・拡縮更新
     * @param deltaTime 経過時間
     */
    void UpdateFiring(float deltaTime);

    /**
     * @brief 発射中の自機との当たり判定（線分 vs 点 / 球）
     */
    void CheckBeamCollision();

    /**
     * @brief プレイヤー GameObject を取得する
     */
    GameObject* GetPlayerObject();

    /**
     * @brief 現在のボスのワールド行列に基づき、最新の発射口ワールド座標を取得する
     */
    Irufemi::Vector3 GetCurrentMuzzlePosition() const;

    Irufemi::Vector3 muzzleLocalOffset_ = {0.0f, 0.0f, 0.0f}; //!< ボス本体から見た発射口のローカルオフセット
    bool hasHitCurrentBeam_ = false;                          //!< 現在のビーム照射で既にヒットしたか
    std::weak_ptr<GameObject> mainCameraObj_; //!< メインカメラのキャッシュ（毎フレームのFindGameObject回避）
};
