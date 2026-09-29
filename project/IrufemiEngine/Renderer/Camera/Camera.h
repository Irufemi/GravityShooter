#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Geometry/Frustum.h"

/**
 * @class Camera
 * @brief 3D空間の視点を管理し、ビュー行列や射影行列を生成するクラス
 */
class Camera {
private: // メンバ変数
    // カメラの位置。ワールド座標。
    Irufemi::Vector3 translate_ = {0.0f, 0.0f, -50.0f};

    // カメラの回転角度
    Irufemi::Vector3 rotate_ = {0.0f, 0.0f, 0.0f};

    // カメラの拡縮(ここはいじらない。)
    const Irufemi::Vector3 scale_ = {1.0f, 1.0f, 1.0f};

#pragma region 正射影行列を構成する変数(カメラで映す空間の範囲)

    // カメラで映す空間の左端のX座標
    float left_ = 0.0f;

    // カメラで映す空間の上端のY座標
    float top_ = 0.0f;

    // カメラで映す空間の右端のX座標
    float right_ = 1280.0f;

    // カメラで映す空間の下端のY座標
    float bottom_ = 720.0f;

    // 近平面。ここではz軸が奥行きになるため一番手前
    float nearClip_ = 0.0f;

    // 遠平面。ここではz軸が奥行きになるため遠さを表す。
    float farClip_ = 100.0f;

#pragma endregion

#pragma region 透視投影行列を構成する変数(カメラで映す空間の範囲)

    // 垂直方向視野角
    float fovAngleY_ = 45.0f * 3.141592654f / 180.0f;

    // ビューポートのアスペクト比
    float aspectRatio_ = 16.0f / 9.0f;

    // 深度限界(手前側)(面なので0だと面が点になって映らない。できるだけ全部が映る遠いところがいい。大体目安は0.1程度)。
    float nearZ_ = 0.1f;

    // 深度限界(奥側)
    float farZ_ = 1000.0f;

#pragma endregion

#pragma region ビューポート行列を構成する変数(ウィンドウ上で映す範囲)

    // 画面上で映す横幅
    float width_ = 1280.0f;

    // 画面上で映す高さ
    float height_ = 720.0f;

    // ウィンドウに映す範囲の左上の座標
    Irufemi::Vector2 leftTop_ = {0.0f, 0.0f};

    // mindepth(最小深度値)
    float minDepth_ = 0.0f;

    // maxDepth(最大深度値)
    float maxDepth_ = 1.0f;

#pragma endregion

    // ワールド行列
    Irufemi::Matrix4x4 worldMatrix_{};

    // ビュー行列
    Irufemi::Matrix4x4 viewMatrix_{};

    // 正射影行列
    Irufemi::Matrix4x4 orthographicMatrix_{};

    // 透視投影行列
    Irufemi::Matrix4x4 perspectiveFovMatrix_{};

    // ビューポート行列
    Irufemi::Matrix4x4 viewportMatrix_{};

    // ダーティフラグ
    bool isDirtyTransform_ = true;
    bool isDirtyProjection_ = true;

    // キャッシュされたビュープロジェクション行列
    Irufemi::Matrix4x4 viewProjectionMatrix3D_{};
    Irufemi::Matrix4x4 viewProjectionMatrix2D_{};

public: // メンバ関数
    /**
     * @brief コンストラクタ
     */
    Camera();

    /**
     * @brief デストラクタ
     */
    ~Camera();

    /**
     * @brief 初期化処理
     * @param windowWidth ウィンドウの幅
     * @param windowHeight ウィンドウの高さ
     */
    void Initialize(const int& windowWidth = 1280, const int& windowHeight = 720);

    /**
     * @brief 更新処理
     */
    void Update();

    /**
     * @brief デバッグ用タブを表示します
     * @param label タブのラベル
     */
    void DrawDebugTab(const char* label);

    /**
     * @brief デバッグUIの内容表示 (TabItemなし)
     */
    void DrawDebugContents();

    /**
     * @brief スクリーンのピクセル座標を、2D(UI)の論理空間座標に変換します。
     * @details ウィンドウサイズが変更された場合でも、正射影行列による描画解像度（UI空間）に対応した座標を返します。
     * @param screenPos マウスなどのスクリーンピクセル座標
     * @return Irufemi::Vector2 変換後のUI空間座標
     */
    Irufemi::Vector2 ScreenToUIPosition(const Irufemi::Vector2& screenPos) const;

    // セッター

    /**
     * @brief カメラの座標を設定します
     * @param translate ワールド座標
     */
    void SetTranslate(const Irufemi::Vector3& translate) {
        this->translate_ = translate;
        this->isDirtyTransform_ = true;
    }

    /**
     * @brief カメラの回転角度を設定します
     * @param rotate オイラー角
     */
    void SetRotate(const Irufemi::Vector3& rotate) {
        this->rotate_ = rotate;
        this->isDirtyTransform_ = true;
    }

