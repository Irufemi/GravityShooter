#pragma once
#include "Renderer/Pipeline/RenderGraph/IRenderPass.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <d3d12.h>
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include "Renderer/Pipeline/RenderGraph/TransientResourceManager.h"

#include "Renderer/Pipeline/RenderGraph/LambdaRenderPass.h"

class DrawManager;
class IrufemiEngine;

/**
 * @class RenderGraph
 * @brief 描画パス（RenderPass）を管理し、順次実行するクラス
 * @details 各パスの Setup を通じてリソースの使用状態を記録し、Execute
 * 時に自動でリソースバリア（Transition）を発行します。
 */
class RenderGraph {
public:
    RenderGraph() = default;
    ~RenderGraph() = default;

    /**
     * @brief 描画パスをグラフに登録する
     * @param[in] pass 登録する IRenderPass を実装したクラスのユニークポインタ
     */
    void AddPass(std::unique_ptr<IRenderPass> pass);

    /**
     * @brief ラムダ式を用いてインラインでパスを登録する（Unreal Engine RDG 準拠）
     */
    template <typename SetupFn, typename ExecFn>
    void AddPass(const std::string& name, SetupFn&& setup, ExecFn&& exec) {
        passes_.push_back(std::make_unique<LambdaRenderPass>(
            name, std::forward<SetupFn>(setup), std::forward<ExecFn>(exec)));
    }

    /**
     * @brief 登録されたすべてのパスを順に実行する
     * @param[in] drawManager 描画コマンドを発行するための DrawManager
     * @param[in] engine エンジン本体
     */
    void Execute(DrawManager* drawManager, IrufemiEngine* engine);

    /**
     * @brief 登録されているすべてのパスを破棄する
     */
    void ClearPasses();

    /**
     * @brief リソースのステート追跡をリセットする（フレーム開始時などに呼ぶ）
     */
    void ResetStates();

    /**
     * @brief リサイズ時に一時リソースキャッシュとステートをクリアする
     */
    void OnResize();

#ifdef USE_IMGUI
    /**
     * @brief デバッグUIの描画
     */
    void DebugUI();
#endif

    /**
     * @brief 描画実行時に一時リソースの実際のポインタを取得する
     */
    ID3D12Resource* GetTransientResource(TransientResourceHandle handle) {
        if (handle < transientPhysicalResources_.size()) {
            return transientPhysicalResources_[handle];
        }
        return nullptr;
    }

    /**
     * @brief 一時リソースの RenderTexture ラッパーを取得する（RTVやSRVが必要な場合）
     */
    class RenderTexture* GetTransientRenderTexture(TransientResourceHandle handle) {
        if (handle < transientRenderTextures_.size()) {
            return transientRenderTextures_[handle].get();
        }
        return nullptr;
    }

    /**
     * @brief TransientResourceManagerの初期化
     */
    void InitializeTransientResourceManager(class DirectXCommon* dxCommon);
    /**
     * @brief TransientResourceManager を取得する。
     * @return 取得された TransientResourceManager
     */
    TransientResourceManager* GetTransientResourceManager() {
        return transientResourceManager_.get();
    }

    /**
     * @brief 特定のリソースの現在のステートを登録する（初期ステートの通知用）
     */
    void SetInitialResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);

    /**
     * @brief 描画終了後に、特定のリソースを特定のステートに戻すよう要求する
     */
    void SetFinalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);

    /**
     * @brief リソースの現在の追跡ステートを取得する
     */
    D3D12_RESOURCE_STATES GetResourceState(ID3D12Resource* resource) const {
        auto it = resourceStates_.find(resource);
        if (it != resourceStates_.end()) {
            return it->second;
        }
        return D3D12_RESOURCE_STATE_COMMON; // 未追跡の場合はデフォルトを返す
    }

private:
    std::vector<std::unique_ptr<IRenderPass>> passes_;
    std::unordered_map<ID3D12Resource*, D3D12_RESOURCE_STATES> resourceStates_;
    std::unordered_map<ID3D12Resource*, D3D12_RESOURCE_STATES> finalResourceStates_;
    std::unordered_map<uint64_t, ID3D12Resource*> activeResourceByOffset_;

    std::unique_ptr<TransientResourceManager> transientResourceManager_;
    std::vector<ID3D12Resource*> transientPhysicalResources_;
    std::vector<std::unique_ptr<class RenderTexture>> transientRenderTextures_;
};
