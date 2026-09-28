#include "Framework/Component/Renderer/SkyDomeComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Resource/Texture/TextureManager.h"
#include "Renderer/DrawManager.h"
#include "Core/Math/MathFunction.h"

SkyDomeComponent::~SkyDomeComponent() {
    if (materialResource_ && mappedMaterialData_) {
        materialResource_->Unmap(0, nullptr);
        mappedMaterialData_ = nullptr;
    }
}

void SkyDomeComponent::Initialize() {
    OnAwake();
}

void SkyDomeComponent::OnAwake() {
    if (!materialResource_) {
        auto* engine = GetEngine();
        if (engine && engine->GetDirectXCommon()) {
            size_t size = (sizeof(Material) + 255) & ~255;
            materialResource_ = engine->GetDirectXCommon()->CreateBufferResource(size);
            if (materialResource_) {
                materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedMaterialData_));
            }
        }
    }

    ReloadTexture();
    UpdateMaterialBuffer();
}

void SkyDomeComponent::ReloadTexture() {
    auto* engine = GetEngine();
    if (!engine || !engine->GetTextureManager() || texturePath_.empty()) {
        return;
    }

    auto texManager = engine->GetTextureManager();
    auto handle = texManager->LoadTexture(texturePath_);
    textureIndex_ = texManager->GetSrvIndex(handle);
}

void SkyDomeComponent::UpdateMaterialBuffer() {
    if (!mappedMaterialData_) {
        return;
    }

    mappedMaterialData_->color = color_ * intensity_;
    mappedMaterialData_->enableLighting = 0;
    mappedMaterialData_->hasTexture = 1;
    mappedMaterialData_->lightingMode = 0;
    mappedMaterialData_->environmentCoefficient = 0.0f;
    mappedMaterialData_->metallic = 0.0f;
    mappedMaterialData_->roughness = 1.0f;
    mappedMaterialData_->useClampSampler = 0; // Wrapサンプラー使用
    mappedMaterialData_->alphaReference = 0.0f;
    mappedMaterialData_->textureIndex = textureIndex_;
    mappedMaterialData_->enableEffectMask = 0;

    // UV変換行列の作成
    Irufemi::Matrix4x4 mat = Irufemi::Math::MakeIdentity4x4();
    mat.m[0][0] = uvTiling_.x;
    mat.m[1][1] = uvTiling_.y;
    mat.m[3][0] = uvOffset_.x;
    mat.m[3][1] = uvOffset_.y;
    mappedMaterialData_->uvTransform = mat;
}

void SkyDomeComponent::Update() {
    UpdateMaterialBuffer();
}

void SkyDomeComponent::Draw() {
    if (!materialResource_) {
        return;
    }

    auto* engine = GetEngine();
    if (engine && engine->GetDrawManager()) {
        engine->GetDrawManager()->SubmitSkydome(materialResource_->GetGPUVirtualAddress());
    }
}

void SkyDomeComponent::SetTexturePath(const std::string& path) {
    texturePath_ = path;
    ReloadTexture();
    UpdateMaterialBuffer();
}

void SkyDomeComponent::OnRegisterProperties() {
    RegisterProperty("Texture Path", &texturePath_).OnChanged([this]() {
        ReloadTexture();
        UpdateMaterialBuffer();
    });
    RegisterProperty("Color", &color_);
    RegisterProperty("Intensity", &intensity_);
    RegisterProperty("UV Offset", &uvOffset_);
    RegisterProperty("UV Tiling", &uvTiling_);
}

nlohmann::json SkyDomeComponent::Serialize() {
    nlohmann::json j;
    j["texturePath"] = texturePath_;
    j["color"] = {color_.x, color_.y, color_.z, color_.w};
    j["intensity"] = intensity_;
    j["uvOffset"] = {uvOffset_.x, uvOffset_.y};
    j["uvTiling"] = {uvTiling_.x, uvTiling_.y};
    return j;
}

void SkyDomeComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("texturePath")) {
        texturePath_ = j["texturePath"].get<std::string>();
    }
    if (j.contains("color")) {
        auto arr = j["color"];
        color_ = {arr[0], arr[1], arr[2], arr[3]};
    }
    if (j.contains("intensity")) {
        intensity_ = j["intensity"].get<float>();
    }
    if (j.contains("uvOffset")) {
        auto arr = j["uvOffset"];
        uvOffset_ = {arr[0], arr[1]};
    }
    if (j.contains("uvTiling")) {
        auto arr = j["uvTiling"];
        uvTiling_ = {arr[0], arr[1]};
    }

    ReloadTexture();
    UpdateMaterialBuffer();
}

