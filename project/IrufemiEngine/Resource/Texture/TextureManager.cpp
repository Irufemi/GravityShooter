#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <thread>
#include <format>

#include "Resource/Texture/TextureManager.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "../../../externals/DirectXTex/DirectXTex.h"
#include "../../../externals/DirectXTex/d3dx12.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Scene/SceneManager.h"
#include "Core/Utility/Log.h"
#include <iostream>

using namespace Irufemi;

static bool IsImageExtImpl(const std::string& ext) {
    static const char* exts[] = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds"};
    for (auto* e : exts) {
        if (_stricmp(ext.c_str(), e) == 0) {
            return true;
        }
    }
    return false;
}

// Initialize: DirectXCommon を保存し、Texture にも渡す
void TextureManager::Initialize(DirectXCommon* dxCommon) {
    dxCommon_ = dxCommon;
    Texture::SetDirectXCommon(dxCommon_);

    // メモリ予算の設定（RTX 3060 (12GB/8GB) 等に合わせて設定: 2GB）
    texturePool_.SetMemoryBudget(2048ULL * 1024ULL * 1024ULL);

    // ThreadPoolの生成 (論理コア数分)
    if (!threadPool_) {
        threadPool_ = std::make_unique<ThreadPool>(std::thread::hardware_concurrency());
    }
    if (!taskGroup_) {
        taskGroup_ = std::make_shared<TaskGroup>();
    }
    if (!backgroundTaskGroup_) {
        backgroundTaskGroup_ = std::make_shared<TaskGroup>();
    }

    // フォールバック用の白テクスチャ生成と登録
    CreateWhiteDummyTexture();
    CreateWhiteCubeMap();
    if (whiteTextureResource_.Get()) {
        Texture::SetWhiteTextureResource(whiteTextureResource_.Get());
    }
}

// 指定フォルダ配下を走査してロード(キーはフルパス文字列)
void TextureManager::LoadAllFromFolder(const std::string& folderPath) {
    if (!std::filesystem::exists(folderPath)) {
        Log::OutPutLog(std::cerr, "[TextureManager] Warning: Folder not found: " + folderPath + "\n");
        return;
    }

    for (auto& entry : std::filesystem::recursive_directory_iterator(folderPath)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto p = entry.path();
        auto ext = p.extension().string();
        if (!IsImageExtImpl(ext)) {
            continue;
        }

        const std::string key = p.generic_string();

        // 非同期ロード開始し、ハンドルはプールに入れておく（戻り値は無視）
        ResourceHandle h = LoadTexture(key);
        ReleaseTexture(h); // すぐにReleaseして参照を0にする（プールに残る）
    }
}

ResourceHandle TextureManager::LoadTexture(const std::string& name) {
    if (name.empty()) {
        return ResourceHandle(); // 無効なハンドル（白テクスチャ扱い）
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 既にハンドルが存在するかチェック
    auto it = nameToHandleMap_.find(name);
    if (it != nameToHandleMap_.end() && texturePool_.IsValid(it->second)) {
        texturePool_.RetainSlot(it->second); // 参照カウントを増やす
        return it->second;
    }

    // 予算を適用して古いものをパージする
    texturePool_.EnforceMemoryBudget([this](uint32_t index) {
        if (index < textureResources_.size() && textureResources_[index]) {
            textureResources_[index].reset(); // テクスチャの破棄（VRAM解放）
        }
    });

    // サイズを推定（仮に1MBとしておき、メタデータ取得後に更新）
    size_t estimatedSize = 1024 * 1024;
    ResourceHandle handle = texturePool_.AllocateSlot(estimatedSize);

    // vectorのサイズを必要に応じて拡張
    if (handle.index >= textureResources_.size()) {
        textureResources_.resize(handle.index + 1);
    }
    textureResources_[handle.index] = std::make_shared<Texture>();

    auto& tex = textureResources_[handle.index];

    nameToHandleMap_[name] = handle;

    // 非同期タスクとして投入
    // shared_ptrをキャプチャして、タスク実行中も生存させる
    auto texPtr = tex;
    EnqueueTask([texPtr, name, handle, this]() {
        texPtr->Initialize(name);

        // ロード完了後にサイズとプールスロットサイズを更新（メインスレッドのI/Oブロックを排除）
        uint32_t width = texPtr->GetWidth();
        uint32_t height = texPtr->GetHeight();
        if (width > 0 && height > 0) {
            size_t actualSize = static_cast<size_t>(width) * height * 4;
            std::lock_guard<std::mutex> lock(mutex_);
            texturePool_.UpdateSlotSize(handle, actualSize);
        }

        std::lock_guard<std::mutex> lock(mutex_);
        texturePool_.SetLoaded(handle, true);
    });

    return handle;
}

ResourceHandle TextureManager::RegisterExternalTexture(const std::string& name,
                                                       Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                                                       uint32_t srvIndex, D3D12_GPU_DESCRIPTOR_HANDLE srvHandle) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 既に同じ名前があれば参照カウントを増やして返す
    auto it = nameToHandleMap_.find(name);
    if (it != nameToHandleMap_.end()) {
        ResourceHandle handle = it->second;
        if (texturePool_.IsValid(handle)) {
            texturePool_.RetainSlot(handle);
            return handle;
        }
    }

    // 新規登録
    size_t memorySize = 0;
    if (resource) {
        auto desc = resource->GetDesc();
        memorySize = desc.Width * desc.Height * 4;
    }
    ResourceHandle handle = texturePool_.AllocateSlot(memorySize);
    if (!handle.IsValid()) {
        return handle; // エラー
    }

    if (textureResources_.size() <= handle.index) {
        textureResources_.resize(handle.index + 1);
    }

    auto tex = std::make_shared<Texture>();
    tex->InitializeFromExternalResource(name, resource, srvIndex, srvHandle);
    textureResources_[handle.index] = std::move(tex);
    nameToHandleMap_[name] = handle;

    return handle;
}

