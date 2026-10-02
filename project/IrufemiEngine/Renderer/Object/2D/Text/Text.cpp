#include "Renderer/Object/2D/Text/Text.h"
#include "Renderer/Font/FontManager.h"
#include "Renderer/DrawManager.h"
#include "Renderer/Camera/Camera.h"
#include "Renderer/Camera/CameraManager.h"
#include "Core/Math/Math.h"
#include "Framework/UI/DebugUI.h"
#include <algorithm>

FontManager* Text::fontManager_ = nullptr;
DrawManager* Text::drawManager_ = nullptr;
CameraManager* Text::cameraManager_ = nullptr;
DebugUI* Text::ui_ = nullptr;

Text::Text() {}

void Text::Initialize(const std::string& fontId) {
    resource_ = std::make_unique<Object2DResource>();
    fontId_ = fontId;
    isTextDirty_ = true;

    // マテリアルの初期化
    if (resource_->GetMaterialData()) {
        resource_->GetMaterialData()->color = {1.0f, 1.0f, 1.0f, 1.0f};
        resource_->GetMaterialData()->enableLighting = false;
        resource_->GetMaterialData()->hasTexture = true;
        resource_->GetMaterialData()->lightingMode = 2; // Unlit相当
        resource_->GetMaterialData()->uvTransform = Irufemi::Math::MakeIdentity4x4();
    }
}

void Text::SetText(const std::wstring& text) {
    if (text_ != text) {
        text_ = text;
        isTextDirty_ = true;
        isDirty_ = true;
    }
}

void Text::SetFontId(const std::string& fontId) {
    if (fontId_ != fontId) {
        fontId_ = fontId;
        isTextDirty_ = true;
        isDirty_ = true;
    }
}

