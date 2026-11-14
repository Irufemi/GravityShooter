#pragma once

#include "Renderer/System/Core/IRenderable.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "Core/Type/Primitive2DType.h"
#include "Renderer/System/Core/Object2DResource.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"

// 前方宣言
namespace Irufemi {
class TextureManager;
}
class DrawManager;
class DebugUI;

/**
 * @class Primitive2DObject
 * @brief 汎用的な2Dプリミティブ（四角形、円、線など）を管理・描画するクラス
 * @details SizeとScaleを分離し、Pivot（原点）をベースにメッシュを再構築する設計。
 */
class Primitive2DObject : public IRenderable {
public:
    Primitive2DObject() = default;
    ~Primitive2DObject() = default;

    /**
     * @brief 初期化処理
     * @param[in] type 初期形状タイプ
     * @param[in] textureName 使用するテクスチャのパス（デフォルトは白テクスチャ推奨）
     */
    void Initialize(Irufemi::Primitive2DType type = Irufemi::Primitive2DType::Rect,
                    const std::string& textureName = "resources/whiteTexture.png");

    /**
     * @brief 更新処理
     */
    void Update();

    /**
     * @brief 描画コマンド発行前の同期処理
     */
    void SyncBeforeDraw() override;

    /**
     * @brief 描画処理
     */
    void Draw() override;

    /**
     * @brief デバッグ・編集用UIを表示する
     * @param[in] label UIウィンドウおよび識別用のラベル
     */
    void Debug(const char* label = "Primitive 2D Object");

    // --- アクセサ・ゲッター ---
    /**
     * @brief D3D12Resource を取得する。
     * @return 取得された D3D12Resource
     */
    Object2DResource* GetD3D12Resource() {
        return resource_.get();
    }
    /**
     * @brief Shape を取得する。
     * @return 取得された Shape
     */
    Irufemi::Primitive2DType GetShape() const {
        return type_;
    }
    /**
     * @brief Size を取得する。
     * @return 取得された Size
     */
    const Irufemi::Vector2& GetSize() const {
        return size_;
    }
    /**
     * @brief Pivot を取得する。
     * @return 取得された Pivot
     */
    const Irufemi::Vector2& GetPivot() const {
        return pivot_;
    }
    /**
     * @brief Position を取得する。
     * @return 取得された Position
     */
    const Irufemi::Vector3& GetPosition() const {
        return resource_->GetTransform().translate;
    }
    /**
     * @brief Rotation を取得する。
     * @return 取得された Rotation
     */
    const Irufemi::Vector3& GetRotation() const {
        return resource_->GetTransform().rotate;
    }
    /**
     * @brief Scale を取得する。
     * @return 取得された Scale
     */
    const Irufemi::Vector3& GetScale() const {
        return resource_->GetTransform().scale;
    }
    /**
     * @brief Color を取得する。
     * @return 取得された Color
     */
    const Irufemi::Vector4& GetColor() const {
        return resource_->GetMaterialData()->color;
    }
    /**
     * @brief Subdivision を取得する。
     * @return 取得された Subdivision
     */
    uint32_t GetSubdivision() const {
        return subdivision_;
    }
    /**
     * @brief Thickness を取得する。
     * @return 取得された Thickness
     */
    float GetThickness() const {
        return thickness_;
    }
    /**
     * @brief IsTopMost かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsTopMost() const {
        return isTopMost_;
    }
    /**
     * @brief TextureHandle を取得する。
     * @return 取得された TextureHandle
     */
    Irufemi::ResourceHandle GetTextureHandle() const {
        return resource_ ? resource_->GetTextureHandle() : Irufemi::ResourceHandle();
    }

    // --- プロパティのセッター ---

    /**
     * @brief 形状を変更し、必要に応じてメッシュを再構築する
     */
    void SetShape(Irufemi::Primitive2DType type);

    /**
     * @brief 基準サイズを設定する（ピクセル単位などを想定）
     */
    void SetSize(const Irufemi::Vector2& size);

    /**
     * @brief ピボット（アンカー）を設定する（0.0～1.0）
     * @details 左上={0,0}, 中心={0.5,0.5}, 右下={1,1} など
     */
    void SetPivot(const Irufemi::Vector2& pivot);

