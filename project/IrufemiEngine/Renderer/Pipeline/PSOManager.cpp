#include "Core/Utility/ErrorUtility.h"
#include "Renderer/Pipeline/PSOManager.h"
#include <cstring>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>
#include "Core/System/ThreadPool.h"
#include "Core/System/TaskGroup.h"
#include "Core/Utility/Log.h"
#include "Core/Utility/JsonUtility.h"
#include <nlohmann/json.hpp>
#include <iostream>

static const std::string kCacheDirectory = "generated/cache/pso/";

// 軽量ハッシュ(キャッシュキー用)
static uint64_t FNV1a(const void* p, size_t n, uint64_t h = 1469598103934665603ull) {
    const uint8_t* b = (const uint8_t*)p;
    while (n--) {
        h ^= *b++;
        h *= 1099511628211ull;
    }
    return h;
}

PSOManager::~PSOManager() {
    SaveManifest();
}

void PSOManager::Initialize(ID3D12Device* device, ID3D12RootSignature* rootSig,
                            const D3D12_INPUT_LAYOUT_DESC& inputLayout, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat,
                            D3D12_PRIMITIVE_TOPOLOGY_TYPE topology) {
    device_ = device;
    rootSig_ = rootSig;
    // ディープコピー: 要素配列を所有し、inputLayout_ には自前のポインタを設定
    inputElements_.assign(inputLayout.pInputElementDescs, inputLayout.pInputElementDescs + inputLayout.NumElements);

    // SemanticName の文字列実体もコピーして保持する必要がある
    semanticNames_.clear();
    semanticNames_.reserve(inputElements_.size());
    for (auto& elem : inputElements_) {
        // C文字列をコピーして保持
        semanticNames_.push_back(std::string(elem.SemanticName));
        // コピーした文字列のポインタをおきかえる
        elem.SemanticName = semanticNames_.back().c_str();
    }

    inputLayout_.pInputElementDescs = inputElements_.data();
    inputLayout_.NumElements = static_cast<UINT>(inputElements_.size());
    rtvFormat_ = rtvFormat; // 既存の RTV 形式
    dsvFormat_ = dsvFormat; // 既存の DSV 形式
    topology_ = topology;   // 三角形トポロジ固定(既存)

    shaderRegistry_.clear();
    cache_.clear();
    cacheKeysByName_.clear();
}

void PSOManager::RegisterShader(const std::string& name, const PipelineStateDesc& desc) {
    std::unique_lock<std::shared_mutex> lock(psoMutex_);

    // 同じ名前のシェーダーが上書き登録された場合、関連するキャッシュを削除する（ホットリロード対応）
    auto it = cacheKeysByName_.find(name);
    if (it != cacheKeysByName_.end()) {
        for (const auto& key : it->second) {
            cache_.erase(key);
        }
        cacheKeysByName_.erase(it);
    }

    shaderRegistry_[name] = desc;
}

