#include "Core/Utility/ErrorUtility.h"
#include "Core/Utility/Log.h"
#include <iostream>
#include "Renderer/DrawManager.h"
using namespace RenderPackets;

#include <Windows.h>
#include <cassert>

#include <dxgidebug.h>
#include "Core/Profiler/GpuProfiler.h"
#include "Renderer/Object/2D/Sprite/Sprite.h"
#include "Renderer/Object/3D/StaticModelObject/StaticModelObject.h"
#include "Renderer/Object/Batch/ModelBatch.h"
#include "Renderer/Object/Batch/PrimitiveBatch.h"
#include "Renderer/Object/Batch/DebugPrimitiveRenderer.h"

#include "Renderer/Object/Line/LineClass.h"
#include "Renderer/Object/Skybox/Skybox.h"
#include "Renderer/System/Core/Object3DResource.h"
#include "Renderer/System/Core/Object2DResource.h"

#include "Renderer/System/Core/LineResource.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "Renderer/Pipeline/RenderGraph/ComputePass.h"
#include "Renderer/Pipeline/RenderGraph/ShadowPass.h"
#include "Renderer/Pipeline/RenderGraph/MainOpaquePass.h"
#include "Renderer/Pipeline/RenderGraph/MainTransparentPass.h"
#include "Renderer/Pipeline/RenderGraph/UIPass.h"
#include "Renderer/Pipeline/RenderGraph/PostUIPass.h"
#include "Renderer/Pipeline/RenderGraph/TopMostPass.h"
#include "Renderer/Pipeline/RenderGraph/PostProcessPass.h"
#include "Renderer/Pipeline/RenderGraph/SelectionOutlinePass.h"
#include "Resource/Model/ModelManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"
#include "Renderer/Data/CameraForGPU.h"
#include "Renderer/Data/DirectionalLight.h"
#include "Renderer/Data/PointLight.h"
#include "Renderer/Data/SpotLight.h"
#include "Renderer/Data/AreaLight.h"
#include "Core/Math/Math.h"
#include "Resource/Model/Data/SkinCluster.h"
#include "RHI/DirectX12/ShadowMap.h"
#include "Resource/Texture/TextureManager.h"

namespace {
// cpp内限定のヌルCBV
Microsoft::WRL::ComPtr<ID3D12Resource> gNullPointLight;
Microsoft::WRL::ComPtr<ID3D12Resource> gNullSpotLight;
D3D12_GPU_VIRTUAL_ADDRESS gNullPointLightVA = 0;
D3D12_GPU_VIRTUAL_ADDRESS gNullSpotLightVA = 0;

void EnsureNullPointLight(DirectXCommon* dx) {
    if (gNullPointLight) {
        return;
    }
    gNullPointLight = dx->CreateBufferResource(sizeof(PointLight));
    PointLight* p = nullptr;
    gNullPointLight->Map(0, nullptr, reinterpret_cast<void**>(&p));
    p->color = {0, 0, 0, 0};
    p->position = {0, 0, 0};
    p->intensity = 0.0f;
    gNullPointLightVA = gNullPointLight->GetGPUVirtualAddress();
}

void EnsureNullSpotLight(DirectXCommon* dx) {
    if (gNullSpotLight) {
        return;
    }
    gNullSpotLight = dx->CreateBufferResource(sizeof(SpotLight));
    SpotLight* s = nullptr;
    gNullSpotLight->Map(0, nullptr, reinterpret_cast<void**>(&s));
    s->color = {0, 0, 0, 0};
    s->position = {0, 0, 0};
    s->intensity = 0.0f;
    s->direction = {0, -1, 0};
    s->distance = 0.0f;
    s->decay = 1.0f;
    s->cosAngle = 1.0f;
    gNullSpotLightVA = gNullSpotLight->GetGPUVirtualAddress();
}
} // namespace

DrawManager::DrawManager() {
    isAlive_ = std::make_shared<std::atomic<bool>>(true);
}

DrawManager::~DrawManager() {
    if (isAlive_) {
        isAlive_->store(false);
    }
    std::lock_guard<std::mutex> lock(registryMutex_);
    registeredQueues_.clear();
}

DrawManager::LocalRenderQueues& DrawManager::GetLocalQueues() {
    struct ThreadLocalWrapper {
        LocalRenderQueues queues;
        DrawManager* registeredManager = nullptr;
        std::weak_ptr<std::atomic<bool>> aliveToken;

        ~ThreadLocalWrapper() {
            if (auto token = aliveToken.lock()) {
                if (token->load() && registeredManager) {
                    registeredManager->UnregisterThreadQueues(&queues);
                }
            }
        }
    };

    thread_local ThreadLocalWrapper wrapper;
    if (wrapper.registeredManager != this) {
        if (auto token = wrapper.aliveToken.lock()) {
            if (token->load() && wrapper.registeredManager) {
                wrapper.registeredManager->UnregisterThreadQueues(&wrapper.queues);
            }
        }
        RegisterThreadQueues(&wrapper.queues);
        wrapper.registeredManager = this;
        wrapper.aliveToken = isAlive_;
    }
    return wrapper.queues;
}

void DrawManager::RegisterThreadQueues(LocalRenderQueues* queues) {
    if (!queues) {
        return;
    }
    std::lock_guard<std::mutex> lock(registryMutex_);
    if (std::find(registeredQueues_.begin(), registeredQueues_.end(), queues) == registeredQueues_.end()) {
        registeredQueues_.push_back(queues);
    }
}

void DrawManager::UnregisterThreadQueues(LocalRenderQueues* queues) {
    if (!queues) {
        return;
    }
    std::lock_guard<std::mutex> lock(registryMutex_);
    auto it = std::find(registeredQueues_.begin(), registeredQueues_.end(), queues);
    if (it != registeredQueues_.end()) {
        registeredQueues_.erase(it);
    }
}

void DrawManager::MergeThreadLocalQueues() {
    std::lock_guard<std::mutex> lock(registryMutex_);
    for (auto* lq : registeredQueues_) {
        if (!lq) {
            continue;
        }

        auto mergeVec = [](auto& dst, auto& src) {
            if (!src.empty()) {
                dst.insert(dst.end(), std::make_move_iterator(src.begin()), std::make_move_iterator(src.end()));
                src.clear();
            }
        };

        mergeVec(standard3DQueue_, lq->standard3DQueue);
        mergeVec(transparent3DQueue_, lq->transparent3DQueue);
        mergeVec(ui3DQueue_, lq->ui3DQueue);
        mergeVec(selectionMaskQueue_, lq->selectionMaskQueue);
        mergeVec(selectionMaskQueue2D_, lq->selectionMaskQueue2D);
        mergeVec(spriteQueue_, lq->spriteQueue);
        mergeVec(spriteBatchQueue_, lq->spriteBatchQueue);
        mergeVec(lineQueue_, lq->lineQueue);
        mergeVec(gpuParticleQueue_, lq->gpuParticleQueue);
        mergeVec(voxelParticleQueue_, lq->voxelParticleQueue);
        mergeVec(skyboxQueue_, lq->skyboxQueue);
        mergeVec(skydomeQueue_, lq->skydomeQueue);
        mergeVec(primitiveBatchQueue_, lq->primitiveBatchQueue);
        mergeVec(primitive2DBatchQueue_, lq->primitive2DBatchQueue);
        mergeVec(modelBatchQueue_, lq->modelBatchQueue);
        mergeVec(debugPrimitiveQueue_, lq->debugPrimitiveQueue);
        mergeVec(postRenderQueue_, lq->postRenderQueue);
        mergeVec(topMostSpriteQueue_, lq->topMostSpriteQueue);
        mergeVec(topMostSpriteBatchQueue_, lq->topMostSpriteBatchQueue);
        mergeVec(textQueue_, lq->textQueue);
        mergeVec(topMostTextQueue_, lq->topMostTextQueue);
    }
}

void DrawManager::SubmitPostRender(std::function<void()> drawFunc) {
    if (drawFunc) {
        GetLocalQueues().postRenderQueue.push_back(std::move(drawFunc));
    }
}

