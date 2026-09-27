#include "Core/Utility/ErrorUtility.h"
#include "Core/Utility/StringUtility.h"
#include "Core/Utility/Log.h"
#include "Core/Utility/FileSystem.h"
#include <iostream>
#include "Resource/Model/ModelManager.h"
#include "Core/System/ThreadPool.h"
#include <filesystem>
#include <Windows.h>
#include <chrono>
#include <string>
#include <iostream>
#include "Core/Utility/Log.h"
#include <thread>
#include <format>
#include "Resource/Model/ModelImporter.h"
#include "Resource/Model/ModelSerializer.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "Resource/Texture/TextureManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Scene/SceneManager.h"
#include "Renderer/Data/Material.h"
#include "Renderer/Data/VertexData.h"
#include "Resource/Model/Data/Node.h"
#include "Resource/Model/Data/SkinCluster.h"
#include <thread>
#include <algorithm>
#include <limits>
#include <unordered_set>

//======================
// キャッシュ系(インスタンス)
//======================

DirectXCommon* GpuMesh::dxCommon_ = nullptr;

GpuMesh::~GpuMesh() {
    if (dxCommon_ && dxCommon_->GetSrvPool() && srvIndex != 0xFFFFFFFF) {
        dxCommon_->GetSrvPool()->FreeAfterFence(srvIndex, dxCommon_->GetCurrentFrameFenceValue());
    }
}

GpuMaterial::~GpuMaterial() {
    if (textureManager_ && textureHandle.IsValid()) {
        textureManager_->ReleaseTexture(textureHandle);
    }
}

ModelManager::ModelManager() = default;
ModelManager::~ModelManager() = default;

void ModelManager::Initialize(DirectXCommon* dxCommon, TextureManager* textureManager) {
    dxCommon_ = dxCommon;
    GpuMesh::dxCommon_ = dxCommon;
    textureManager_ = textureManager;
    if (rootDir_.empty()) {
        rootDir_ = FileSystem::GetResourcePath("model");
    }
    if (!threadPool_) {
        threadPool_ = std::make_unique<ThreadPool>(4); // 推奨された4スレッド
    }
    if (!taskGroup_) {
        taskGroup_ = std::make_shared<TaskGroup>();
    }
    if (!backgroundTaskGroup_) {
        backgroundTaskGroup_ = std::make_shared<TaskGroup>();
    }

    // メモリ予算の設定（RTX 3060 (12GB/8GB) 等に合わせて設定: 1GB）
    modelPool_.SetMemoryBudget(1024ULL * 1024ULL * 1024ULL);
}

void ModelManager::SetRootDirectory(std::string root) {
    std::replace(root.begin(), root.end(), '\\', '/');
    if (!root.empty() && root.back() == '/') {
        root.pop_back();
    }
    rootDir_ = std::move(root);

    // DirectoryWatcherの初期化
    directoryWatcher_ = std::make_unique<DirectoryWatcher>(rootDir_, [this]() { OnDirectoryChanged(); });
}

ResourceHandle ModelManager::LoadModel(const std::string& filename) {
    if (filename.empty()) {
        return ResourceHandle(); // 無効なハンドル
    }

    const std::string key = filename;
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (auto it = nameToHandleMap_.find(key); it != nameToHandleMap_.end() && modelPool_.IsValid(it->second)) {
        modelPool_.RetainSlot(it->second);
        return it->second;
    }

    // 古いキャッシュを破棄
    modelPool_.EnforceMemoryBudget([this](uint32_t index) {
        if (index < managedModels_.size() && managedModels_[index]) {
            managedModels_[index].reset();
        }
    });

    // サイズを推定 (仮: 5MB)
    size_t estimatedSize = 5 * 1024 * 1024;
    ResourceHandle handle = modelPool_.AllocateSlot(estimatedSize);

    if (handle.index >= managedModels_.size()) {
        managedModels_.resize(handle.index + 1);
    }
    managedModels_[handle.index] = std::make_shared<ManagedModel>();
    auto& managedModel = managedModels_[handle.index];
    managedModel->status.store(ManagedModel::LoadingStatus::Pending);

    nameToHandleMap_[key] = handle;

    // ファイルパスを解決
    std::string fullPath;
    if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos) {
        fullPath = NormalizeAndResolve(filename);
    } else {
        fullPath = FindFileRecursive(filename);
    }

    if (fullPath.empty() || !std::filesystem::exists(fullPath)) {
        Log::OutPutLog(std::cerr, "[ModelManager] File not found: " + filename);
        managedModel->status.store(ManagedModel::LoadingStatus::Failed);
        return handle;
    }

    Log::OutPutLog(std::cout,
                   std::format("[ModelManager] [Thread:{}] Request async load: {}", GetCurrentThreadId(), filename));

    auto managedModelPtr = managedModel;
    const_cast<ModelManager*>(this)->EnqueueTask([managedModelPtr, fullPath, handle, this]() {
        LoadInternal(managedModelPtr, fullPath);
        modelPool_.SetLoaded(handle, true);
    });

    return handle;
}

void ModelManager::ReleaseModel(ResourceHandle handle) {
    modelPool_.ReleaseSlot(handle);
}

ManagedModel* ModelManager::Resolve(ResourceHandle handle) const {
    if (!modelPool_.IsValid(handle)) {
        return nullptr;
    }
    const_cast<ModelManager*>(this)->modelPool_.TouchSlot(handle);

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (handle.index < managedModels_.size() && managedModels_[handle.index]) {
        return managedModels_[handle.index].get();
    }
    return nullptr;
}