ID3D12PipelineState* PSOManager::GetPSO(const std::string& name, Irufemi::BlendMode blend, DepthWrite depth,
                                        CullMode cull) {
    {
        std::lock_guard<std::mutex> lock(manifestMutex_);
        recordedGraphicsPSOs_.insert({name, blend, depth, cull});
    }

    Key key{Hash(name, blend, depth, cull)};
    PipelineStateDesc psoDesc;
    {
        std::shared_lock<std::shared_mutex> readLock(psoMutex_);
        if (auto cit = cache_.find(key); cit != cache_.end()) {
            return cit->second.Get();
        }
        auto it = shaderRegistry_.find(name);
        if (it == shaderRegistry_.end()) {
            return nullptr;
        }
        psoDesc = it->second;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSig_.Get();
    if (psoDesc.useNullInputLayout) {
        desc.InputLayout = {nullptr, 0};
    } else {
        desc.InputLayout = inputLayout_;
    }
    desc.VS = {psoDesc.shaders.vsBlob ? psoDesc.shaders.vsBlob->GetBufferPointer() : nullptr,
               psoDesc.shaders.vsBlob ? psoDesc.shaders.vsBlob->GetBufferSize() : 0};
    desc.PS = {psoDesc.shaders.psBlob ? psoDesc.shaders.psBlob->GetBufferPointer() : nullptr,
               psoDesc.shaders.psBlob ? psoDesc.shaders.psBlob->GetBufferSize() : 0};
    if (psoDesc.shaders.gsBlob) {
        desc.GS = {psoDesc.shaders.gsBlob->GetBufferPointer(), psoDesc.shaders.gsBlob->GetBufferSize()};
    }

    desc.BlendState = MakeBlend(blend);

    D3D12_RASTERIZER_DESC rast{};
    rast.FillMode =
        (psoDesc.topology == D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE) ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    switch (cull) {
    case CullMode::Back:
        rast.CullMode = D3D12_CULL_MODE_BACK;
        break;
    case CullMode::Front:
        rast.CullMode = D3D12_CULL_MODE_FRONT;
        break;
    case CullMode::None:
    default:
        rast.CullMode = D3D12_CULL_MODE_NONE;
        break;
    }
    rast.FrontCounterClockwise = FALSE;

    // シャドウマップ特有のラスタライザ設定
    if (psoDesc.isDepthOnly) {
        rast.DepthBias = 3000;
        rast.SlopeScaledDepthBias = 1.0f;
    } else {
        rast.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rast.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    }
    rast.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rast.DepthClipEnable = TRUE;
    rast.MultisampleEnable = FALSE;
    rast.AntialiasedLineEnable = FALSE;
    rast.ForcedSampleCount = 0;
    rast.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    desc.RasterizerState = rast;

    if (psoDesc.disableDepthTest) {
        desc.DepthStencilState = MakeDepth(DepthWrite::Off);
    } else {
        desc.DepthStencilState = MakeDepth(depth);
    }

    if (psoDesc.noDSV) {
        desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    } else {
        desc.DSVFormat = psoDesc.dsvFormat != DXGI_FORMAT_UNKNOWN ? psoDesc.dsvFormat : dsvFormat_;
    }

    if (psoDesc.isDepthOnly) {
        desc.NumRenderTargets = 0;
    } else {
        desc.NumRenderTargets = psoDesc.numRenderTargets;
        desc.RTVFormats[0] = psoDesc.rtvFormat != DXGI_FORMAT_UNKNOWN ? psoDesc.rtvFormat : rtvFormat_;
        if (psoDesc.numRenderTargets > 1) {
            desc.RTVFormats[1] = psoDesc.rtvFormat1;
        }
        if (psoDesc.numRenderTargets > 2) {
            desc.RTVFormats[2] = psoDesc.rtvFormat2;
        }
        if (psoDesc.numRenderTargets > 3) {
            desc.RTVFormats[3] = psoDesc.rtvFormat3;
        }
        if (psoDesc.numRenderTargets > 4) {
            desc.RTVFormats[4] = psoDesc.rtvFormat4;
        }
    }

    desc.PrimitiveTopologyType = psoDesc.topology;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    std::string cacheFileName = std::to_string(key.hash) + ".pso";
    std::vector<uint8_t> cachedData = LoadCachedBlob(cacheFileName);
    if (!cachedData.empty()) {
        desc.CachedPSO.pCachedBlob = cachedData.data();
        desc.CachedPSO.CachedBlobSizeInBytes = cachedData.size();
    } else {
        desc.CachedPSO.pCachedBlob = nullptr;
        desc.CachedPSO.CachedBlobSizeInBytes = 0;
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));

    // キャッシュが古くて生成に失敗した場合は、キャッシュ無しで再試行
    bool usedFallback = false;
    if (FAILED(hr) && !cachedData.empty()) {
        desc.CachedPSO.pCachedBlob = nullptr;
        desc.CachedPSO.CachedBlobSizeInBytes = 0;
        hr = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
        usedFallback = true;
    }

    ASSERT_IF_FAILED(hr);
    if (FAILED(hr)) {
        return nullptr;
    }

    // 新規コンパイルした場合はキャッシュを保存
    if (cachedData.empty() || usedFallback) {
        SaveCachedBlob(cacheFileName, pso.Get());
    }

    {
        std::unique_lock<std::shared_mutex> writeLock(psoMutex_);
        if (auto cit = cache_.find(key); cit != cache_.end()) {
            return cit->second.Get(); // 別スレッドで先行して登録されていればそれを返す
        }
        cache_[key] = pso;
        cacheKeysByName_[name].push_back(key);
        return pso.Get();
    }
}

