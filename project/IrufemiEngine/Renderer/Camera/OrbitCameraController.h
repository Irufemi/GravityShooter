#pragma once
#include "Renderer/Camera/Camera.h"
#include "Platform/Input/InputManager.h"
#include "Core/Math/Vector3.h"

/**
 * @class OrbitCameraController
 * @brief マウス操作によるパン、オービット、ズーム機能を提供するカメラコントローラー。
 *        データ(Camera)と操作(Controller)を分離するためのクラス。
 */
class OrbitCameraController {
public:
    /**
     * @brief カメラに対するマウス・キーボード入力処理を行い、位置・回転を更新します。
     * @param camera 操作対象のカメラ
     * @param input 入力マネージャ
     */
    void UpdateCameraInput(Camera* camera, InputManager* input);

    /**
     * @brief 指定したワールド座標をカメラの中心に収めるよう移動・注視します。
     * @param camera 操作対象のカメラ
     * @param targetPosition フォーカスする対象の座標
     * @param distance カメラと対象との距離（0以下の場合は現在の距離を維持）
     */
    void Focus(Camera* camera, const Irufemi::Vector3& targetPosition, float distance = -1.0f);

    /**
     * @brief バウンディング球（中心と半径）に基づいて対象全体が収まる位置へ配置し注視します (Unity: FrameSelected)
     * @param camera 操作対象のカメラ
     * @param center 注視点（ピボット）のワールド座標
     * @param radius オブジェクトのバウンディング半径
     */
    void Frame(Camera* camera, const Irufemi::Vector3& center, float radius);

    const Irufemi::Vector3& GetTarget() const {
        return target_;
    }
    float GetDistance() const {
        return distance_;
    }

    /**
     * @brief 現在のカメラの位置・回転から、注視点（Target）を再計算し状態を同期させます。
     *        カメラが外部要因で動かされた直後などに呼び出します。
     * @param camera 操作対象のカメラ
     * @param distance 注視点までの想定距離
     */
    void SyncTargetFromCamera(const Camera* camera, float distance = 10.0f);

    /**
     * @brief プリセットの視点にカメラを設定します。
     */
    enum class Preset {
        TopDown,  // 見下ろし
        Diagonal, // 斜め見下ろし
        Front     // 正面
    };
    /**
     * @brief Preset を設定する。
     * @param[in] preset 設定する Preset の値
     * @param[in] camera 設定する Preset の値
     */
    void SetPreset(Preset preset, Camera* camera);

private:
    bool isInitialized_ = false;
    Irufemi::Vector3 target_ = {0.0f, 0.0f, 0.0f};
    float distance_ = 10.0f;
    Irufemi::Vector3 lastCameraPosition_ = {0.0f, 0.0f, 0.0f};
    Irufemi::Vector3 lastCameraRotation_ = {0.0f, 0.0f, 0.0f};
};
