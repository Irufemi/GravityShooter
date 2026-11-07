#pragma once

#include "Renderer/PostProcess/PostProcessManager.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include <d3d12.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <typeindex>

class RenderGraph;

namespace Irufemi {
class IPostProcessEffect;
class BloomEffect;
class LightShaftsEffect;
class SeparableBlurEffect;
class KawaseBlurEffect;
class CombinedPostEffect;
} // namespace Irufemi

/**
 * @class PostProcessRunner
 * @brief エフェクトのチェイン実行（Ping-Pong描画）に特化した軽量ディスパッチャ
 */
class PostProcessRunner {
public:
    PostProcessRunner();
    ~PostProcessRunner();

    /**
     * @brief 有効なエフェクト群およびピンポンバッファに必要な一時リソースを申告する
     */
    void DeclareResources(RenderGraphBuilder& builder, const D3D12_RESOURCE_DESC& baseDesc,
                          const std::vector<PostProcessManager::Mode>& modes,
                          PostProcessManager::Layer layer = PostProcessManager::Layer::PreUI);

    /**
     * @brief RenderGraphから一時リソースを解決する
     */
    void ResolveResources(RenderGraph* renderGraph, const std::vector<PostProcessManager::Mode>& modes,
                          PostProcessManager::Layer layer = PostProcessManager::Layer::PreUI);

    /**
     * @brief Ping-Pong方式で指定されたエフェクト配列を順次ディスパッチする
     */
    class RenderTexture* Run(PostProcessManager* manager, ID3D12GraphicsCommandList* commandList,
                             const std::vector<PostProcessManager::Mode>& modes, class RenderTexture* srcTexture,
                             D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
                             PostProcessManager::Layer layer = PostProcessManager::Layer::PreUI,
                             bool isFinalOutput = true, bool isBackBufferTarget = false,
                             const D3D12_VIEWPORT* targetViewport = nullptr, const D3D12_RECT* targetScissor = nullptr);

    /**
     * @brief RenderGraphにポストプロセスの全サブパスを展開・登録する (Zero-Manual-Barriers)
     */
    void PopulatePasses(RenderGraph* renderGraph, PostProcessManager* manager,
                        const std::vector<PostProcessManager::Mode>& modes, TransientResourceHandle srcHandle,
                        TransientResourceHandle dstHandle,
                        PostProcessManager::Layer layer = PostProcessManager::Layer::PreUI, bool isFinalOutput = true);

    /**
     * @brief 各分離エフェクトの専用PSOおよび定数バッファを初期化する
     */
    void InitializeEffects(ID3D12Device* device, class DirectXCommon* dxCommon, ID3D12RootSignature* rootSig,
                           D3D12_SHADER_BYTECODE vsBytecode, DXGI_FORMAT rtvFormat);

    /**
     * @brief 全ポストプロセスエフェクトを初期状態にリセットする
     * @param layer リセット対象のレイヤー (PreUI / PostUI / All)
     */
    void ResetAllEffects(PostProcessManager::Layer layer = PostProcessManager::Layer::All);

    Irufemi::KawaseBlurEffect* GetKawaseBlurEffect() const {
        return kawaseBlurEffect_.get();
    }

    /**
     * @brief 型指定による自己完結エフェクトの高速安全取得（レジストリパターン）
     */
    template <typename T> T* GetEffect() const {
        auto it = effectRegistry_.find(std::type_index(typeid(T)));
        if (it != effectRegistry_.end()) {
            return static_cast<T*>(it->second);
        }
        return nullptr;
    }

private:
    bool RequiresSeparatePass(PostProcessManager::Mode mode) const;

    void TransitionResource(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource,
                            D3D12_RESOURCE_STATES targetState);

    std::unordered_map<ID3D12Resource*, D3D12_RESOURCE_STATES> localResourceStates_;
    std::unordered_map<std::type_index, Irufemi::IPostProcessEffect*> effectRegistry_;

    TransientResourceHandle workHandles_[2][2] = {{kInvalidHandle, kInvalidHandle}, {kInvalidHandle, kInvalidHandle}};
    class RenderTexture* workTextures_[2][2] = {{nullptr, nullptr}, {nullptr, nullptr}};

    std::unique_ptr<Irufemi::BloomEffect> bloomEffect_;
    std::unique_ptr<Irufemi::LightShaftsEffect> lightShaftsEffect_;
    std::unique_ptr<Irufemi::SeparableBlurEffect> smoothingEffect_;
    std::unique_ptr<Irufemi::SeparableBlurEffect> gaussianEffect_;
    std::unique_ptr<Irufemi::KawaseBlurEffect> kawaseBlurEffect_;
    std::unique_ptr<Irufemi::CombinedPostEffect> combinedEffect_;
};