ID3D12PipelineState* PSOManager::GetCopyImage() {
    if (!copyImageShaders_.vsBlob || !copyImageShaders_.psBlob) {
        return nullptr;
    }

    // キャッシュキー
    constexpr uint64_t kCopyTag = 0x434F5059494D47ull;
    Key key{static_cast<uint64_t>(
        Hash("CopyImage", Irufemi::BlendMode::kBlendModeNone, DepthWrite::Off, CullMode::None) ^ kCopyTag)};

    {
        std::shared_lock<std::shared_mutex> readLock(psoMutex_);
        if (auto it = cache_.find(key); it != cache_.end()) {
            return it->second.Get();
        }
    }

    std::unique_lock<std::shared_mutex> writeLock(psoMutex_);
    if (auto it = cache_.find(key); it != cache_.end()) {
        return it->second.Get();
    }

    // 直接作成 (デバッグ・確実性の理由)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSig_.Get();
    desc.InputLayout = {nullptr, 0}; // 頂点バッファなし(SV_VertexID)
    desc.VS = {copyImageShaders_.vsBlob->GetBufferPointer(), copyImageShaders_.vsBlob->GetBufferSize()};
    desc.PS = {copyImageShaders_.psBlob->GetBufferPointer(), copyImageShaders_.psBlob->GetBufferSize()};
    desc.BlendState = MakeBlend(Irufemi::BlendMode::kBlendModeNone);
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.DepthStencilState = MakeDepth(DepthWrite::Off);
    desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = rtvFormat_;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    std::string cacheFileName = std::to_string(key.hash) + "_copy.pso";
    std::vector<uint8_t> cachedData = LoadCachedBlob(cacheFileName);
    if (!cachedData.empty()) {
        desc.CachedPSO.pCachedBlob = cachedData.data();
        desc.CachedPSO.CachedBlobSizeInBytes = cachedData.size();
    } else {
        desc.CachedPSO.pCachedBlob = nullptr;
        desc.CachedPSO.CachedBlobSizeInBytes = 0;
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));

    bool usedFallback = false;
    if (FAILED(hr) && !cachedData.empty()) {
        desc.CachedPSO.pCachedBlob = nullptr;
        desc.CachedPSO.CachedBlobSizeInBytes = 0;
        hr = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
        usedFallback = true;
    }

    IRUFEMI_ASSERT(SUCCEEDED(hr) && "Direct CreateGraphicsPipelineState failed for CopyImage");

    if (SUCCEEDED(hr)) {
        if (cachedData.empty() || usedFallback) {
            SaveCachedBlob(cacheFileName, pso.Get());
        }
        cache_[key] = pso;
        return pso.Get();
    }
    return nullptr;
}

