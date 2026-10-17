#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "Renderer/DrawManager.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Math/MathFunction.h"
#include "Renderer/Data/RenderContext.h"
#include <algorithm>
#include <string>
#include "Core/Utility/StringUtility.h"

void RenderGraph::AddPass(std::unique_ptr<IRenderPass> pass) {
    if (pass) {
        passes_.push_back(std::move(pass));
    }
}

void RenderGraph::InitializeTransientResourceManager(DirectXCommon* dxCommon) {
    transientResourceManager_ = std::make_unique<TransientResourceManager>();
    transientResourceManager_->Initialize(dxCommon, 512 * 1024 * 1024); // 512MBへ拡大してOOMを防止
}

void RenderGraph::Execute(DrawManager* drawManager, IrufemiEngine* engine) {
    auto* dxCommon = drawManager ? drawManager->GetDxCommon() : nullptr;
    auto* cmdList = dxCommon ? dxCommon->GetCommandList() : nullptr;
    auto* device = dxCommon ? dxCommon->GetDevice() : nullptr;

    if (transientResourceManager_) {
        transientResourceManager_->ResetForFrame();
    }

    // RenderContext の構築
    Irufemi::RenderContext rc{};
    rc.engine = engine;
    rc.drawManager = drawManager;
    rc.dxCommon = dxCommon;
    rc.textureManager = engine ? engine->GetTextureManager() : nullptr;
    rc.fontManager = engine ? engine->GetFontManager() : nullptr;
    rc.psoManager = engine ? engine->GetPSOManager() : nullptr;
    rc.commandList = cmdList;

#ifdef EditorMode
    rc.isEditorMode = true;
    if (engine && engine->GetMainRenderTexture()) {
        rc.outputResource = engine->GetMainRenderTexture()->GetResource();
        rc.outputRTV = engine->GetMainRenderTexture()->GetRtvHandle();
    }
    rc.viewport.Width = static_cast<float>(engine ? engine->GetGameResolutionWidth() : 1280);
    rc.viewport.Height = static_cast<float>(engine ? engine->GetGameResolutionHeight() : 720);
    rc.viewport.TopLeftX = 0.0f;
    rc.viewport.TopLeftY = 0.0f;
    rc.viewport.MinDepth = 0.0f;
    rc.viewport.MaxDepth = 1.0f;

    rc.scissorRect.left = 0;
    rc.scissorRect.right = engine ? engine->GetGameResolutionWidth() : 1280;
    rc.scissorRect.top = 0;
    rc.scissorRect.bottom = engine ? engine->GetGameResolutionHeight() : 720;
#else
    rc.isEditorMode = false;
    rc.outputResource = nullptr;
    if (dxCommon) {
        rc.outputRTV = dxCommon->GetRtvHandles(dxCommon->GetCurrentBackBufferIndex());
    }
    if (engine) {
        float clientW = static_cast<float>(engine->GetClientWidth());
        float clientH = static_cast<float>(engine->GetClientHeight());
        float gameW = static_cast<float>(engine->GetGameResolutionWidth());
        float gameH = static_cast<float>(engine->GetGameResolutionHeight());
        Irufemi::Math::LetterboxInfo view = Irufemi::Math::CalculateLetterbox(clientW, clientH, gameW, gameH);

        rc.viewport.Width = view.viewW;
        rc.viewport.Height = view.viewH;
        rc.viewport.TopLeftX = view.viewX;
        rc.viewport.TopLeftY = view.viewY;
        rc.viewport.MinDepth = 0.0f;
        rc.viewport.MaxDepth = 1.0f;

        rc.scissorRect.left = static_cast<LONG>(view.viewX);
        rc.scissorRect.right = static_cast<LONG>(view.viewX + view.viewW);
        rc.scissorRect.top = static_cast<LONG>(view.viewY);
        rc.scissorRect.bottom = static_cast<LONG>(view.viewY + view.viewH);
    }
#endif

    // 1. Compile Phase (全パスの Setup を実行し、リソース要求を収集)
    for (size_t i = 0; i < passes_.size(); ++i) {
        builder_.SetCurrentPassIndex(i);
        passes_[i]->Setup(builder_, rc);
    }

    // 2. Resource Allocation Phase (Transient Resource の寿命計算とエイリアシング)
    struct TransientResourceLifetime {
        size_t firstPass = static_cast<size_t>(-1);
        size_t lastPass = 0;
        uint64_t size = 0;
        uint64_t alignment = 0;
        uint64_t offset = 0;
        ID3D12Resource* physicalResource = nullptr;
    };

    const auto& transientDescs = builder_.GetTransientDescs();
    const auto& transientUsages = builder_.GetTransientUsages();
    std::vector<TransientResourceLifetime> lifetimes(transientDescs.size());

    for (size_t i = 0; i < transientDescs.size(); ++i) {
        auto info = device->GetResourceAllocationInfo(0, 1, &transientDescs[i].desc);
        lifetimes[i].size = info.SizeInBytes;
        lifetimes[i].alignment = info.Alignment;
    }

    for (const auto& usage : transientUsages) {
        auto& lf = lifetimes[usage.handle];
        if (lf.firstPass == static_cast<size_t>(-1)) {
            lf.firstPass = usage.passIndex;
        }
        lf.lastPass = (std::max)(lf.lastPass, usage.passIndex);
    }

    transientPhysicalResources_.resize(transientDescs.size(), nullptr);

    // RenderTexture をサイズに合わせて拡張（既存のものは再利用）
    while (transientRenderTextures_.size() < transientDescs.size()) {
        transientRenderTextures_.push_back(std::make_unique<RenderTexture>());
    }

    // エイリアシングによるヒープオフセットの決定 (貪欲法)
    for (size_t i = 0; i < lifetimes.size(); ++i) {
        if (lifetimes[i].firstPass == static_cast<size_t>(-1)) {
            continue;
        }

        uint64_t requiredSize = lifetimes[i].size;
        uint64_t alignment = lifetimes[i].alignment;
        uint64_t bestOffset = 0;

        bool placed = false;
        while (!placed) {
            placed = true;
            for (size_t j = 0; j < i; ++j) {
                if (lifetimes[j].firstPass == static_cast<size_t>(-1)) {
                    continue;
                }

                // 寿命が重なるか？
                bool overlapTime = (lifetimes[i].firstPass <= lifetimes[j].lastPass) &&
                                   (lifetimes[i].lastPass >= lifetimes[j].firstPass);
                if (overlapTime) {
                    uint64_t startI = bestOffset;
                    uint64_t endI = startI + requiredSize;
                    uint64_t startJ = lifetimes[j].offset;
                    uint64_t endJ = startJ + lifetimes[j].size;

                    // メモリ領域も重なるなら、J の後ろに配置を試みる
                    if (startI < endJ && endI > startJ) {
                        bestOffset = (endJ + alignment - 1) & ~(alignment - 1);
                        placed = false;
                        break;
                    }
                }
            }
        }
        lifetimes[i].offset = bestOffset;

        // PlacedResource の生成
        if (transientResourceManager_) {
            const auto& tDesc = transientDescs[i];
            D3D12_RESOURCE_STATES actualState = D3D12_RESOURCE_STATE_COMMON;
            lifetimes[i].physicalResource = transientResourceManager_->AcquirePlacedResource(
                tDesc.desc, bestOffset, D3D12_RESOURCE_STATE_COMMON, tDesc.hasClearValue ? &tDesc.clearValue : nullptr,
                &actualState);
            if (lifetimes[i].physicalResource) {
                // キャッシュ再利用時の実ステートを正確に登録（不正なCOMMON仮定バリアを防止）
                resourceStates_[lifetimes[i].physicalResource] = actualState;

                std::wstring wname = ConvertString(tDesc.name);
                lifetimes[i].physicalResource->SetName(wname.c_str());

                // RenderTextureラッパーも初期化
                transientRenderTextures_[i]->InitializeFromResource(drawManager->GetDxCommon(),
                                                                    lifetimes[i].physicalResource, tDesc.desc.Format);
            }
        }
        transientPhysicalResources_[i] = lifetimes[i].physicalResource;
    }

    // 3. Execution Phase (バリアの発行と描画コマンドの積み込み)
    for (size_t passIdx = 0; passIdx < passes_.size(); ++passIdx) {
        auto& pass = passes_[passIdx];
        std::vector<D3D12_RESOURCE_BARRIER> barriers;

        // このパスで最初に使われる Transient Resource に対するエイリアシングバリア
        // DirectX 12 公式仕様（Simple Model）:
        // pResourceBefore を nullptr に指定することで、該当ヒープ領域の先行リソースとのエイリアシング競合を解消し、
        // pResourceAfter をアクティブ化する。
        for (size_t i = 0; i < lifetimes.size(); ++i) {
            if (lifetimes[i].firstPass == passIdx && lifetimes[i].physicalResource) {
                D3D12_RESOURCE_BARRIER b{};
                b.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
                b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                b.Aliasing.pResourceBefore = nullptr;
                b.Aliasing.pResourceAfter = lifetimes[i].physicalResource;
                barriers.push_back(b);
            }
        }

        // 通常リソースの Transition バリア
        for (const auto& usage : builder_.GetUsages()) {
            if (usage.passIndex != passIdx || !usage.resource) {
                continue;
            }

            auto it = resourceStates_.find(usage.resource);
            D3D12_RESOURCE_STATES currentState =
                (it != resourceStates_.end()) ? it->second : D3D12_RESOURCE_STATE_COMMON;

            if (currentState != usage.state) {
                D3D12_RESOURCE_BARRIER b{};
                b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                b.Transition.pResource = usage.resource;
                b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                b.Transition.StateBefore = currentState;
                b.Transition.StateAfter = usage.state;
                barriers.push_back(b);
                resourceStates_[usage.resource] = usage.state;
            }
        }

        // Transient リソースの Transition バリア
        for (const auto& usage : transientUsages) {
            if (usage.passIndex != passIdx) {
                continue;
            }
            auto* res = lifetimes[usage.handle].physicalResource;
            if (!res) {
                continue;
            }

            auto it = resourceStates_.find(res);
            D3D12_RESOURCE_STATES currentState =
                (it != resourceStates_.end()) ? it->second : D3D12_RESOURCE_STATE_COMMON;

            if (currentState != usage.state) {
                D3D12_RESOURCE_BARRIER b{};
                b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                b.Transition.pResource = res;
                b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                b.Transition.StateBefore = currentState;
                b.Transition.StateAfter = usage.state;
                barriers.push_back(b);
                resourceStates_[res] = usage.state;
            }
        }

        if (!barriers.empty()) {
            cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
        }

        // パスの実行
        pass->Execute(rc);
    }

    // 全パス実行後、要求された最終ステートへの TransitionBarrier を発行
    std::vector<D3D12_RESOURCE_BARRIER> finalBarriers;
    for (const auto& [resource, finalState] : finalResourceStates_) {
        if (!resource) {
            continue;
        }
        auto it = resourceStates_.find(resource);
        D3D12_RESOURCE_STATES currentState = (it != resourceStates_.end()) ? it->second : D3D12_RESOURCE_STATE_COMMON;

        if (currentState != finalState) {
            D3D12_RESOURCE_BARRIER b{};
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            b.Transition.pResource = resource;
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b.Transition.StateBefore = currentState;
            b.Transition.StateAfter = finalState;
            finalBarriers.push_back(b);
            resourceStates_[resource] = finalState;
        }
    }

    if (!finalBarriers.empty()) {
        cmdList->ResourceBarrier(static_cast<UINT>(finalBarriers.size()), finalBarriers.data());
    }

    // 一時リソースの最終ステートを TransientResourceManager に同期
    if (transientResourceManager_) {
        for (const auto& [res, state] : resourceStates_) {
            transientResourceManager_->UpdateResourceState(res, state);
        }
    }
}

void RenderGraph::ClearPasses() {
    passes_.clear();
    builder_.Clear();
}

void RenderGraph::ResetStates() {
    resourceStates_.clear();
    finalResourceStates_.clear();
}

void RenderGraph::OnResize() {
    ResetStates();
    if (transientResourceManager_) {
        transientResourceManager_->ClearCache();
    }
}

void RenderGraph::SetInitialResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) {
    if (resource) {
        resourceStates_[resource] = state;
    }
}

void RenderGraph::SetFinalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) {
    if (resource) {
        finalResourceStates_[resource] = state;
    }
}

#ifdef USE_IMGUI
void RenderGraph::DebugUI() {
    if (transientResourceManager_) {
        transientResourceManager_->DebugUI();
    }
}
#endif
