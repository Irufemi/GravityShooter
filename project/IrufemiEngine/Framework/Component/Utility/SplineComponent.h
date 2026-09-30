#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include <vector>
#include <string>

/**
 * @class SplineComponent
 * @brief 複数のウェイポイント間を Catmull-Rom スプラインで補間し、座標や接線を取得する汎用コンポーネント
 */
class SplineComponent : public Component {
public:
    SplineComponent() = default;
    ~SplineComponent() override = default;

    /**
     * @brief OnRegisterProperties を実行する。
     */
    void OnRegisterProperties() override;
    /**
     * @brief Update を実行する。
     */
    void Update() override;

    /**
     * @brief エディタモード中も子オブジェクトの配置変更を検知して更新を行うかを判定します
     */
    bool CanUpdateInEditMode() const override {
        return true;
    }

    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "SplineComponent";
    }

    /**
     * @brief 0.0 ~ 1.0 の進行度(t)から、スプライン上の座標を取得する
     * @param t 進行度 (0.0=開始地点, 1.0=終了地点)
     * @return 補間された座標
     */
    Irufemi::Vector3 GetPointAt(float t) const;

    /**
     * @brief 指定した進行度(t)における接線（進行方向）ベクトルを取得する
     * @param t 進行度 (0.0~1.0)
     * @return 正規化された接線ベクトル
     */
    Irufemi::Vector3 GetTangentAt(float t) const;

    /**
     * @brief 距離（m）ベースでスプライン上の座標を取得する
     */
    Irufemi::Vector3 GetPointAtDistance(float distance) const;

    /**
     * @brief 距離（m）ベースで接線（進行方向）を取得する
     */
    Irufemi::Vector3 GetTangentAtDistance(float distance) const;

    /**
     * @brief スプラインの総距離（m）を取得する
     */
    float GetTotalLength() const;

    /**
     * @brief 距離ルックアップテーブルを再計算する
     */
    void UpdateDistanceTable();

    // ウェイポイントのリストを取得・設定
    /**
     * @brief Waypoints を取得する。
     * @return 取得された Waypoints
     */
    const std::vector<Irufemi::Vector3>& GetWaypoints() const {
        return waypoints_;
    }
    /**
     * @brief Waypoints を設定する。
     * @param[in] points 設定する Waypoints の値
     */
    void SetWaypoints(const std::vector<Irufemi::Vector3>& points) {
        waypoints_ = points;
        UpdateDistanceTable();
    }

    /**
     * @brief 子オブジェクトのTransformからウェイポイントを更新する
     */
    void UpdateWaypointsFromChildren();
    /**
     * @brief Initialize を実行する。
     */
    void Initialize() override;
    /**
     * @brief 生成時の自己完結初期化（Line3DBatchの初期化・距離テーブル更新）を行います
     */
    void OnAwake() override;
    /**
     * @brief Draw を実行する。
     */
    void Draw() override;

    bool GetDrawDebugRail() const {
        return drawDebugRail_;
    }
    void SetDrawDebugRail(bool enable) {
        drawDebugRail_ = enable;
    }

    bool GetDrawDebugNodes() const {
        return drawDebugNodes_;
    }
    void SetDrawDebugNodes(bool enable) {
        drawDebugNodes_ = enable;
    }

    float GetNodeRadius() const {
        return nodeRadius_;
    }
    void SetNodeRadius(float radius) {
        nodeRadius_ = radius;
    }

private:
    std::vector<Irufemi::Vector3> waypoints_; ///< スプラインの制御点
    std::vector<float> distanceLUT_; ///< 距離のルックアップテーブル (t=0.0~1.0を等分した距離の累積)
    float totalLength_ = 0.0f;       ///< スプラインの総距離
    int lastChildCount_ = -1;        ///< 子オブジェクト数のキャッシュ
    bool drawDebugRail_ = true;      ///< エディタやデバッグ時にレール曲線を描画するかどうか
    bool drawDebugNodes_ = true; ///< エディタやデバッグ時に各ウェイポイントの球体ギズモを描画するかどうか
    float nodeRadius_ = 0.5f;                           ///< ウェイポイント球体ギズモの描画半径
    std::unique_ptr<class Line3DBatch> debugLineBatch_; ///< デバッグ描画用のラインバッチ
};
