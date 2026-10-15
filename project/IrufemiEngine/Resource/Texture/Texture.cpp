#include "Core/Utility/ErrorUtility.h"
#include "Resource/Texture/Texture.h"
#include "../../../externals/DirectXTex/DirectXTex.h"
#include "../../../externals/DirectXTex/d3dx12.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "Core/Utility/Log.h"
#include <iostream>
#include <cassert>

DirectXCommon* Texture::dxCommon_ = nullptr;
uint32_t Texture::index_ = 0;
DescriptorPool* Texture::srvPool_ = nullptr;
ID3D12Resource* Texture::whiteResource_ = nullptr;

Texture::Texture() {
    // コンストラクタでSRV枠を確保し、初期状態として白テクスチャを割り当てておく
    if (srvPool_) {
        srvIndex_ = srvPool_->Allocate();
        if (srvIndex_ != DescriptorPool::kInvalid) {
            textureSrvHandleCPU_ = srvPool_->GetCPUHandle(srvIndex_);
            textureSrvHandleGPU_ = srvPool_->GetGPUHandle(srvIndex_);

            // 未ロード時のアクセス違反を防ぐため白テクスチャSRVで初期化（フォールバック）
            if (whiteResource_ && dxCommon_) {
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = 1;
                dxCommon_->GetDevice()->CreateShaderResourceView(whiteResource_, &srvDesc, textureSrvHandleCPU_);
            }
        }
    }
}

Texture::~Texture() {
    if (srvPool_ && srvIndex_ != UINT32_MAX && dxCommon_) {
        // GPU が参照し終わるまで遅延解放
        srvPool_->FreeAfterFence(srvIndex_, dxCommon_->GetCurrentFrameFenceValue());
        srvIndex_ = UINT32_MAX;
    }
}

void Texture::Initialize(const std::string& filePath) {
    this->filePath_ = filePath;
    status_.store(LoadingStatus::Loading);

    try {
        mipImages_ = dxCommon_->LoadTexture(filePath_);
        const DirectX::TexMetadata& metadata = mipImages_.GetMetadata();
        width_ = static_cast<uint32_t>(metadata.width);
        height_ = static_cast<uint32_t>(metadata.height);

        textureResource_ = dxCommon_->CreateTextureResource(metadata);
        intermediateResource_ = dxCommon_->UploadTextureData(textureResource_.Get(), mipImages_);

        // --- アップロード完了後に中間リソースを解放するように登録 ---
        dxCommon_->ReleaseAfterFence(intermediateResource_);
        intermediateResource_ = nullptr;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = metadata.format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        isCubemap_ = metadata.IsCubemap();
        if (isCubemap_) {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MostDetailedMip = 0;
            srvDesc.TextureCube.MipLevels = UINT_MAX;
            srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        } else {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);
        }

        // SRV上書き (データ競合を防ぐため、メインスレッドの安全なタイミングで更新するようキューに積む)
        if (textureSrvHandleCPU_.ptr != 0) {
            dxCommon_->EnqueueSRVUpdate(textureResource_, srvDesc, textureSrvHandleCPU_);
        }

        status_.store(LoadingStatus::Loaded);
    } catch (const std::exception& e) {
        Log::OutPutLog(std::cerr, "[Texture] Failed to load '" + filePath_ + "': " + e.what() + "\n");
        status_.store(LoadingStatus::Failed);
        // 失敗してもSRV自体は白テクスチャを指したままなので描画上は安全
    } catch (...) {
        Log::OutPutLog(std::cerr, "[Texture] Failed to load '" + filePath_ + "' (Unknown error)\n");
        status_.store(LoadingStatus::Failed);
        // 失敗してもSRV自体は白テクスチャを指したままなので描画上は安全
    }
}

