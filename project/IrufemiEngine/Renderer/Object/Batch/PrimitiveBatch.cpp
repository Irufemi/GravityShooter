#include "Core/Utility/ErrorUtility.h"
#include "Renderer/Object/Batch/PrimitiveBatch.h"
#include <cassert>
#include <cmath>
#include "Core/System/IrufemiEngine.h"
#include "Resource/Texture/TextureManager.h"
#include "Renderer/DrawManager.h"

using namespace Irufemi;

void PrimitiveBatch::Initialize(Irufemi::PrimitiveType type, const std::string& textureName) {
    type_ = type;
    isCustomPrimitive_ = false;
    boundingSphereRadius_ = 1.0f; // 基本形状の半径

    EnsureMaterialResources();
    EnsureSharedTexture(textureName);
}

void PrimitiveBatch::InitializeRing(const RingParams& params, const std::string& textureName) {
    type_ = Irufemi::PrimitiveType::Ring;
    isCustomPrimitive_ = true;

    PrimitiveData ringData = PrimitiveManager::CreateRing(params);
    primitiveManager_->CreateGPUResource(ringData, customPrimitiveResource_);

    // リング頂点群から外接球半径を算出
    float maxDistSq = 0.0f;
    for (const auto& v : ringData.vertices) {
        float distSq = v.position.x * v.position.x + v.position.y * v.position.y + v.position.z * v.position.z;
        if (distSq > maxDistSq) {
            maxDistSq = distSq;
        }
    }
    boundingSphereRadius_ = (maxDistSq > 0.0f) ? std::sqrt(maxDistSq) : 1.0f;

    EnsureMaterialResources();
    EnsureSharedTexture(textureName);
}

void PrimitiveBatch::EnsureMaterialResources() {
    if (auto engine = dx_->GetEngine()) {
        if (materialCbIndex_ == static_cast<uint32_t>(-1)) {
            materialCbIndex_ = engine->GetMaterialBufferManager()->Allocate();
        }
    }
    cpuMaterialData_.color = {1.0f, 1.0f, 1.0f, 1.0f};
    cpuMaterialData_.enableLighting = 1;
    cpuMaterialData_.uvTransform = Irufemi::Math::MakeIdentity4x4();
    cpuMaterialData_.metallic = 0.0f;
    cpuMaterialData_.roughness = 0.5f;
    cpuMaterialData_.environmentCoefficient = 0.0f;
    cpuMaterialData_.hasTexture = 1;
    cpuMaterialData_.lightingMode = 3;

    if (auto engine = dx_->GetEngine()) {
        for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
            engine->GetMaterialBufferManager()->Update(materialCbIndex_, cpuMaterialData_, i);
        }
    }
}

void PrimitiveBatch::EnsureSharedTexture(const std::string& textureName) {
    if (textureHandle_.IsValid()) {
        textureManager_->ReleaseTexture(textureHandle_);
    }
    if (!textureName.empty()) {
        textureHandle_ = textureManager_->LoadTexture(textureName);
    } else {
        textureHandle_ = ResourceHandle();
    }
}

float PrimitiveBatch::GetBoundingSphereRadius() const {
    return boundingSphereRadius_;
}

void PrimitiveBatch::Draw() {
    if (instances_.empty() && instanceWorlds_.empty()) {
        return;
    }

    SyncBeforeDraw();

    const PrimitiveResource* res = nullptr;
    if (isCustomPrimitive_) {
        res = &customPrimitiveResource_;
    } else {
        res = &primitiveManager_->GetStandardResource(type_);
    }

    if (!res || !res->vertexResource) {
        return;
    }

    RenderPackets::PrimitiveBatchPacket p{};
    // We need to make sure DrawManager understands PrimitiveBatchPacket
    p.vertexBufferView = res->vertexBufferView;
    p.indexBufferView = res->indexBufferView;
    p.indexCount = res->indexCount;

    p.materialAddress = GetMaterialVAddress();
    p.instancingSrvHandleGPU = GetInstancingSrvHandleGPU();
    p.instanceCount = GetInstanceCount();

    p.blendMode = GetBlendMode();
    p.depthWrite = GetDepthWrite();
    p.cullMode = GetCullMode();
    p.castShadows = GetCastShadows();
    p.customPSO = GetCustomPSO();
    p.customCBVAddress = GetCustomCBVAddress();

    drawManager_->SubmitPrimitiveBatch(p);
}