void DrawManager::Initialize(IrufemiEngine* engine, DirectXCommon* dx) {
    engine_ = engine;
    dxCommon_ = dx;
    commandList_ = dx->GetCommandList();

    // 定数バッファのサイズ (256バイトアラインメント)
    const size_t perFrameSize = (sizeof(PerFrameData) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) &
                                ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
    const size_t lightCommonSize = (sizeof(LightCommonData) + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) &
                                   ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
    const uint32_t kMaxLights = 1024;

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        auto& fr = frameResources_[i];

        // フレーム定数バッファ (PerFrameData + LightCommonData)
        fr.frameResource = dxCommon_->CreateBufferResource(perFrameSize + lightCommonSize);
        uint8_t* mapped = nullptr;
        fr.frameResource->Map(0, nullptr, reinterpret_cast<void**>(&mapped));

        fr.perFrameData = reinterpret_cast<PerFrameData*>(mapped);
        fr.lightCommonData = reinterpret_cast<LightCommonData*>(mapped + perFrameSize);

        fr.frameData.camera = fr.frameResource->GetGPUVirtualAddress();
        fr.frameData.lightCommon = fr.frameData.camera + perFrameSize;

        // StructuredBuffer の初期化 (ひとまず1024個分を確保)
        fr.pointLightResource = dxCommon_->CreateBufferResource(sizeof(PointLight) * kMaxLights);
        fr.spotLightResource = dxCommon_->CreateBufferResource(sizeof(SpotLight) * kMaxLights);
        fr.areaLightResource = dxCommon_->CreateBufferResource(sizeof(AreaLight) * kMaxLights);

        // ライト SRV デスクリプタの一括確保 (Point, Spot, Area 用に 3 つ)
        auto pool = dxCommon_->GetSrvPool();
        fr.lightSrvBaseIndex = pool->Allocate(3);
        fr.lightSrvHandle = pool->GetGPUHandle(fr.lightSrvBaseIndex);

        // StructuredBuffer SRV の作成
        pool->CreateSRVForStructuredBuffer(fr.lightSrvBaseIndex + 0, fr.pointLightResource.Get(), kMaxLights,
                                           sizeof(PointLight));
        pool->CreateSRVForStructuredBuffer(fr.lightSrvBaseIndex + 1, fr.spotLightResource.Get(), kMaxLights,
                                           sizeof(SpotLight));
        pool->CreateSRVForStructuredBuffer(fr.lightSrvBaseIndex + 2, fr.areaLightResource.Get(), kMaxLights,
                                           sizeof(AreaLight));
    }

    // レンダーグラフの構築
    renderGraph_ = std::make_unique<RenderGraph>();
    renderGraph_->InitializeTransientResourceManager(dxCommon_);

    renderGraph_->AddPass(std::make_unique<ComputePass>());
    renderGraph_->AddPass(std::make_unique<ShadowPass>());
    renderGraph_->AddPass(std::make_unique<MainOpaquePass>());
    renderGraph_->AddPass(std::make_unique<MainTransparentPass>());
    renderGraph_->AddPass(std::make_unique<PostProcessPass>());
    renderGraph_->AddPass(std::make_unique<UIPass>());
    renderGraph_->AddPass(std::make_unique<PostUIPass>());
    renderGraph_->AddPass(std::make_unique<TopMostPass>());
    renderGraph_->AddPass(std::make_unique<SelectionOutlinePass>());

    // シャドウマップの初期化 (2048x2048) - 全フレーム分
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        shadowMaps_[i] = std::make_unique<ShadowMap>();
        shadowMaps_[i]->Initialize(dxCommon_, 2048, 2048);

        // RenderGraph にリソースの初期ステートを登録
        renderGraph_->SetInitialResourceState(shadowMaps_[i]->GetResource(),
                                              D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    // Command Signature for GPU Culling (ExecuteIndirect)
    D3D12_INDIRECT_ARGUMENT_DESC argumentDescs[1] = {};
    argumentDescs[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

    D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
    commandSignatureDesc.pArgumentDescs = argumentDescs;
    commandSignatureDesc.NumArgumentDescs = 1;
    commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS); // 20 bytes

    HRESULT hr = dxCommon_->GetDevice()->CreateCommandSignature(&commandSignatureDesc, nullptr,
                                                                IID_PPV_ARGS(&commandSignature_));
    ASSERT_IF_FAILED(hr);
}

void DrawManager::ExecuteComputePasses() {
    for (auto* task : computeTasks_) {
        task->DispatchCompute();
    }

    // GPU Culling の実行
    bool dispatchedGPUCulling = false;
    for (const auto& packet : modelBatchQueue_) {
        if (packet.useGPUCulling) {
            DispatchGPUCulling(packet);
            dispatchedGPUCulling = true;
        }
    }

    // パイプラインのボトルネック解消のため、各モデルごとではなく
    // 全てのコンピュートタスクのディスパッチ完了後に一括してグローバルUAVバリアを発行する
    if (!computeTasks_.empty() || dispatchedGPUCulling) {
        ExecuteUAVBarrier(nullptr);
    }

    computeTasks_.clear();
}

void DrawManager::Finalize() {
    // グローバル無名名前空間のNullリソースを明示的解放（DirectXデバイス破棄後のLIVE_OBJECT警告防止）
    gNullPointLight.Reset();
    gNullSpotLight.Reset();
    gNullPointLightVA = 0;
    gNullSpotLightVA = 0;

    auto* srvPool = dxCommon_->GetSrvPool();
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        auto& fr = frameResources_[i];
        if (fr.frameResource && fr.perFrameData) {
            fr.frameResource->Unmap(0, nullptr);
            fr.frameResource.Reset();
        }
        fr.pointLightResource.Reset();
        fr.spotLightResource.Reset();
        fr.areaLightResource.Reset();

        // SRVの解放
        if (srvPool && fr.lightSrvBaseIndex != 0xFFFFFFFFu) {
            for (uint32_t j = 0; j < 3; ++j) {
                srvPool->FreeAfterFence(fr.lightSrvBaseIndex + j, dxCommon_->GetCurrentFrameFenceValue());
            }
            fr.lightSrvBaseIndex = 0xFFFFFFFFu;
        }
    }

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        shadowMaps_[i].reset();
    }

    shadowMaps_[0].reset();
    shadowMaps_[1].reset();
}

void DrawManager::OnResize(int32_t width, int32_t height) {
    if (renderGraph_) {
        renderGraph_->OnResize();

        // 永続リソースであるシャドウマップのステートも再登録する
        for (int i = 0; i < kMaxFramesInFlight; ++i) {
            if (shadowMaps_[i]) {
                // フレーム完了時点ではSRV状態になっているため、その状態を登録
                renderGraph_->SetInitialResourceState(shadowMaps_[i]->GetResource(),
                                                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
        }
    }
}

void DrawManager::SetInitialResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) {
    if (renderGraph_) {
        renderGraph_->SetInitialResourceState(resource, state);
    }
}

void DrawManager::SetFinalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) {
    if (renderGraph_) {
        renderGraph_->SetFinalResourceState(resource, state);
    }
}

void DrawManager::BindPSO(ID3D12PipelineState* pso) {
    if (pso) {
        commandList_->SetPipelineState(pso);
    }
}

