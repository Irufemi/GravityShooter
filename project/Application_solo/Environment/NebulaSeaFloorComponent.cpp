#include "Environment/NebulaSeaFloorComponent.h"

#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/RootSignatureConfig.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "Renderer/DrawManager.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "RailMechanics/RailShooterPlayerComponent.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

namespace {
// グリッドメッシュ生成用定数
constexpr int kGridResolution = 64;           //!< グリッド分割数 (64 x 64)
constexpr float kGridHalfExtent = 1800.0f;     //!< 半径 1800m (全幅 3600m)
constexpr float kCellSize = (kGridHalfExtent * 2.0f) / static_cast<float>(kGridResolution); //!< 1セルの幅 (約56.25m)
} // namespace

NebulaSeaFloorComponent::~NebulaSeaFloorComponent() {
    if (constantBuffer_ && mappedParams_) {
        constantBuffer_->Unmap(0, nullptr);
        mappedParams_ = nullptr;
    }
}

void NebulaSeaFloorComponent::Initialize() {
    CreateConstantBuffer();
    CreateMeshGrid();
}

void NebulaSeaFloorComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("Surface Y", &params_.surfaceY);
    RegisterProperty("Rail Speed", &params_.railSpeed);
    RegisterProperty("Volume Depth", &params_.volumeDepth);
    RegisterProperty("Density Scale", &params_.densityScale);
    RegisterProperty("Fade Start Dist", &params_.fadeStartDistance);
    RegisterProperty("Fade End Dist", &params_.fadeEndDistance);
    RegisterProperty("Soft Range", &params_.softIntersectionRange);
}

void NebulaSeaFloorComponent::CreateConstantBuffer() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    auto dx = engine->GetDirectXCommon();
    if (!dx) {
        return;
    }

    constantBuffer_ = dx->CreateBufferResource(sizeof(NebulaSeaParams));
    if (constantBuffer_) {
        HRESULT hr = constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedParams_));
        assert(SUCCEEDED(hr) && "Failed to map NebulaSea constant buffer.");
        if (SUCCEEDED(hr) && mappedParams_) {
            *mappedParams_ = params_;
        }
    }
}

