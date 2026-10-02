#pragma once

#include "Renderer/System/Core/IRenderable.h"
#include <d3d12.h>
#include <vector>
#include <string>
#include <cstdint>
#include "Renderer/System/Core/Object2DResource.h"
#include "Core/Math/Vector2.h"
#include <wrl.h>
#include <memory>

class FontManager;
class DrawManager;
class DebugUI;
class CameraManager;

enum class TextAlignment { Left, Center, Right };
enum class TextVerticalAlignment { Top, Middle, Bottom };

/**
 * @class Text
 * @brief 2Dテキストを描画・管理するクラス (MSDF対応)
 * @details アライメント機能付き
 */
class Text : public IRenderable {
public:
    Text();
    ~Text() = default;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize(const std::string& fontId = "MainFont");
    /**
     * @brief Update を実行する。
     */
    void Update();
    /**
     * @brief SyncBeforeDraw を実行する。
     */
    void SyncBeforeDraw() override;
    /**
     * @brief Draw を実行する。
     */
    void Draw() override;
    /**
     * @brief DrawOutlineMask を実行する。
     */
    void DrawOutlineMask() override;

    // Setters
    /**
     * @brief Text を設定する。
     * @param[in] text 設定する Text の値
     */
    void SetText(const std::wstring& text);
    /**
     * @brief FontId を設定する。
     * @param[in] fontId 設定する FontId の値
     */
    void SetFontId(const std::string& fontId);
    /**
     * @brief Position を設定する。
     * @param[in] x 設定する Position の値
     * @param[in] y 設定する Position の値
     * @param[in] 0.0f 設定する Position の値
     */
    void SetPosition(const float& x, const float& y, const float& z = 0.0f) {
        if (resource_) {
            resource_->GetTransform().translate = {x, y, z};
        }
        isDirty_ = true;
    }
    /**
     * @brief Rotation を設定する。
     * @param[in] rotate 設定する Rotation の値
     */
    void SetRotation(const float& rotate) {
        if (resource_) {
            resource_->GetTransform().rotate = {0.0f, 0.0f, rotate};
        }
        isDirty_ = true;
    }
    /**
     * @brief Scale を設定する。
     * @param[in] scaleX 設定する Scale の値
     * @param[in] scaleY 設定する Scale の値
     */
    void SetScale(const float& scaleX, const float& scaleY) {
        if (resource_) {
            resource_->GetTransform().scale = {scaleX, scaleY, 1.0f};
        }
        isDirty_ = true;
    }
    /**
     * @brief Color を設定する。
     * @param[in] color 設定する Color の値
     */
    void SetColor(const Irufemi::Vector4& color) {
        color_ = color;
        if (resource_) {
            resource_->GetMaterialData()->color = color;
        }
        isDirty_ = true;
    }
    /**
     * @brief Color を取得する。
     * @return 取得された Color
     */
    Irufemi::Vector4 GetColor() const {
        return color_;
    }
    /**
     * @brief TopMost を設定する。
     * @param[in] isTopMost 設定する TopMost の値
     */
    void SetTopMost(bool isTopMost) {
        isTopMost_ = isTopMost;
    }
    /**
     * @brief IsTopMost かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsTopMost() const {
        return isTopMost_;
    }
    /**
     * @brief BaseScale を設定する。
     * @param[in] baseScale 設定する BaseScale の値
     */
    void SetBaseScale(float baseScale) {
        baseScale_ = baseScale;
        isTextDirty_ = true;
    }
    /**
     * @brief Alignment を設定する。
     * @param[in] align 設定する Alignment の値
     */
    void SetAlignment(TextAlignment align) {
        alignment_ = align;
        isTextDirty_ = true;
    }

    /**
     * @brief VerticalAlignment を設定する。
     * @param[in] vAlign 設定する VerticalAlignment の値
     */
    void SetVerticalAlignment(TextVerticalAlignment vAlign) {
        verticalAlignment_ = vAlign;
        isTextDirty_ = true;
    }

    /**
     * @brief D3D12Resource を取得する。
     * @return 取得された D3D12Resource
     */
    Object2DResource* GetD3D12Resource() {
        return resource_.get();
    }
    /**
     * @brief Text を取得する。
     * @return 取得された Text
     */
    const std::wstring& GetText() const {
        return text_;
    }
    /**
     * @brief FontId を取得する。
     * @return 取得された FontId
     */
    const std::string& GetFontId() const {
        return fontId_;
    }
    /**
     * @brief BaseScale を取得する。
     * @return 取得された BaseScale
     */
    float GetBaseScale() const {
        return baseScale_;
    }
    /**
     * @brief Alignment を取得する。
     * @return 取得された Alignment
     */
    TextAlignment GetAlignment() const {
        return alignment_;
    }