void TextureManager::ReleaseTexture(ResourceHandle handle) {
    texturePool_.ReleaseSlot(handle);
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::Resolve(ResourceHandle handle) const {
    if (!texturePool_.IsValid(handle)) {
        return whiteTextureHandle_;
    }

    // 使用されたことをプールに通知（LRUアクセス時刻の更新）
    const_cast<TextureManager*>(this)->texturePool_.TouchSlot(handle);

    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.index < textureResources_.size() && textureResources_[handle.index]) {
        return textureResources_[handle.index]->GetTextureSrvHandleGPU();
    }
    return whiteTextureHandle_;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::ResolveCubeMap(ResourceHandle handle) const {
    if (!texturePool_.IsValid(handle)) {
        return whiteCubeMapHandle_;
    }

    // 使用されたことをプールに通知（LRUアクセス時刻の更新）
    const_cast<TextureManager*>(this)->texturePool_.TouchSlot(handle);

    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.index < textureResources_.size() && textureResources_[handle.index]) {
        if (textureResources_[handle.index]->GetStatus() == Texture::LoadingStatus::Loaded) {
            if (textureResources_[handle.index]->IsCubemap()) {
                return textureResources_[handle.index]->GetTextureSrvHandleGPU();
            }
        }
    }
    return whiteCubeMapHandle_;
}

uint32_t TextureManager::GetSrvIndex(ResourceHandle handle) const {
    if (!texturePool_.IsValid(handle)) {
        return GetWhiteTextureSrvIndex();
    }

    // 使用されたことをプールに通知（LRUアクセス時刻の更新）
    const_cast<TextureManager*>(this)->texturePool_.TouchSlot(handle);

    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.index < textureResources_.size() && textureResources_[handle.index]) {
        return textureResources_[handle.index]->GetSrvIndex();
    }
    return GetWhiteTextureSrvIndex();
}

uint32_t TextureManager::GetWhiteTextureSrvIndex() const {
    return whiteTexture_ ? whiteTexture_->GetSrvIndex() : 0;
}

uint32_t TextureManager::GetWhiteCubeMapSrvIndex() const {
    return whiteCubeMap_ ? whiteCubeMap_->GetSrvIndex() : 0;
}

const Texture* TextureManager::GetTextureObject(ResourceHandle handle) const {
    if (!texturePool_.IsValid(handle)) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle.index < textureResources_.size()) {
        return textureResources_[handle.index].get();
    }
    return nullptr;
}

Texture::LoadingStatus TextureManager::GetTextureStatus(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nameToHandleMap_.find(name);
    if (it == nameToHandleMap_.end() || !texturePool_.IsValid(it->second)) {
        return Texture::LoadingStatus::Failed;
    }
    return textureResources_[it->second.index]->GetStatus();
}

const DirectX::ScratchImage* TextureManager::GetScratchImage(const std::string& name) const {
    ResourceHandle handle;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nameToHandleMap_.find(name);
        if (it != nameToHandleMap_.end() && texturePool_.IsValid(it->second)) {
            return textureResources_[it->second.index]->GetScratchImage();
        }
    }

    // キャッシュになければロード (同期的に動く)
    handle = const_cast<TextureManager*>(this)->LoadTexture(name);

    std::lock_guard<std::mutex> lock(mutex_);
    return textureResources_[handle.index]->GetScratchImage();
}

