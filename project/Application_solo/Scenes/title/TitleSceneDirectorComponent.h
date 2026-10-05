#pragma once

#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include <memory>
#include <string>
#include <vector>

class TransformComponent;
class TitleCosmicNebulaComponent;

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
    void SetupThrusterEffect();

    /**
     * @brief 自機オブジェクトの TransformComponent を取得します。
     * @return TransformComponent* 存在しない場合は nullptr
     */
    TransformComponent* GetShipTransform() const;

    /**
     * @brief カメラオブジェクトの TransformComponent を取得します。
     * @return TransformComponent* 存在しない場合は nullptr
     */
    TransformComponent* GetCameraTransform() const;

    /**
     * @brief カメラオブジェクトの CameraComponent を取得します。
     * @return CameraComponent* 存在しない場合は nullptr
     */
    class CameraComponent* GetCameraComponent() const;

    /**
     * @brief 星雲コンポーネントを取得します。
     * @return TitleCosmicNebulaComponent* 存在しない場合は nullptr
     */
    TitleCosmicNebulaComponent* GetNebulaComponent() const;

private: // メンバ変数
    // 対象エンティティの安全な弱参照（Dangling Pointer防止）
    std::weak_ptr<GameObject> shipObj_;
    std::weak_ptr<GameObject> cameraObj_;
    std::vector<std::weak_ptr<GameObject>> debrisObjs_;
    std::weak_ptr<GameObject> nebulaObj_;

    // 自機スラスター演出（GameScene完全準拠のTransform Scale制御）
    std::weak_ptr<GameObject> thrusterObj_;
    Irufemi::Vector3 nozzleOffset_{0.0f, 0.0f, -0.48f};
    float currentThrusterScaleZ_ = 0.85f;
    float targetThrusterScaleZ_ = 0.85f;

    // 初期トランスフォームとカメラパラメータのキャッシュ
    Irufemi::Vector3 initialShipPos_{0.0f, 0.0f, 0.0f};
    Irufemi::Vector3 initialShipRot_{0.08f, -0.25f, 0.05f};
    Irufemi::Vector3 launchStartRot_{0.08f, -0.25f, 0.05f};
    Irufemi::Vector3 initialCameraPos_{0.0f, 0.5f, -5.0f};
    float initialCameraFov_{0.785398f}; ///< 初期視野角 (約45度)
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

    // 出撃シーケンスのステージ分割定数（全体長: 2.0秒）
    static constexpr float kDurationCharge_     = 0.50f; ///< [Phase 1: 蓄勢] タメ・重力収束
    static constexpr float kDurationAccelerate_ = 1.10f; ///< [Phase 2: 咆哮] アフターバーナー急加速
    static constexpr float kDurationBreak_      = 0.40f; ///< [Phase 3: 突破] 空間突破・シーン遷移
    static constexpr float kTotalLaunchDuration_ = kDurationCharge_ + kDurationAccelerate_ + kDurationBreak_;

    // 出撃シーケンス用状態フラグ
    bool isLaunching_ = false;
    float launchTimer_ = 0.0f;
    bool hasTriggeredSceneTransition_ = false;
    bool hasPlayedShake_ = false;
};
