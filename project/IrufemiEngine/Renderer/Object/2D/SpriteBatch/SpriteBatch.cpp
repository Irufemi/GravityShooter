#include "Renderer/Object/2D/SpriteBatch/SpriteBatch.h"
#include "Renderer/DrawManager.h"
#include "Resource/Texture/TextureManager.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"
#include "Core/Math/Math.h"
#include "Renderer/System/Core/Object2DResource.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "../../../../../externals/DirectXTex/d3dx12.h"

using namespace Irufemi;

Irufemi::TextureManager* SpriteBatch::textureManager_ = nullptr;
Irufemi::DrawManager* SpriteBatch::drawManager_ = nullptr;
CameraManager* SpriteBatch::cameraManager_ = nullptr;
DirectXCommon* SpriteBatch::dx_ = nullptr;
DescriptorPool* SpriteBatch::srvPool_ = nullptr;

SpriteBatch::SpriteBatch() {}

SpriteBatch::~SpriteBatch() {
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (instancingSrvIndex_[i] != static_cast<uint32_t>(-1)) {
            srvPool_->Free(instancingSrvIndex_[i]);
        }
        if (instanceBuffer_[i]) {
            dx_->ReleaseAfterFence(instanceBuffer_[i]);
            instanceBuffer_[i].Reset();
        }
    }
}

void SpriteBatch::Initialize(const std::string& textureName) {
    baseResource_ = std::make_unique<Object2DResource>();

    // 頂点データはSpriteと同じ単位矩形
    baseResource_->GetVertexDataList().push_back({{0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}});
    baseResource_->GetVertexDataList().push_back({{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.00}, {0.0f, 0.0f, -1.0f}});
    baseResource_->GetVertexDataList().push_back({{1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}});
    baseResource_->GetVertexDataList().push_back({{1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}});

    baseResource_->GetIndexDataList().push_back(0);
    baseResource_->GetIndexDataList().push_back(1);
    baseResource_->GetIndexDataList().push_back(2);
    baseResource_->GetIndexDataList().push_back(1);
    baseResource_->GetIndexDataList().push_back(3);
    baseResource_->GetIndexDataList().push_back(2);

    baseResource_->CreateResource();
    baseResource_->Map();

    if (baseResource_->GetVertexData()) {
        std::copy(baseResource_->GetVertexDataList().begin(), baseResource_->GetVertexDataList().end(),
                  baseResource_->GetVertexData());
    }
    if (baseResource_->GetIndexData()) {
        std::copy(baseResource_->GetIndexDataList().begin(), baseResource_->GetIndexDataList().end(),
                  baseResource_->GetIndexData());
    }

    if (baseResource_->GetMaterialData()) {
        baseResource_->GetMaterialData()->color = {1.0f, 1.0f, 1.0f, 1.0f};
        baseResource_->GetMaterialData()->enableLighting = false;
        baseResource_->GetMaterialData()->hasTexture = true;
        baseResource_->GetMaterialData()->lightingMode = 2; // Unlit
        baseResource_->GetMaterialData()->uvTransform = Irufemi::Math::MakeIdentity4x4();
    }

    if (textureManager_) {
        if (baseResource_->GetTextureHandle().IsValid()) {
            textureManager_->ReleaseTexture(baseResource_->GetTextureHandle());
        }
        baseResource_->SetTextureHandle(textureManager_->LoadTexture(textureName));
        uint32_t tw = 0, th = 0;
        if (textureManager_->GetTextureSize(textureName, tw, th)) {
            textureSize_ = {static_cast<float>(tw), static_cast<float>(th)};
        }

        // Bindless用にtextureIndexをマテリアルに設定
        if (baseResource_->GetMaterialData()) {
            baseResource_->GetMaterialData()->textureIndex =
                textureManager_->GetSrvIndex(baseResource_->GetTextureHandle());
        }
    }

    // SRV確保
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        instancingSrvIndex_[i] = srvPool_->Allocate();
        instancingSrvCPU_[i] = srvPool_->GetCPUHandle(instancingSrvIndex_[i]);
        instancingSrvGPU_[i] = srvPool_->GetGPUHandle(instancingSrvIndex_[i]);
        instanceCapacity_[i] = 0;
        instanceData_[i] = nullptr;
    }
}

void SpriteBatch::AddInstance(const Irufemi::Transform& transform, const Irufemi::Vector4& color) {
    instances_.push_back(
        {transform, color, {0.5f, 0.5f}, {textureSize_.x * transform.scale.x, textureSize_.y * transform.scale.y}});
    instanceDirty_ = true;
}

void SpriteBatch::AddInstance(const Irufemi::Vector2& position, const Irufemi::Vector2& size, float rotation,
                              const Irufemi::Vector4& color, const Irufemi::Vector2& anchor) {
    Irufemi::Transform tf;
    tf.translate = {position.x, position.y, 0.0f};
    tf.scale = {1.0f, 1.0f, 1.0f};
    tf.rotate = {0.0f, 0.0f, rotation};
    instances_.push_back({tf, color, anchor, size});
    instanceDirty_ = true;
}

void SpriteBatch::ClearInstances() {
    instances_.clear();
    visibleInstanceCount_ = 0;
    instanceDirty_ = true;
}

void SpriteBatch::Update() {
    visibleInstanceCount_ = static_cast<uint32_t>(instances_.size());
    if (visibleInstanceCount_ > 0) {
        BuildInstanceBuffer();
    }
}

