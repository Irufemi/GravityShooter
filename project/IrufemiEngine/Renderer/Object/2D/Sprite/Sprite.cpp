#define NOMINMAX
#include "Renderer/Object/2D/Sprite/Sprite.h"

#include "Framework/UI/DebugUI.h"
#include "Resource/Texture/TextureManager.h"
#include "Renderer/DrawManager.h"
#include "Renderer/Camera/Camera.h"
#include "Renderer/Camera/CameraManager.h"
#include "Core/Math/Math.h"

#include <algorithm>

TextureManager* Sprite::textureManager_ = nullptr;
DrawManager* Sprite::drawManager_ = nullptr;
DebugUI* Sprite::ui_ = nullptr;
CameraManager* Sprite::cameraManager_ = nullptr;

void Sprite::Initialize(const std::string& textureName) {
    auto* texMgr = GetTextureManagerInstance();
    auto* camMgr = GetCameraManagerInstance();

    resource_ = std::make_unique<Object2DResource>();
    if (texMgr) {
        resource_->SetTextureManager(texMgr);
    }

    // 頂点はユニットクワッド(0..1)に統一(サイズはscaleで与える)
    // 左下
    resource_->GetVertexDataList().push_back({{0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}});
    // 左上
    resource_->GetVertexDataList().push_back({{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.00}, {0.0f, 0.0f, -1.0f}});
    // 右下
    resource_->GetVertexDataList().push_back({{1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}});
    // 右上
    resource_->GetVertexDataList().push_back({{1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}});

    resource_->GetIndexDataList().push_back(0);
    resource_->GetIndexDataList().push_back(1);
    resource_->GetIndexDataList().push_back(2);
    resource_->GetIndexDataList().push_back(1);
    resource_->GetIndexDataList().push_back(3);
    resource_->GetIndexDataList().push_back(2);

    // リソースのメモリを確保
    resource_->CreateResource();

    // 書き込めるようにする
    resource_->Map();

    // データのコピー
    if (resource_->GetVertexData()) {
        std::copy(resource_->GetVertexDataList().begin(), resource_->GetVertexDataList().end(),
                  resource_->GetVertexData());
    }
    if (resource_->GetIndexData()) {
        std::copy(resource_->GetIndexDataList().begin(), resource_->GetIndexDataList().end(),
                  resource_->GetIndexData());
    }

    // VB/IB作成とMapが済んだ後に一度アンカーを頂点に反映しておく
    ApplyAnchorToVertices();

    // デフォルトサイズを設定(テクスチャがない場合や読み込み失敗時の保険)
    SetSize(size_.x, size_.y);

    // 初回の行列計算
    if (camMgr) {
        if (Camera* activeCam = camMgr->GetActiveCamera()) {
            resource_->UpdateTransform(*activeCam);
        }
    }

    // マテリアル
    if (resource_->GetMaterialData()) {
        resource_->GetMaterialData()->color = {1.0f, 1.0f, 1.0f, 1.0f};
        resource_->GetMaterialData()->enableLighting = false;
        resource_->GetMaterialData()->hasTexture = true;
        resource_->GetMaterialData()->lightingMode = 2;
        resource_->GetMaterialData()->uvTransform = Irufemi::Math::MakeIdentity4x4();
    }

    // テクスチャ設定
    if (texMgr) {
        if (resource_->GetTextureHandle().IsValid()) {
            texMgr->ReleaseTexture(resource_->GetTextureHandle());
        }
        resource_->SetTextureHandle(texMgr->LoadTexture(textureName));

        // テクスチャサイズを直接取得して描画サイズに反映
        uint32_t tw = 0, th = 0;
        if (texMgr->GetTextureSize(textureName, tw, th) && tw > 0 && th > 0) {
            textureSize_ = {static_cast<float>(tw), static_cast<float>(th)};
            SetSize(textureSize_.x, textureSize_.y);
            if (camMgr) {
                if (Camera* activeCam = camMgr->GetActiveCamera()) {
                    resource_->UpdateTransform(*activeCam);
                }
            }
        }

        // デバッグUI(コンボ)の初期インデックス決定
        auto textureNames = texMgr->GetTextureNamesForDebug();
        auto it = std::find(textureNames.begin(), textureNames.end(), textureName);
        selectedTextureIndex_ =
            (it != textureNames.end()) ? static_cast<int>(std::distance(textureNames.begin(), it)) : 0;
    }
}

