#include "Renderer/Object/Skybox/Skybox.h"
#include "Renderer/Camera/CameraManager.h"

#include "Core/System/IrufemiEngine.h"
#include "Core/Math/Math.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Renderer/DrawManager.h"
#include "Resource/Texture/TextureManager.h"
#include "Renderer/Camera/Camera.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif
#include "Renderer/Object/PrimitiveManager.h"

IrufemiEngine* Skybox::engine_ = nullptr;

Skybox::Skybox() {}

Skybox::~Skybox() {
    UnMapResource();
    if (engine_ && engine_->GetTextureManager() && textureHandle_.IsValid()) {
        engine_->GetTextureManager()->ReleaseTexture(textureHandle_);
    }
}

void Skybox::Initialize(const std::string& textureName) {

    // PrimitiveManager からスカイボックス用の形状（Cube）を取得
    PrimitiveManager* primitiveManager = engine_->GetPrimitiveManager();
    const PrimitiveData& primitiveData = primitiveManager->GetPrimitiveData(Irufemi::PrimitiveType::Skybox);

    vertexDataList_ = primitiveData.vertices;
    indexDataList_ = primitiveData.indices;

    CreateResource();
    MapResource();

    // 頂点バッファの設定
    vertexBufferView_ = {};
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
    vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertexDataList_.size());

    std::copy(vertexDataList_.begin(), vertexDataList_.end(), vertexData_);

    // インデックスバッファの設定
    indexBufferView_ = {};
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indexDataList_.size());
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    std::copy(indexDataList_.begin(), indexDataList_.end(), indexData_);

    // フラグ更新
    isDirty_ = false;
    if (Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera()) {
        lastViewMatrix_ = activeCam->GetViewMatrix();
        lastProjectionMatrix_ = activeCam->GetPerspectiveFovMatrix();
    }

    TextureManager* textureManager = engine_->GetTextureManager();

    if (textureHandle_.IsValid()) {
        textureManager->ReleaseTexture(textureHandle_);
        textureHandle_ = ResourceHandle();
    }

    if (textureName != "whiteCubeMap" && !textureName.empty()) {
        auto textureNames = textureManager->GetCubeMapNamesForDebug();
        if (!textureNames.empty()) {
            textureHandle_ = textureManager->LoadTexture(textureName);

            // コンボボックス用に selectedIndex を初期化
            auto it = std::find(textureNames.begin(), textureNames.end(), textureName);
            if (it != textureNames.end()) {
                selectedTextureIndex_ = static_cast<int>(std::distance(textureNames.begin(), it));
            } else {
                selectedTextureIndex_ = 0;
            }
        }
    }
}

void Skybox::Update() {

    Irufemi::Matrix4x4 worldMatrix =
        Irufemi::Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    // シェーダー側でgCameraを使用するようになったためWVPの計算を省略
    transformationMatrix_.WVP = Irufemi::Math::MakeIdentity4x4();
    transformationMatrix_.World = worldMatrix;
    // フラグ更新
    isDirty_ = false;
    MarkAsDirty();
    if (Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera()) {
        lastViewMatrix_ = activeCam->GetViewMatrix();
        lastProjectionMatrix_ = activeCam->GetPerspectiveFovMatrix();
    }
}

void Skybox::SyncBeforeDraw() {
    uint32_t frameIndex = engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex();

    // [Bindless] テクスチャインデックスの反映
    if (materialBuffer_[frameIndex]) {
        TextureManager* tm = engine_->GetTextureManager();
        if (textureHandle_.IsValid()) {
            materialBuffer_[frameIndex]->textureIndex = tm->GetSrvIndex(textureHandle_);
        } else {
            materialBuffer_[frameIndex]->textureIndex = tm->GetWhiteCubeMapSrvIndex();
        }
    }

    if (CheckAndClearDirty(frameIndex)) {
        transformationBuffer_.Update(transformationMatrix_, frameIndex);
    }
}

