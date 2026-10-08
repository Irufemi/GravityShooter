#pragma once
#include "Framework/Component/Component.h"
#include "Renderer/Object/2D/Primitive/Primitive2DObject.h"
#include "Core/Type/Primitive2DType.h"
#include <memory>
#include <string>
#include "Core/Math/Vector4.h"
#include "Core/Math/Vector2.h"

class TransformComponent;

/**
 * @class Primitive2DRendererComponent
 * @brief 2Dプリミティブ描画用コンポーネント
 * @details GameObjectにアタッチして汎用的な2Dプリミティブ（四角、円、線など）の描画とUI操作を提供します
 */
class Primitive2DRendererComponent : public Component {
public:
    Primitive2DRendererComponent();
    virtual ~Primitive2DRendererComponent();

    /**
     * @brief 初期化処理を実行します
     */
    void Initialize() override;

    /**
     * @brief 生成時の自己完結初期化（Primitive2DObjectの生成・初期プロパティ適用）を行います
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
     * @brief 描画マネージャへ2Dプリミティブ描画コマンドを登録します
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
     * @brief 描画可能な内部オブジェクト（Primitive2DObject）へのポインタを取得します
     * @return IRenderable インターフェースポインタ
     */
    IRenderable* GetRenderable() override {
        return primitive_.get();
    }

    // プロパティ操作
    /**
     * @brief 描画する2Dプリミティブの形状タイプ（Rect, Circle, Triangle, Line 等）を設定します
     * @param[in] type プリミティブタイプ
     */
    void SetShape(Irufemi::Primitive2DType type);

    /**
     * @brief 基本乗算カラー（RGBA）を設定します
     * @param[in] color 設定するカラー
     */
    void SetColor(const Irufemi::Vector4& color);

    /**
     * @brief 表面に貼り付けるテクスチャパスを設定します
     * @param[in] texturePath テクスチャファイルの相対パス
     */
    void SetTexture(const std::string& texturePath);

    /**
     * @brief ピボット（原点位置：0.0f〜1.0f）を設定します
     * @param[in] pivot ピボット座標
     */
    void SetPivot(const Irufemi::Vector2& pivot);

    /**
     * @brief 描画サイズ（幅・高さ）を設定します
     * @param[in] size 描画サイズ
     */
    void SetSize(const Irufemi::Vector2& size);

    /**
     * @brief 枠線の太さ（アウトライン描画時）を設定します
     * @param[in] thickness 線の太さ
     */
    void SetThickness(float thickness);

    /**
     * @brief 円などの曲線の分割数を設定します
     * @param[in] subdivision 分割数
     */
    void SetSubdivision(int subdivision);

    /**
     * @brief 最前面（UI TopMostレイヤー）に描画するかどうかを設定します
     * @param[in] isTopMost 最前面にする場合は true
     */
    void SetTopMost(bool isTopMost);

    // プロパティ取得（Editor用など）
    /**
     * @brief 形状タイプを取得します
     * @return プリミティブタイプ
     */
    Irufemi::Primitive2DType GetShape() const {
        return static_cast<Irufemi::Primitive2DType>(currentTypeIndex_);
    }

    /**
     * @brief 設定されているカラーを取得します
     * @return カラー値
     */
    const Irufemi::Vector4& GetColor() const {
        return color_;
    }

    /**
     * @brief 設定されているテクスチャパスを取得します
     * @return テクスチャパス文字列
     */
    const std::string& GetTexture() const {
        return texturePath_;
    }

    /**
     * @brief 設定されているピボットを取得します
     * @return ピボット座標
     */
    const Irufemi::Vector2& GetPivot() const {
        return pivot_;
    }

    /**
     * @brief 設定されている描画サイズを取得します
     * @return サイズ
     */
    const Irufemi::Vector2& GetSize() const {
        return size_;
    }

    /**
     * @brief 設定されている線の太さを取得します
     * @return 太さ
     */
    float GetThickness() const {
        return thickness_;
    }

    /**
     * @brief 曲線の分割数を取得します
     * @return 分割数
     */
    int GetSubdivision() const {
        return subdivision_;
    }

    /**
     * @brief 最前面描画が有効かどうかを取得します
     * @return 有効な場合は true
     */
    bool IsTopMost() const {
        return isTopMost_;
    }

    /**
     * @brief 内部の Primitive2DObject を取得します
     * @return Primitive2DObject 生ポインタ
     */
    Primitive2DObject* GetPrimitive() const {
        return primitive_.get();
    }

    /**
     * @brief コンポーネントの識別名を取得します
     * @return クラス名文字列
     */
    std::string GetComponentName() const override {
        return "Primitive2DRendererComponent";
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
    std::unique_ptr<Primitive2DObject> primitive_;
    int currentTypeIndex_ = 0; // デフォルトは Rect
    std::string texturePath_ = "";
    bool isTopMost_ = false;

    // 描画パラメータのバックアップ（Primitive2DObject生成前やSerialize用）
    Irufemi::Vector2 size_ = {100.0f, 100.0f};
    Irufemi::Vector2 pivot_ = {0.5f, 0.5f};
    Irufemi::Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f};
    float thickness_ = 2.0f;
    int subdivision_ = 32;
};
