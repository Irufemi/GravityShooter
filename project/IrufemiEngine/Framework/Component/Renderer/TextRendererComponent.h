#pragma once
#include "Framework/Component/Component.h"
#include "Renderer/Object/2D/Text/Text.h"
#include <memory>
#include <string>
#include "Core/Math/Vector4.h"

class TransformComponent;

/**
 * @class TextRendererComponent
 * @brief 2Dテキスト描画用コンポーネント (MSDF対応)
 */
class TextRendererComponent : public Component {
public:
    TextRendererComponent();
    virtual ~TextRendererComponent();

    /**
     * @brief 初期化処理を実行します
     */
    void Initialize() override;

    /**
     * @brief 生成時の自己完結初期化（Textオブジェクトの生成・フォントやアライメントの初期プロパティ適用）を行います
     */
    void OnAwake() override;

    /**
     * @brief スポーン時にワールド座標・Transform確定時の描画ステート同期を行います
     */
    void OnSpawned() override;

    /**
     * @brief TransformComponentの最新位置・回転・スケールを描画オブジェクトへ反映します
     */
    void SyncRenderState() override;

    /**
     * @brief 毎フレームの更新処理（描画ステート同期）を実行します
     */
    void Update() override;

    /**
     * @brief 描画マネージャへテキスト描画コマンドを登録します
     */
    void Draw() override;

    /**
     * @brief エディタモード中も更新を行うかを判定します
     * @return 常に true
     */
    bool CanUpdateInEditMode() const override {
        return true;
    }

    // エディタのRaycast用
    /**
     * @brief エディタピッキング用のレイキャスト判定を行います
     * @param[in] ray 判定用レイ
     * @param[out] outDistance ヒット時の距離
     * @return ヒットした場合は true
     */
    bool Raycast(const Irufemi::Ray& ray, float& outDistance) const override;

    /**
     * @brief 描画可能な内部オブジェクト（Text）へのポインタを取得します
     * @return IRenderable インターフェースポインタ
     */
    IRenderable* GetRenderable() override {
        return textObj_.get();
    }

    /**
     * @brief エディタ選択ハイライト用のアウトラインマスクを描画します
     */
    void DrawOutlineMask() override {
        if (textObj_) {
            textObj_->DrawOutlineMask();
        }
    }

    // 文字列の設定
    /**
     * @brief 描画するテキスト文字列（ワイド文字列）を設定します
     * @param[in] text 設定する文字列
     */
    void SetText(const std::wstring& text);

    /**
     * @brief 現在設定されているテキスト文字列を取得します
     * @return テキスト文字列
     */
    std::wstring GetText() const {
        return text_;
    }

    // フォントの変更
    /**
     * @brief 使用するフォントのアセットID（FontManager登録名）を設定します
     * @param[in] fontId フォントID
     */
    void SetFontId(const std::string& fontId);

    /**
     * @brief 現在設定されているフォントIDを取得します
     * @return フォントID
     */
    std::string GetFontId() const {
        return fontId_;
    }

    // ベーススケール（文字サイズ）
    /**
     * @brief 基本フォントサイズ（ピクセル相当スケール）を設定します
     * @param[in] baseScale フォントサイズ
     */
    void SetBaseScale(float baseScale);

    /**
     * @brief 基本フォントサイズを取得します
     * @return フォントサイズ
     */
    float GetBaseScale() const {
        return baseScale_;
    }

    // アライメント
    /**
     * @brief テキストの横揃えアライメント（左揃え・中央揃え・右揃え）を設定します
     * @param[in] align アライメント指定
     */
    void SetAlignment(TextAlignment align);

    /**
     * @brief テキストの横揃えアライメントを取得します
     * @return アライメント
     */
    TextAlignment GetAlignment() const {
        return alignment_;
    }

    /**
     * @brief テキストの縦揃えアライメント（上揃え・中央揃え・下揃え）を設定します
     * @param[in] vAlign 垂直アライメント指定
     */
    void SetVerticalAlignment(TextVerticalAlignment vAlign);

    /**
     * @brief テキストの縦揃えアライメントを取得します
     * @return 垂直アライメント
     */
    TextVerticalAlignment GetVerticalAlignment() const {
        return verticalAlignment_;
    }

    // 文字色
    /**
     * @brief テキストの乗算カラー（RGBA）を設定します
     * @param[in] color カラー値
     */
    void SetColor(const Irufemi::Vector4& color);

    /**
     * @brief テキストの乗算カラーを取得します
     * @return カラー値
     */
    Irufemi::Vector4 GetColor() const {
        return color_;
    }

    // UIとして最前面に描画するか
    /**
     * @brief 最前面（UI TopMostレイヤー）に描画するかどうかを設定します
     * @param[in] isTopMost 最前面にする場合は true
     */
    void SetTopMost(bool isTopMost);

    /**
     * @brief 最前面描画が有効かどうかを取得します
     * @return 有効な場合は true
     */
    bool IsTopMost() const {
        return isTopMost_;
    }

    // バウンディングボックス取得（ローカル座標系）
    /**
     * @brief テキスト全体のローカルバウンディングボックスの最小座標（左上等）を取得します
     * @return 最小座標
     */
    Irufemi::Vector2 GetLocalBoundsMin() const {
        return textObj_ ? textObj_->GetLocalBoundsMin() : Irufemi::Vector2{0.0f, 0.0f};
    }

    /**
     * @brief テキスト全体のローカルバウンディングボックスの最大座標（右下等）を取得します
     * @return 最大座標
     */
    Irufemi::Vector2 GetLocalBoundsMax() const {
        return textObj_ ? textObj_->GetLocalBoundsMax() : Irufemi::Vector2{0.0f, 0.0f};
    }

    /**
     * @brief 内部の Text オブジェクトを取得します
     * @return Text 生ポインタ
     */
    Text* GetTextObject() const {
        return textObj_.get();
    }

    /**
     * @brief コンポーネントの識別名を取得します
     * @return クラス名文字列
     */
    std::string GetComponentName() const override {
        return "TextRendererComponent";
    }

    /**
     * @brief リフレクションシステムへコンポーネントのプロパティを登録します
     */
    void OnRegisterProperties() override;

    /**
     * @brief JSONからコンポーネントデータを復元し、内部状態を即座に同期します
     * @param[in] j デシリアライズ元のJSONオブジェクト
     */
    void Deserialize(const nlohmann::json& j) override;

private:
    std::unique_ptr<Text> textObj_;
    std::wstring text_ = L"Text";
    std::string textU8_ = "Text"; // For Reflection
    std::string fontId_ = "MainFont";
    float baseScale_ = 64.0f;
    Irufemi::Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f};
    TextAlignment alignment_ = TextAlignment::Left;
    int alignmentInt_ = 0; // For Reflection (0:Left, 1:Center, 2:Right)
    TextVerticalAlignment verticalAlignment_ = TextVerticalAlignment::Top;
    int verticalAlignmentInt_ = 0; // For Reflection (0:Top, 1:Middle, 2:Bottom)
    bool isTopMost_ = false;
};