void DrawManager::PreDraw(std::array<float, 4> clearColor, float clearDepth, uint8_t clearStencil) {

    // 1. GPU同期 (これから使うスロットが前回の使用（通し番号）を終えるまで待つ)
    ID3D12Fence* fence = dxCommon_->GetFence();
    uint64_t waitValue = dxCommon_->GetFenceValue(); // このスロットが最後に使われた時の通し番号
    if (fence->GetCompletedValue() < waitValue) {
        fence->SetEventOnCompletion(waitValue, dxCommon_->GetFenceEvent());
        WaitForSingleObject(dxCommon_->GetFenceEvent(), INFINITE);
    }

    // 2. コマンドリストとアロケータのリセット (現在のフレーム用)
    ID3D12CommandAllocator* allocator = dxCommon_->GetCommandAllocator();
    HRESULT hr = allocator->Reset();
    ASSERT_IF_FAILED(hr);
    hr = commandList_->Reset(allocator, nullptr);
    ASSERT_IF_FAILED(hr);

    // フレーム開始時に、ポーズ中でSetFrameDataが呼ばれなくてもバッファが常に同期待ちにならないようキャッシュを現在のバッファへコピーする
    SyncCachedFrameData();

    // GPU計測開始
    if (auto* profiler = dxCommon_->GetGpuProfiler()) {
        profiler->StartFrame(commandList_);
    }

    // バックバッファとRTV/DSVの取得 (これはスワップチェーン依存なのでそのままでよい)

    // バックバッファとRTV/DSVの取得
    const UINT backIdx = dxCommon_->GetSwapChain()->GetCurrentBackBufferIndex();
    ID3D12Resource* backBuffer = dxCommon_->GetSwapChainResources(backIdx);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetRtvHandles(backIdx);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);

    /*完璧な画面クリアを目指して*/

    /// TransitionBarrierを張るコード

    // TransitionBarrierの設定（Present -> RenderTarget）
    DirectXUtils::TransitionBarrier(commandList_, backBuffer, D3D12_RESOURCE_STATE_PRESENT,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);

    // レンダーターゲット追跡の更新 (バックバッファ)
    currentRenderTexture_ = nullptr;

    /*画面の色を変えよう*/

    /// コマンドを積み込んで確定させる

    // 描画先のRTVを設定する
    commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
    // 指定した色で画面全体をクリアする
    commandList_->ClearRenderTargetView(rtvHandle, clearColor.data(), 0, nullptr);

    /*前後関係を正しくしよう*/

    // 指定した深度で画面全体をクリアする
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, clearDepth, clearStencil, 0, nullptr);

    // フレーム共通のビューポート/シザーを一度だけ設定（GameResolution基準）
    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(engine_->GetGameResolutionWidth());
    viewport.Height = static_cast<float>(engine_->GetGameResolutionHeight());
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissor{};
    scissor.left = 0;
    scissor.right = engine_->GetGameResolutionWidth();
    scissor.top = 0;
    scissor.bottom = engine_->GetGameResolutionHeight();

    commandList_->RSSetViewports(1, &viewport);
    commandList_->RSSetScissorRects(1, &scissor);

    // フレームで利用するSRVヒープを設定(全描画共通)
    ID3D12DescriptorHeap* descriptorHeaps[] = {dxCommon_->GetSrvDescriptorHeap()};
    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // --- フレーム共通CBV/SRVをここで一度だけバインド ---
    BindCommonParameters();

    // 環境マップをバインド
    if (environmentMapHandle_.ptr != 0) {
    } else if (textureManager_) {
    }
}

void DrawManager::PostDraw() {

    const UINT backIdx = dxCommon_->GetSwapChain()->GetCurrentBackBufferIndex();
    ID3D12Resource* backBuffer = dxCommon_->GetSwapChainResources(backIdx);

    /*完璧な画面クリアを目指して*/

    if (auto scm = dxCommon_->GetEngine()->GetScreenCaptureManager()) {
        scm->OnPostUIDraw(commandList_, backBuffer);
    }

    // 画面に描く処理はすべて終わり、画面に映すので、状態を遷移（RenderTarget -> Present）
    DirectXUtils::TransitionBarrier(commandList_, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PRESENT);

    /*画面の色を変えよう*/

    /// コマンドを積み込んで確定させる

    // GPU計測終了
    if (auto* profiler = dxCommon_->GetGpuProfiler()) {
        profiler->EndFrame(commandList_);
    }

    // コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
    HRESULT hr = commandList_->Close();
    ASSERT_IF_FAILED(hr);

    /// コマンドをキックする

    // GPUにコマンドリストの実行を行わせる
    ID3D12CommandList* commandLists[] = {commandList_};
    dxCommon_->GetCommandQueue()->ExecuteCommandLists(_countof(commandLists), commandLists);
    // GPUとOSに画面の交換を行うよう通知する
    UINT syncInterval = vSyncEnabled_ ? 1 : 0;
    UINT presentFlags = (dxCommon_->IsTearingSupported() && !vSyncEnabled_) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    hr = dxCommon_->GetSwapChain()->Present(syncInterval, presentFlags);
    // デバイスが削除されたかどうかのチェック
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            HRESULT removedReason = dxCommon_->GetDevice()->GetDeviceRemovedReason();
            char str[256];
            sprintf_s(str, "Device Removed or Reset, reason code: 0x%08X", removedReason);
            /**
             * @brief エディタのコンソールパネルにも出力するため、Log::OutPutLog を使用
             */
            Log::OutPutLog(std::cerr, std::string(str));
            throw std::runtime_error(str);
        } else {
            throw std::runtime_error("Present failed with an unknown error.");
        }
    }

    // 1. フェンスをシグナル (通し番号をインクリメントして記録)
    uint64_t nextValue = dxCommon_->IncrementGlobalFence();
    dxCommon_->GetFenceValue() = nextValue; // このスロットの完了番号として保存
    dxCommon_->GetCommandQueue()->Signal(dxCommon_->GetFence(), nextValue);

    // 2. 次のフレームへインデックスを進める
    dxCommon_->AdvanceFrameIndex();

    dxCommon_->UpdateFixFPS();
}

void DrawManager::SetFrameData(const CameraForGPU& camera, float time, float deltaTime, const DirectionalLight& light,
                               const std::vector<PointLight*>& pointLights, const std::vector<SpotLight*>& spotLights,
                               const std::vector<AreaLight*>& areaLights, const Irufemi::Vector2& resolution) {
    cachedPerFrame_.camera = camera;
    cachedPerFrame_.time = time;
    cachedPerFrame_.deltaTime = deltaTime;
    cachedPerFrame_.resolution = resolution;
    cachedDirectionalLight_ = light;

    cachedPointLights_.clear();
    for (auto* pl : pointLights) {
        cachedPointLights_.push_back(*pl);
    }

    cachedSpotLights_.clear();
    for (auto* sl : spotLights) {
        cachedSpotLights_.push_back(*sl);
    }

    cachedAreaLights_.clear();
    for (auto* al : areaLights) {
        cachedAreaLights_.push_back(*al);
    }

    SyncCachedFrameData();
}

void DrawManager::SyncCachedFrameData() {
    auto& fr = frameResources_[dxCommon_->GetFrameIndex()];

    if (fr.perFrameData) {
        *fr.perFrameData = cachedPerFrame_;
        // [Bindless] 環境マップとシャドウマップのインデックスを設定
        fr.perFrameData->envMapIndex = dxCommon_->GetSrvPool()->GetIndexFromGPUHandle(environmentMapHandle_);
        ShadowMap* shadowMap = shadowMaps_[dxCommon_->GetFrameIndex()].get();
        fr.perFrameData->shadowMapIndex = shadowMap ? shadowMap->GetSrvIndex() : 0xFFFFFFFFu;
        fr.perFrameData->depthMapIndex = 0xFFFFFFFFu; // 未実装
    }
    if (fr.lightCommonData) {
        // ライト共通データの更新（b1）
        fr.lightCommonData->directionalLight = cachedDirectionalLight_;
        fr.lightCommonData->pointLightCount = static_cast<int32_t>(cachedPointLights_.size());
        fr.lightCommonData->spotLightCount = static_cast<int32_t>(cachedSpotLights_.size());
        fr.lightCommonData->areaLightCount = static_cast<int32_t>(cachedAreaLights_.size());

        // シャドウマップの行列更新
        ShadowMap* shadowMap = shadowMaps_[dxCommon_->GetFrameIndex()].get();
        if (shadowMap) {
            Irufemi::Vector3 targetPos =
                useCustomShadowParams_ ? shadowTargetPos_ : cachedPerFrame_.camera.worldPosition;
            float orthoSize = useCustomShadowParams_ ? shadowOrthoSize_ : 128.0f;
            shadowMap->UpdateMatrix(cachedDirectionalLight_.direction, targetPos, orthoSize);
            fr.lightCommonData->viewProjection = shadowMap->GetViewProjection();
        }

        // [Bindless] ライト用バッファのインデックスを設定
        if (fr.lightSrvBaseIndex != 0xFFFFFFFFu) {
            fr.lightCommonData->pointLightBufferIndex = fr.lightSrvBaseIndex + 0;
            fr.lightCommonData->spotLightBufferIndex = fr.lightSrvBaseIndex + 1;
            fr.lightCommonData->areaLightBufferIndex = fr.lightSrvBaseIndex + 2;
        } else {
            fr.lightCommonData->pointLightBufferIndex = 0xFFFFFFFFu;
            fr.lightCommonData->spotLightBufferIndex = 0xFFFFFFFFu;
            fr.lightCommonData->areaLightBufferIndex = 0xFFFFFFFFu;
        }
    }

    // 各 StructuredBuffer へ書き込み
    auto copyLights = [](ID3D12Resource* res, const auto& lightVec) {
        if (!res || lightVec.empty()) {
            return;
        }
        using LightType = std::remove_pointer_t<typename std::decay_t<decltype(lightVec)>::value_type>;
        LightType* mapped = nullptr;
        if (SUCCEEDED(res->Map(0, nullptr, reinterpret_cast<void**>(&mapped))) && mapped) {
            for (size_t i = 0; i < lightVec.size(); ++i) {
                mapped[i] = lightVec[i];
            }
            res->Unmap(0, nullptr);
        }
    };

    copyLights(fr.pointLightResource.Get(), cachedPointLights_);
    copyLights(fr.spotLightResource.Get(), cachedSpotLights_);
    copyLights(fr.areaLightResource.Get(), cachedAreaLights_);
}