    /**
     * @brief VerticalAlignment を取得する。
     * @return 取得された VerticalAlignment
     */
    TextVerticalAlignment GetVerticalAlignment() const {
        return verticalAlignment_;
    }

    /**
     * @brief LocalBoundsMin を取得する。
     * @return 取得された LocalBoundsMin
     */
    const Irufemi::Vector2& GetLocalBoundsMin() const {
        return localBoundsMin_;
    }
    /**
     * @brief LocalBoundsMax を取得する。
     * @return 取得された LocalBoundsMax
     */
    const Irufemi::Vector2& GetLocalBoundsMax() const {
        return localBoundsMax_;
    }

    // Engine dependencies
    /**
     * @brief インスタンス固有の FontManager を設定する。
     * @param[in] fm 設定する FontManager のポインタ
     */
    void SetFontManagerInstance(FontManager* fm) {
        instFontManager_ = fm;
    }

    /**
     * @brief インスタンス固有の DrawManager を設定する。
     * @param[in] dm 設定する DrawManager のポインタ
     */
    void SetDrawManagerInstance(DrawManager* dm) {
        instDrawManager_ = dm;
    }

    /**
     * @brief インスタンス固有の CameraManager を設定する。
     * @param[in] cm 設定する CameraManager のポインタ
     */
    void SetCameraManagerInstance(CameraManager* cm) {
        instCameraManager_ = cm;
    }

    /**
     * @brief 使用する FontManager を取得する（インスタンスメンバ優先）。
     * @return FontManager のポインタ
     */
    FontManager* GetFontManagerInstance() const {
        return instFontManager_ ? instFontManager_ : fontManager_;
    }

    /**
     * @brief 使用する DrawManager を取得する（インスタンスメンバ優先）。
     * @return DrawManager のポインタ
     */
    DrawManager* GetDrawManagerInstance() const {
        return instDrawManager_ ? instDrawManager_ : drawManager_;
    }

    /**
     * @brief 使用する CameraManager を取得する（インスタンスメンバ優先）。
     * @return CameraManager のポインタ
     */
    CameraManager* GetCameraManagerInstance() const {
        return instCameraManager_ ? instCameraManager_ : cameraManager_;
    }

    static void SetFontManager(FontManager* fm) {
        fontManager_ = fm;
    }
    /**
     * @brief FontManager を取得する。
     * @return 取得された FontManager
     */
    static FontManager* GetFontManager() {
        return fontManager_;
    }
    /**
     * @brief DrawManager を設定する。
     * @param[in] dm 設定する DrawManager の値
     */
    static void SetDrawManager(DrawManager* dm) {
        drawManager_ = dm;
    }
    /**
     * @brief CameraManager を設定する。
     * @param[in] cm 設定する CameraManager の値
     */
    static void SetCameraManager(CameraManager* cm) {
        cameraManager_ = cm;
    }
    /**
     * @brief DebugUI を設定する。
     * @param[in] ui 設定する DebugUI の値
     */
    static void SetDebugUI(DebugUI* ui) {
        ui_ = ui;
    }

private:
    FontManager* instFontManager_ = nullptr;
    DrawManager* instDrawManager_ = nullptr;
    CameraManager* instCameraManager_ = nullptr;
    /**
     * @brief GenerateVertices を実行する。
     */
    void GenerateVertices();

    std::unique_ptr<Object2DResource> resource_ = nullptr;
    std::wstring text_ = L"";
    std::string fontId_ = "MainFont";
    float baseScale_ = 64.0f; // MSDF生成時のピクセルサイズを基準とするスケーリング
    TextAlignment alignment_ = TextAlignment::Left;
    TextVerticalAlignment verticalAlignment_ = TextVerticalAlignment::Top;
    Irufemi::Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f};

    Irufemi::Vector2 localBoundsMin_ = {0.0f, 0.0f};
    Irufemi::Vector2 localBoundsMax_ = {0.0f, 0.0f};

    bool isDirty_ = true;
    bool isTextDirty_ = true;
    bool isTopMost_ = false;
    bool hasPendingGlyphs_ = false;
    uint64_t lastAtlasVersion_ = 0;

    static FontManager* fontManager_;
    static DrawManager* drawManager_;
    static CameraManager* cameraManager_;
    static DebugUI* ui_;

    Irufemi::Matrix4x4 lastViewMatrix_ = {};
    Irufemi::Matrix4x4 lastProjectionMatrix_ = {};
    ResourceHandle lastAtlasHandle_ = {};
};