void SpriteBatch::CreateOrResizeInstanceBuffer(uint32_t instanceCount) {
    uint32_t frameIndex = dx_->GetCurrentBackBufferIndex();
    if (instanceCapacity_[frameIndex] < instanceCount) {
        uint32_t doubled = instanceCapacity_[frameIndex] * 2;
        uint32_t newCapacity = instanceCount > doubled ? instanceCount : doubled;
        if (newCapacity < 64) {
            newCapacity = 64;
        }

        if (instanceBuffer_[frameIndex]) {
            instanceBuffer_[frameIndex]->Unmap(0, nullptr);
            dx_->ReleaseAfterFence(instanceBuffer_[frameIndex]);
            instanceBuffer_[frameIndex].Reset();
        }

        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(InstanceData) * newCapacity);

        HRESULT hr = dx_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                               D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                               IID_PPV_ARGS(&instanceBuffer_[frameIndex]));

        if (SUCCEEDED(hr)) {
            instanceBuffer_[frameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_[frameIndex]));
            instanceCapacity_[frameIndex] = newCapacity;

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.NumElements = newCapacity;
            srvDesc.Buffer.StructureByteStride = sizeof(InstanceData);

            dx_->GetDevice()->CreateShaderResourceView(instanceBuffer_[frameIndex].Get(), &srvDesc,
                                                       instancingSrvCPU_[frameIndex]);
        }
    }
}

void SpriteBatch::ApplyAnchorToVertices() {
    // 頂点のローカル座標をアンカーに合わせて動的に変更するのではなく、
    // インスタンスごとにサイズとアンカーからWVPを計算するため、基本は(0..1)のままでよい。
    // アンカー処理はBuildInstanceBuffer()内でWVPを作る時に反映する。
}

void SpriteBatch::BuildInstanceBuffer(bool force) {
    uint32_t frameIndex = dx_->GetCurrentBackBufferIndex();
    CreateOrResizeInstanceBuffer(visibleInstanceCount_);

    Camera* camera = cameraManager_->GetActiveCamera();
    if (!camera) {
        return;
    }

    Irufemi::Matrix4x4 viewProj = camera->GetOrthographicMatrix(); // Sprite uses Orthographic

    for (uint32_t i = 0; i < visibleInstanceCount_; ++i) {
        const auto& inst = instances_[i];

        // アンカーとスケールを合成（0..1矩形をアンカー分オフセットさせた上でサイズを乗じる）
        float offsetX = -inst.anchor.x * inst.size.x;
        float offsetY = -inst.anchor.y * inst.size.y;

        // Local(Anchor * Scale) 行列を直接構築
        Irufemi::Matrix4x4 localMat = Irufemi::Math::MakeIdentity4x4();
        localMat.m[0][0] = inst.size.x;
        localMat.m[1][1] = inst.size.y;
        localMat.m[3][0] = offsetX;
        localMat.m[3][1] = offsetY;

        Irufemi::Matrix4x4 rotMat = Irufemi::Math::MakeRotateZMatrix(inst.transform.rotate.z);
        Irufemi::Matrix4x4 transMat = Irufemi::Math::MakeTranslateMatrix(inst.transform.translate);

        // World = Local(Anchor*Scale) * Rot * Trans
        Irufemi::Matrix4x4 worldMat = Irufemi::Math::Multiply(localMat, rotMat);
        worldMat = Irufemi::Math::Multiply(worldMat, transMat);

        instanceData_[frameIndex][i].WVP = Irufemi::Math::Multiply(worldMat, viewProj);
        instanceData_[frameIndex][i].color = inst.color;
    }

    instanceDirty_ = false;
}

void SpriteBatch::SyncBeforeDraw() {
    if (textureManager_ && baseResource_->GetMaterialData()) {
        baseResource_->GetMaterialData()->textureIndex =
            textureManager_->GetSrvIndex(baseResource_->GetTextureHandle());
    }
    baseResource_->SyncBeforeDraw();
}

void SpriteBatch::Draw() {
    Draw(isTopMost_);
}

void SpriteBatch::Draw(bool isTopMost) {
    if (visibleInstanceCount_ == 0) {
        return;
    }

    SyncBeforeDraw();

    RenderPackets::SpriteBatchPacket packet{};
    packet.resource = baseResource_.get();
    packet.instancingSrvHandleGPU = GetInstancingSrvHandleGPU();
    packet.instanceCount = visibleInstanceCount_;
    packet.blendMode = Irufemi::BlendMode::kBlendModeNormal;
    packet.depthWrite = PSOManager::DepthWrite::Off;
    packet.cullMode = PSOManager::CullMode::None;
    packet.customPSO = GetCustomPSO();
    packet.customCBVAddress = customCBVAddress_;

    if (isTopMost) {
        drawManager_->SubmitTopMostSpriteBatch(packet);
    } else {
        drawManager_->SubmitSpriteBatch(packet);
    }
}

ID3D12PipelineState* SpriteBatch::GetCustomPSO() const {
    if (!customPSOName_.empty() && dx_) {
        if (auto* pm = dx_->GetPSOManager()) {
            return pm->GetPSO(customPSOName_, customBlend_, customDepth_, customCull_);
        }
    }
    return customPSO_;
}

D3D12_GPU_DESCRIPTOR_HANDLE SpriteBatch::GetInstancingSrvHandleGPU() const {
    return instancingSrvGPU_.data()[dx_->GetCurrentBackBufferIndex()];
}