void PSOManager::RegisterComputeShader(const std::string& name, const Microsoft::WRL::ComPtr<IDxcBlob>& csBlob,
                                       ID3D12RootSignature* computeRootSig) {
    if (!csBlob || !computeRootSig) {
        return;
    }
    std::unique_lock<std::shared_mutex> lock(psoMutex_);
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = computeRootSig;
    desc.CS = {csBlob->GetBufferPointer(), csBlob->GetBufferSize()};

    uint64_t hash = FNV1a(name.c_str(), name.length(), 0);
    std::string cacheFileName = std::to_string(hash) + "_cs.pso";
    std::vector<uint8_t> cachedData = LoadCachedBlob(cacheFileName);
    if (!cachedData.empty()) {
        desc.CachedPSO.pCachedBlob = cachedData.data();
        desc.CachedPSO.CachedBlobSizeInBytes = cachedData.size();
    } else {
        desc.CachedPSO.pCachedBlob = nullptr;
        desc.CachedPSO.CachedBlobSizeInBytes = 0;
    }

    ComPtr pso;
    HRESULT hr = device_->CreateComputePipelineState(&desc, IID_PPV_ARGS(pso.GetAddressOf()));

    bool usedFallback = false;
    if (FAILED(hr) && !cachedData.empty()) {
        desc.CachedPSO.pCachedBlob = nullptr;
        desc.CachedPSO.CachedBlobSizeInBytes = 0;
        hr = device_->CreateComputePipelineState(&desc, IID_PPV_ARGS(pso.GetAddressOf()));
        usedFallback = true;
    }

    ASSERT_IF_FAILED(hr);
    if (SUCCEEDED(hr)) {
        if (cachedData.empty() || usedFallback) {
            SaveCachedBlob(cacheFileName, pso.Get());
        }
        computeCache_[name] = pso;
    }
}

ID3D12PipelineState* PSOManager::GetComputePSO(const std::string& name) {
    {
        std::lock_guard<std::mutex> lock(manifestMutex_);
        recordedComputePSOs_.insert(name);
    }
    std::shared_lock<std::shared_mutex> lock(psoMutex_);
    auto it = computeCache_.find(name);
    return (it != computeCache_.end()) ? it->second.Get() : nullptr;
}

void PSOManager::ClearCache() {
    std::unique_lock<std::shared_mutex> lock(psoMutex_);
    // 古いPSOを直ちに破棄せず、退避用リストに移動して安全に寿命を延長する（Deferred Release）
    // これにより、ホットリロード直後に古いポインタが一時的に参照されても 0xC0000005 クラッシュを起こさない
    for (auto& [k, pso] : cache_) {
        if (pso) {
            retiredPSOs_.push_back(std::move(pso));
        }
    }
    for (auto& [k, pso] : computeCache_) {
        if (pso) {
            retiredPSOs_.push_back(std::move(pso));
        }
    }
    cache_.clear();
    cacheKeysByName_.clear();
    computeCache_.clear();

    // 退避リストが肥大化しないよう、前々回以前の古い退避分（上限256件）を適宜間引く
    if (retiredPSOs_.size() > 256) {
        retiredPSOs_.erase(retiredPSOs_.begin(), retiredPSOs_.begin() + 64);
    }

    // ホットリロード等で強制クリアされた場合、古いディスクキャッシュも破棄する
    std::filesystem::path cacheDir = kCacheDirectory;
    if (std::filesystem::exists(cacheDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(cacheDir)) {
            if (entry.path().extension() == ".pso") {
                std::error_code ec;
                std::filesystem::remove(entry.path(), ec);
            }
        }
    }
}

void PSOManager::SaveManifest(const std::string& manifestPath) const {
    std::string path = manifestPath.empty() ? (kCacheDirectory + "pso_manifest.json") : manifestPath;

    std::vector<GraphicsPSORecord> graphicsList;
    std::vector<std::string> computeList;
    {
        std::lock_guard<std::mutex> lock(manifestMutex_);
        graphicsList.assign(recordedGraphicsPSOs_.begin(), recordedGraphicsPSOs_.end());
        computeList.assign(recordedComputePSOs_.begin(), recordedComputePSOs_.end());
    }

    if (graphicsList.empty() && computeList.empty()) {
        return;
    }

    nlohmann::json j;
    j["version"] = 1;
    nlohmann::json gArray = nlohmann::json::array();
    for (const auto& item : graphicsList) {
        nlohmann::json obj;
        obj["name"] = item.name;
        obj["blend"] = static_cast<int>(item.blend);
        obj["depth"] = static_cast<int>(item.depth);
        obj["cull"] = static_cast<int>(item.cull);
        gArray.push_back(obj);
    }
    j["graphics"] = gArray;
    j["compute"] = computeList;

    std::error_code ec;
    std::filesystem::create_directories(kCacheDirectory, ec);

    std::ofstream file(path);
    if (file.is_open()) {
        file << j.dump(4);
        Log::OutPutLog(std::cout, std::format("[PSOManager] Saved PSO manifest (Graphics: {}, Compute: {}) to {}\n",
                                              graphicsList.size(), computeList.size(), path));
    }
}

