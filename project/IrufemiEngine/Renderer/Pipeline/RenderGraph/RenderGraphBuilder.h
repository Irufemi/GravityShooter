#pragma once
#include <d3d12.h>
#include <vector>
#include <string>

// 一時リソースの仮想ハンドル
typedef uint32_t TransientResourceHandle;
constexpr TransientResourceHandle kInvalidHandle = 0xFFFFFFFF;

struct TransientResourceDesc {
    D3D12_RESOURCE_DESC desc;
    D3D12_CLEAR_VALUE clearValue;
    bool hasClearValue;
    std::string name;
};

/**
 * @class RenderGraphBuilder
 * @brief 各描画パスが必要とするリソース（テクスチャやレンダーターゲット等）の状態を登録するためのビルダークラス
 */
class RenderGraphBuilder {
public:
    struct ResourceUsage {
        ID3D12Resource* resource;
        D3D12_RESOURCE_STATES state;
        size_t passIndex; // この要求を出したパスのインデックス
    };

    struct TransientResourceUsage {
        TransientResourceHandle handle;
        D3D12_RESOURCE_STATES state;
        size_t passIndex;
    };

    /**
     * @brief CurrentPassIndex を設定する。
     * @param[in] index 設定する CurrentPassIndex の値
     */
    void SetCurrentPassIndex(size_t index) {
        currentPassIndex_ = index;
    }

    /**
     * @brief 特定のリソースがこのパスの実行時に指定したステートであることを要求する
     * @param resource 必須ステートを要求する対象のリソース
     * @param state 要求するステート
     */
    void RequireState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) {
        if (resource) {
            for (auto it = usages_.rbegin(); it != usages_.rend(); ++it) {
                if (it->passIndex != currentPassIndex_) {
                    break;
                }
                if (it->resource == resource) {
                    it->state = state;
                    return;
                }
            }
            usages_.push_back({resource, state, currentPassIndex_});
        }
    }

    /**
     * @brief RenderGraph内で完結する一時テクスチャ（エイリアシング対象）を作成する
     */
    TransientResourceHandle CreateTransientResource(const std::string& name, const D3D12_RESOURCE_DESC& desc,
                                                    const D3D12_CLEAR_VALUE* clearValue = nullptr) {
        TransientResourceHandle handle = static_cast<TransientResourceHandle>(transientDescs_.size());
        TransientResourceDesc tDesc{};
        tDesc.desc = desc;
        if (clearValue) {
            tDesc.clearValue = *clearValue;
            tDesc.hasClearValue = true;
        } else {
            tDesc.hasClearValue = false;
        }
        tDesc.name = name;
        transientDescs_.push_back(tDesc);
        return handle;
    }

    /**
     * @brief 名前から TransientResourceHandle を検索する
     */
    TransientResourceHandle FindTransientResource(const std::string& name) const {
        for (size_t i = 0; i < transientDescs_.size(); ++i) {
            if (transientDescs_[i].name == name) {
                return static_cast<TransientResourceHandle>(i);
            }
        }
        return kInvalidHandle;
    }

    /**
     * @brief 既に作成済みの場合は既存のハンドルを返し、なければ新しく作成する
     */
    TransientResourceHandle GetOrCreateTransientResource(const std::string& name, const D3D12_RESOURCE_DESC& desc,
                                                         const D3D12_CLEAR_VALUE* clearValue = nullptr) {
        auto existing = FindTransientResource(name);
        if (existing != kInvalidHandle) {
            return existing;
        }
        return CreateTransientResource(name, desc, clearValue);
    }

    /**
     * @brief 一時テクスチャのステート要求
     */
    void RequireTransientState(TransientResourceHandle handle, D3D12_RESOURCE_STATES state) {
        if (handle != kInvalidHandle) {
            for (auto it = transientUsages_.rbegin(); it != transientUsages_.rend(); ++it) {
                if (it->passIndex != currentPassIndex_) {
                    break;
                }
                if (it->handle == handle) {
                    it->state = state;
                    return;
                }
            }
            transientUsages_.push_back({handle, state, currentPassIndex_});
        }
    }

    /**
     * @brief Usages を取得する。
     * @return 取得された Usages
     */
    const std::vector<ResourceUsage>& GetUsages() const {
        return usages_;
    }
    /**
     * @brief TransientDescs を取得する。
     * @return 取得された TransientDescs
     */
    const std::vector<TransientResourceDesc>& GetTransientDescs() const {
        return transientDescs_;
    }
    /**
     * @brief TransientUsages を取得する。
     * @return 取得された TransientUsages
     */
    const std::vector<TransientResourceUsage>& GetTransientUsages() const {
        return transientUsages_;
    }

    /**
     * @brief Clear を実行する。
     */
    void Clear() {
        usages_.clear();
        transientDescs_.clear();
        transientUsages_.clear();
        currentPassIndex_ = 0;
    }

private:
    std::vector<ResourceUsage> usages_;
    std::vector<TransientResourceDesc> transientDescs_;
    std::vector<TransientResourceUsage> transientUsages_;
    size_t currentPassIndex_ = 0;
};