void ModelManager::LoadInternal(std::shared_ptr<ManagedModel> managedModel, const std::string& fullPath) {

    std::string key = SplitDirectoryAndFile(fullPath).second;
    Log::OutPutLog(std::cout, std::format("[ModelManager] [Thread:{}] Worker START: {}", GetCurrentThreadId(), key));

    managedModel->status.store(ManagedModel::LoadingStatus::Loading);

    try {
        uint64_t currentLwt = 0;
        std::error_code ec;
        if (std::filesystem::exists(fullPath, ec)) {
            auto lastWrite = std::filesystem::last_write_time(fullPath, ec);
            currentLwt = std::chrono::duration_cast<std::chrono::seconds>(lastWrite.time_since_epoch()).count();
        }
        managedModel->lastLoadTime = currentLwt;
        managedModel->sourceFilePath = fullPath;

        std::string binPathStr = StringUtility::GetCacheFilePath(fullPath, "model", ".model.ibin");
        std::filesystem::path binPathFs(binPathStr);
        if (binPathFs.has_parent_path()) {
            std::filesystem::create_directories(binPathFs.parent_path());
        }
        std::string binPath = binPathStr;

        bool shouldImport = true;

        if (std::filesystem::exists(binPath, ec)) {
            auto loaded = std::make_shared<ObjModel>();
            uint64_t cachedLwt = 0;
            if (ModelSerializer::Deserialize(binPath, *loaded, cachedLwt) && cachedLwt == currentLwt) {
                managedModel->cpuModel = loaded;
                shouldImport = false;
                Log::OutPutLog(std::cout, std::format("[ModelManager] [Thread:{}] Loaded from Cache: {}",
                                                      GetCurrentThreadId(), key));
            }
        }

        if (shouldImport) {
            managedModel->cpuModel = std::make_shared<ObjModel>(ModelImporter::Import(fullPath));
            if (!managedModel->cpuModel->meshes.empty()) {
                ModelSerializer::Serialize(binPath, *managedModel->cpuModel, currentLwt);
            }
        }

        // GPUリソース生成
        managedModel->gpuMeshes.reserve(managedModel->cpuModel->meshes.size());
        managedModel->gpuMaterials.reserve(managedModel->cpuModel->meshes.size());

        for (const auto& cpuMesh : managedModel->cpuModel->meshes) {
            auto gpuMesh = std::make_shared<GpuMesh>();

            // Vertex Buffer
            if (!cpuMesh.vertices.empty()) {
                const size_t vbSize = sizeof(VertexData) * cpuMesh.vertices.size();
                gpuMesh->vertexResource = dxCommon_->CreateBufferResource(vbSize);
                if (!gpuMesh->vertexResource) {
                    Log::OutPutLog(std::cerr, "[ModelManager] Failed to create vertex buffer resource!");
                    continue; // Skip this mesh if resource creation failed
                }
                gpuMesh->vertexCount = static_cast<UINT>(cpuMesh.vertices.size());
                gpuMesh->vertexBufferView.BufferLocation = gpuMesh->vertexResource->GetGPUVirtualAddress();
                gpuMesh->vertexBufferView.SizeInBytes = static_cast<UINT>(vbSize);
                gpuMesh->vertexBufferView.StrideInBytes = sizeof(VertexData);
                VertexData* vbData = nullptr;
                gpuMesh->vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vbData));
                std::memcpy(vbData, cpuMesh.vertices.data(), vbSize);
                gpuMesh->vertexResource->Unmap(0, nullptr);

                gpuMesh->srvIndex = dxCommon_->GetSrvPool()->Allocate();
                IRUFEMI_ASSERT(gpuMesh->srvIndex != DescriptorPool::kInvalid);
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                srvDesc.Format = DXGI_FORMAT_UNKNOWN;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srvDesc.Buffer.FirstElement = 0;
                srvDesc.Buffer.NumElements = gpuMesh->vertexCount;
                srvDesc.Buffer.StructureByteStride = sizeof(VertexData);
                dxCommon_->GetDevice()->CreateShaderResourceView(
                    gpuMesh->vertexResource.Get(), &srvDesc, dxCommon_->GetSrvPool()->GetCPUHandle(gpuMesh->srvIndex));
                gpuMesh->vertexSrvHandle = dxCommon_->GetSrvPool()->GetGPUHandle(gpuMesh->srvIndex);
            }

            // Index Buffer
            if (!cpuMesh.indices.empty()) {
                const size_t ibSize = sizeof(uint32_t) * cpuMesh.indices.size();
                gpuMesh->indexResource = dxCommon_->CreateBufferResource(ibSize);
                if (gpuMesh->indexResource) {
                    gpuMesh->indexCount = static_cast<UINT>(cpuMesh.indices.size());
                    gpuMesh->indexBufferView.BufferLocation = gpuMesh->indexResource->GetGPUVirtualAddress();
                    gpuMesh->indexBufferView.SizeInBytes = static_cast<UINT>(ibSize);
                    gpuMesh->indexBufferView.Format = DXGI_FORMAT_R32_UINT;
                    uint32_t* ibData = nullptr;
                    gpuMesh->indexResource->Map(0, nullptr, reinterpret_cast<void**>(&ibData));
                    std::memcpy(ibData, cpuMesh.indices.data(), ibSize);
                    gpuMesh->indexResource->Unmap(0, nullptr);
                } else {
                    Log::OutPutLog(std::cerr, "[ModelManager] Failed to create index buffer resource!");
                }
            }
            managedModel->gpuMeshes.push_back(std::move(gpuMesh));

            // Materialリソース生成
            auto gpuMaterial = std::make_shared<GpuMaterial>(textureManager_);
            gpuMaterial->materialResource = dxCommon_->CreateBufferResource(sizeof(Material));
            Material* materialData = nullptr;
            gpuMaterial->materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

            materialData->color = cpuMesh.material.color;
            materialData->enableLighting = cpuMesh.material.enableLighting;
            materialData->uvTransform = cpuMesh.material.uvTransform;
            materialData->metallic = cpuMesh.material.metallic;
            materialData->roughness = cpuMesh.material.roughness;
            materialData->hasTexture = !cpuMesh.material.textureFilePath.empty();
            materialData->environmentCoefficient = 0.0f;
            materialData->lightingMode = cpuMesh.material.enableLighting ? 3 : 0;
            if (materialData->color.w <= 0.0f) {
                materialData->color.w = 1.0f;
            }

            if (materialData->hasTexture) {
                gpuMaterial->textureHandle = textureManager_->LoadTexture(cpuMesh.material.textureFilePath);
            } else {
                gpuMaterial->textureHandle = ResourceHandle();
            }
            managedModel->gpuMaterials.push_back(std::move(gpuMaterial));
        }

        // --- すべてのテクスチャのロード完了を待機 ---
        std::vector<std::string> texturePaths;
        for (const auto& mesh : managedModel->cpuModel->meshes) {
            if (!mesh.material.textureFilePath.empty()) {
                texturePaths.push_back(mesh.material.textureFilePath);
            }
        }

        bool allTexturesReady = false;
        while (!allTexturesReady) {
            allTexturesReady = true;
            for (const auto& path : texturePaths) {
                auto status = textureManager_->GetTextureStatus(path);
                if (status == Texture::LoadingStatus::Loading || status == Texture::LoadingStatus::Pending) {
                    allTexturesReady = false;
                    break;
                }
            }
            if (!allTexturesReady) {
                std::this_thread::yield(); // 他のロードタスク（TextureManager側）に CPU を譲る
            }
        }

        managedModel->status.store(ManagedModel::LoadingStatus::Loaded);
        Log::OutPutLog(std::cout,
                       std::format("[ModelManager] [Thread:{}] Worker FINISH: {}", GetCurrentThreadId(), key));
    } catch (...) {
        managedModel->status.store(ManagedModel::LoadingStatus::Failed);
        Log::OutPutLog(std::cerr,
                       std::format("[ModelManager] [Thread:{}] Worker FAILED: {}", GetCurrentThreadId(), key));
    }
}

