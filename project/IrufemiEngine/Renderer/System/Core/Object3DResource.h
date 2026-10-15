#pragma once
#include "Renderer/System/Core/BaseResource.h"
#include <vector>
#include "RHI/DirectX12/DirectXCommon.h"
#include <wrl.h>
#include <d3d12.h>
#include "Renderer/Data/VertexData.h"
#include "Renderer/Data/Material.h"
#include "Renderer/Data/TransformationMatrix.h"
#include "RHI/DirectX12/DynamicConstantBuffer.h"
#include "Core/System/ResourceHandle.h"
#include "Core/Math/Transform.h"

class Camera;

class Object3DResource : public BaseResource {
public:
    virtual ~Object3DResource();

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

    /**
     * @brief CustomPSO を設定する（生ポインタ指定・下位互換用）。
     * @param[in] pso 設定する CustomPSO の値
     */
    void SetCustomPSO(ID3D12PipelineState* pso) {
        customPSOName_.clear();
        customPSO_ = pso;
    }
    /**
     * @brief CustomPSO を設定する（名前指定・推奨）。
     * @param[in] psoName 設定する PSO 名
     * @param[in] blend ブレンドモード
     * @param[in] depth 深度書き込み設定
     * @param[in] cull カリングモード
     */
    void SetCustomPSO(const std::string& psoName, Irufemi::BlendMode blend = Irufemi::BlendMode::kBlendModeNormal,
                      PSOManager::DepthWrite depth = PSOManager::DepthWrite::Enable,
                      PSOManager::CullMode cull = PSOManager::CullMode::Back) {
        customPSOName_ = psoName;
        customBlend_ = blend;
        customDepth_ = depth;
        customCull_ = cull;

        // 設定時に即座に解決してキャッシュ
        customPSO_ = nullptr;
        if (!customPSOName_.empty()) {
            if (auto* dxCommon = GetDxCommon()) {
                if (auto* pm = dxCommon->GetPSOManager()) {
                    customPSO_ = pm->GetPSO(customPSOName_, customBlend_, customDepth_, customCull_);
                }
            }
        }
    }
    /**
     * @brief CustomPSO を取得する（事前解決済みキャッシュを O(1) で返却）。
     * @return 取得された CustomPSO
     */
    ID3D12PipelineState* GetCustomPSO() const {
        if (!customPSO_ && !customPSOName_.empty()) {
            if (auto* dxCommon = GetDxCommon()) {
                if (auto* pm = dxCommon->GetPSOManager()) {
                    customPSO_ = pm->GetPSO(customPSOName_, customBlend_, customDepth_, customCull_);
                }
            }
        }
        return customPSO_;
    }

    /**
     * @brief CustomCBVAddress を設定する。
     * @param[in] addr 設定する CustomCBVAddress の値
     */
    void SetCustomCBVAddress(D3D12_GPU_VIRTUAL_ADDRESS addr) {
        customCBVAddress_ = addr;
    }
    /**
     * @brief CustomCBVAddress を取得する。
     * @return 取得された CustomCBVAddress
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetCustomCBVAddress() const {
        return customCBVAddress_;
    }

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
        if (vertexResource_ && vertexResource_ != res) {
            if (auto* dx = GetDxCommon()) {
                dx->ReleaseAfterFence(vertexResource_);
            }
        }
        vertexResource_ = res;
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> GetIndexResource() const {
        return indexResource_;
    }
    void SetIndexResource(Microsoft::WRL::ComPtr<ID3D12Resource> res) {
        if (indexResource_ && indexResource_ != res) {
            if (auto* dx = GetDxCommon()) {
                dx->ReleaseAfterFence(indexResource_);
            }
        }
        indexResource_ = res;
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

    const Irufemi::Transform& GetUVTransform() const {
        return uvTransform_;
    }
    Irufemi::Transform& GetUVTransform() {
        return uvTransform_;
    }
    void SetUVTransform(const Irufemi::Transform& uvTransform) {
        uvTransform_ = uvTransform;
    }

    const TransformationMatrix& GetTransformationMatrix() const {
        return transformationMatrix_;
    }
    TransformationMatrix& GetTransformationMatrix() {
        return transformationMatrix_;
    }

    // --- マテリアルアクセサ ---
    Material* GetMaterialData() {
        return &cpuMaterialData_;
    }
    const Material* GetMaterialData() const {
        return &cpuMaterialData_;
    }

    // --- テクスチャアクセサ ---
    ResourceHandle GetTextureHandle() const {
        return textureHandle_;
    }
    void SetTextureHandle(ResourceHandle handle) {
        textureHandle_ = handle;
    }

    void SetTextureManager(class TextureManager* tm) {
        textureManager_ = tm;
    }
    class TextureManager* GetTextureManager() const {
        return textureManager_;
    }

    // --- 頂点・インデックス配列アクセサ ---
    std::vector<VertexData>& GetVertexDataList() {
        return vertexDataList_;
    }
    const std::vector<VertexData>& GetVertexDataList() const {
        return vertexDataList_;
    }
    VertexData* GetVertexData() {
        return vertexData_;
    }

    std::vector<uint32_t>& GetIndexDataList() {
        return indexDataList_;
    }
    const std::vector<uint32_t>& GetIndexDataList() const {
        return indexDataList_;
    }
    uint32_t* GetIndexData() {
        return indexData_;
    }

    // --- getters ---
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

    // --- 外部リソースの借用 (StaticModelObject/AnimationModel等で共有するため) ---
    /**
     * @brief ExternalTransformCbIndex を設定する。
     * @param[in] externalCbIndex 設定する ExternalTransformCbIndex の値
     */
    void SetExternalTransformCbIndex(uint32_t* externalCbIndex) {
        externalTransformCbIndex_ = externalCbIndex;
    }

protected:
    // --- 頂点バッファ ---
    std::vector<VertexData> vertexDataList_{};
    VertexData* vertexData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    // --- インデックスバッファ ---
    std::vector<uint32_t> indexDataList_{};
    uint32_t* indexData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_ = nullptr;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    uint32_t indexCount_ = 0;

    // --- マテリアル ---
    Irufemi::Transform uvTransform_{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    Material cpuMaterialData_{};
    uint32_t materialCbIndex_ = static_cast<uint32_t>(-1);

    // --- トランスフォーム ---
    Irufemi::Transform transform_{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    TransformationMatrix transformationMatrix_{};
    uint32_t transformCbIndex_ = static_cast<uint32_t>(-1);
    uint32_t* externalTransformCbIndex_ = nullptr;

    // --- テクスチャ ---
    ResourceHandle textureHandle_;
    class TextureManager* textureManager_ = nullptr;

    // --- カスタム描画設定 ---
    mutable ID3D12PipelineState* customPSO_ = nullptr;
    std::string customPSOName_ = "";
    Irufemi::BlendMode customBlend_ = Irufemi::BlendMode::kBlendModeNormal;
    PSOManager::DepthWrite customDepth_ = PSOManager::DepthWrite::Enable;
    PSOManager::CullMode customCull_ = PSOManager::CullMode::Back;
    D3D12_GPU_VIRTUAL_ADDRESS customCBVAddress_ = 0;
};
