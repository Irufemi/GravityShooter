#pragma once

#include "Framework/Component/Component.h"
#include "Combat/EnemyBeamStates.h"
#include "Core/Math/Transform.h"
#include "Renderer/Object/3D/Primitive/Primitive3DObject.h"
#include "Renderer/Data/LightningParams.h"
#include "Renderer/Data/AOEParams.h"
#include "RHI/DirectX12/ConstantBuffer.h"
#include <memory>

class GameObject;

/**
 * @class EnemyBeamComponent
 * @brief 敵が発射するビーム演出および当たり判定を管理するコンポーネント (State Pattern)
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

    /**
     * @brief ビームが稼働中（チャージ中または発射中）か
     */
    bool IsActive() const {
        return currentState_ != nullptr;
    }

    /**
     * @brief ビームステートを遷移させる
     * @param newState 新しいステートインスタンス（nullptrの場合はIDLE化）
     */
    void ChangeState(std::unique_ptr<IBeamState> newState);

    // --- 各種データ・オブジェクトアクセサ (State側から利用) ---
    BeamConfig& GetConfig() { return config_; }
    const BeamConfig& GetConfig() const { return config_; }

    const Irufemi::Vector3& GetStartPos() const { return startPos_; }
    void SetStartPos(const Irufemi::Vector3& pos) { startPos_ = pos; }

    const Irufemi::Vector3& GetDirection() const { return direction_; }
    void SetDirection(const Irufemi::Vector3& dir) { direction_ = dir; }

    Primitive3DObject* GetChargeSphere() { return chargeSphere_.get(); }
    Primitive3DObject* GetTelegraphCylinder() { return telegraphCylinder_.get(); }
    Primitive3DObject* GetAttackCylinder() { return attackCylinder_.get(); }
    Primitive3DObject* GetAttackCylinderOuter() { return attackCylinderOuter_.get(); }

    ConstantBuffer<AOEParams>& GetAOEParamsBuffer() { return aoeParamsBuffer_; }
    AOEParams& GetAOEParamsData() { return aoeParamsData_; }

    ConstantBuffer<LightningParams>& GetBeamParamsBuffer() { return beamParamsBuffer_; }
    LightningParams& GetBeamParamsData() { return beamParamsData_; }

    ConstantBuffer<LightningParams>& GetAuraParamsBuffer() { return auraParamsBuffer_; }
    LightningParams& GetAuraParamsData() { return auraParamsData_; }

    void SetHasHitCurrentBeam(bool hit) { hasHitCurrentBeam_ = hit; }
    bool HasHitCurrentBeam() const { return hasHitCurrentBeam_; }

    GameObject* GetPlayerObject();
    Irufemi::Vector3 GetCurrentMuzzlePosition() const;
    void CheckBeamCollision();

private:
    void EnsureResources();
    void UpdateParameters();

    BeamConfig config_;
    std::unique_ptr<IBeamState> currentState_ = nullptr;

    Irufemi::Vector3 startPos_{0.0f, 0.0f, 0.0f};
    Irufemi::Vector3 direction_{0.0f, 0.0f, 1.0f};

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
    std::weak_ptr<GameObject> mainCameraObj_;

    Irufemi::Vector3 muzzleLocalOffset_ = {0.0f, 0.0f, 0.0f};
    bool hasHitCurrentBeam_ = false;
};
