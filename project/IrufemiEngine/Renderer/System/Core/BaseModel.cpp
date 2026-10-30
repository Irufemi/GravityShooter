#include "Renderer/System/Core/BaseModel.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Renderer/System/Core/BaseResource.h"
#include "Resource/Model/ModelManager.h"

IrufemiEngine* BaseModel::engine_ = nullptr;

BaseModel::~BaseModel() {
    if (transformCbIndex_ != static_cast<uint32_t>(-1)) {
        if (engine_) {
            engine_->GetTransformBufferManager()->Free(transformCbIndex_);
        }
    }
}

D3D12_GPU_VIRTUAL_ADDRESS BaseModel::GetTransformationGpuAddress() const {
    if (transformCbIndex_ == static_cast<uint32_t>(-1) || !engine_ || !engine_->GetTransformBufferManager()) {
        return 0;
    }
    auto* dxCommon = BaseResource::GetDirectXCommon();
    if (!dxCommon) {
        return 0;
    }
    return engine_->GetTransformBufferManager()->GetGPUVirtualAddress(transformCbIndex_, dxCommon->GetFrameIndex());
}

std::shared_ptr<ObjModel> BaseModel::GetCpuModel() const {
    if (engine_ && engine_->GetObjModelManager()) {
        if (auto m = engine_->GetObjModelManager()->Resolve(modelHandle_)) {
            return m->cpuModel;
        }
    }
    return nullptr;
}

size_t BaseModel::GetMeshCount() const {
    if (engine_ && engine_->GetObjModelManager()) {
        if (auto m = engine_->GetObjModelManager()->Resolve(modelHandle_)) {
            if (m->cpuModel) {
                return m->cpuModel->meshes.size();
            }
        }
    }
    return 0;
}

const ObjMaterial* BaseModel::GetMaterial(size_t meshIndex) const {
    if (engine_ && engine_->GetObjModelManager()) {
        if (auto m = engine_->GetObjModelManager()->Resolve(modelHandle_)) {
            if (m->cpuModel && meshIndex < m->cpuModel->meshes.size()) {
                return &m->cpuModel->meshes[meshIndex].material;
            }
        }
    }
    return nullptr;
}

ObjMaterial* BaseModel::GetMaterial(size_t meshIndex) {
    if (engine_ && engine_->GetObjModelManager()) {
        if (auto m = engine_->GetObjModelManager()->Resolve(modelHandle_)) {
            if (m->cpuModel && meshIndex < m->cpuModel->meshes.size()) {
                return &m->cpuModel->meshes[meshIndex].material;
            }
        }
    }
    return nullptr;
}

void BaseModel::UpdateMaterials() {
    if (!engine_ || !engine_->GetObjModelManager()) {
        return;
    }
    auto m = engine_->GetObjModelManager()->Resolve(modelHandle_);
    if (!m || !m->cpuModel || meshResources_.empty()) {
        return;
    }

    // 全メッシュのマテリアルを更新
    for (size_t i = 0; i < m->cpuModel->meshes.size(); ++i) {
        if (i >= meshResources_.size()) {
            break;
        }

        auto& res = meshResources_[i];
        if (!res->GetMaterialData()) {
            continue;
        }

        const ObjMaterial* cpuMatPtr = &m->cpuModel->meshes[i].material;
        if (!materialOverrides_.empty()) {
            auto it = materialOverrides_.find(i);
            if (it != materialOverrides_.end()) {
                cpuMatPtr = &it->second;
            }
        }
        const ObjMaterial& cpuMat = *cpuMatPtr;
        Material* mappedData = res->GetMaterialData();

        // インスタンスカラーとマテリアルカラーを乗算
        mappedData->color.x = cpuMat.color.x * color_.x;
        mappedData->color.y = cpuMat.color.y * color_.y;
        mappedData->color.z = cpuMat.color.z * color_.z;
        mappedData->color.w = cpuMat.color.w * color_.w;
        if (mappedData->color.w <= 0.0f) {
            mappedData->color.w = 1.0f;
        }

        // ライティングの有効状態 (個別上書き優先)
        int32_t finalEnableLighting =
            (enableLightingOverride_ != -1) ? (enableLightingOverride_ == 1) : (cpuMat.enableLighting ? 1 : 0);
        mappedData->enableLighting = finalEnableLighting;

        mappedData->uvTransform = cpuMat.uvTransform;
        mappedData->metallic = cpuMat.metallic;
        mappedData->roughness = cpuMat.roughness;
        mappedData->hasTexture = !cpuMat.textureFilePath.empty();

        // 映り込み係数 (モデル値 * インスタンス係数)
        mappedData->environmentCoefficient = cpuMat.environmentCoefficient * environmentCoefficient_;

        // ライティングモード (個別上書き優先、指定なしならモデル値、ライティング無効なら0)
        if (lightingModeOverride_ != -1) {
            mappedData->lightingMode = lightingModeOverride_;
        } else {
            mappedData->lightingMode = finalEnableLighting ? cpuMat.lightingMode : 0;
        }

        // サンプラー設定 (個別上書き優先)
        mappedData->useClampSampler =
            (useClampSamplerOverride_ != -1) ? useClampSamplerOverride_ : cpuMat.useClampSampler;

        // アルファテスト用閾値
        mappedData->alphaReference = cpuMat.alphaReference;

        // エフェクトマスクとカスタムエフェクトの設定
        mappedData->enableEffectMask = enableEffectMask_ ? 1 : cpuMat.enableEffectMask;
        mappedData->customEffectType = customEffectType_ != 0 ? customEffectType_ : cpuMat.customEffectType;
        mappedData->customEffectParam = customEffectParam_ != 0.0f ? customEffectParam_ : cpuMat.customEffectParam;

        // (マテリアルバッファへの転送は SyncBeforeDraw() で行われるため、ここでは SyncMaterialData は呼ばない)
    }
}