bool PSOManager::LoadAndPreWarmManifest(ThreadPool* threadPool, const std::string& manifestPath) {
    std::string path;
    if (!manifestPath.empty()) {
        path = manifestPath;
    } else {
        // ローカルキャッシュ → 配布同梱アセットの順にフォールバック探索
        const std::vector<std::string> searchPaths = {
            kCacheDirectory + "pso_manifest.json", "resources/cache/pso_manifest.json", "resources/pso_manifest.json"};

        for (const auto& sp : searchPaths) {
            if (std::filesystem::exists(sp)) {
                path = sp;
                break;
            }
        }
    }

    if (path.empty() || !std::filesystem::exists(path)) {
        return false;
    }

    nlohmann::json j;
    if (!Irufemi::JsonUtility::LoadFromFile(path, j)) {
        return false;
    }

    Log::OutPutLog(std::cout, "[PSOManager] Loading and pre-warming PSO manifest from: " + path + "\n");

    std::shared_ptr<TaskGroup> taskGroup;
    if (threadPool) {
        taskGroup = std::make_shared<TaskGroup>();
    }

    size_t graphicsCount = 0;
    if (j.contains("graphics") && j["graphics"].is_array()) {
        for (const auto& item : j["graphics"]) {
            std::string name = item.value("name", "");
            if (name.empty()) {
                continue;
            }
            auto blend = static_cast<Irufemi::BlendMode>(item.value("blend", 0));
            auto depth = static_cast<DepthWrite>(item.value("depth", 0));
            auto cull = static_cast<CullMode>(item.value("cull", 0));

            if (threadPool && taskGroup) {
                threadPool->Enqueue(taskGroup,
                                    [this, name, blend, depth, cull]() { GetPSO(name, blend, depth, cull); });
            } else {
                GetPSO(name, blend, depth, cull);
            }
            ++graphicsCount;
        }
    }

    size_t computeCount = 0;
    if (j.contains("compute") && j["compute"].is_array()) {
        for (const auto& item : j["compute"]) {
            std::string name = item.get<std::string>();
            if (name.empty()) {
                continue;
            }
            if (threadPool && taskGroup) {
                threadPool->Enqueue(taskGroup, [this, name]() { GetComputePSO(name); });
            } else {
                GetComputePSO(name);
            }
            ++computeCount;
        }
    }

    if (taskGroup) {
        taskGroup->Wait();
    }

    Log::OutPutLog(std::cout,
                   std::format("[PSOManager] Pre-warmed {} Graphics PSOs and {} Compute PSOs from manifest: {}\n",
                               graphicsCount, computeCount, path));
    return (graphicsCount > 0 || computeCount > 0);
}

void PSOManager::PreWarmCommonPSOs(ThreadPool* threadPool) {
    // 1. データ駆動: マニフェスト（pso_manifest.json）から全PSOを一括プリウォーム
    if (LoadAndPreWarmManifest(threadPool)) {
        GetCopyImage();
        return;
    }

    Log::OutPutLog(std::cout, "[PSOManager] Warning: No PSO manifest found to pre-warm.\n");
    GetCopyImage();
}

