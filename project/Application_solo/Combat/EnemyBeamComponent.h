#pragma once

#include "Framework/Component/Component.h"
#include "Combat/EnemyBeamStates.h"
#include "Core/Math/Transform.h"
#include "Renderer/Object/3D/Primitive/Primitive3DObject.h"
#include "Renderer/Data/LightningParams.h"
#include "Renderer/Data/AOEParams.h"
#include "RHI/DirectX12/ConstantBuffer.h"
#include <memory>

namespace Irufemi {
class GameObject;
}

/**
 * @class EnemyBeamComponent
 * @brief 敵が発射するビーム演出および当たり判定を管理するコンポーネント (State Pattern)
 */
class EnemyBeamComponent : public Irufemi::Component {
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
    std::shared_ptr<Irufemi::Component> Clone() override;

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
    const BeamConfig& GetConfig() const {
        return config_;
    }

    const Irufemi::Vector3& GetStartPos() const {
        return startPos_;
    }
    void SetStartPos(const Irufemi::Vector3& pos) {
        startPos_ = pos;
    }

    const Irufemi::Vector3& GetDirection() const {
        return direction_;
    }
    void SetDirection(const Irufemi::Vector3& dir) {
        direction_ = dir;
    }

    Irufemi::Primitive3DObject* GetChargeSphere() {
        return chargeSphere_.get();
    }
    Irufemi::Primitive3DObject* GetTelegraphCylinder() {
        return telegraphCylinder_.get();
    }
    Irufemi::Primitive3DObject* GetAttackCylinder() {
        return attackCylinder_.get();
    }
    Irufemi::Primitive3DObject* GetAttackCylinderOuter() {
        return attackCylinderOuter_.get();
    }

    /**
     * @brief AOE予兆パラメータを更新しバッファへ反映
     * @param warningRatio 溜め進行度 (0.0f～1.0f)
     * @param frameIndex バックバッファインデックス
     */
    void UpdateAOEParams(float warningRatio, uint32_t frameIndex);

    /**
     * @brief AOE予兆定数バッファのGPU仮想アドレスを取得
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetAOEParamsGPUAddress(uint32_t frameIndex) const {
        return aoeParamsBuffer_.GetGPUVirtualAddress(frameIndex);
    }

    /**
     * @brief ビームコア定数バッファのGPU仮想アドレスを取得
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetBeamParamsGPUAddress(uint32_t frameIndex) const {
        return beamParamsBuffer_.GetGPUVirtualAddress(frameIndex);
    }

    /**
     * @brief オーラ定数バッファのGPU仮想アドレスを取得
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetAuraParamsGPUAddress(uint32_t frameIndex) const {
        return auraParamsBuffer_.GetGPUVirtualAddress(frameIndex);
    }

    void SetHasHitCurrentBeam(bool hit) {
        hasHitCurrentBeam_ = hit;
    }
    bool HasHitCurrentBeam() const {
        return hasHitCurrentBeam_;
    }

    Irufemi::GameObject* GetPlayerObject();
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
    std::unique_ptr<Irufemi::Primitive3DObject> chargeSphere_ = nullptr;
    std::unique_ptr<Irufemi::Primitive3DObject> telegraphCylinder_ = nullptr;   // AOE予兆危険円柱
    std::unique_ptr<Irufemi::Primitive3DObject> attackCylinder_ = nullptr;      // 内側の極太レーザーコア
    std::unique_ptr<Irufemi::Primitive3DObject> attackCylinderOuter_ = nullptr; // 外側の電撃オーラ

    // シェーダーパラメータ定数バッファ
    Irufemi::ConstantBuffer<Irufemi::AOEParams> aoeParamsBuffer_;
    Irufemi::AOEParams aoeParamsData_{};

    Irufemi::ConstantBuffer<Irufemi::LightningParams> beamParamsBuffer_;
    Irufemi::LightningParams beamParamsData_{};

    Irufemi::ConstantBuffer<Irufemi::LightningParams> auraParamsBuffer_;
    Irufemi::LightningParams auraParamsData_{};

    // プレイヤーオブジェクトのキャッシュ
    std::weak_ptr<Irufemi::GameObject> playerObj_;
    std::weak_ptr<Irufemi::GameObject> mainCameraObj_;

    Irufemi::Vector3 muzzleLocalOffset_ = {0.0f, 0.0f, 0.0f};
    bool hasHitCurrentBeam_ = false;
};