bool ModelManager::IsCurrentSceneInitializing() const {
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

void ModelManager::PreloadAllUnder(const std::string& relativeFolder) {
    namespace fs = std::filesystem;
    const std::string rootBase = rootDir_.empty() ? "resources/model" : rootDir_;
    fs::path start = fs::path(rootBase) / relativeFolder;
    if (!fs::exists(start)) {
        Log::OutPutLog(std::cerr, "[ModelManager] Warning: Preload directory not found: " + start.string() + "\n");
        return;
    }

    for (auto& entry : fs::recursive_directory_iterator(start)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto p = entry.path();
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
            ResourceHandle h = LoadModel(p.filename().string());
            ReleaseModel(h); // すぐにReleaseして参照カウントを0にする（プールに残る）
        }
    }
}

std::vector<std::string> ModelManager::GetCachedKeys() const {
    std::vector<std::string> out;
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (const auto& kv : nameToHandleMap_) {
        if (modelPool_.IsValid(kv.second)) {
            out.push_back(kv.first);
        }
    }
    return out;
}

void ModelManager::RefreshAvailableModels() {
    namespace fs = std::filesystem;
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    availableModelsCache_.clear();

    const fs::path rootPath = rootDir_.empty() ? "resources/model" : rootDir_;
    if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
        Log::OutPutLog(std::cerr,
                       "[ModelManager] Warning: Model root directory not found: " + rootPath.string() + "\n");
        isAvailableModelsCached_ = true;
        return;
    }

    for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (ext == ".obj" || ext == ".gltf" || ext == ".fbx" || ext == ".glb") {
                // 同名ファイル対策として、ルートディレクトリからの相対パスでリスト化する
                std::string relPath = std::filesystem::relative(entry.path(), rootDir_).string();
                std::replace(relPath.begin(), relPath.end(), '\\', '/');
                availableModelsCache_.push_back(relPath);
            }
        }
    }
    isAvailableModelsCached_ = true;
}

std::vector<std::string> ModelManager::GetAvailableModels() const {
    if (!isAvailableModelsCached_) {
        const_cast<ModelManager*>(this)->RefreshAvailableModels();
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return availableModelsCache_;
}

void ModelManager::ClearAll() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    managedModels_.clear();
    nameToHandleMap_.clear();
    modelPool_.ClearAll([](uint32_t) {});
    filePathCache_.clear();
}