void Text::GenerateVertices() {
    auto* fm = GetFontManagerInstance();
    if (!fm) {
        return;
    }

    // 文字が存在しない可能性があるため、まずは非同期生成要求をかける
    fm->PrecacheText(fontId_, text_);

    resource_->GetVertexDataList().clear();
    resource_->GetIndexDataList().clear();

    if (text_.empty()) {
        localBoundsMin_ = {0.0f, 0.0f};
        localBoundsMax_ = {0.0f, 0.0f};
        return;
    }

    std::vector<float> lineWidths;
    float currentLineWidth = 0.0f;
    float scaleFactor = baseScale_ / 32.0f; // GLYPH_SIZE(32)を基準にスケール

    bool hasPendingGlyphs = false;

    for (wchar_t c : text_) {
        if (c == L'\n') {
            lineWidths.push_back(currentLineWidth);
            currentLineWidth = 0.0f;
            continue;
        }
        const auto glyph = fm->GetGlyph(fontId_, c);
        if (glyph) {
            if (glyph->width < 0.0f) {
                hasPendingGlyphs = true;
            }
            currentLineWidth += glyph->advanceX * scaleFactor;
        }
    }
    lineWidths.push_back(currentLineWidth);

    auto getStartX = [&](int lIndex) {
        if (alignment_ == TextAlignment::Center) {
            return -lineWidths[lIndex] * 0.5f;
        } else if (alignment_ == TextAlignment::Right) {
            return -lineWidths[lIndex];
        }
        return 0.0f; // Left
    };

    int lineIndex = 0;
    float currentX = getStartX(lineIndex);
    float currentY = 0.0f;

    float minX = (std::numeric_limits<float>::max)();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = (std::numeric_limits<float>::max)();
    float maxY = std::numeric_limits<float>::lowest();

    for (wchar_t c : text_) {
        if (c == L'\n') {
            lineIndex++;
            currentX = getStartX(lineIndex);
            currentY += scaleFactor * 32.0f * 1.2f; // 行間 (Y下方向が正)
            continue;
        }

        const auto glyph = fm->GetGlyph(fontId_, c);
        if (!glyph) {
            continue; // 未知の文字
        }

        if (glyph->width < 0.0f) {
            // ダミーキャッシュ(非同期生成中)
            hasPendingGlyphs = true;
            currentX += glyph->advanceX * scaleFactor;
            continue;
        } else if (glyph->width == 0.0f || glyph->height == 0.0f) {
            // スペースなどの空白文字
            currentX += glyph->advanceX * scaleFactor;
            continue;
        }

        // MSDFのbitmapはFontManagerで上下反転してAtlasに格納されているため、
        // uvTopLeft.y が「文字の上端」、uvBottomRight.y が「文字の下端」になります。
        // Screen座標はYが下が正なので、top < bottom となります。
        float left = currentX + glyph->offsetX * scaleFactor;
        float right = left + glyph->width * scaleFactor;
        float top = currentY + glyph->offsetY * scaleFactor;
        float bottom = top + glyph->height * scaleFactor;

        minX = (std::min)(minX, left);
        maxX = (std::max)(maxX, right);
        minY = (std::min)(minY, top);
        maxY = (std::max)(maxY, bottom);

        uint32_t startIndex = static_cast<uint32_t>(resource_->GetVertexDataList().size());

        // 頂点の並び: 左下(0), 左上(1), 右下(2), 右上(3)
        // Spriteコンポーネントに合わせて法線は Z=-1
        resource_->GetVertexDataList().push_back({{left, bottom, 0.0f, 1.0f},
                                                  {glyph->uvTopLeft.x, glyph->uvBottomRight.y},
                                                  {0.0f, 0.0f, -1.0f},
                                                  {1.0f, 1.0f, 1.0f, 1.0f}});
        resource_->GetVertexDataList().push_back({{left, top, 0.0f, 1.0f},
                                                  {glyph->uvTopLeft.x, glyph->uvTopLeft.y},
                                                  {0.0f, 0.0f, -1.0f},
                                                  {1.0f, 1.0f, 1.0f, 1.0f}});
        resource_->GetVertexDataList().push_back({{right, bottom, 0.0f, 1.0f},
                                                  {glyph->uvBottomRight.x, glyph->uvBottomRight.y},
                                                  {0.0f, 0.0f, -1.0f},
                                                  {1.0f, 1.0f, 1.0f, 1.0f}});
        resource_->GetVertexDataList().push_back({{right, top, 0.0f, 1.0f},
                                                  {glyph->uvBottomRight.x, glyph->uvTopLeft.y},
                                                  {0.0f, 0.0f, -1.0f},
                                                  {1.0f, 1.0f, 1.0f, 1.0f}});

        resource_->GetIndexDataList().push_back(startIndex + 0);
        resource_->GetIndexDataList().push_back(startIndex + 1);
        resource_->GetIndexDataList().push_back(startIndex + 2);
        resource_->GetIndexDataList().push_back(startIndex + 1);
        resource_->GetIndexDataList().push_back(startIndex + 3);
        resource_->GetIndexDataList().push_back(startIndex + 2);

        currentX += glyph->advanceX * scaleFactor;
    }

    if (minX <= maxX && minY <= maxY) {
        localBoundsMin_ = {minX, minY};
        localBoundsMax_ = {maxX, maxY};

        // Center アライメント時、描画クアッド全体の幾何学的中心が X=0.0f (オブジェクト中心) に一致するよう補正
        if (alignment_ == TextAlignment::Center) {
            float geometricCenterX = (minX + maxX) * 0.5f;
            for (auto& vertex : resource_->GetVertexDataList()) {
                vertex.position.x -= geometricCenterX;
            }
            localBoundsMin_.x -= geometricCenterX;
            localBoundsMax_.x -= geometricCenterX;
        }

        // 垂直アライメント補正
        if (verticalAlignment_ == TextVerticalAlignment::Middle) {
            float geometricCenterY = (minY + maxY) * 0.5f;
            for (auto& vertex : resource_->GetVertexDataList()) {
                vertex.position.y -= geometricCenterY;
            }
            localBoundsMin_.y -= geometricCenterY;
            localBoundsMax_.y -= geometricCenterY;
        } else if (verticalAlignment_ == TextVerticalAlignment::Bottom) {
            float offsetY = maxY;
            for (auto& vertex : resource_->GetVertexDataList()) {
                vertex.position.y -= offsetY;
            }
            localBoundsMin_.y -= offsetY;
            localBoundsMax_.y -= offsetY;
        }
    } else {
        localBoundsMin_ = {0.0f, 0.0f};
        localBoundsMax_ = {0.0f, 0.0f};
    }

    hasPendingGlyphs_ = hasPendingGlyphs;

    // SRVを設定
    if (fm) {
        resource_->SetTextureHandle(fm->GetAtlasHandle());
    }

    // 頂点がなければ終了
    if (resource_->GetVertexDataList().empty()) {
        return;
    }

    // GPUリソースの再生成(文字数によって頂点数が可変なため、毎回バッファを作り直すか拡張する)
    resource_->CreateResource();
    resource_->Map();
    if (resource_->GetVertexData()) {
        std::copy(resource_->GetVertexDataList().begin(), resource_->GetVertexDataList().end(),
                  resource_->GetVertexData());
    }
    if (resource_->GetIndexData()) {
        std::copy(resource_->GetIndexDataList().begin(), resource_->GetIndexDataList().end(),
                  resource_->GetIndexData());
    }
}