void Texture::InitializeFromMemory(const std::string& name, const uint32_t* pixels, uint32_t width, uint32_t height) {
    this->filePath_ = name;
    this->width_ = width;
    this->height_ = height;
    status_.store(LoadingStatus::Loading);

    try {
        // sRGB フォーマットで初期化
        HRESULT hr = mipImages_.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, width, height, 1, 1);
        IRUFEMI_ASSERT_HR(hr, "Failed to initialize 2D texture memory");

        // ピクセルデータのコピー
        memcpy(mipImages_.GetImage(0, 0, 0)->pixels, pixels, width * height * sizeof(uint32_t));

        const DirectX::TexMetadata& metadata = mipImages_.GetMetadata();
        textureResource_ = dxCommon_->CreateTextureResource(metadata);
        intermediateResource_ = dxCommon_->UploadTextureData(textureResource_.Get(), mipImages_);

        dxCommon_->ReleaseAfterFence(intermediateResource_);
        intermediateResource_ = nullptr;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = metadata.format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        // SRV上書き
        if (textureSrvHandleCPU_.ptr != 0) {
            dxCommon_->EnqueueSRVUpdate(textureResource_, srvDesc, textureSrvHandleCPU_);
        }

        status_.store(LoadingStatus::Loaded);
    } catch (const std::exception& e) {
        Log::OutPutLog(std::cerr, "[Texture] Failed to load memory texture '" + filePath_ + "': " + e.what() + "\n");
        status_.store(LoadingStatus::Failed);
    } catch (...) {
        Log::OutPutLog(std::cerr, "[Texture] Failed to load memory texture '" + filePath_ + "' (Unknown error)\n");
        status_.store(LoadingStatus::Failed);
    }
}

void Texture::InitializeCubeFromMemory(const std::string& name, const uint32_t* pixels, uint32_t width,
                                       uint32_t height) {
    this->filePath_ = name;
    this->width_ = width;
    this->height_ = height;
    status_.store(LoadingStatus::Loading);

    try {
        // CubeMap として初期化
        HRESULT hr = mipImages_.InitializeCube(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, width, height, 1, 1);
        IRUFEMI_ASSERT_HR(hr, "Failed to initialize CubeMap texture memory");

        // 6面分のピクセルデータのコピー
        for (size_t i = 0; i < 6; ++i) {
            const DirectX::Image* img = mipImages_.GetImage(0, i, 0);
            memcpy(img->pixels, pixels + (i * width * height), width * height * sizeof(uint32_t));
        }

        const DirectX::TexMetadata& metadata = mipImages_.GetMetadata();
        textureResource_ = dxCommon_->CreateTextureResource(metadata);
        intermediateResource_ = dxCommon_->UploadTextureData(textureResource_.Get(), mipImages_);

        dxCommon_->ReleaseAfterFence(intermediateResource_);
        intermediateResource_ = nullptr;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = metadata.format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = 1;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

        // SRV上書き
        if (textureSrvHandleCPU_.ptr != 0) {
            dxCommon_->EnqueueSRVUpdate(textureResource_, srvDesc, textureSrvHandleCPU_);
        }

        isCubemap_ = true;
        status_.store(LoadingStatus::Loaded);
    } catch (const std::exception& e) {
        Log::OutPutLog(std::cerr, "[Texture] Failed to load cubemap '" + filePath_ + "': " + e.what() + "\n");
        status_.store(LoadingStatus::Failed);
    } catch (...) {
        Log::OutPutLog(std::cerr, "[Texture] Failed to load cubemap '" + filePath_ + "' (Unknown error)\n");
        status_.store(LoadingStatus::Failed);
    }
}

void Texture::InitializeFromExternalResource(const std::string& name, Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                                             uint32_t srvIndex, D3D12_GPU_DESCRIPTOR_HANDLE srvHandle) {
    this->filePath_ = name;
    this->textureResource_ = resource;

    // Textureコンストラクタで確保済みの古いsrvIndexを解放する
    if (srvPool_ && srvIndex_ != UINT32_MAX && dxCommon_) {
        srvPool_->FreeAfterFence(srvIndex_, dxCommon_->GetCurrentFrameFenceValue());
    }

    // 新しいインデックスとハンドルを保持
    this->srvIndex_ = srvIndex;
    this->textureSrvHandleGPU_ = srvHandle;
    this->textureSrvHandleCPU_.ptr = 0;

    if (resource) {
        auto desc = resource->GetDesc();
        this->width_ = static_cast<uint32_t>(desc.Width);
        this->height_ = static_cast<uint32_t>(desc.Height);
    }

    status_.store(LoadingStatus::Loaded);
}
