#include "Renderer/Camera/Camera.h"

#include "Framework/UI/DebugUI.h"
#include "Core/Math/Math.h"

#include <cmath>
#include <algorithm>

Camera::Camera() {}
Camera::~Camera() {}

// 初期化
void Camera::Initialize(const int& windowWidth, const int& windowHeight) {
    width_ = static_cast<float>(windowWidth);
    height_ = static_cast<float>(windowHeight);

    // ウィンドウサイズに基づいてアスペクト比を更新(3D用)
    aspectRatio_ = (height_ != 0.0f) ? (width_ / height_) : 1.0f;

    // 2D正射影境界は、ウィンドウサイズが変わってもUIレイアウトが自動拡縮されるよう
    // 論理解像度(1280x720)に固定する
    right_ = 1280.0f;
    bottom_ = 720.0f;

    isDirtyTransform_ = true;
    isDirtyProjection_ = true;
    UpdateMatrix();
}

// 更新
void Camera::Update() {
    // ダーティな場合のみ行列を更新する
    if (isDirtyTransform_ || isDirtyProjection_) {
        UpdateMatrix();
    }
}

void Camera::DrawDebugTab([[maybe_unused]] const char* label) {
#if defined USE_IMGUI
    if (ImGui::BeginTabItem(label)) {
        DrawDebugContents();
        ImGui::EndTabItem();
    }
#endif
}

void Camera::DrawDebugContents() {
#if defined USE_IMGUI
    if (ImGui::DragFloat3("translate", &translate_.x, 0.1f) || ImGui::DragFloat3("rotate", &rotate_.x, 0.1f)) {
        isDirtyTransform_ = true;
    }
#endif
}

const Irufemi::Matrix4x4& Camera::GetViewProjectionMatrix2D() {
    if (isDirtyTransform_ || isDirtyProjection_) {
        UpdateMatrix();
    }
    return viewProjectionMatrix2D_;
}

const Irufemi::Matrix4x4& Camera::GetViewProjectionMatrix3D() {
    if (isDirtyTransform_ || isDirtyProjection_) {
        UpdateMatrix();
    }
    return viewProjectionMatrix3D_;
}

// ワールド行列の作成
void Camera::MakeWorldMatrix() {

    worldMatrix_ = Irufemi::Math::MakeAffineMatrix(scale_, rotate_, translate_);
}

// ビュー行列の作成
void Camera::MakeViewMatrix() {
    viewMatrix_ = Irufemi::Math::Inverse(worldMatrix_); // カメラのワールド行列の逆行列がビュー行列
}

// 透視投影行列の更新
void Camera::UpdatePerspectiveFovMatrix() {

    perspectiveFovMatrix_ = Irufemi::Math::MakePerspectiveFovMatrix(fovAngleY_, aspectRatio_, nearZ_, farZ_);
}

// 正射行列の更新
void Camera::UpdateOrthographicMatrix() {

    orthographicMatrix_ = Irufemi::Math::MakeOrthographicMatrix(left_, top_, right_, bottom_, nearClip_, farClip_);
}

// ビューポート行列の更新
void Camera::UpdateViewportMatrix() {

    viewportMatrix_ = Irufemi::Math::MakeViewportMatrix(leftTop_.x, leftTop_.y, width_, height_, minDepth_, maxDepth_);
}

// 各行列の更新
void Camera::UpdateMatrix() {
    bool matrixChanged = false;
    if (isDirtyTransform_) {
        MakeWorldMatrix();
        MakeViewMatrix();
        isDirtyTransform_ = false;
        matrixChanged = true;
    }
    if (isDirtyProjection_) {
        UpdatePerspectiveFovMatrix();
        UpdateOrthographicMatrix();
        UpdateViewportMatrix();
        isDirtyProjection_ = false;
        matrixChanged = true;
    }

    if (matrixChanged) {
        viewProjectionMatrix3D_ = viewMatrix_ * perspectiveFovMatrix_;
        viewProjectionMatrix2D_ = viewMatrix_ * orthographicMatrix_;

        // 視錐台の更新
        frustum_.SetFromViewProjection(viewProjectionMatrix3D_);
    }
}

Irufemi::Vector2 Camera::ScreenToUIPosition(const Irufemi::Vector2& screenPos) const {
    if (width_ <= 0.0f || height_ <= 0.0f) {
        return screenPos;
    }

    return {screenPos.x * (right_ / width_), screenPos.y * (bottom_ / height_)};
}

// カメラ行列を取得する
const Irufemi::Matrix4x4& Camera::GetCameraMatrix() {
    worldMatrix_ = Irufemi::Math::MakeAffineMatrix(scale_, rotate_, translate_);
    return worldMatrix_;
}