void Sprite::Update() {
    auto* camMgr = GetCameraManagerInstance();
    if (!resource_ || !camMgr) {
        return;
    }
    Camera* activeCam = camMgr->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    // アンカーの変更を頂点へ反映
    ApplyAnchorToVertices();

    // 基本的な行列更新の前にスケールを適用
    resource_->GetTransform().scale = {size_.x * uiScale_, size_.y * uiScale_, 1.0f};
    resource_->UpdateTransform(*activeCam);

    // UV 変換(flip → crop → userUV)
    if (resource_->GetMaterialData()) {
        // userUV: 既存の uvTransform(回転/スクロール)
        Irufemi::Matrix4x4 userUV =
            Irufemi::Math::MakeAffineMatrix(resource_->GetUVTransform().scale, resource_->GetUVTransform().rotate,
                                            resource_->GetUVTransform().translate);

        // crop: px指定 → 正規化UVに変換
        Irufemi::Matrix4x4 cropUV = Irufemi::Math::MakeIdentity4x4();
        if (useTexRect_ && textureSize_.x > 0.0f && textureSize_.y > 0.0f) {
            float u0 = texRectLeftTop_.x / textureSize_.x;
            float v0 = texRectLeftTop_.y / textureSize_.y;
            float du = texRectSize_.x / textureSize_.x;
            float dv = texRectSize_.y / textureSize_.y;
            cropUV = Irufemi::Math::MakeAffineMatrix(Irufemi::Vector3{du, dv, 1.0f}, Irufemi::Vector3{0.0f, 0.0f, 0.0f},
                                                     Irufemi::Vector3{u0, v0, 0.0f});
        }

        // flip を最初に、次に crop、最後に userUV を適用
        Irufemi::Vector3 flipScale{isFlipX_ ? -1.0f : 1.0f, isFlipY_ ? -1.0f : 1.0f, 1.0f};
        Irufemi::Vector3 flipTrans{isFlipX_ ? 1.0f : 0.0f, isFlipY_ ? 1.0f : 0.0f, 0.0f};
        Irufemi::Matrix4x4 flipUV =
            Irufemi::Math::MakeAffineMatrix(flipScale, Irufemi::Vector3{0.0f, 0.0f, 0.0f}, flipTrans);

        Irufemi::Matrix4x4 base = Irufemi::Math::Multiply(cropUV, userUV);
        resource_->GetMaterialData()->uvTransform = Irufemi::Math::Multiply(flipUV, base);
    }

    // フラグ更新
    isDirty_ = false;
    lastViewMatrix_ = activeCam->GetViewMatrix();
    lastProjectionMatrix_ = activeCam->GetOrthographicMatrix();
}

void Sprite::SyncBeforeDraw() {
    resource_->SyncBeforeDraw();
}

void Sprite::Draw() {
    auto* drawMgr = GetDrawManagerInstance();
    auto* camMgr = GetCameraManagerInstance();
    if (!resource_ || !drawMgr || !camMgr) {
        return;
    }
    Camera* activeCam = camMgr->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    // カメラの行列が変更されたか、オブジェクト自体が変更されたかチェック
    bool cameraChanged =
        (std::memcmp(&lastViewMatrix_, &activeCam->GetViewMatrix(), sizeof(Irufemi::Matrix4x4)) != 0 ||
         std::memcmp(&lastProjectionMatrix_, &activeCam->GetOrthographicMatrix(), sizeof(Irufemi::Matrix4x4)) != 0);

    if (isDirty_ || cameraChanged) {
        Update();
    }

    // --- 描画直前のバッファ同期 ---
    SyncBeforeDraw();

    if (isTopMost_) {
        drawMgr->SubmitTopMostSprite(resource_.get());
    } else {
        drawMgr->SubmitSprite(resource_.get());
    }
}

