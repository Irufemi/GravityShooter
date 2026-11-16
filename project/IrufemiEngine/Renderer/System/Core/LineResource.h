#pragma once
#include "Renderer/System/Core/BaseResource.h"
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "RHI/DirectX12/DynamicConstantBuffer.h"
#include "Renderer/Data/VertexData.h"
#include "Renderer/Data/Material.h"
#include "Renderer/Data/TransformationMatrix.h"
#include "Core/Math/Transform.h"

namespace Irufemi {

class Camera;

class LineResource : public BaseResource {
public:
    virtual ~LineResource();

    /**
     * @brief CreateResource を実行する。
     */
    void CreateResource() override;
    /**
     * @brief Map を実行する。
     */
    void Map() override;
    /**
     * @brief Unmap を実行する。
     */
    void Unmap() override;

    /**
     * @brief UpdateTransform を実行する。
     */
    void UpdateTransform(const Camera& camera);

public:
    // --- バッファビュー & 描画情報アクセサ ---
    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const {
        return vertexBufferView_;
    }
    void SetVertexBufferView(const D3D12_VERTEX_BUFFER_VIEW& vbv) {
        vertexBufferView_ = vbv;
    }

    const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const {
        return indexBufferView_;
    }
    void SetIndexBufferView(const D3D12_INDEX_BUFFER_VIEW& ibv) {
        indexBufferView_ = ibv;
    }

    uint32_t GetIndexCount() const {
        return indexCount_;
    }
    void SetIndexCount(uint32_t count) {
        indexCount_ = count;
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> GetVertexResource() const {
        return vertexResource_;
    }
    void SetVertexResource(Microsoft::WRL::ComPtr<ID3D12Resource> res) {
        vertexResource_ = res;
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> GetIndexResource() const {
        return indexResource_;
    }
    void SetIndexResource(Microsoft::WRL::ComPtr<ID3D12Resource> res) {
        indexResource_ = res;
    }

    Irufemi::VertexData* GetVertexData() {
        return vertexData_;
    }
    const Irufemi::VertexData* GetVertexData() const {
        return vertexData_;
    }
    uint32_t* GetIndexData() {
        return indexData_;
    }
    const uint32_t* GetIndexData() const {
        return indexData_;
    }

    // --- トランスフォームアクセサ ---
    const Irufemi::Transform& GetTransform() const {
        return transform_;
    }
    Irufemi::Transform& GetTransform() {
        return transform_;
    }
    void SetTransform(const Irufemi::Transform& transform) {
        transform_ = transform;
    }

    const Irufemi::TransformationMatrix& GetTransformationMatrix() const {
        return transformationMatrix_;
    }
    Irufemi::TransformationMatrix& GetTransformationMatrix() {
        return transformationMatrix_;
    }

    // --- マテリアルアクセサ ---
    Irufemi::Material* GetMaterialData() {
        return &cpuMaterialData_;
    }
    const Irufemi::Material* GetMaterialData() const {
        return &cpuMaterialData_;
    }

    /**
     * @brief MaterialVAddress を取得する。
     * @return 取得された MaterialVAddress
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetMaterialVAddress() const;
    /**
     * @brief TransformVAddress を取得する。
     * @return 取得された TransformVAddress
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetTransformVAddress() const;

    /**
     * @brief SyncBeforeDraw を実行する。
     */
    void SyncBeforeDraw();

protected:
    // --- 頂点バッファ ---
    Irufemi::VertexData* vertexData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    // --- インデックスバッファ ---
    uint32_t* indexData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_ = nullptr;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    uint32_t indexCount_ = 0;

    // --- マテリアル ---
    Irufemi::Material cpuMaterialData_{};
    uint32_t materialCbIndex_ = static_cast<uint32_t>(-1);

    // --- トランスフォーム ---
    Irufemi::Transform transform_{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    Irufemi::TransformationMatrix transformationMatrix_{};
    uint32_t transformCbIndex_ = static_cast<uint32_t>(-1);
};

} // namespace Irufemi
