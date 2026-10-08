#pragma once
#include "Framework/Component/Component.h"
#include <memory>
#include <string>
#include "Core/Type/PrimitiveType.h"
#include "Core/Shape/Sphere.h"

// 前方宣言
class Primitive3DObject;
class TransformComponent;

/**
 * @class PrimitiveRendererComponent
 * @brief 3D基本プリミティブ描画用レンダラーコンポーネント
 * @details Cube, Sphere, Cylinder, Capsule, Torus などの3D基本形状を動的生成し、
 *          PBRマテリアルパラメータ（Metallic, Roughness等）やライティング設定を適用して描画します。
 */
class PrimitiveRendererComponent : public Component {
public:
    PrimitiveRendererComponent();
    ~PrimitiveRendererComponent() override;

    /**
     * @brief 初期化処理を実行します
     */
    void Initialize() override;

    /**
     * @brief 生成時の自己完結初期化（Primitive3DObjectの生成・メッシュ構築）を行います
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
     * @brief 描画マネージャへプリミティブ描画コマンドを登録します
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
     * @brief 描画可能な内部オブジェクト（Primitive3DObject）へのポインタを取得します
     * @return IRenderable インターフェースポインタ
     */
    IRenderable* GetRenderable() override;

    // エディタのRaycast用
    /**
     * @brief エディタピッキング用のワールドバウンディングスフィアを取得します
     * @return バウンディングスフィア
     */
    Irufemi::Sphere GetWorldSphere() const;

    /**
     * @brief エディタピッキング用のレイキャスト判定を行います
     * @param[in] ray 判定用レイ
     * @param[out] outDistance ヒット時の距離
     * @return ヒットした場合は true
     */
    bool Raycast(const Irufemi::Ray& ray, float& outDistance) const override;

    /**
     * @brief コンポーネントの識別名を取得します
     * @return クラス名文字列
     */
    std::string GetComponentName() const override {
        return "PrimitiveRendererComponent";
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

    // プロパティ操作
    /**
     * @brief 描画するプリミティブ形状のタイプを設定し、メッシュを再構築します
     * @param[in] type プリミティブ形状タイプ（Cube, Sphere 等）
     */
    void SetShape(Irufemi::PrimitiveType type);

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
     * @brief ライティング計算を有効にするかどうかを設定します
     * @param[in] enable 有効にする場合は true
     */
    void SetEnableLighting(bool enable);

    /**
     * @brief ライティングモデル（Lambert, Half-Lambert, Phong, PBR等）を設定します
     * @param[in] mode ライティングモード番号
     */
    void SetLightingMode(int mode);

    /**
     * @brief PBR メタリック（金属度：0.0f〜1.0f）を設定します
     * @param[in] metallic メタリック値
     */
    void SetMetallic(float metallic);

    /**
     * @brief PBR ラフネス（粗さ：0.0f〜1.0f）を設定します
     * @param[in] roughness ラフネス値
     */
    void SetRoughness(float roughness);

    /**
     * @brief アルファテストの基準値（カットオフ閾値）を設定します
     * @param[in] alphaRef 閾値
     */
    void SetAlphaReference(float alphaRef);

    /**
     * @brief テクスチャサンプラーを Clamp に固定するかどうかを設定します
     * @param[in] useClamp Clamp サンプラーを使用する場合は 1
     */
    void SetUseClampSampler(int32_t useClamp);

    /**
     * @brief 現在の形状設定・分割数パラメータに基づいて 3D メッシュを再生成します
     */
    void RebuildMesh();

    Primitive3DObject* GetPrimitive() const {
        return primitive_.get();
    }
    int GetCurrentTypeIndex() const {
        return currentTypeIndex_;
    }
    float GetRadius() const {
        return radius_;
    }
    void SetRadius(float radius) {
        radius_ = radius;
    }
    int GetSubdivisions() const {
        return subdivisions_;
    }
    void SetSubdivisions(int subdivisions) {
        subdivisions_ = subdivisions;
    }
    float GetHeight() const {
        return height_;
    }
    void SetHeight(float height) {
        height_ = height;
    }
    float GetTopRadius() const {
        return topRadius_;
    }
    void SetTopRadius(float topRadius) {
        topRadius_ = topRadius;
    }
    float GetBottomRadius() const {
        return bottomRadius_;
    }
    void SetBottomRadius(float bottomRadius) {
        bottomRadius_ = bottomRadius;
    }
    bool HasTop() const {
        return hasTop_;
    }
    void SetHasTop(bool hasTop) {
        hasTop_ = hasTop;
    }
    bool HasBottom() const {
        return hasBottom_;
    }
    void SetHasBottom(bool hasBottom) {
        hasBottom_ = hasBottom;
    }
    float GetTorusMajorRadius() const {
        return torusMajorRadius_;
    }
    void SetTorusMajorRadius(float r) {
        torusMajorRadius_ = r;
    }
    float GetTorusMinorRadius() const {
        return torusMinorRadius_;
    }
    void SetTorusMinorRadius(float r) {
        torusMinorRadius_ = r;
    }
    int GetTorusMajorSegments() const {
        return torusMajorSegments_;
    }
    void SetTorusMajorSegments(int s) {
        torusMajorSegments_ = s;
    }
    int GetTorusMinorSegments() const {
        return torusMinorSegments_;
    }
    void SetTorusMinorSegments(int s) {
        torusMinorSegments_ = s;
    }

private:
    std::unique_ptr<Primitive3DObject> primitive_;
    int currentTypeIndex_ = 2; // デフォルトは Cube (2)

    // --- メッシュ生成用パラメータ ---
    // 汎用
    float radius_ = 1.0f;
    int subdivisions_ = 16;
    float height_ = 1.0f;

    // Irufemi::Cylinder / Cone 用
    float topRadius_ = 1.0f;
    float bottomRadius_ = 1.0f;
    bool hasTop_ = true;
    bool hasBottom_ = true;

    // Torus 用
    float torusMajorRadius_ = 1.0f;
    float torusMinorRadius_ = 0.3f;
    int torusMajorSegments_ = 32;
    int torusMinorSegments_ = 16;
};