void NebulaSeaFloorComponent::CreateMeshGrid() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }
    auto dx = engine->GetDirectXCommon();
    if (!dx) {
        return;
    }

    // 1. 頂点データ生成 ( (N+1) x (N+1) )
    std::vector<Vertex> vertices;
    vertices.reserve((kGridResolution + 1) * (kGridResolution + 1));

    for (int z = 0; z <= kGridResolution; ++z) {
        float normZ = static_cast<float>(z) / static_cast<float>(kGridResolution);
        float posZ = -kGridHalfExtent + normZ * (kGridHalfExtent * 2.0f);

        for (int x = 0; x <= kGridResolution; ++x) {
            float normX = static_cast<float>(x) / static_cast<float>(kGridResolution);
            float posX = -kGridHalfExtent + normX * (kGridHalfExtent * 2.0f);

            Vertex v{};
            v.position = {posX, 0.0f, posZ};
            v.normal = {0.0f, 1.0f, 0.0f};
            v.texcoord = {normX, normZ};
            vertices.push_back(v);
        }
    }

    // 2. インデックスデータ生成 ( N x N x 2 トライアングル )
    std::vector<uint32_t> indices;
    indices.reserve(kGridResolution * kGridResolution * 6);

    const int stride = kGridResolution + 1;
    for (int z = 0; z < kGridResolution; ++z) {
        for (int x = 0; x < kGridResolution; ++x) {
            uint32_t i0 = z * stride + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = (z + 1) * stride + x;
            uint32_t i3 = i2 + 1;

            // 三角形 1
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            // 三角形 2
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
    indexCount_ = static_cast<uint32_t>(indices.size());

    // 3. DirectX 12 頂点バッファ作成 & アップロード
    size_t vbSize = sizeof(Vertex) * vertices.size();
    vertexBuffer_ = dx->CreateBufferResource(vbSize);
    if (vertexBuffer_) {
        void* vbData = nullptr;
        if (SUCCEEDED(vertexBuffer_->Map(0, nullptr, &vbData))) {
            memcpy(vbData, vertices.data(), vbSize);
            vertexBuffer_->Unmap(0, nullptr);
        }
        vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
        vertexBufferView_.SizeInBytes = static_cast<UINT>(vbSize);
        vertexBufferView_.StrideInBytes = sizeof(Vertex);
    }

    // 4. DirectX 12 インデックスバッファ作成 & アップロード
    size_t ibSize = sizeof(uint32_t) * indices.size();
    indexBuffer_ = dx->CreateBufferResource(ibSize);
    if (indexBuffer_) {
        void* ibData = nullptr;
        if (SUCCEEDED(indexBuffer_->Map(0, nullptr, &ibData))) {
            memcpy(ibData, indices.data(), ibSize);
            indexBuffer_->Unmap(0, nullptr);
        }
        indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
        indexBufferView_.SizeInBytes = static_cast<UINT>(ibSize);
        indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    }
}

void NebulaSeaFloorComponent::Update() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    float dt = engine->GetDeltaTime();
    if (dt <= 0.0f) {
        dt = 1.0f / 60.0f;
    }
    totalTime_ += dt;

    // カメラ位置の取得（セル単位スナップによるジッター防止）
    if (auto scene = GetScene()) {
        if (auto camObj = scene->FindGameObject("MainCamera")) {
            if (auto t = camObj->GetTransform()) {
                lastCameraPos_ = t->GetPosition();
            }
        }
    }

    // カメラのXZ位置をセルサイズ単位でスナップさせてオフセット
    float snapX = std::floor(lastCameraPos_.x / kCellSize) * kCellSize;
    float snapZ = std::floor(lastCameraPos_.z / kCellSize) * kCellSize;
    params_.cameraSnapXZ = {snapX, snapZ, 0.0f, 0.0f};

    // 時間更新
    params_.time = totalTime_;

    if (mappedParams_) {
        *mappedParams_ = params_;
    }
}

void NebulaSeaFloorComponent::Draw() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    auto drawManager = engine->GetDrawManager();
    if (!drawManager || !constantBuffer_ || !vertexBuffer_ || !indexBuffer_) {
        return;
    }

    // 不透明パス直前（BeforeOpaque）に提出し、3D空間の底知れぬ雲海として描画
    drawManager->SubmitCustomPass(Irufemi::RenderStage::BeforeOpaque, [this, engine]() {
        auto dx = engine->GetDirectXCommon();
        if (!dx) {
            return;
        }

        auto cmdList = dx->GetCommandList();
        auto psoManager = engine->GetPSOManager();
        if (!cmdList || !psoManager) {
            return;
        }

        // NebulaSeaFloor PSO (NebulaSeaFloor.VS.hlsl + NebulaSeaFloor.PS.hlsl)
        // 深度テスト有効・書き込み有効、カリングなし
        auto pso = psoManager->GetPSO("NebulaSeaFloor", Irufemi::BlendMode::kBlendModeNormal,
                                      PSOManager::DepthWrite::Enable, PSOManager::CullMode::None);
        if (pso) {
            cmdList->SetPipelineState(pso);
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // register(b6) / RootSlot::Special に定数バッファをバインド
            cmdList->SetGraphicsRootConstantBufferView(static_cast<UINT>(RootSlot::Special),
                                                       constantBuffer_->GetGPUVirtualAddress());

            // 頂点・インデックスバッファのセット
            cmdList->IASetVertexBuffers(0, 1, &vertexBufferView_);
            cmdList->IASetIndexBuffer(&indexBufferView_);

            // グリッドメッシュの描画
            cmdList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
        }
    });
}