void Sprite::SetSize(float width, float height) {
    size_.x = width;
    size_.y = height;
    // 実サイズはscaleとuiScale_で表現
    if (resource_) {
        resource_->GetTransform().scale = {size_.x * uiScale_, size_.y * uiScale_, 1.0f};
    }
    isDirty_ = true;
}

Irufemi::Vector2 Sprite::GetPosition2D() const {
    if (!resource_) {
        return {0.0f, 0.0f};
    }
    return {resource_->GetTransform().translate.x, resource_->GetTransform().translate.y};
}

void Sprite::ApplyAnchorToVertices() {
    if (!resource_ || resource_->GetVertexDataList().size() < 4) {
        return;
    }

    // アンカーによるローカル頂点のずらし
    const float left = 0.0f - anchor_.x;
    const float right = 1.0f - anchor_.x;
    const float top = 0.0f - anchor_.y;
    const float bottom = 1.0f - anchor_.y;

    // 頂点の並び
    // 0: 左下, 1: 左上, 2: 右下, 3: 右上
    resource_->GetVertexDataList()[0].position = {left, bottom, 0.0f, 1.0f};
    resource_->GetVertexDataList()[1].position = {left, top, 0.0f, 1.0f};
    resource_->GetVertexDataList()[2].position = {right, bottom, 0.0f, 1.0f};
    resource_->GetVertexDataList()[3].position = {right, top, 0.0f, 1.0f};
}

bool Sprite::SetTextureRectPixels(int x, int y, int w, int h, bool autoResize) {
    if (textureSize_.x <= 0.0f || textureSize_.y <= 0.0f) {
        return false;
    }

    const int texW = static_cast<int>(textureSize_.x);
    const int texH = static_cast<int>(textureSize_.y);

    int sx = std::clamp(x, 0, texW);
    int sy = std::clamp(y, 0, texH);
    int ex = std::clamp(sx + std::max(w, 0), 0, texW);
    int ey = std::clamp(sy + std::max(h, 0), 0, texH);
    if (ex <= sx || ey <= sy) {
        return false;
    }

    texRectLeftTop_ = {static_cast<float>(sx), static_cast<float>(sy)};
    texRectSize_ = {static_cast<float>(ex - sx), static_cast<float>(ey - sy)};
    useTexRect_ = true;

    if (autoResize) {
        SetSize(texRectSize_.x, texRectSize_.y);
    }
    isDirty_ = true;
    return true;
}

void Sprite::ClearTextureRect() {
    useTexRect_ = false;
    texRectLeftTop_ = {0.0f, 0.0f};
    texRectSize_ = {0.0f, 0.0f};
    isDirty_ = true;
}

void Sprite::SetTexture(const std::string& textureName) {
    if (!resource_ || !textureManager_) {
        return;
    }

    if (resource_->GetTextureHandle().IsValid()) {
        textureManager_->ReleaseTexture(resource_->GetTextureHandle());
    }
    resource_->SetTextureHandle(textureManager_->LoadTexture(textureName));

    // テクスチャサイズを直接取得して描画サイズに反映
    uint32_t tw = 0, th = 0;
    if (textureManager_->GetTextureSize(textureName, tw, th) && tw > 0 && th > 0) {
        textureSize_ = {static_cast<float>(tw), static_cast<float>(th)};
        SetSize(textureSize_.x, textureSize_.y);

        // デバッグ用インデックスの更新
        auto textureNames = textureManager_->GetTextureNamesForDebug();
        auto it = std::find(textureNames.begin(), textureNames.end(), textureName);
        selectedTextureIndex_ =
            (it != textureNames.end()) ? static_cast<int>(std::distance(textureNames.begin(), it)) : 0;

        isDirty_ = true;
    }
}