void Text::Update() {
    auto* fm = GetFontManagerInstance();
    auto* camMgr = GetCameraManagerInstance();
    if (!resource_ || !camMgr || !fm) {
        return;
    }

    // AtlasのSRVが変わったか(リビルドされた等)、テキストに変更があった場合は再生成
    ResourceHandle currentAtlas = fm->GetAtlasHandle();
    if (lastAtlasHandle_ != currentAtlas) {
        isTextDirty_ = true;
        lastAtlasHandle_ = currentAtlas;
    }

    // 非同期生成中のグリフがあり、アトラスがGPU更新された場合は再試行
    uint64_t currentAtlasVersion = fm->GetAtlasVersion();
    if (hasPendingGlyphs_ && lastAtlasVersion_ != currentAtlasVersion) {
        isTextDirty_ = true;
        lastAtlasVersion_ = currentAtlasVersion;
    }

    if (isTextDirty_) {
        isTextDirty_ = false;
        GenerateVertices();
        lastAtlasVersion_ = fm->GetAtlasVersion();
        isDirty_ = true;
    }

    Camera* activeCam = camMgr->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    if (isDirty_) {
        resource_->UpdateTransform(*activeCam);
        isDirty_ = false;
    }

    lastViewMatrix_ = activeCam->GetViewMatrix();
    lastProjectionMatrix_ = activeCam->GetOrthographicMatrix();
}

void Text::SyncBeforeDraw() {
    if (resource_) {
        resource_->SyncBeforeDraw();
    }
}

void Text::Draw() {
    auto* drawMgr = GetDrawManagerInstance();
    auto* camMgr = GetCameraManagerInstance();
    if (!resource_ || !drawMgr || !camMgr) {
        return;
    }
    Camera* activeCam = camMgr->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    bool cameraChanged =
        (std::memcmp(&lastViewMatrix_, &activeCam->GetViewMatrix(), sizeof(Irufemi::Matrix4x4)) != 0 ||
         std::memcmp(&lastProjectionMatrix_, &activeCam->GetOrthographicMatrix(), sizeof(Irufemi::Matrix4x4)) != 0);

    if (isDirty_ || cameraChanged || isTextDirty_) {
        Update();
    }

    SyncBeforeDraw();

    if (resource_->GetVertexDataList().empty()) {
        return; // 描画するものがなければスキップ
    }

    // TextRenderer用の描画キューに送信
    if (isTopMost_) {
        drawMgr->SubmitTopMostText(resource_.get());
    } else {
        drawMgr->SubmitText(resource_.get());
    }
}

void Text::DrawOutlineMask() {
    auto* drawMgr = GetDrawManagerInstance();
    auto* camMgr = GetCameraManagerInstance();
    if (!resource_ || !drawMgr || !camMgr) {
        return;
    }
    Camera* activeCam = camMgr->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    if (isDirty_ || isTextDirty_) {
        Update();
    }
    SyncBeforeDraw();

    if (resource_->GetVertexDataList().empty()) {
        return;
    }

    drawMgr->SubmitTextOutlineMask(resource_.get());
}