    /**
     * @brief 位置（World Irufemi::Transform）を設定する
     */
    void SetPosition(const Irufemi::Vector3& position);
    /**
     * @brief Position を設定する。
     * @param[in] position 設定する Position の値
     */
    void SetPosition(const Irufemi::Vector2& position) {
        SetPosition({position.x, position.y, 0.0f});
    }

    /**
     * @brief 回転（Z軸のみ想定）を設定する
     */
    void SetRotationZ(float rad);

    /**
     * @brief スケール（倍率）を設定する
     */
    void SetScale(const Irufemi::Vector3& scale);
    /**
     * @brief Scale を設定する。
     * @param[in] scale 設定する Scale の値
     */
    void SetScale(const Irufemi::Vector2& scale) {
        SetScale({scale.x, scale.y, 1.0f});
    }

    /**
     * @brief ベースカラーを設定する
     */
    void SetColor(const Irufemi::Vector4& color);

    /**
     * @brief テクスチャを設定する
     */
    void SetTexture(const std::string& textureName);

    /**
     * @brief 最前面描画（UIなど）のフラグを設定する
     */
    void SetTopMost(bool isTopMost) {
        isTopMost_ = isTopMost;
    }

    /**
     * @brief リング形状などでの「太さ（線幅など）」を設定する（ピクセル単位等）
     */
    void SetThickness(float thickness);

    /**
     * @brief 円やリングなどの頂点分割数を設定する
     */
    void SetSubdivision(uint32_t subdiv);

    // --- 各種マネージャの静的設定 ---
    /**
     * @brief TextureManager を設定する。
     * @param[in] texM 設定する TextureManager の値
     */
    static void SetTextureManager(Irufemi::TextureManager* texM) {
        textureManager_ = texM;
    }
    /**
     * @brief DrawManager を設定する。
     * @param[in] drawM 設定する DrawManager の値
     */
    static void SetDrawManager(DrawManager* drawM) {
        drawManager_ = drawM;
    }
    /**
     * @brief DebugUI を設定する。
     * @param[in] ui 設定する DebugUI の値
     */
    static void SetDebugUI(DebugUI* ui) {
        ui_ = ui;
    }
    /**
     * @brief Engine を設定する。
     * @param[in] engine 設定する Engine の値
     */
    static void SetEngine(class IrufemiEngine* engine) {
        engine_ = engine;
    }

private:
    /**
     * @brief 現在の type_, size_, pivot_ に基づいてメッシュ（頂点・インデックス）を再構築する
     * @details 構築したデータは Object2DResource に渡し、バッファ再生成が必要かどうかの判断は
     *          Object2DResource 側に委譲します。
     */
    void RebuildMesh();

    // 内部メッシュ構築用ヘルパー
    /**
     * @brief BuildRect を実行する。
     */
    void BuildRect();
    /**
     * @brief BuildTriangle を実行する。
     */
    void BuildTriangle();
    /**
     * @brief BuildCircle を実行する。
     */
    void BuildCircle(uint32_t subdiv);
    /**
     * @brief BuildRing を実行する。
     */
    void BuildRing(uint32_t subdiv);
    /**
     * @brief BuildLine を実行する。
     */
    void BuildLine();

private:
    std::unique_ptr<Object2DResource> resource_ = nullptr;

    Irufemi::Primitive2DType type_ = Irufemi::Primitive2DType::Rect;
    Irufemi::Vector2 size_ = {100.0f, 100.0f};
    Irufemi::Vector2 pivot_ = {0.5f, 0.5f};
    float thickness_ = 10.0f;   //!< RingやLineでの太さ
    uint32_t subdivision_ = 64; //!< 円系の分割数

    bool isTopMost_ = false;       //!< UI用など、最前面描画フラグ
    bool isMeshDirty_ = true;      //!< メッシュ再構築フラグ
    int selectedTextureIndex_ = 0; //!< デバッグUI用

    // 静的マネージャ参照
    static Irufemi::TextureManager* textureManager_;
    static DrawManager* drawManager_;
    static DebugUI* ui_;
    static class IrufemiEngine* engine_;
};
