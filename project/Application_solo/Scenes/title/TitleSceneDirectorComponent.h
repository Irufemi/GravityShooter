#pragma once

#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include <memory>
#include <string>
#include <vector>

class TransformComponent;
class TitleGravityWaveComponent;

/**
 * @class TitleSceneDirectorComponent
 * @brief タイトル画面の3D空間アニメーションおよび出撃シーケンス統括コンポーネント
 * @details 自機の待機ホバリング、周囲ガレキの公転、カメラの呼吸微動を制御し、
 *          GAME START 決定時に重力パルス・ガレキ収束・スラスター急加速・ドリーインを経て
 *          InGame シーンへシームレスに突入する一連の演出タイムラインを管理します。
 */
class TitleSceneDirectorComponent : public Component {
public:
    TitleSceneDirectorComponent() = default;
    ~TitleSceneDirectorComponent() override = default;

    void Initialize() override;
    void Update() override;
    void OnRegisterProperties() override;

    std::string GetComponentName() const override {
        return "TitleSceneDirectorComponent";
    }

    /**
     * @brief GAME START 決定時の出撃シーケンスを開始する
     */
    void StartLaunchSequence();

    /**
     * @brief 現在出撃シーケンス中かどうか
     */
    bool IsLaunching() const {
        return isLaunching_;
    }

private:
    void CacheEntities();
    void UpdateIdling(float deltaTime);
    void UpdateLaunchSequence(float deltaTime);

private: // メンバ変数
    // 対象エンティティのTransform
    TransformComponent* shipTransform_ = nullptr;
    TransformComponent* cameraTransform_ = nullptr;
    std::vector<TransformComponent*> debrisTransforms_;

    TitleGravityWaveComponent* gravityWaveComp_ = nullptr;

    // 初期トランスフォームのキャッシュ
    Irufemi::Vector3 initialShipPos_{0.0f, 0.0f, 0.0f};
    Irufemi::Vector3 initialShipRot_{0.08f, -0.25f, 0.05f};
    Irufemi::Vector3 initialCameraPos_{0.0f, 0.5f, -5.0f};
    std::vector<Irufemi::Vector3> initialDebrisPositions_;

    // アイドリング用タイマー
    float idleTimer_ = 0.0f;

    // ガレキ公転設定 (半径, 角速度, 軌道傾斜角)
    struct OrbitConfig {
        float radiusX;
        float radiusZ;
        float speed;
        float phase;
        float heightOffset;
    };
    std::vector<OrbitConfig> debrisOrbits_;

    // 出撃シーケンス用タイマー
    bool isLaunching_ = false;
    float launchTimer_ = 0.0f;
    const float kTotalLaunchDuration_ = 0.95f;
    bool hasTriggeredSceneTransition_ = false;
};