void Skybox::Draw() {
    if (!vertexResource_ || !indexResource_ || !engine_) {
        return;
    }
    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    // カメラの行列が変更されたか、オブジェクト自体が変更されたかチェック
    bool cameraChanged =
        (std::memcmp(&lastViewMatrix_, &activeCam->GetViewMatrix(), sizeof(Irufemi::Matrix4x4)) != 0 ||
         std::memcmp(&lastProjectionMatrix_, &activeCam->GetPerspectiveFovMatrix(), sizeof(Irufemi::Matrix4x4)) != 0);

    if (isDirtyBuffer_[engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex()] || cameraChanged) {
        Update();
    }

    SyncBeforeDraw();

    DrawManager* drawManager = engine_->GetDrawManager();

    engine_->ApplyPSO("Skybox");

    uint32_t frameIndex = engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex();
    // [Bindless] gpuHandle を渡さない
    drawManager->SubmitSkybox(vertexBufferView_, indexBufferView_,
                              materialBuffer_.GetResource(frameIndex)->GetGPUVirtualAddress(),
                              transformationBuffer_.GetResource(frameIndex)->GetGPUVirtualAddress(),
                              static_cast<UINT>(indexDataList_.size()));
}

void Skybox::Debug() {
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("Skybox")) {
        TextureManager* textureManager = engine_->GetTextureManager();
        auto textureNames = textureManager->GetCubeMapNamesForDebug();

        if (!textureNames.empty()) {
            if (ImGui::Combo(
                    "Texture", &selectedTextureIndex_,
                    [](void* data, int idx) {
                        auto* names = reinterpret_cast<std::vector<std::string>*>(data);
                        if (idx < 0 || idx >= static_cast<int>(names->size())) {
                            return (const char*)nullptr;
                        }
                        return (*names)[idx].c_str();
                    },
                    &textureNames, static_cast<int>(textureNames.size()))) {
                // 選択が変更された
                std::string selectedName = textureNames[selectedTextureIndex_];
                if (textureHandle_.IsValid()) {
                    textureManager->ReleaseTexture(textureHandle_);
                    textureHandle_ = ResourceHandle();
                }
                if (selectedName != "whiteCubeMap" && selectedName != "white") {
                    textureHandle_ = textureManager->LoadTexture(selectedName);
                }
            }
            uint32_t frameIndex = engine_->GetDrawManager()->GetDxCommon()->GetFrameIndex();
            if (materialBuffer_[frameIndex]) {
                ImGui::SliderFloat("Intensity", &materialBuffer_[frameIndex]->intensity, 0.0f, 10.0f);
            }
        }
    }
#endif
}

void Skybox::CreateResource() {

    DirectXCommon* dxCommon = engine_->GetDrawManager()->GetDxCommon();

    // 頂点・インデックスの静的リソースは単一バッファのまま
    if (!vertexResource_) {
        vertexResource_ =
            dxCommon->CreateBufferResource(sizeof(VertexData) * static_cast<size_t>(vertexDataList_.size()));
        vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
        vertexBufferView_.SizeInBytes = sizeof(VertexData) * static_cast<UINT>(vertexDataList_.size());
        vertexBufferView_.StrideInBytes = sizeof(VertexData);
    }
    if (!indexResource_) {
        indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * static_cast<size_t>(indexDataList_.size()));
        indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
        indexBufferView_.SizeInBytes = sizeof(uint32_t) * static_cast<UINT>(indexDataList_.size());
        indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    }

    // マテリアルとワールド変換状態をマルチバッファで確保
    materialBuffer_.Initialize(dxCommon);
    transformationBuffer_.Initialize(dxCommon);
}

void Skybox::MapResource() {
    if (vertexResource_) {
        vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
        for (size_t i = 0; i < vertexDataList_.size(); ++i) {
            vertexData_[i] = vertexDataList_[i];
        }
    }
    if (indexResource_) {
        indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
        for (size_t i = 0; i < indexDataList_.size(); ++i) {
            indexData_[i] = indexDataList_[i];
        }
    }
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (materialBuffer_[i]) {
            // 初期値
            materialBuffer_[i]->color = {1.0f, 1.0f, 1.0f, 1.0f};
            materialBuffer_[i]->intensity = 1.0f;
        }
        if (transformationBuffer_[i]) {
            // 初期行列
            transformationBuffer_[i]->WVP = Irufemi::Math::MakeIdentity4x4();
            transformationBuffer_[i]->World = Irufemi::Math::MakeIdentity4x4();
            transformationBuffer_[i]->WorldInverseTranspose = Irufemi::Math::MakeIdentity4x4();
        }
    }
}

void Skybox::UnMapResource() {
    if (vertexResource_ && vertexData_) {
        vertexResource_->Unmap(0, nullptr);
        vertexData_ = nullptr;
    }
    if (indexResource_ && indexData_) {
        indexResource_->Unmap(0, nullptr);
        indexData_ = nullptr;
    }
}