void DrawManager::SetEnvironmentMap(D3D12_GPU_DESCRIPTOR_HANDLE envMapHandle) {
    environmentMapHandle_ = envMapHandle;
}

void DrawManager::SubmitSprite(const Object2DResource* resource) {
    if (!resource) {
        return;
    }
    SpritePacket p{};
    p.resource = resource;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    GetLocalQueues().spriteQueue.push_back(p);
}

void DrawManager::SubmitTopMostSprite(const Object2DResource* resource) {
    if (!resource) {
        return;
    }
    SpritePacket p{};
    p.resource = resource;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    GetLocalQueues().topMostSpriteQueue.push_back(p);
}

void DrawManager::DrawSprite(const RenderPackets::SpritePacket& packet) {
    const Object2DResource* resource = packet.resource;
    if (!resource || !commandList_) {
        return;
    }

    // トポロジ設定
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 頂点バッファとインデックスバッファの設定
    commandList_->IASetVertexBuffers(0, 1, &resource->GetVertexBufferView());
    commandList_->IASetIndexBuffer(&resource->GetIndexBufferView());

    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, resource->GetMaterialVAddress());
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Transform, resource->GetTransformVAddress());

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = textureManager_->Resolve(resource->GetTextureHandle());
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::LegacyPSTexture, gpuHandle);

    commandList_->DrawIndexedInstanced(resource->GetIndexCount(), 1, 0, 0, 0);
}

void DrawManager::SubmitSpriteBatch(const RenderPackets::SpriteBatchPacket& packet) {
    GetLocalQueues().spriteBatchQueue.push_back(packet);
}

void DrawManager::DrawSpriteBatch(const RenderPackets::SpriteBatchPacket& packet) {
    if (packet.instanceCount == 0 || !packet.resource) {
        return;
    }

    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &packet.resource->GetVertexBufferView());
    commandList_->IASetIndexBuffer(&packet.resource->GetIndexBufferView());

    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, packet.resource->GetMaterialVAddress());
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Instancing, packet.instancingSrvHandleGPU); // VS t0

    commandList_->DrawIndexedInstanced(packet.resource->GetIndexCount(), packet.instanceCount, 0, 0, 0);
}

void DrawManager::SubmitTopMostSpriteBatch(const RenderPackets::SpriteBatchPacket& packet) {
    GetLocalQueues().topMostSpriteBatchQueue.push_back(packet);
}

void DrawManager::DrawTopMostSpriteBatch(const RenderPackets::SpriteBatchPacket& packet) {
    DrawSpriteBatch(packet);
}

void DrawManager::SubmitText(const Object2DResource* resource) {
    if (!resource) {
        return;
    }
    SpritePacket p{};
    p.resource = resource;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    GetLocalQueues().textQueue.push_back(p);
}

void DrawManager::SubmitTopMostText(const Object2DResource* resource) {
    if (!resource) {
        return;
    }
    SpritePacket p{};
    p.resource = resource;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    GetLocalQueues().topMostTextQueue.push_back(p);
}

void DrawManager::DrawText(const RenderPackets::SpritePacket& packet) {
    const Object2DResource* resource = packet.resource;
    if (!resource || !commandList_) {
        return;
    }

    if (packet.customPSO) {
        commandList_->SetPipelineState(packet.customPSO);
    }

    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &resource->GetVertexBufferView());
    commandList_->IASetIndexBuffer(&resource->GetIndexBufferView());
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, resource->GetMaterialVAddress());
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Transform, resource->GetTransformVAddress());

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = textureManager_->Resolve(resource->GetTextureHandle());
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::LegacyPSTexture, gpuHandle);

    commandList_->DrawIndexedInstanced(resource->GetIndexCount(), 1, 0, 0, 0);
}

void DrawManager::SubmitModelBatch(const ModelBatchPacket& packet) {
    GetLocalQueues().modelBatchQueue.push_back(packet);
}

void DrawManager::DrawModelBatch(const RenderPackets::ModelBatchPacket& packet) {
    const GpuMesh* gpuMesh = packet.gpuMesh;
    if (!gpuMesh || gpuMesh->vertexCount == 0 || packet.instanceCount == 0) {
        return;
    }

    // IA設定 (共有リソースから)
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &gpuMesh->vertexBufferView);
    if (gpuMesh->indexCount > 0) {
        commandList_->IASetIndexBuffer(&gpuMesh->indexBufferView);
    }

    // Material (CBV)
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, packet.materialAddress);

    // Texture (SRV)

    // Instances (SRV)
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Instancing, packet.instancingSrvHandleGPU);

    // Draw
    if (packet.useGPUCulling && packet.indirectCommandBuffer) {
        // UAV(Compute) から IndirectArgument へのバリア (ComputePassでUAVBarrierはかかっているが、State遷移が必要)
        DirectXUtils::TransitionBarrier(commandList_, packet.indirectCommandBuffer,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

        // OutputInstancesBuffer も UAV から SRV (NON_PIXEL_SHADER_RESOURCE) への遷移が必要
        if (packet.outputInstancesBuffer) {
            DirectXUtils::TransitionBarrier(
                commandList_, packet.outputInstancesBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        commandList_->ExecuteIndirect(commandSignature_.Get(), 1, packet.indirectCommandBuffer, 0, nullptr, 0);

        // 状態を戻す
        if (packet.outputInstancesBuffer) {
            DirectXUtils::TransitionBarrier(commandList_, packet.outputInstancesBuffer,
                                            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
        DirectXUtils::TransitionBarrier(commandList_, packet.indirectCommandBuffer,
                                        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    } else {
        if (gpuMesh->indexCount > 0) {
            commandList_->DrawIndexedInstanced(gpuMesh->indexCount, packet.instanceCount, 0, 0, 0);
        } else {
            commandList_->DrawInstanced(gpuMesh->vertexCount, packet.instanceCount, 0, 0);
        }
    }
}

void DrawManager::DispatchGPUCulling(const RenderPackets::ModelBatchPacket& packet) {
    if (!packet.inputInstancesSrv.ptr || !packet.outputInstancesUav.ptr || !packet.cullingDataAddress ||
        !packet.indirectCommandBuffer || !packet.indirectCommandUploadBuffer) {
        return;
    }

    // 1. IndirectCommandBufferの初期化 (UploadBuffer からコピー)
    DirectXUtils::TransitionBarrier(commandList_, packet.indirectCommandBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                    D3D12_RESOURCE_STATE_COPY_DEST);
    commandList_->CopyBufferRegion(packet.indirectCommandBuffer, 0, packet.indirectCommandUploadBuffer, 0,
                                   sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
    DirectXUtils::TransitionBarrier(commandList_, packet.indirectCommandBuffer, D3D12_RESOURCE_STATE_COPY_DEST,
                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // 2. Compute Shader の実行
    ID3D12PipelineState* pso = dxCommon_->GetPSOManager()->GetComputePSO("GPUCulling");
    if (!pso) {
        return;
    }

    commandList_->SetPipelineState(pso);

    // Compute Shader の RootSignature とパラメータバインド
    // (現在は ComputeRootSignature が全体で1つという前提)
    commandList_->SetComputeRootConstantBufferView(4, packet.cullingDataAddress); // b0 (Slot 4)
    commandList_->SetComputeRootDescriptorTable(0, packet.inputInstancesSrv);     // t0 (Slot 0)
    commandList_->SetComputeRootDescriptorTable(3, packet.outputInstancesUav);    // u0 (Slot 3)
    commandList_->SetComputeRootDescriptorTable(6, packet.indirectCommandUav);    // u1 (Slot 6)

    // スレッドグループ数を計算 (スレッド数は1グループあたり64に固定)
    UINT dispatchX = (packet.maxInstanceCount + 63) / 64;
    commandList_->Dispatch(dispatchX, 1, 1);
}

void DrawManager::SubmitPrimitiveBatch(const RenderPackets::PrimitiveBatchPacket& packet) {
    GetLocalQueues().primitiveBatchQueue.push_back(packet);
}

void DrawManager::DrawPrimitiveBatch(const RenderPackets::PrimitiveBatchPacket& packet) {
    if (packet.indexCount == 0 || packet.instanceCount == 0) {
        return;
    }

    // IA
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &packet.vertexBufferView);
    commandList_->IASetIndexBuffer(&packet.indexBufferView);

    // CBV (PS)
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, packet.materialAddress); // PS b0

    // SRV (PS t0 / VS t0)            // PS t0
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Instancing, packet.instancingSrvHandleGPU); // VS t0

    // Draw
    commandList_->DrawIndexedInstanced(packet.indexCount, packet.instanceCount, 0, 0, 0);
}

void DrawManager::SubmitPrimitive2DBatch(const RenderPackets::Primitive2DBatchPacket& packet) {
    GetLocalQueues().primitive2DBatchQueue.push_back(packet);
}

void DrawManager::DrawPrimitive2DBatch(const RenderPackets::Primitive2DBatchPacket& packet) {
    if (packet.indexCount == 0 || packet.instanceCount == 0) {
        return;
    }

    // IA
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &packet.vertexBufferView);
    commandList_->IASetIndexBuffer(&packet.indexBufferView);

    // CBV (PS b0)
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, packet.materialAddress);

    // SRV (VS t0 -> Instancing Data)
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Instancing, packet.instancingSrvHandleGPU);

    // Draw
    commandList_->DrawIndexedInstanced(packet.indexCount, packet.instanceCount, 0, 0, 0);
}

void DrawManager::SubmitLineInstanced(const LineResource* resource,
                                      const D3D12_GPU_DESCRIPTOR_HANDLE& instancingSrvHandleGPU,
                                      const UINT& instanceCount, PSOManager::DepthWrite depthWrite) {
    if (!resource || instanceCount == 0) {
        return;
    }
    using namespace RenderPackets;
    LinePacket p{};
    p.resource = resource;
    p.instancingSrvHandleGPU = instancingSrvHandleGPU;
    p.instanceCount = instanceCount;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = depthWrite;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    GetLocalQueues().lineQueue.push_back(p);
}

void DrawManager::DrawLineInstanced(const RenderPackets::LinePacket& packet) {
    const LineResource* resource = packet.resource;
    if (!resource || packet.instanceCount == 0) {
        return;
    }

    // IA
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    commandList_->IASetVertexBuffers(0, 1, &resource->GetVertexBufferView());
    commandList_->IASetIndexBuffer(&resource->GetIndexBufferView());

    // SRV (VS t1)
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::LineInstancing, packet.instancingSrvHandleGPU);

    // Draw
    commandList_->DrawIndexedInstanced(2, packet.instanceCount, 0, 0, 0);
}