std::string Sprite::GetTextureName() const {
    if (textureManager_) {
        auto names = textureManager_->GetTextureNamesForDebug();
        if (selectedTextureIndex_ >= 0 && selectedTextureIndex_ < static_cast<int>(names.size())) {
            return names[selectedTextureIndex_];
        }
    }
    return "";
}

void Sprite::AdjustTextureSize() {
    if (!textureManager_) {
        return;
    }

    auto names = textureManager_->GetTextureNamesForDebug();
    if (names.empty()) {
        return;
    }

    selectedTextureIndex_ = std::clamp(selectedTextureIndex_, 0, static_cast<int>(names.size()) - 1);

    uint32_t tw = 0, th = 0;
    if (textureManager_->GetTextureSize(names[selectedTextureIndex_], tw, th) && tw > 0 && th > 0) {
        textureSize_ = {static_cast<float>(tw), static_cast<float>(th)};
        SetSize(textureSize_.x, textureSize_.y);
    }
}

void Sprite::Debug([[maybe_unused]] const char* spriteName) {
#if defined USE_IMGUI
    std::string name = std::string("Sprite: ") + spriteName;
    ImGui::Begin(name.c_str());

    if (ui_ && resource_) {
        ui_->DebugTransform2D(resource_->GetTransform());
        ui_->DebugMaterialBy2D(resource_->GetMaterialData());
        ui_->DebugTexture(resource_.get(), selectedTextureIndex_);
        ui_->DebugUvTransform(resource_->GetUVTransform());

        ImGui::Checkbox("Flip X", &isFlipX_);
        ImGui::Checkbox("Flip Y", &isFlipY_);
        ImGui::Separator();

        float a[2] = {anchor_.x, anchor_.y};
        if (ImGui::SliderFloat2("Anchor (0..1)", a, 0.0f, 1.0f)) {
            SetAnchor(a[0], a[1]);
        }
        if (ImGui::SmallButton("TopLeft (0,0)")) {
            SetAnchor(0.0f, 0.0f);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Center (0.5,0.5)")) {
            SetAnchor(0.5f, 0.5f);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("BottomRight (1,1)")) {
            SetAnchor(1.0f, 1.0f);
        }

        float sz[2] = {size_.x, size_.y};
        if (ImGui::DragFloat2("Size (px)", sz, 1.0f, 1.0f, 8192.0f)) {
            SetSize(sz[0], sz[1]);
        }

        const float left = resource_->GetTransform().translate.x - anchor_.x * size_.x;
        const float top = resource_->GetTransform().translate.y - anchor_.y * size_.y;
        const float right = left + size_.x;
        const float bottom = top + size_.y;
        ImGui::Text("Rect L=%.1f T=%.1f R=%.1f B=%.1f", left, top, right, bottom);

        if (ImGui::CollapsingHeader("Texture Rect (px)")) {
            bool enabled = useTexRect_;
            if (ImGui::Checkbox("Enable", &enabled)) {
                useTexRect_ = enabled;
                if (!useTexRect_) {
                    ClearTextureRect();
                }
            }
            int lt[2] = {static_cast<int>(texRectLeftTop_.x), static_cast<int>(texRectLeftTop_.y)};
            int szpx[2] = {static_cast<int>(texRectSize_.x), static_cast<int>(texRectSize_.y)};
            bool changed = false;
            changed |= ImGui::DragInt2("LeftTop", lt, 1);
            changed |= ImGui::DragInt2("Size", szpx, 1);
            if (changed && enabled) {
                SetTextureRectPixels(lt[0], lt[1], std::max(1, szpx[0]), std::max(1, szpx[1]), false);
            }
            if (ImGui::SmallButton("Reset Full")) {
                ClearTextureRect();
            }
            ImGui::Text("TexSize: (%.0f, %.0f)", textureSize_.x, textureSize_.y);
        }
    }
    ImGui::End();
#endif
    Update();
}