void ModelManager::OnDirectoryChanged() {
    // 変更が複数回呼ばれることを防ぐため少し待つ
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<std::shared_ptr<ManagedModel>> modelsToReload;
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        for (const auto& modelPtr : managedModels_) {
            if (modelPtr && modelPtr->status.load() == ManagedModel::LoadingStatus::Loaded) {
                if (modelPtr->sourceFilePath.empty()) {
                    continue;
                }

                std::error_code ec;
                if (std::filesystem::exists(modelPtr->sourceFilePath, ec)) {
                    auto lastWrite = std::filesystem::last_write_time(modelPtr->sourceFilePath, ec);
                    uint64_t currentLwt =
                        std::chrono::duration_cast<std::chrono::seconds>(lastWrite.time_since_epoch()).count();

                    // タイムスタンプが新しければリロード対象
                    if (currentLwt > modelPtr->lastLoadTime) {
                        modelsToReload.push_back(modelPtr);
                        modelPtr->lastLoadTime = currentLwt; // 二重検知を防ぐ
                    }
                }
            }
        }
    }

    for (auto model : modelsToReload) {
        Log::OutPutLog(std::cout, "[ModelManager] Hot-Reloading: " + model->sourceFilePath);
        // Criticalタスクとして積むことで、Sceneの更新を止めて安全にリソースをスワップする
        EnqueueTask(true, [this, model]() {
            model->gpuMeshes.clear();
            model->gpuMaterials.clear();
            LoadInternal(model, model->sourceFilePath);
        });
    }
}

std::string ModelManager::NormalizeAndResolve(const std::string& filename) const {
    std::string f = filename;
    std::replace(f.begin(), f.end(), '\\', '/');
    std::string f_clean = f;
    if (f_clean.find("./") == 0) {
        f_clean = f_clean.substr(2);
    } else if (f_clean.find("/") == 0) {
        f_clean = f_clean.substr(1);
    }

    std::string root_clean = rootDir_;
    if (root_clean.find("./") == 0) {
        root_clean = root_clean.substr(2);
    } else if (root_clean.find("/") == 0) {
        root_clean = root_clean.substr(1);
    }

    if (StartsWith(f_clean, root_clean + "/")) {
        f = "./" + f_clean;
    } else if (StartsWith(f_clean, root_clean)) {
        f = "./" + root_clean + "/" + f_clean.substr(root_clean.size());
    } else {
        f = "./" + root_clean + "/" + f_clean;
    }
    std::transform(f.begin(), f.end(), f.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return f;
}

bool ModelManager::StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), s.begin());
}

std::pair<std::string, std::string> ModelManager::SplitDirectoryAndFile(const std::string& full) {
    auto pos = full.find_last_of('/');
    if (pos == std::string::npos) {
        return {".", full};
    }
    return {full.substr(0, pos), full.substr(pos + 1)};
}

void ModelManager::DebugLogLoad(const std::string& key, size_t meshCount) {
#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
    std::string msg = "[ModelManager] Loaded GPU resources for: " + key + " meshes=" + std::to_string(meshCount) + "\n";
    Log::OutPutLog(std::cout, msg);
#endif
}

std::string ModelManager::FindFileRecursive(const std::string& filename) const {
    namespace fs = std::filesystem;
    std::string lowerFilename = filename;
    std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (auto it = filePathCache_.find(lowerFilename); it != filePathCache_.end()) {
            return it->second;
        }
    }

    const fs::path rootPath = rootDir_;
    if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
        Log::OutPutLog(std::cerr, "[ModelManager] Warning: Materials directory not found: " + rootPath.string() + "\n");
        return "";
    }

    for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
        if (entry.is_regular_file()) {
            std::string entryFilename = entry.path().filename().string();

            // _stricmp を用いて大文字小文字を無視した比較を行う（ループ内の無駄なアロケーションを排除）
            if (_stricmp(entryFilename.c_str(), filename.c_str()) == 0) {
                std::string foundPath = entry.path().string();
                std::replace(foundPath.begin(), foundPath.end(), '\\', '/');
                {
                    std::lock_guard<std::recursive_mutex> lock(mutex_);
                    filePathCache_[lowerFilename] = foundPath;
                }
                return foundPath;
            }
        }
    }

    return ""; // 見つからなかった
}

//======================
// 静的ロード関数群(旧 Function.h 移植)
//======================

namespace {
// レイと三角形の交差判定
bool IntersectRayTriangle(const Irufemi::Vector3& origin, const Irufemi::Vector3& direction, const Irufemi::Vector3& v0,
                          const Irufemi::Vector3& v1, const Irufemi::Vector3& v2, float& t) {
    const float kEpsilon = 1e-6f;
    Irufemi::Vector3 edge1 = Irufemi::Math::Subtract(v1, v0);
    Irufemi::Vector3 edge2 = Irufemi::Math::Subtract(v2, v0);
    Irufemi::Vector3 h = Irufemi::Math::Cross(direction, edge2);
    float a = Irufemi::Math::Dot(edge1, h);
    if (a > -kEpsilon && a < kEpsilon) {
        return false; // レイは三角形と平行
    }

    float f = 1.0f / a;
    Irufemi::Vector3 s = Irufemi::Math::Subtract(origin, v0);
    float u = f * Irufemi::Math::Dot(s, h);
    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    Irufemi::Vector3 q = Irufemi::Math::Cross(s, edge1);
    float v = f * Irufemi::Math::Dot(direction, q);
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    t = f * Irufemi::Math::Dot(edge2, q);
    return (t > kEpsilon);
}

// 点と三角形の最近接点を求める
Irufemi::Vector3 ClosestPointOnTriangle(const Irufemi::Vector3& p, const Irufemi::Vector3& a, const Irufemi::Vector3& b,
                                        const Irufemi::Vector3& c) {
    Irufemi::Vector3 ab = b - a;
    Irufemi::Vector3 ac = c - a;
    Irufemi::Vector3 ap = p - a;
    float d1 = Irufemi::Math::Dot(ab, ap);
    float d2 = Irufemi::Math::Dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        return a;
    }

