#pragma once
#include "Framework/Component/Component.h"
#include "Renderer/Object/2D/Sprite/Sprite.h"
#include <memory>
#include <string>
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector4.h"

class TransformComponent;

/**
 * @class SpriteRendererComponent
 * @brief 2Dスプライト描画用コンポーネント
 * @details 2D画像テクスチャのアンカー・フリップ・カラー・サイズ管理を行い、ワールドトランスフォームに同期して描画します
 */
class SpriteRendererComponent : public Component {
public:
    SpriteRendererComponent();
    virtual ~SpriteRendererComponent();

    /**
     * @brief 初期化処理を実行します
     */
    void Initialize() override;

    /**
     * @brief 生成時の自己完結初期化（Spriteオブジェクトの生成・初期プロパティ適用）を行います
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
     * @brief 描画マネージャへスプライト描画コマンドを登録します
     */
    void Draw() override;

    /**
     * @brief エディタモード中も更新を行うかを判定します
     * @return 常に true
     */
    bool CanUpdateInEditMode() const override {
        return true;
    }

    /**
     * @brief 描画可能な内部オブジェクト（Sprite）へのポインタを取得します
     * @return IRenderable インターフェースポインタ
     */
    IRenderable* GetRenderable() override {
        return sprite_.get();
    }

    /**
     * @brief 描画するテクスチャパスを取得します
     * @return テクスチャパス文字列
     */
    const std::string& GetTexturePath() const {
        return texturePath_;
    }

    /**
     * @brief 最前面描画が有効かどうかを取得します
     * @return 最前面描画が有効なら true
     */
    bool IsTopMost() const {
        return isTopMost_;
    }

    /**
     * @brief 最前面描画の有効/無効を設定します
     * @param[in] isTopMost 最前面描画フラグ
     */
    void SetTopMost(bool isTopMost);

    /**
     * @brief 水平反転が有効かどうかを取得します
     * @return 水平反転が有効なら true
     */
    bool IsFlipX() const {
        return isFlipX_;
    }

    /**
     * @brief 水平反転の有効/無効を設定します
     * @param[in] isFlipX 水平反転フラグ
     */
    void SetFlipX(bool isFlipX);

    /**
     * @brief 垂直反転が有効かどうかを取得します
     * @return 垂直反転が有効なら true
     */
    bool IsFlipY() const {
        return isFlipY_;
    }

    /**
     * @brief 垂直反転の有効/無効を設定します
     * @param[in] isFlipY 垂直反転フラグ
     */
    void SetFlipY(bool isFlipY);

    /**
     * @brief 描画するテクスチャパスを設定します
     * @param[in] texturePath 読み込むテクスチャの相対パス
     */
    void SetTexture(const std::string& texturePath);

    /**
     * @brief 内部の Sprite オブジェクトを取得します
     * @return Sprite オブジェクトの生ポインタ
     */
    Sprite* GetSprite() const {
        return sprite_.get();
    }

    /**
     * @brief アンカーポイント（原点位置：0.0f〜1.0f）を設定します
     * @param[in] anchor 設定するアンカー座標
     */
    void SetAnchor(const Irufemi::Vector2& anchor);

    /**
     * @brief アンカーポイントを取得します
     * @return 現在のアンカー座標
     */
    const Irufemi::Vector2& GetAnchor() const {
        return anchor_;
    }

    /**
     * @brief ベース描画サイズを設定します
     * @param[in] size 設定するサイズ（幅・高さ）
     */
    void SetBaseSize(const Irufemi::Vector2& size);

    /**
     * @brief ベース描画サイズを取得します
     * @return 現在のベースサイズ
     */
    const Irufemi::Vector2& GetBaseSize() const {
        return size_;
    }

    /**
     * @brief スプライトの基準カラーを取得します
     * @return 現在設定されているカラー
     */
    const Irufemi::Vector4& GetColor() const {
        return color_;
    }

    /**
     * @brief スプライトの基準カラーを設定します
     * @param[in] color 設定するカラー
     */
    void SetColor(const Irufemi::Vector4& color) {
        color_ = color;
        if (sprite_) {
            sprite_->SetColor(color_);
        }
    }

    /**
     * @brief コンポーネントの識別名を取得します
     * @return クラス名文字列
     */
    std::string GetComponentName() const override {
        return "SpriteRendererComponent";
    }

    /**
     * @brief コンポーネントの状態を JSON にシリアライズします
     * @return シリアライズされた JSON オブジェクト
     */
    nlohmann::json Serialize() override;

    /**
     * @brief JSON からコンポーネントの状態を復元します
     * @param[in] j 読み込む JSON オブジェクト
     */
    void Deserialize(const nlohmann::json& j) override;

private:
    std::unique_ptr<Sprite> sprite_;
    std::string texturePath_ = "";
    bool isTopMost_ = false;
    bool isFlipX_ = false;
    bool isFlipY_ = false;
    Irufemi::Vector2 anchor_{0.5f, 0.5f};
    Irufemi::Vector2 size_{640.0f, 360.0f};
    Irufemi::Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f};
};
