#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <string>
#include "Renderer/Data/Material.h"
#include "Renderer/Data/VertexData.h"
#include "Renderer/System/Core/MultiBufferSyncState.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector4.h"
#include <vector>
#include <array>
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/ConstantBuffer.h"
#include "Core/System/ResourceHandle.h"

// 前方宣言
class Camera;
class IrufemiEngine;

#include "Renderer/System/Core/IRenderable.h"

/**
 * @class Skybox
 * @brief スカイボックスの描画を管理するクラス
 */
class Skybox : public IRenderable, public MultiBufferSyncState {
public:
public: // メンバ関数
    // コンストラクタ
    Skybox();
    // デストラクタ
    ~Skybox();
    // 初期化
    /**
     * @brief スカイボックスの初期化（キューブメッシュ生成およびテクスチャ読み込み）
     * @param[in] textureName キューブマップテクスチャのパス（デフォルトは Rostock Laage Airport）
     */
    void Initialize(const std::string& textureName = "resources/rostock_laage_airport_4k.dds");

    /**
     * @brief スカイボックスの更新処理（アクティブカメラ追従および行列計算）
     */
    void Update();

    /**
     * @brief 描画前フレーム同期（定数バッファへのデータ転送およびDrawManagerへのパケット登録）
     */
    void SyncBeforeDraw() override;

    /**
     * @brief スカイボックスの直接描画処理
     */
    void Draw() override;

    /**
     * @brief デバッグ用 ImGui ウィンドウを描画する
     */
    void Debug();

public: // メンバ関数(セッター/ゲッター)
    /**
     * @brief 静的エンジン参照を設定する
     * @param[in] engine エンジンのポインタ
     */
    static void SetEngine(IrufemiEngine* engine) {
        engine_ = engine;
    }

    /**
     * @brief 頂点バッファビューを取得する
     * @return 頂点バッファビューの参照
     */
    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const {
        return vertexBufferView_;
    }

    /**
     * @brief インデックスバッファビューを取得する
     * @return インデックスバッファビューの参照
     */
    const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const {
        return indexBufferView_;
    }

    /**
     * @brief キューブマップテクスチャのハンドルを取得する
     * @return テクスチャリソースハンドル
     */
    Irufemi::ResourceHandle GetTextureHandle() const {
        return textureHandle_;
    }

    /**
     * @brief インデックス数を取得する
     * @return インデックスデータの総要素数
     */
    UINT GetIndexSize() const {
        return static_cast<UINT>(indexDataList_.size());
    }

private: // メンバ関数(内部ヘルパ)
    /**
     * @brief 頂点バッファおよびインデックスバッファのリソースを生成する
     */
    void CreateResource();

    /**
     * @brief リソースのメモリマップを行い、CPU側ポインタを取得する
     */
    void MapResource();

    /**
     * @brief リソースのメモリマップを解除する
     */
    void UnMapResource();

private: // メンバ変数(resource)
    /// vertex
    std::vector<Irufemi::VertexData> vertexDataList_{};
    Irufemi::VertexData* vertexData_ = nullptr;
    // 頂点データバッファ
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;

    /// index
    std::vector<uint32_t> indexDataList_{};
    uint32_t* indexData_ = nullptr;
    // 頂点インデックスバッファ
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_ = nullptr;

    /// Irufemi::Transform
    // transform(scale,rotate,translate)
    Irufemi::Transform transform_ = {
        {500.0f, 500.0f, 500.0f}, // scale
        {0.0f, 0.0f, 0.0f},       // rotate
        {0.0f, 0.0f, 0.0f}        // translate
    };
    struct SkyboxTransformationMatrix {
        Irufemi::Matrix4x4 WVP;
        Irufemi::Matrix4x4 World;
        Irufemi::Matrix4x4 WorldInverseTranspose;
    };
    SkyboxTransformationMatrix transformationMatrix_{};
    Irufemi::ConstantBuffer<SkyboxTransformationMatrix> transformationBuffer_;

    // Material
    struct SkyboxMaterial {
        Irufemi::Vector4 color;
        float intensity;
        uint32_t textureIndex; // [Bindless]
        uint32_t padding[2];
    };
    Irufemi::ConstantBuffer<SkyboxMaterial> materialBuffer_;

    // texture
    Irufemi::ResourceHandle textureHandle_;
    int selectedTextureIndex_ = 0;

    // カメラ(ポインタ参照)

    // engine(ポインタ参照)
    static IrufemiEngine* engine_;

    // 行列更新の最適化用
    bool isDirty_ = true;
    Irufemi::Matrix4x4 lastViewMatrix_ = {};
    Irufemi::Matrix4x4 lastProjectionMatrix_ = {};
};