void DrawManager::SubmitDebugPrimitive(const RenderPackets::DebugPrimitivePacket& packet) {
    if (packet.indexCount == 0 || packet.instanceCount == 0) {
        return;
    }
    RenderPackets::DebugPrimitivePacket p = packet;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    GetLocalQueues().debugPrimitiveQueue.push_back(p);
}

void DrawManager::DrawDebugPrimitive(const RenderPackets::DebugPrimitivePacket& packet) {
    if (packet.indexCount == 0 || packet.instanceCount == 0) {
        return;
    }

    // IA
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    commandList_->IASetVertexBuffers(0, 1, &packet.vertexBufferView);
    commandList_->IASetIndexBuffer(&packet.indexBufferView);

    // SRV (VS t1)
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::LineInstancing, packet.instancingSrvHandleGPU);

    // Draw
    commandList_->DrawIndexedInstanced(packet.indexCount, packet.instanceCount, 0, 0, 0);
}

void DrawManager::DispatchSkinning(const SkinCluster& skinCluster, const ManagedModel* model, uint32_t numVertices) {
    if (!model || !model->gpuMeshes[0] || !dxCommon_) {
        return;
    }

    // --- コンピュートシェーダーによるスキニング実行 ---
    // PSOをコンピュート用に切り替え
    commandList_->SetPipelineState(dxCommon_->GetPSOManager()->GetComputePSO("Skinning"));

    // RootSignatureはSkipして共通のComputeRootSignatureを使用する想定
    // (PSO設定時にセットされているはずだが、念のため管理が必要な場合はここでセット)

    // Parameterの設定
    uint32_t frameIndex = dxCommon_->GetFrameIndex();

    // 0: Palette (t0)
    commandList_->SetComputeRootDescriptorTable(0, skinCluster.paletteSrvHandle[frameIndex].second);
    // 1: Input Vertices (t1) (結合済みの全頂点バッファを使用)
    commandList_->SetComputeRootDescriptorTable(1, skinCluster.inputVertexSrvHandle.second);
    // 2: Influences (t2)
    commandList_->SetComputeRootDescriptorTable(2, skinCluster.influenceSrvHandle.second);
    // 3: Output Vertices (u0)
    commandList_->SetComputeRootDescriptorTable(3, skinCluster.skinnedVertexUavHandle[frameIndex].second);
    // 4: Skinning Information (b0)
    commandList_->SetComputeRootConstantBufferView(4, skinCluster.skinningInformationResource->GetGPUVirtualAddress());

    // Dispatch (numthreads = 256)
    commandList_->Dispatch((numVertices + 255) / 256, 1, 1);
}

void DrawManager::ExecuteUAVBarrier(ID3D12Resource* resource) {
    DirectXUtils::UAVBarrier(commandList_, resource);
}

void DrawManager::SubmitSkybox(const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
                               const D3D12_INDEX_BUFFER_VIEW& indexBufferView,
                               D3D12_GPU_VIRTUAL_ADDRESS materialAddress,
                               D3D12_GPU_VIRTUAL_ADDRESS transformationAddress, const UINT& indexCount) {
    RenderPackets::SkyboxPacket packet;
    packet.vertexBufferView = vertexBufferView;
    packet.indexBufferView = indexBufferView;
    packet.materialAddress = materialAddress;
    packet.transformationAddress = transformationAddress;
    packet.indexCount = indexCount;
    // packet.textureHandle is removed

    GetLocalQueues().skyboxQueue.push_back(packet);
}

void DrawManager::DrawSkybox(const RenderPackets::SkyboxPacket& packet) {
    if (dxCommon_->GetEngine()->GetScreenCaptureManager() &&
        dxCommon_->GetEngine()->GetScreenCaptureManager()->IsCaptureWithAlphaRequested()) {
        return;
    }

    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &packet.vertexBufferView);
    commandList_->IASetIndexBuffer(&packet.indexBufferView);
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, packet.materialAddress);        // b0
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Transform, packet.transformationAddress); // b0 (VS)
    // // deleted
    commandList_->DrawIndexedInstanced(packet.indexCount, 1, 0, 0, 0);
}

void DrawManager::SubmitSkydome(D3D12_GPU_VIRTUAL_ADDRESS materialAddress) {
    RenderPackets::SkydomePacket packet;
    packet.materialAddress = materialAddress;
    GetLocalQueues().skydomeQueue.push_back(packet);
}

void DrawManager::DrawSkydome(const RenderPackets::SkydomePacket& packet) {
    if (dxCommon_->GetEngine()->GetScreenCaptureManager() &&
        dxCommon_->GetEngine()->GetScreenCaptureManager()->IsCaptureWithAlphaRequested()) {
        return;
    }

    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 0, nullptr); // 頂点バッファ不要（SV_VertexID）
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, packet.materialAddress); // b0
    commandList_->DrawInstanced(3, 1, 0, 0); // 全画面三角形
}