std::vector<std::string> TextureManager::GetTextureNames() const {
    std::vector<std::string> keys;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& kv : nameToHandleMap_) {
        if (texturePool_.IsValid(kv.second)) {
            keys.push_back(kv.first);
        }
    }
    return keys;
}

std::vector<std::string> TextureManager::GetTextureNamesForDebug() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (const auto& pair : nameToHandleMap_) {
        if (texturePool_.IsValid(pair.second)) {
            uint32_t id = pair.second.index;
            if (id < textureResources_.size() && textureResources_[id] && !textureResources_[id]->IsCubemap()) {
                names.push_back(pair.first);
            }
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> TextureManager::GetAllAllocatedSrvHandles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> handles;
    for (const auto& tex : textureResources_) {
        if (tex && tex->GetTextureSrvHandleGPU().ptr != 0) {
            handles.push_back(tex->GetTextureSrvHandleGPU());
        }
    }
    return handles;
}

std::vector<std::string> TextureManager::GetCubeMapNamesForDebug() const {
    std::vector<std::string> keys;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& kv : nameToHandleMap_) {
            if (texturePool_.IsValid(kv.second)) {
                uint32_t id = kv.second.index;
                if (id < textureResources_.size() && textureResources_[id]) {
                    if (textureResources_[id]->IsCubemap() || kv.first == "whiteCubeMap") {
                        keys.push_back(kv.first);
                    }
                }
            }
        }
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

void TextureManager::CreateWhiteDummyTexture() {
    if (whiteTextureHandle_.ptr != 0) {
        return;
    }
    if (!dxCommon_) {
        return;
    }

    uint32_t whitePixels[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};

    whiteTexture_ = std::make_unique<Texture>();
    whiteTexture_->InitializeFromMemory("white", whitePixels, 2, 2);

    whiteTextureResource_ = dxCommon_->CreateTextureResource(whiteTexture_->GetScratchImage()->GetMetadata());
    auto intermediate = dxCommon_->UploadTextureData(whiteTextureResource_, *whiteTexture_->GetScratchImage());
    dxCommon_->ReleaseAfterFence(intermediate);

    Texture::SetWhiteTextureResource(whiteTextureResource_.Get());

    whiteTextureHandle_ = whiteTexture_->GetTextureSrvHandleGPU();
}

void TextureManager::CreateWhiteCubeMap() {
    if (whiteCubeMapHandle_.ptr != 0) {
        return;
    }
    if (!dxCommon_) {
        return;
    }

    uint32_t whitePixels[6] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};

    whiteCubeMap_ = std::make_unique<Texture>();
    whiteCubeMap_->InitializeCubeFromMemory("whiteCubeMap", whitePixels, 1, 1);

    whiteCubeMapResource_ = dxCommon_->CreateTextureResource(whiteCubeMap_->GetScratchImage()->GetMetadata());
    auto intermediate = dxCommon_->UploadTextureData(whiteCubeMapResource_, *whiteCubeMap_->GetScratchImage());
    dxCommon_->ReleaseAfterFence(intermediate);

    whiteCubeMapHandle_ = whiteCubeMap_->GetTextureSrvHandleGPU();
}

bool TextureManager::GetTextureSize(const std::string& name, uint32_t& outWidth, uint32_t& outHeight) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nameToHandleMap_.find(name);
    if (it == nameToHandleMap_.end() || !texturePool_.IsValid(it->second)) {
        return false;
    }

    outWidth = textureResources_[it->second.index]->GetWidth();
    outHeight = textureResources_[it->second.index]->GetHeight();
    return true;
}

bool TextureManager::IsCurrentSceneInitializing() const {
    if (!dxCommon_) {
        return false;
    }
    auto engine = dxCommon_->GetEngine();
    if (!engine) {
        return false;
    }
    auto sceneManager = engine->GetSceneManager();
    if (!sceneManager) {
        return false;
    }
    return sceneManager->IsInitializing();
}

bool TextureManager::IsCubeMap(const std::string& name) const {
    if (name == "whiteCubeMap") {
        return true;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nameToHandleMap_.find(name);
    if (it != nameToHandleMap_.end() && texturePool_.IsValid(it->second)) {
        return textureResources_[it->second.index]->IsCubemap();
    }
    return false;
}