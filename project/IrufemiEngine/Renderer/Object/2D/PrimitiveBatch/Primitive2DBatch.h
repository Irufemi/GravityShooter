#pragma once

#include "Renderer/System/Core/BaseBatch.h"
#include "Renderer/Object/2D/Primitive/Primitive2DObject.h" // For Primitive2DType
#include "Renderer/System/Core/Object2DResource.h"          // For VertexData

#include <vector>
#include <string>
#include <wrl.h>

namespace Irufemi {

/**
 * @class Primitive2DBatch
 * @brief 2Dプリミティブのインスタンシング（大量描画）を行うためのバッチクラス
 */
class Primitive2DBatch : public BaseBatch {
public:
    Primitive2DBatch();
    ~Primitive2DBatch() override;

    /**
     * @brief バッチを初期化し、指定された形状のメッシュを生成する
     * @param type 生成する形状の種類
     * @param textureName 適用するテクスチャパス
     */
    void Initialize(Irufemi::Primitive2DType type, const std::string& textureName = "resources/uvChecker.png");

    /**
     * @brief 描画命令を発行（DrawManager に登録）
     */
    void Draw() override;

    /**
     * @brief GPUと同期
     */
    void SyncBeforeDraw() override;

    // --- Mesh properties ---
    /**
     * @brief Subdivision を設定する。
     * @param[in] subdiv 設定する Subdivision の値
     */
    void SetSubdivision(uint32_t subdiv);
    /**
     * @brief Thickness を設定する。
     * @param[in] thickness 設定する Thickness の値
     */
    void SetThickness(float thickness);

    // --- ゲッター ---
    /**
     * @brief VertexBufferView を取得する。
     * @return 取得された VertexBufferView
     */
    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const {
        return vertexBufferView_;
    }
    /**
     * @brief IndexBufferView を取得する。
     * @return 取得された IndexBufferView
     */
    const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const {
        return indexBufferView_;
    }
    /**
     * @brief IndexCount を取得する。
     * @return 取得された IndexCount
     */
    uint32_t GetIndexCount() const {
        return indexCount_;
    }
    /**
     * @brief Type を取得する。
     * @return 取得された Type
     */
    Irufemi::Primitive2DType GetType() const {
        return type_;
    }

protected:
    /**
     * @brief BoundingSphereRadius を取得する。
     * @return 取得された BoundingSphereRadius
     */
    float GetBoundingSphereRadius() const override {
        return 1000.0f; /* 2D なので適当な大きな値 */
    }

    // メッシュ再構築
    /**
     * @brief RebuildMesh を実行する。
     */
    void RebuildMesh();
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
    void BuildCircle(uint32_t subdivision);
    /**
     * @brief BuildRing を実行する。
     */
    void BuildRing(uint32_t subdivision);
    /**
     * @brief BuildLine を実行する。
     */
    void BuildLine();

    /**
     * @brief CreateResource を実行する。
     */
    void CreateResource();

private:
    Irufemi::Primitive2DType type_ = Irufemi::Primitive2DType::Rect;
    uint32_t subdivision_ = 16;
    float thickness_ = 0.1f;
    bool isMeshDirty_ = true;
    Irufemi::ResourceHandle textureHandle_;

    // メッシュデータ
    std::vector<Irufemi::VertexData> vertexDataList_;
    std::vector<uint32_t> indexDataList_;

    // GPUリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    uint32_t indexCount_ = 0;
};

} // namespace Irufemi