void DrawManager::SubmitStandard3D(const Object3DResource* resource,
                                   const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride, bool castShadows,
                                   ID3D12Resource* vertexBufferResourceOverride,
                                   D3D12_GPU_VIRTUAL_ADDRESS overrideMaterialCBV) {
    if (!resource) {
        return;
    }
    Standard3DPacket p{};
    p.resource = resource;
    p.vertexBufferViewOverride = vertexBufferViewOverride;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    p.castShadows = castShadows;
    p.customPSO = resource->GetCustomPSO();
    p.customCBVAddress = resource->GetCustomCBVAddress();
    p.vertexBufferResourceOverride = vertexBufferResourceOverride;
    p.overrideMaterialCBV = overrideMaterialCBV;
    p.castShadows = castShadows;
    p.distanceToCamera = 0.0f; // Standardの場合は不要
    GetLocalQueues().standard3DQueue.push_back(p);
}

void DrawManager::SubmitTransparent3D(const Object3DResource* resource,
                                      const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride, bool castShadows,
                                      ID3D12Resource* vertexBufferResourceOverride,
                                      D3D12_GPU_VIRTUAL_ADDRESS overrideMaterialCBV) {
    if (!resource) {
        return;
    }
    Standard3DPacket p{};
    p.resource = resource;
    p.vertexBufferViewOverride = vertexBufferViewOverride;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    p.customPSO = resource->GetCustomPSO();
    p.customCBVAddress = resource->GetCustomCBVAddress();
    p.vertexBufferResourceOverride = vertexBufferResourceOverride;
    p.overrideMaterialCBV = overrideMaterialCBV;
    p.castShadows = castShadows;

    // カメラからの距離を計算
    p.distanceToCamera = 0.0f;
    if (auto engine = dxCommon_->GetEngine()) {
        if (auto camera = engine->GetCameraManager()->GetActiveCamera()) {
            Irufemi::Vector3 camPos = camera->GetTranslate();
            Irufemi::Vector3 objPos = resource->GetTransform().translate;
            float dx = camPos.x - objPos.x;
            float dy = camPos.y - objPos.y;
            float dz = camPos.z - objPos.z;
            p.distanceToCamera = dx * dx + dy * dy + dz * dz; // 距離の2乗（比較用なら2乗のままで十分）
        }
    }
    GetLocalQueues().transparent3DQueue.push_back(p);
}

void DrawManager::SubmitUI3D(const Object3DResource* resource,
                             const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride) {
    if (!resource) {
        return;
    }
    Standard3DPacket p{};
    p.resource = resource;
    p.vertexBufferViewOverride = vertexBufferViewOverride;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    p.customPSO = resource->GetCustomPSO();
    p.customCBVAddress = resource->GetCustomCBVAddress();
    GetLocalQueues().ui3DQueue.push_back(p);
}

void DrawManager::SubmitOutlineMask(const Object3DResource* resource,
                                    const D3D12_VERTEX_BUFFER_VIEW* vertexBufferViewOverride,
                                    ID3D12Resource* vertexBufferResourceOverride) {
    if (!resource) {
        return;
    }
    Standard3DPacket p{};
    p.resource = resource;
    p.vertexBufferViewOverride = vertexBufferViewOverride;
    p.vertexBufferResourceOverride = vertexBufferResourceOverride;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    p.customPSO = resource->GetCustomPSO();
    p.customCBVAddress = resource->GetCustomCBVAddress();
    GetLocalQueues().selectionMaskQueue.push_back(p);
}

void DrawManager::SubmitTextOutlineMask(const Object2DResource* resource) {
    if (!resource) {
        return;
    }
    SpritePacket p{};
    p.resource = resource;
    p.blendMode = dxCommon_->GetEngine()->currentBlend_;
    p.depthWrite = dxCommon_->GetEngine()->currentDepth_;
    p.cullMode = dxCommon_->GetEngine()->currentCull_;
    p.customPSO = resource->GetCustomPSO();
    p.customCBVAddress = resource->GetCustomCBVAddress();
    GetLocalQueues().selectionMaskQueue2D.push_back(p);
}

void DrawManager::DrawStandard3D(const RenderPackets::Standard3DPacket& packet) {
    const Object3DResource* resource = packet.resource;
    if (!resource || !commandList_) {
        return;
    }

    // --- 描画前: UAV -> VBV ---
    if (packet.vertexBufferResourceOverride) {
        DirectXUtils::TransitionBarrier(commandList_, packet.vertexBufferResourceOverride,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    }

    // トポロジ設定
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 頂点バッファの設定 (オーバーライドがあれば優先)
    if (packet.vertexBufferViewOverride) {
        commandList_->IASetVertexBuffers(0, 1, packet.vertexBufferViewOverride);
    } else {
        commandList_->IASetVertexBuffers(0, 1, &resource->GetVertexBufferView());
    }
    commandList_->IASetIndexBuffer(&resource->GetIndexBufferView());

    // 各種リソースのバインド
    if (packet.overrideMaterialCBV != 0) {
        commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, packet.overrideMaterialCBV);
    } else {
        commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, resource->GetMaterialVAddress());
    }
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Transform, resource->GetTransformVAddress());

    // customCBVAddress が設定されていれば Special (b6) にバインドする
    if (packet.customCBVAddress != 0) {
        commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Special, packet.customCBVAddress);
    }

    // 描画
    commandList_->DrawIndexedInstanced(resource->GetIndexCount(), 1, 0, 0, 0);

    // --- 描画後: VBV -> UAV に戻す (次フレームのCompute用) ---
    if (packet.vertexBufferResourceOverride) {
        DirectXUtils::TransitionBarrier(commandList_, packet.vertexBufferResourceOverride,
                                        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
}

void DrawManager::SubmitGPUParticle(const RenderPackets::GPUParticlePacket& packet) {
    if (packet.instanceCount == 0) {
        return;
    }
    GetLocalQueues().gpuParticleQueue.push_back(packet);
}

void DrawManager::DrawGPUParticle(const RenderPackets::GPUParticlePacket& packet) {
    if (!commandList_) {
        return;
    }

    // IA 設定: VB/Topology
    commandList_->IASetVertexBuffers(0, 1, &packet.vbv);
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // --- CBV のバインド ---
    // (rootParameters[(UINT)RootSlot::Material] に対応、PixelShader 側の b0 想定)
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, packet.materialAddress);
    // (rootParameters[(UINT)RootSlot::Transform] に対応、VertexShader の b0 配置)
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Transform, packet.perViewAddress);
    // エミッター設定 (RootSlot::Special -> register b6)
    if (packet.emitterAddress != 0) {
        commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Special, packet.emitterAddress);
    }

    // --- SRVのバインド ---
    // テクスチャ (PS t0)
    // パーティクルデータ (VS t0 -> Slot 5: Instancing)
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Instancing, packet.particleSrvHandle);

    // ソートデータ (VS t1 -> Slot 9: LineInstancing)
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::LineInstancing, packet.sortListSrvHandle);

    if (packet.indexCount > 0) {
        commandList_->IASetIndexBuffer(&packet.ibv);
        commandList_->DrawIndexedInstanced(packet.indexCount, packet.instanceCount, 0, 0, 0);
    } else {
        // 従来のビルボード互換
        commandList_->DrawInstanced(6, packet.instanceCount, 0, 0);
    }
}

void DrawManager::SubmitVoxelParticle(uint32_t instanceCount, const D3D12_VERTEX_BUFFER_VIEW& vbv,
                                      const D3D12_INDEX_BUFFER_VIEW& ibv, uint32_t indexCount,
                                      D3D12_GPU_VIRTUAL_ADDRESS systemCbAddress,
                                      D3D12_GPU_DESCRIPTOR_HANDLE emitterHandle,
                                      D3D12_GPU_DESCRIPTOR_HANDLE particleDataHandle, ID3D12Resource* particleResource,
                                      ID3D12PipelineState* drawPSO) {
    if (instanceCount == 0) {
        return;
    }
    VoxelParticlePacket p{};
    p.instanceCount = instanceCount;
    p.vbv = vbv;
    p.ibv = ibv;
    p.indexCount = indexCount;
    p.systemCbAddress = systemCbAddress;
    p.emitterHandle = emitterHandle;
    p.particleDataHandle = particleDataHandle;
    p.particleResource = particleResource;
    p.drawPSO = drawPSO;
    GetLocalQueues().voxelParticleQueue.push_back(p);
}