    /**
     * @brief ViewMatrix を設定する。
     * @param[in] viewMatrix 設定する ViewMatrix の値
     */
    void SetViewMatrix(const Irufemi::Matrix4x4& viewMatrix) {
        this->viewMatrix_ = viewMatrix;
        this->isDirtyTransform_ = true;
    }

    /**
     * @brief PerspectiveFovMatrix を設定する。
     * @param[in] perspectiveFovMatrix 設定する PerspectiveFovMatrix の値
     */
    void SetPerspectiveFovMatrix(const Irufemi::Matrix4x4& perspectiveFovMatrix) {
        this->perspectiveFovMatrix_ = perspectiveFovMatrix;
        this->isDirtyProjection_ = true;
    }

    /**
     * @brief FarClip を設定する。
     * @param[in] farClip 設定する FarClip の値
     */
    void SetFarClip(const float& farClip) {
        this->farClip_ = farClip;
        this->isDirtyProjection_ = true;
    }

    /**
     * @brief FovY を設定する。
     * @param[in] fovY 設定する FovY の値
     */
    void SetFovY(const float& fovY) {
        this->fovAngleY_ = fovY;
        this->isDirtyProjection_ = true;
    }

    /**
     * @brief 垂直方向視野角を取得する
     * @return 垂直方向視野角 (ラジアン)
     */
    float GetFovY() const {
        return this->fovAngleY_;
    }

    // ゲッター

    /**
     * @brief カメラの座標を取得します
     * @return const Irufemi::Vector3& ワールド座標
     */
    const Irufemi::Vector3& GetTranslate() const {
        return this->translate_;
    }

    /**
     * @brief カメラの回転角度を取得します
     * @return const Irufemi::Vector3& オイラー角
     */
    const Irufemi::Vector3& GetRotate() const {
        return this->rotate_;
    }

    /**
     * @brief カメラ行列(ワールド行列)を取得します
     * @return const Irufemi::Matrix4x4& カメラのワールド行列
     */
    const Irufemi::Matrix4x4& GetCameraMatrix();

    /**
     * @brief ワールド行列を取得します
     * @return const Irufemi::Matrix4x4& ワールド行列
     */
    const Irufemi::Matrix4x4& GetWorldMatrix() const {
        return worldMatrix_;
    }

    /**
     * @brief ビュー行列を取得します
     * @return const Irufemi::Matrix4x4& ビュー行列
     */
    const Irufemi::Matrix4x4& GetViewMatrix() const {
        return viewMatrix_;
    }

    /**
     * @brief 透視投影行列を取得します
     * @return const Irufemi::Matrix4x4& 透視投影行列
     */
    const Irufemi::Matrix4x4& GetPerspectiveFovMatrix() const {
        return perspectiveFovMatrix_;
    }

    /**
     * @brief 正射影行列を取得します
     * @return const Irufemi::Matrix4x4& 正射影行列
     */
    const Irufemi::Matrix4x4& GetOrthographicMatrix() const {
        return orthographicMatrix_;
    }

    /**
     * @brief ビューポート変換行列を取得します
     * @return const Irufemi::Matrix4x4& ビューポート変換行列
     */
    const Irufemi::Matrix4x4& GetViewportMatrix() const {
        return viewportMatrix_;
    }
    /**
     * @brief Frustum を取得する。
     * @return 取得された Frustum
     */
    const Irufemi::Frustum& GetFrustum() const {
        return frustum_;
    }

    /**
     * @brief ViewProjectionMatrix2D を取得する。
     * @return 取得された ViewProjectionMatrix2D
     */
    const Irufemi::Matrix4x4& GetViewProjectionMatrix2D();

    /**
     * @brief ViewProjectionMatrix3D を取得する。
     * @return 取得された ViewProjectionMatrix3D
     */
    const Irufemi::Matrix4x4& GetViewProjectionMatrix3D();

    /**
     * @brief NearZ を取得する。
     * @return 取得された NearZ
     */
    const float& GetNearZ() const {
        return nearZ_;
    }
    /**
     * @brief FarZ を取得する。
     * @return 取得された FarZ
     */
    const float& GetFarZ() const {
        return farZ_;
    }

    /**
     * @brief ワールド行列を再計算します
     */
    void MakeWorldMatrix();

    /**
     * @brief ビュー行列を再計算します
     */
    void MakeViewMatrix();

    /**
     * @brief 透視投影行列を再計算します
     */
    void UpdatePerspectiveFovMatrix();

    /**
     * @brief 正射影行列を再計算します
     */
    void UpdateOrthographicMatrix();

    /**
     * @brief ビューポート行列を再計算します
     */
    void UpdateViewportMatrix();

private:
    Irufemi::Frustum frustum_;

public:
    /**
     * @brief すべての行列を更新します
     */
    void UpdateMatrix();

    // 2Dで使うための現在のビューポートサイズ取得
    /**
     * @brief ViewportWidth を取得する。
     * @return 取得された ViewportWidth
     */
    const float& GetViewportWidth() const {
        return width_;
    }
    /**
     * @brief ViewportHeight を取得する。
     * @return 取得された ViewportHeight
     */
    const float& GetViewportHeight() const {
        return height_;
    }
};