    Irufemi::Vector3 bp = p - b;
    float d3 = Irufemi::Math::Dot(ab, bp);
    float d4 = Irufemi::Math::Dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) {
        return b;
    }

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return a + v * ab;
    }

    Irufemi::Vector3 cp = p - c;
    float d5 = Irufemi::Math::Dot(ab, cp);
    float d6 = Irufemi::Math::Dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) {
        return c;
    }

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return a + w * ac;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

// 重心座標を計算
Irufemi::Vector3 Barycentric(const Irufemi::Vector3& p, const Irufemi::Vector3& a, const Irufemi::Vector3& b,
                             const Irufemi::Vector3& c) {
    Irufemi::Vector3 v0 = b - a, v1 = c - a, v2 = p - a;
    float d00 = Irufemi::Math::Dot(v0, v0);
    float d01 = Irufemi::Math::Dot(v0, v1);
    float d11 = Irufemi::Math::Dot(v1, v1);
    float d20 = Irufemi::Math::Dot(v2, v0);
    float d21 = Irufemi::Math::Dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;
    return {u, v, w};
}
} // namespace

VoxelizedModel ModelManager::VoxelizeModel(const ObjModel& model, const Irufemi::Vector3Int& resolution,
                                           TextureManager* textureManager) {
    VoxelizedModel result;
    result.resolution = resolution;

    // 1. Irufemi::AABB(バウンディングボックス)の計算
    result.aabbMin = {(std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(),
                      (std::numeric_limits<float>::max)()};
    result.aabbMax = {(std::numeric_limits<float>::lowest)(), (std::numeric_limits<float>::lowest)(),
                      (std::numeric_limits<float>::lowest)()};

    for (const auto& mesh : model.meshes) {
        for (const auto& vertex : mesh.vertices) {
            result.aabbMin.x = (std::min)(result.aabbMin.x, vertex.position.x);
            result.aabbMin.y = (std::min)(result.aabbMin.y, vertex.position.y);
            result.aabbMin.z = (std::min)(result.aabbMin.z, vertex.position.z);
            result.aabbMax.x = (std::max)(result.aabbMax.x, vertex.position.x);
            result.aabbMax.y = (std::max)(result.aabbMax.y, vertex.position.y);
            result.aabbMax.z = (std::max)(result.aabbMax.z, vertex.position.z);
        }
    }

    Irufemi::Vector3 aabbSize = {result.aabbMax.x - result.aabbMin.x, result.aabbMax.y - result.aabbMin.y,
                                 result.aabbMax.z - result.aabbMin.z};
    Irufemi::Vector3 voxelSize = {aabbSize.x / resolution.x, aabbSize.y / resolution.y, aabbSize.z / resolution.z};

    // 2. 全てのボクセルをループ処理
    for (int z = 0; z < resolution.z; ++z) {
        for (int y = 0; y < resolution.y; ++y) {
            for (int x = 0; x < resolution.x; ++x) {
                // 3. 各ボクセルの中心座標を計算
                Irufemi::Vector3 voxelCenter = {result.aabbMin.x + (x + 0.5f) * voxelSize.x,
                                                result.aabbMin.y + (y + 0.5f) * voxelSize.y,
                                                result.aabbMin.z + (z + 0.5f) * voxelSize.z};

                int intersections = 0;

                // 3方向にレイを飛ばして多数決で内外判定 (1方向だと法線平行のポリゴンで誤判定しやすい)
                const Irufemi::Vector3 rayDirs[3] = {
                    {1.0f, 0.0f, 0.0f}, // X+
                    {0.0f, 1.0f, 0.0f}, // Y+
                    {0.0f, 0.0f, 1.0f}, // Z+
                };
                int intersectionPerDir[3] = {0, 0, 0};

                float minDistance = (std::numeric_limits<float>::max)();
                const ObjMesh* closestMesh = nullptr;
                VertexData closestTri[3];

                // 4. メッシュとの交差判定と最も近いポリゴンの探索
                for (const auto& mesh : model.meshes) {
                    size_t faceCount = mesh.indices.empty() ? mesh.vertices.size() : mesh.indices.size();
                    for (size_t i = 0; i < faceCount; i += 3) {
                        VertexData v0 = mesh.indices.empty() ? mesh.vertices[i] : mesh.vertices[mesh.indices[i]];
                        VertexData v1 =
                            mesh.indices.empty() ? mesh.vertices[i + 1] : mesh.vertices[mesh.indices[i + 1]];
                        VertexData v2 =
                            mesh.indices.empty() ? mesh.vertices[i + 2] : mesh.vertices[mesh.indices[i + 2]];

                        Irufemi::Vector3 p0 = {v0.position.x, v0.position.y, v0.position.z};
                        Irufemi::Vector3 p1 = {v1.position.x, v1.position.y, v1.position.z};
                        Irufemi::Vector3 p2 = {v2.position.x, v2.position.y, v2.position.z};

                        // 3方向それぞれ独立にカウント
                        for (int d = 0; d < 3; ++d) {
                            float t;
                            if (IntersectRayTriangle(voxelCenter, rayDirs[d], p0, p1, p2, t) && t > 0.0f) {
                                intersectionPerDir[d]++;
                            }
                        }

                        // 最も近いポリゴンを見つける
                        Irufemi::Vector3 closestPoint = ClosestPointOnTriangle(voxelCenter, p0, p1, p2);
                        Irufemi::Vector3 diff = closestPoint - voxelCenter;
                        float distanceSq = Irufemi::Math::Dot(diff, diff);

                        if (distanceSq < minDistance) {
                            minDistance = distanceSq;
                            closestMesh = &mesh;
                            closestTri[0] = v0;
                            closestTri[1] = v1;
                            closestTri[2] = v2;
                        }
                    }
                }

                // 5. 内外判定：各方向の交差回数が奇数なら「内部」→ 2/3以上で内部と判断（多数決）
                int insideVotes = 0;
                for (int d = 0; d < 3; ++d) {
                    if (intersectionPerDir[d] % 2 != 0) {
                        insideVotes++;
                    }
                }

                // 内部のボクセルのみ生成（2/3方向以上が内部判定で採用）
                if (insideVotes >= 2) {
                    Irufemi::Voxel newVoxel;
                    newVoxel.position = voxelCenter;
                    newVoxel.normal = {0.0f, 1.0f, 0.0f};      // 初期法線
                    newVoxel.color = {1.0f, 1.0f, 1.0f, 1.0f}; // 初期色
                    newVoxel.uv = {0.0f, 0.0f};                // 初期UV

                    if (closestMesh != nullptr) {
                        Irufemi::Vector3 p0 = {closestTri[0].position.x, closestTri[0].position.y,
                                               closestTri[0].position.z};
                        Irufemi::Vector3 p1 = {closestTri[1].position.x, closestTri[1].position.y,
                                               closestTri[1].position.z};
                        Irufemi::Vector3 p2 = {closestTri[2].position.x, closestTri[2].position.y,
                                               closestTri[2].position.z};

                        // Barycentric(重心座標)の計算
                        Irufemi::Vector3 closestPoint = ClosestPointOnTriangle(voxelCenter, p0, p1, p2);
                        Irufemi::Vector3 uvw = Barycentric(closestPoint, p0, p1, p2);

                        // ==========================================
                        // 法線(Normal)の補間と設定
                        // ==========================================
                        Irufemi::Vector3 n0 = closestTri[0].normal;
                        Irufemi::Vector3 n1 = closestTri[1].normal;
                        Irufemi::Vector3 n2 = closestTri[2].normal;

                        Irufemi::Vector3 interpolatedNormal = {n0.x * uvw.x + n1.x * uvw.y + n2.x * uvw.z,
                                                               n0.y * uvw.x + n1.y * uvw.y + n2.y * uvw.z,
                                                               n0.z * uvw.x + n1.z * uvw.y + n2.z * uvw.z};
                        newVoxel.normal = Irufemi::Math::Normalize(interpolatedNormal); // 正規化してボクセルに保存

                        // UVの取得
                        Irufemi::Vector2 uv0 = closestTri[0].texcoord;
                        Irufemi::Vector2 uv1 = closestTri[1].texcoord;
                        Irufemi::Vector2 uv2 = closestTri[2].texcoord;

                        Irufemi::Vector2 interpolatedUV = {uv0.x * uvw.x + uv1.x * uvw.y + uv2.x * uvw.z,
                                                           uv0.y * uvw.x + uv1.y * uvw.y + uv2.y * uvw.z};
                        newVoxel.uv = interpolatedUV;

                        // ==========================================
                        // 法線マップからの詳細な法線の計算・焼き付け
                        // ==========================================
                        if (!closestMesh->material.normalMapFilePath.empty() && textureManager) {
                            const DirectX::ScratchImage* nimg =
                                textureManager->GetScratchImage(closestMesh->material.normalMapFilePath);
                            if (nimg) {
                                int nwidth = static_cast<int>(nimg->GetMetadata().width);
                                int nheight = static_cast<int>(nimg->GetMetadata().height);

                                int ntexX = static_cast<int>(interpolatedUV.x * nwidth) % nwidth;
                                int ntexY = static_cast<int>(interpolatedUV.y * nheight) % nheight;
                                if (ntexX < 0) {
                                    ntexX += nwidth;
                                }
                                if (ntexY < 0) {
                                    ntexY += nheight;
                                }

                                const DirectX::Image* nimage = nimg->GetImage(0, 0, 0);
                                if (nimage) {
                                    uint8_t* npixels = nimage->pixels;
                                    size_t nrowPitch = nimage->rowPitch;
                                    size_t npixelStride = DirectX::BitsPerPixel(nimg->GetMetadata().format) / 8;
                                    uint8_t* npixel = npixels + (ntexY * nrowPitch) + (ntexX * npixelStride);

                                    // 1. サンプリングしたRGB[0, 255]を[-1.0, 1.0]のベクトルに変換
                                    Irufemi::Vector3 sampledNormal;
                                    sampledNormal.x = (npixel[0] / 255.0f) * 2.0f - 1.0f;
                                    sampledNormal.y = (npixel[1] / 255.0f) * 2.0f - 1.0f;
                                    sampledNormal.z = (npixel[2] / 255.0f) * 2.0f - 1.0f;

                                    // 2. 接空間ベクトル (Tangent, Bitangent) の計算
                                    Irufemi::Vector3 edge1 = p1 - p0;
                                    Irufemi::Vector3 edge2 = p2 - p0;
                                    Irufemi::Vector2 deltaUV1 = uv1 - uv0;
                                    Irufemi::Vector2 deltaUV2 = uv2 - uv0;

                                    float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

                                    Irufemi::Vector3 tangent;
                                    tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
                                    tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
                                    tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
                                    tangent = Irufemi::Math::Normalize(tangent);

                                    // グラム・シュミットの直交化を用いてTangentを再直交化
                                    tangent = Irufemi::Math::Normalize(
                                        tangent - newVoxel.normal * Irufemi::Math::Dot(tangent, newVoxel.normal));

                                    // Bitangentの計算 (NormalとTangentの外積に、UV方向による符号を掛ける)
                                    Irufemi::Vector3 bitangent = Irufemi::Math::Cross(newVoxel.normal, tangent);
                                    if (f < 0.0f) {
                                        bitangent.x *= -1.0f;
                                        bitangent.y *= -1.0f;
                                        bitangent.z *= -1.0f;
                                    }

                                    // 3. Tangent SpaceからLocal Spaceへの変換行列で合成
                                    // Matrix TBN( tangent, bitangent, newVoxel.normal )
                                    Irufemi::Vector3 localNormal;
                                    localNormal.x = tangent.x * sampledNormal.x + bitangent.x * sampledNormal.y +
                                                    newVoxel.normal.x * sampledNormal.z;
                                    localNormal.y = tangent.y * sampledNormal.x + bitangent.y * sampledNormal.y +
                                                    newVoxel.normal.y * sampledNormal.z;
                                    localNormal.z = tangent.z * sampledNormal.x + bitangent.z * sampledNormal.y +
                                                    newVoxel.normal.z * sampledNormal.z;

                                    newVoxel.normal = Irufemi::Math::Normalize(localNormal);
                                }
                            }
                        }
                        // ==========================================
                        // UVからテクスチャカラーをサンプリング
                        // ==========================================

                        if (!closestMesh->material.textureFilePath.empty() && textureManager) {
                            // GetScratchImage を使用する
                            const DirectX::ScratchImage* img =
                                textureManager->GetScratchImage(closestMesh->material.textureFilePath);

                            if (img) {
                                int width = static_cast<int>(img->GetMetadata().width);
                                int height = static_cast<int>(img->GetMetadata().height);

                                int texX = static_cast<int>(interpolatedUV.x * width) % width;
                                int texY = static_cast<int>(interpolatedUV.y * height) % height;
                                if (texX < 0) {
                                    texX += width;
                                }
                                if (texY < 0) {
                                    texY += height;
                                }

                                // 元のコードに合わせて GetImage(0, 0, 0) からピクセルデータを取得
                                const DirectX::Image* image = img->GetImage(0, 0, 0);
                                if (image && !DirectX::IsCompressed(img->GetMetadata().format)) {
                                    uint8_t* pixels = image->pixels;
                                    size_t rowPitch = image->rowPitch;
                                    size_t pixelStride = DirectX::BitsPerPixel(img->GetMetadata().format) / 8;

                                    // RGBA8 系統の場合のみ安全に読み取れる（雑な実装なため）
                                    if (pixelStride >= 4) {
                                        uint8_t* pixel = pixels + (texY * rowPitch) + (texX * pixelStride);
                                        newVoxel.color.x = pixel[0] / 255.0f;
                                        newVoxel.color.y = pixel[1] / 255.0f;
                                        newVoxel.color.z = pixel[2] / 255.0f;
                                        newVoxel.color.w = pixel[3] / 255.0f;
                                    } else {
                                        newVoxel.color = closestMesh->material.color;
                                    }
                                } else {
                                    // 圧縮テクスチャや不明な形式の場合はマテリアルカラーで代用
                                    newVoxel.color = closestMesh->material.color;
                                }
                            } else {
                                newVoxel.color = closestMesh->material.color;
                            }
                        } else {
                            newVoxel.color = closestMesh->material.color;
                        }
                    }

                    result.voxels.push_back(newVoxel);
                }
            }
        }
    }

    // 6. フォールバック処理（薄型モデル・オープンメッシュ対応）
    // 閉じたソリッド（水密メッシュ）でない場合や解像度が粗い場合、insideVotes >= 2 に該当するボクセルが0個になる。
    // その場合、メッシュ表面の各ポリゴン重心をボクセルグリッドにサンプリングして確実にボクセルを生成する。
    if (result.voxels.empty()) {
        std::unordered_set<uint64_t> occupiedCells;
        auto getCellKey = [](int x, int y, int z) -> uint64_t {
            return (static_cast<uint64_t>(x & 0x1FFFFF) << 42) | (static_cast<uint64_t>(y & 0x1FFFFF) << 21) |
                   (static_cast<uint64_t>(z & 0x1FFFFF));
        };

        for (const auto& mesh : model.meshes) {
            size_t faceCount = mesh.indices.empty() ? mesh.vertices.size() : mesh.indices.size();
            for (size_t i = 0; i < faceCount; i += 3) {
                VertexData v0 = mesh.indices.empty() ? mesh.vertices[i] : mesh.vertices[mesh.indices[i]];
                VertexData v1 = mesh.indices.empty() ? mesh.vertices[i + 1] : mesh.vertices[mesh.indices[i + 1]];
                VertexData v2 = mesh.indices.empty() ? mesh.vertices[i + 2] : mesh.vertices[mesh.indices[i + 2]];

                Irufemi::Vector3 p0 = {v0.position.x, v0.position.y, v0.position.z};
                Irufemi::Vector3 p1 = {v1.position.x, v1.position.y, v1.position.z};
                Irufemi::Vector3 p2 = {v2.position.x, v2.position.y, v2.position.z};

                // ポリゴンの重心位置
                Irufemi::Vector3 centroid = {(p0.x + p1.x + p2.x) / 3.0f, (p0.y + p1.y + p2.y) / 3.0f,
                                             (p0.z + p1.z + p2.z) / 3.0f};

                int gx =
                    std::clamp(static_cast<int>((centroid.x - result.aabbMin.x) / (std::max)(voxelSize.x, 0.0001f)), 0,
                               resolution.x - 1);
                int gy =
                    std::clamp(static_cast<int>((centroid.y - result.aabbMin.y) / (std::max)(voxelSize.y, 0.0001f)), 0,
                               resolution.y - 1);
                int gz =
                    std::clamp(static_cast<int>((centroid.z - result.aabbMin.z) / (std::max)(voxelSize.z, 0.0001f)), 0,
                               resolution.z - 1);

                uint64_t key = getCellKey(gx, gy, gz);
                if (occupiedCells.find(key) != occupiedCells.end()) {
                    continue;
                }
                occupiedCells.insert(key);

                Irufemi::Voxel newVoxel;
                newVoxel.position = {result.aabbMin.x + (gx + 0.5f) * voxelSize.x,
                                     result.aabbMin.y + (gy + 0.5f) * voxelSize.y,
                                     result.aabbMin.z + (gz + 0.5f) * voxelSize.z};

                // 法線の平均
                Irufemi::Vector3 avgNormal = {(v0.normal.x + v1.normal.x + v2.normal.x) / 3.0f,
                                              (v0.normal.y + v1.normal.y + v2.normal.y) / 3.0f,
                                              (v0.normal.z + v1.normal.z + v2.normal.z) / 3.0f};
                newVoxel.normal = Irufemi::Math::Normalize(avgNormal);

                // テクスチャサンプリングまたはマテリアルカラー
                Irufemi::Vector2 avgUV = {(v0.texcoord.x + v1.texcoord.x + v2.texcoord.x) / 3.0f,
                                          (v0.texcoord.y + v1.texcoord.y + v2.texcoord.y) / 3.0f};
                newVoxel.uv = avgUV;

                newVoxel.color = mesh.material.color;
                if (!mesh.material.textureFilePath.empty() && textureManager) {
                    const DirectX::ScratchImage* img = textureManager->GetScratchImage(mesh.material.textureFilePath);
                    if (img) {
                        int width = static_cast<int>(img->GetMetadata().width);
                        int height = static_cast<int>(img->GetMetadata().height);
                        if (width > 0 && height > 0) {
                            int texX = static_cast<int>(avgUV.x * width) % width;
                            int texY = static_cast<int>(avgUV.y * height) % height;
                            if (texX < 0) {
                                texX += width;
                            }
                            if (texY < 0) {
                                texY += height;
                            }

                            const DirectX::Image* image = img->GetImage(0, 0, 0);
                            if (image && !DirectX::IsCompressed(img->GetMetadata().format)) {
                                uint8_t* pixels = image->pixels;
                                size_t rowPitch = image->rowPitch;
                                size_t pixelStride = DirectX::BitsPerPixel(img->GetMetadata().format) / 8;
                                if (pixelStride >= 4) {
                                    uint8_t* pixel = pixels + (texY * rowPitch) + (texX * pixelStride);
                                    newVoxel.color.x = pixel[0] / 255.0f;
                                    newVoxel.color.y = pixel[1] / 255.0f;
                                    newVoxel.color.z = pixel[2] / 255.0f;
                                    newVoxel.color.w = pixel[3] / 255.0f;
                                }
                            }
                        }
                    }
                }

                result.voxels.push_back(newVoxel);
            }
        }
    }

    // 万一ポリゴンが0個の特殊メッシュでも空返却によるエラーを防止する安全ガード
    if (result.voxels.empty()) {
        Irufemi::Voxel fallbackVoxel;
        fallbackVoxel.position = {(result.aabbMin.x + result.aabbMax.x) * 0.5f,
                                  (result.aabbMin.y + result.aabbMax.y) * 0.5f,
                                  (result.aabbMin.z + result.aabbMax.z) * 0.5f};
        fallbackVoxel.normal = {0.0f, 1.0f, 0.0f};
        fallbackVoxel.color = {1.0f, 1.0f, 1.0f, 1.0f};
        fallbackVoxel.uv = {0.0f, 0.0f};
        result.voxels.push_back(fallbackVoxel);
    }

    return result;
}

std::shared_ptr<VoxelizedModel> ModelManager::GetVoxelizedModel(const std::string& filename,
                                                                const Irufemi::Vector3Int& resolution) {
    ResourceHandle handle = LoadModel(filename);
    ManagedModel* managedModel = Resolve(handle);

    if (!managedModel) {
        return nullptr;
    }

    while (true) {
        auto status = managedModel->status.load();
        if (status == ManagedModel::LoadingStatus::Loaded || status == ManagedModel::LoadingStatus::Failed) {
            break;
        }
        std::this_thread::yield();
    }

    if (managedModel->status.load() == ManagedModel::LoadingStatus::Failed || !managedModel->cpuModel) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(managedModel->voxelMutex);

    // 既に同じ解像度でボクセル化されていれば、それを返す
    for (const auto& cached : managedModel->cachedVoxelModels) {
        if (cached->resolution.x == resolution.x && cached->resolution.y == resolution.y &&
            cached->resolution.z == resolution.z) {
            return cached;
        }
    }

    // 見つからなければ新規計算して、キャッシュリストに追加
    auto vModel = std::make_shared<VoxelizedModel>(VoxelizeModel(*managedModel->cpuModel, resolution, textureManager_));
    managedModel->cachedVoxelModels.push_back(vModel);

    return vModel;
}