void DrawManager::DrawVoxelParticle(const RenderPackets::VoxelParticlePacket& packet) {
    if (!commandList_) {
        return;
    }

    // リソースバリヤー: UAV -> ShaderResource (読み取り)
    if (packet.particleResource) {
        DirectXUtils::TransitionBarrier(commandList_, packet.particleResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                                            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

    // VoxelParticle 専用PSOをバインド
    if (packet.drawPSO) {
        commandList_->SetPipelineState(packet.drawPSO);
    }

    // トポロジ設定
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 頂点バッファとインデックスバッファの設定
    commandList_->IASetVertexBuffers(0, 1, &packet.vbv);
    commandList_->IASetIndexBuffer(&packet.ibv);

    // VoxelParticle 特有のバインド
    // Slot 1: Irufemi::Transform (b0) <- VoxelSystemCb
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Transform, packet.systemCbAddress);
    // Slot 4: Instancing (t0) <- Emitters
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Instancing, packet.emitterHandle);
    // Slot 7: LineInstancing (t1) <- ParticleData
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::LineInstancing, packet.particleDataHandle);

    commandList_->DrawIndexedInstanced(packet.indexCount, packet.instanceCount, 0, 0, 0);

    // リソースバリヤー: ShaderResource -> UAV (次のフレームの計算用に戻す)
    if (packet.particleResource) {
        DirectXUtils::TransitionBarrier(commandList_, packet.particleResource,
                                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                                            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
}

void DrawManager::BeginRenderTextures(const std::vector<class RenderTexture*>& renderTargets,
                                      const std::vector<Irufemi::Vector4>& clearColors) {
    if (renderTargets.empty()) {
        return;
    }

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
    rtvHandles.reserve(renderTargets.size());

    // 1. Transition Barrier (SRV -> RenderTarget)
    for (auto* rt : renderTargets) {
        if (rt) {
            DirectXUtils::TransitionBarrier(commandList_, rt->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET);
            rtvHandles.push_back(rt->GetRtvHandle());
        }
    }

    // 先頭のテクスチャを基準に追跡
    currentRenderTexture_ = renderTargets[0];
    if (renderTargets.size() > 1) {
        currentRenderTexture2_ = renderTargets[1];
    } else {
        currentRenderTexture2_ = nullptr;
    }

    // 2. Set Render Target
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);
    commandList_->OMSetRenderTargets(static_cast<UINT>(rtvHandles.size()), rtvHandles.data(), false, &dsvHandle);

    // 3. Clear
    for (size_t i = 0; i < rtvHandles.size(); ++i) {
        if (i < clearColors.size()) {
            commandList_->ClearRenderTargetView(rtvHandles[i], &clearColors[i].x, 0, nullptr);
        } else {
            const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            commandList_->ClearRenderTargetView(rtvHandles[i], clearColor, 0, nullptr);
        }
    }
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // 4. Set Viewport/Scissor (先頭のテクスチャサイズを基準とする)
    auto* baseRt = renderTargets[0];
    D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(baseRt->GetWidth()), static_cast<float>(baseRt->GetHeight()),
                            0.0f, 1.0f};
    D3D12_RECT scissor{0, 0, static_cast<long>(baseRt->GetWidth()), static_cast<long>(baseRt->GetHeight())};
    commandList_->RSSetViewports(1, &viewport);
    commandList_->RSSetScissorRects(1, &scissor);

    // 5. Descriptor Heaps (念のため再設定)
    ID3D12DescriptorHeap* descriptorHeaps[] = {dxCommon_->GetSrvDescriptorHeap()};
    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
}

void DrawManager::EndRenderTextures(const std::vector<class RenderTexture*>& renderTargets) {
    // 1. Transition Barrier (RenderTarget -> SRV)
    for (auto* rt : renderTargets) {
        if (rt) {
            DirectXUtils::TransitionBarrier(commandList_, rt->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
    }
}

void DrawManager::BeginRenderTexture(RenderTexture* rt, const Irufemi::Vector4& clearColor, RenderTexture* rt2,
                                     const Irufemi::Vector4& clearColor2) {
    // 1. Transition Barrier (SRV -> RenderTarget)
    DirectXUtils::TransitionBarrier(commandList_, rt->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                    D3D12_RESOURCE_STATE_RENDER_TARGET);
    if (rt2) {
        DirectXUtils::TransitionBarrier(commandList_, rt2->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                        D3D12_RESOURCE_STATE_RENDER_TARGET);
    }

    // レンダーターゲットを追跡
    currentRenderTexture_ = rt;
    currentRenderTexture2_ = rt2;

    // 2. Set Render Target
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2] = {rt->GetRtvHandle()};
    uint32_t numRTVs = 1;
    if (rt2) {
        rtvHandles[1] = rt2->GetRtvHandle();
        numRTVs = 2;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);
    commandList_->OMSetRenderTargets(numRTVs, rtvHandles, false, &dsvHandle);

    // 3. Clear
    commandList_->ClearRenderTargetView(rtvHandles[0], &clearColor.x, 0, nullptr);
    if (rt2) {
        commandList_->ClearRenderTargetView(rtvHandles[1], &clearColor2.x, 0, nullptr);
    }
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // 4. Set Viewport/Scissor
    D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(rt->GetWidth()), static_cast<float>(rt->GetHeight()),
                            0.0f, 1.0f};
    D3D12_RECT scissor{0, 0, static_cast<long>(rt->GetWidth()), static_cast<long>(rt->GetHeight())};
    commandList_->RSSetViewports(1, &viewport);
    commandList_->RSSetScissorRects(1, &scissor);

    // 5. Descriptor Heaps (念のため再設定)
    ID3D12DescriptorHeap* descriptorHeaps[] = {dxCommon_->GetSrvDescriptorHeap()};
    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
}

void DrawManager::EndRenderTexture(RenderTexture* rt, RenderTexture* rt2) {
    // 1. Transition Barrier (RenderTarget -> SRV)
    DirectXUtils::TransitionBarrier(commandList_, rt->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if (rt2) {
        DirectXUtils::TransitionBarrier(commandList_, rt2->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                                        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
}

void DrawManager::SetRenderTargetToBackBuffer(bool useDepth) {
    const UINT backIdx = dxCommon_->GetSwapChain()->GetCurrentBackBufferIndex();
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetRtvHandles(backIdx);
    if (useDepth) {
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);
        commandList_->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
    } else {
        commandList_->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    }

    // ビューポートとシザーをスワップチェーンサイズに合わせてLetterbox計算して戻す
    float clientW = dxCommon_->GetViewport().Width;
    float clientH = dxCommon_->GetViewport().Height;
    float targetAspect = static_cast<float>(engine_->GetGameResolutionWidth()) / engine_->GetGameResolutionHeight();
    float windowAspect = clientW / clientH;

    D3D12_VIEWPORT viewport{};
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    if (windowAspect > targetAspect) {
        viewport.Height = clientH;
        viewport.Width = clientH * targetAspect;
        viewport.TopLeftX = (clientW - viewport.Width) / 2.0f;
        viewport.TopLeftY = 0.0f;
    } else {
        viewport.Width = clientW;
        viewport.Height = clientW / targetAspect;
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = (clientH - viewport.Height) / 2.0f;
    }

    commandList_->RSSetViewports(1, &viewport);
    commandList_->RSSetScissorRects(1, &dxCommon_->GetScissorRect());

    // レンダーターゲット追跡のリセット
    currentRenderTexture_ = nullptr;
    currentRenderTexture2_ = nullptr;
}

void DrawManager::DrawRenderTexture(RenderTexture* renderTexture, ID3D12PipelineState* pso,
                                    D3D12_GPU_VIRTUAL_ADDRESS cbvAddress, D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle) {
    if (!renderTexture) {
        return;
    }

    // 1. PSOの設定 (引数が渡された場合はそれを使用、そうでなければデフォルトのCopyImage)
    if (pso) {
        commandList_->SetPipelineState(pso);
    } else {
        ID3D12PipelineState* defaultPso = dxCommon_->GetPSOManager()->GetCopyImage();
        if (!defaultPso) {
            return;
        }
        commandList_->SetPipelineState(defaultPso);
    }

    // --- ルートシグネチャのセット ---
    commandList_->SetGraphicsRootSignature(dxCommon_->GetRootSignature());

    // --- Bindless SRV用ディスクリプタテーブルのセット (Slot 2) ---
    // [Bindless] srvPoolの先頭(index=0)から最大数までをバインドする
    // descriptorTable の先頭アドレスをセット
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::BindlessSRV, dxCommon_->GetSrvPool()->GetGPUHandle(0));

    // --- カメラ用 定数バッファのセット (Slot 5) ---
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 4. テクスチャの設定 (RootParameter[(UINT)RootSlot::Texture])

    // 深度テクスチャの設定 (RootParameter[(UINT)RootSlot::EnvMap])
    if (depthSrvHandle.ptr != 0) {
    }

    // ConstantBuffer の設定 (RootParameter[(UINT)RootSlot::Material])
    if (cbvAddress != 0) {
        commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Material, cbvAddress);
    }

    // 5. 描画 (3頂点のインデックスなし描画: SV_VertexIDを使用するためVBいらず)
    commandList_->DrawInstanced(3, 1, 0, 0);
}
void DrawManager::BindCommonParameters() {
    if (!commandList_ || !dxCommon_) {
        return;
    }

    commandList_->SetGraphicsRootSignature(dxCommon_->GetRootSignature());
    commandList_->SetComputeRootSignature(dxCommon_->GetComputeRootSignature());

    auto& fr = frameResources_[dxCommon_->GetFrameIndex()];
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::Camera, fr.frameData.camera);
    commandList_->SetGraphicsRootConstantBufferView((UINT)RootSlot::LightCommon, fr.frameData.lightCommon);

    // [Bindless] srvPoolの先頭(index=0)から最大数までをバインドする
    // descriptorTable の先頭アドレスをセット
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::BindlessSRV, dxCommon_->GetSrvPool()->GetGPUHandle(0));

    // レガシー用バインド (HLSL側のBindless完全移行が終わるまで必要)
    // 点光源、スポットライト、面光源を3つのテーブル(Slot 10等)で一括設定
    commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::Lights, fr.lightSrvHandle);

    if (ShadowMap* shadowMap = GetShadowMap()) {
        commandList_->SetGraphicsRootDescriptorTable((UINT)RootSlot::ShadowMap, shadowMap->GetSrvHandle());
    }
}