// Multiply : out = src * dst
// Screen : out = src * (1 - dst) + dst * 1
D3D12_BLEND_DESC PSOManager::MakeBlend(Irufemi::BlendMode m) {
    D3D12_BLEND_DESC d{};
    for (int i = 0; i < 8; ++i) {
        d.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // すべての色要素を書き込む
    }
    auto& rt = d.RenderTarget[0];

    switch (m) {
    case Irufemi::BlendMode::kBlendModeNone:
        // BlendEnable = FALSE(ブレンドなし)
        break;

    case Irufemi::BlendMode::kBlendModeNormal: // Normal
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE; // αの設定
        rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        break;

    case Irufemi::BlendMode::kBlendModeAdd: // Add
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D12_BLEND_ONE;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_ONE;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        break;

    case Irufemi::BlendMode::kBlendModeSubtract: // Subtract(RGBは REV_SUBTRACT)
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_ONE; // RGB: 1 - 1 の係数で REV_SUBTRACT
        rt.DestBlend = D3D12_BLEND_ONE;
        rt.BlendOp = D3D12_BLEND_OP_REV_SUBTRACT; // dst - src
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_ONE;
        rt.BlendOpAlpha = D3D12_BLEND_OP_REV_SUBTRACT;
        break;

    case Irufemi::BlendMode::kBlendModeMultiply: // Multiply(src * dst)
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_DEST_COLOR;
        rt.DestBlend = D3D12_BLEND_ZERO;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_ZERO;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        break;

    case Irufemi::BlendMode::kBlendModeScreen: // Screen(src*(1-dst)+dst)
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
        rt.DestBlend = D3D12_BLEND_ONE;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_ONE;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        break;
    case Irufemi::BlendMode::kBlendModePremultiplied: // Premultiplied Alpha (src*1 + dst*(1-srcA))
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_ONE;
        rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        break;

    default:
        break;
    }
    return d;
}

D3D12_DEPTH_STENCIL_DESC PSOManager::MakeDepth(DepthWrite w) {
    D3D12_DEPTH_STENCIL_DESC d{};
    if (w == DepthWrite::Off) {
        d.DepthEnable = FALSE;
        d.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        d.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    } else {
        d.DepthEnable = TRUE;
        d.DepthWriteMask = (w == DepthWrite::Enable) ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        d.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }
    return d;
}

uint64_t PSOManager::Hash(const std::string& name, Irufemi::BlendMode b, DepthWrite d, CullMode c) {
    uint64_t h = 0;
    h = FNV1a(name.c_str(), name.length(), h);
    h = FNV1a(&b, sizeof(b), h);
    h = FNV1a(&d, sizeof(d), h);
    h = FNV1a(&c, sizeof(c), h);
    return h;
}

std::vector<uint8_t> PSOManager::LoadCachedBlob(const std::string& cacheFileName) const {
    std::string path = kCacheDirectory + cacheFileName;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }

    std::streampos pos = file.tellg();
    if (pos <= 0) {
        return {};
    }

    size_t size = static_cast<size_t>(pos);
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return buffer;
    }
    return {};
}

void PSOManager::SaveCachedBlob(const std::string& cacheFileName, ID3D12PipelineState* pso) const {
    if (!pso) {
        return;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    HRESULT hr = pso->GetCachedBlob(&blob);
    if (FAILED(hr) || !blob) {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            Log::OutPutLog(std::cout,
                           "[PSOManager] Note: GPU driver does not support ID3D12PipelineState::GetCachedBlob (HR: " +
                               std::to_string(hr) + "). Manifest-based auto-prewarming is active.\n");
        }
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(kCacheDirectory, ec);

    std::string path = kCacheDirectory + cacheFileName;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(blob->GetBufferPointer()), blob->GetBufferSize());
    }
}

void PSOManager::ClearCacheByName(const std::string& name) {
    auto it = cacheKeysByName_.find(name);
    if (it != cacheKeysByName_.end()) {
        for (const auto& key : it->second) {
            auto cit = cache_.find(key);
            if (cit != cache_.end()) {
                if (cit->second) {
                    retiredPSOs_.push_back(std::move(cit->second));
                }
                cache_.erase(cit);
            }
        }
        it->second.clear(); // ベクターをクリア
    }
}