void DrawManager::BeginShadowPass() {
    ShadowMap* shadowMap = GetShadowMap();
    if (!shadowMap) {
        return;
    }
    isShadowPass_ = true;

    // 1. シャドウマップの準備 (バリア遷移、クリア、DSVセット)
    shadowMap->Begin(commandList_);

    // 2. ライト行列を定数バッファに反映
    auto& fr = frameResources_[dxCommon_->GetFrameIndex()];
    if (fr.lightCommonData) {
        // すでに SetFrameData で計算済みだが、念のため最新の状態を反映
        fr.lightCommonData->viewProjection = shadowMap->GetViewProjection();
    }

    // 3. DescriptorHeap再設定
    ID3D12DescriptorHeap* descriptorHeaps[] = {dxCommon_->GetSrvDescriptorHeap()};
    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // 4. バインド (ライト行列を定数バッファに反映させるため)
    BindCommonParameters();
}

void DrawManager::EndShadowPass() {
    ShadowMap* shadowMap = GetShadowMap();
    if (!shadowMap) {
        return;
    }
    isShadowPass_ = false;

    // 1. バリア遷移を元に戻す (DepthWrite -> SRV)
    shadowMap->End(commandList_);

    // 2. レンダーターゲットを復帰させる
    if (currentRenderTexture_) {
        // 元の RenderTexture があればそれを再設定 (クリアはしない)
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2] = {currentRenderTexture_->GetRtvHandle()};
        uint32_t numRTVs = 1;
        if (currentRenderTexture2_) {
            rtvHandles[1] = currentRenderTexture2_->GetRtvHandle();
            numRTVs = 2;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon_->GetDSVCPUDescriptorHandle(0);
        commandList_->OMSetRenderTargets(numRTVs, rtvHandles, false, &dsvHandle);

        // ビューポート等も復帰
        D3D12_VIEWPORT viewport{0.0f,
                                0.0f,
                                static_cast<float>(currentRenderTexture_->GetWidth()),
                                static_cast<float>(currentRenderTexture_->GetHeight()),
                                0.0f,
                                1.0f};
        D3D12_RECT scissor{0, 0, static_cast<long>(currentRenderTexture_->GetWidth()),
                           static_cast<long>(currentRenderTexture_->GetHeight())};
        commandList_->RSSetViewports(1, &viewport);
        commandList_->RSSetScissorRects(1, &scissor);
    } else {
        // なければバックバッファに戻す
        SetRenderTargetToBackBuffer(true);
    }
}

void DrawManager::ExecuteRenderQueues(IrufemiEngine* engine) {
    // 全スレッドのローカル描画キューをメインキューへ一括マージ
    MergeThreadLocalQueues();

    if (renderGraph_) {
        // メインレンダリングテクスチャの初期状態を登録 (RenderGraph内で遷移するため)
        renderGraph_->SetInitialResourceState(engine->GetMainRenderTexture()->GetResource(),
                                              D3D12_RESOURCE_STATE_RENDER_TARGET);
        if (engine->GetEffectMaskTexture()) {
            renderGraph_->SetInitialResourceState(engine->GetEffectMaskTexture()->GetResource(),
                                                  D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
        if (engine->GetNormalTexture()) {
            renderGraph_->SetInitialResourceState(engine->GetNormalTexture()->GetResource(),
                                                  D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
        if (engine->GetMaterialTexture()) {
            renderGraph_->SetInitialResourceState(engine->GetMaterialTexture()->GetResource(),
                                                  D3D12_RESOURCE_STATE_RENDER_TARGET);
        }
        if (engine->GetVelocityTexture()) {
            renderGraph_->SetInitialResourceState(engine->GetVelocityTexture()->GetResource(),
                                                  D3D12_RESOURCE_STATE_RENDER_TARGET);
        }

        // 深度バッファの初期状態も登録 (DepthBasedOutline 等で参照するため)
        renderGraph_->SetInitialResourceState(dxCommon_->GetDepthStencilResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

        // 実行後の最終ステート（SRV）を登録
        renderGraph_->SetFinalResourceState(engine->GetMainRenderTexture()->GetResource(),
                                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        if (engine->GetEffectMaskTexture()) {
            renderGraph_->SetFinalResourceState(engine->GetEffectMaskTexture()->GetResource(),
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        if (engine->GetNormalTexture()) {
            renderGraph_->SetFinalResourceState(engine->GetNormalTexture()->GetResource(),
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        if (engine->GetMaterialTexture()) {
            renderGraph_->SetFinalResourceState(engine->GetMaterialTexture()->GetResource(),
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        if (engine->GetVelocityTexture()) {
            renderGraph_->SetFinalResourceState(engine->GetVelocityTexture()->GetResource(),
                                                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        renderGraph_->Execute(this, engine);
    }

    // RenderGraph 終了後はバックバッファを描画対象とする (TopMost UI など用)
    SetRenderTargetToBackBuffer(false);

    if (auto scm = engine->GetScreenCaptureManager()) {
        scm->OnPostDepthDraw(engine->GetCommandList(), dxCommon_->GetDepthStencilResource());
    }

    if (engine->GetDebugPrimitiveRenderer()) {
        engine->GetDebugPrimitiveRenderer()->ClearDrawInstances();
    }
    // TODO: engine->GetCollisionManager()->ClearDebugLines()
    // 等、他のグローバルデバッグバッチもここでクリアするアーキテクチャに統一できる

    ClearRenderQueues();
}

void DrawManager::ClearRenderQueues() {
    standard3DQueue_.clear();
    transparent3DQueue_.clear();
    ui3DQueue_.clear();
    selectionMaskQueue_.clear();
    selectionMaskQueue2D_.clear();
    spriteQueue_.clear();
    spriteBatchQueue_.clear();

    lineQueue_.clear();
    gpuParticleQueue_.clear();
    voxelParticleQueue_.clear();
    skyboxQueue_.clear();
    skydomeQueue_.clear();
    primitiveBatchQueue_.clear();
    primitive2DBatchQueue_.clear();
    modelBatchQueue_.clear();
    debugPrimitiveQueue_.clear();
    postRenderQueue_.clear();
    textQueue_.clear();
    topMostTextQueue_.clear();
    topMostSpriteQueue_.clear();
    topMostSpriteBatchQueue_.clear();

    // 登録された各スレッドのローカルキューも安全のためクリア
    std::lock_guard<std::mutex> lock(registryMutex_);
    for (auto* lq : registeredQueues_) {
        if (lq) {
            lq->Clear();
        }
    }
}
