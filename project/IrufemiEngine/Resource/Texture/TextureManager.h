#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <d3d12.h>
#include <wrl.h>
#include "Resource/Texture/Texture.h"
#include "../../../externals/DirectXTex/DirectXTex.h"
#include "Core/System/ThreadPool.h"
#include "Core/System/TaskGroup.h"
#include <atomic>
#include <future>
#include <type_traits>
#include <functional>
#include "Core/System/ResourceCachePool.h"

// 前方宣言
/**
 * @namespace DirectX
 * @brief DirectXTex および Microsoft 公式 DirectX 補助ライブラリの名前空間
 */
namespace DirectX {
class ScratchImage;
}

class DirectXCommon;

/**
 * @class TextureManager
 * @brief テクスチャのロードと管理を一括して行うマネージャクラス
 * @details テクスチャの重複ロードを防ぐためのキャッシュ機構を持ち、IDによる指定でSRVハンドルを提供します。
 */
class TextureManager {
public:
    /**
     * @brief コンストラクタ
     */
    TextureManager() = default;

    /**
     * @brief デストラクタ
     */
    ~TextureManager() = default;

    /**
     * @brief 初期化
     * @param[in] dxCommon DirectX 12 基礎クラスのポインタ
     */
    void Initialize(DirectXCommon* dxCommon);

    /**
     * @brief 指定フォルダ内のすべての画像をロードする
     * @param[in] folderPath ロード対象のフォルダパス
     */
    void LoadAllFromFolder(const std::string& folderPath);

    /**
     * @brief テクスチャ名からテクスチャをロードし、リソースハンドルを取得する
     * @details コンポーネントの初期化時などに呼び出し、返されたハンドルを保持してください。
     * @param[in] name ファイルパスまたは識別名
     * @return リソースハンドル
     */
    ResourceHandle LoadTexture(const std::string& name);

    /**
     * @brief 外部で生成されたテクスチャを登録し、リソースハンドルを発行する
     * @param[in] name 識別名
     * @param[in] resource ID3D12Resource
     * @param[in] srvIndex 割り当て済みのSRVインデックス
     * @param[in] srvHandle GPUディスクリプタハンドル
     * @return リソースハンドル
     */
    ResourceHandle RegisterExternalTexture(const std::string& name, Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                                           uint32_t srvIndex, D3D12_GPU_DESCRIPTOR_HANDLE srvHandle);

    /**
     * @brief テクスチャのリソースハンドルを解放する（参照カウントを減らす）
     * @details コンポーネントの破棄時などに必ず呼び出してください。
     * @param[in] handle リソースハンドル
     */
    void ReleaseTexture(ResourceHandle handle);

    /**
     * @brief リソースハンドルから描画用のGPU側のSRVハンドルを取得する
     * @details 毎フレームの描画時に呼び出します。ロード中やパージ済みの場合は自動的に白テクスチャを返します。
     * @param[in] handle リソースハンドル
     * @return GPU側のSRVハンドル
     */
    D3D12_GPU_DESCRIPTOR_HANDLE Resolve(ResourceHandle handle) const;

    /**
     * @brief [Bindless] リソースハンドルからSRVインデックスを取得する
     * @details Bindlessアクセス用のインデックスを取得します。
     * @param[in] handle リソースハンドル
     * @return SRVのインデックス
     */
    uint32_t GetSrvIndex(ResourceHandle handle) const;

    /**
     * @brief ハンドルからGPUのSRVハンドル(DescriptorHandle)を解決する（キューブマップ用）
     * @details 未ロードまたは無効な場合は白のキューブマップを返します。
     * @param[in] handle リソースハンドル
     * @return GPUが参照可能なディスクリプタハンドル
     */
    D3D12_GPU_DESCRIPTOR_HANDLE ResolveCubeMap(ResourceHandle handle) const;

    /**
     * @brief ハンドルからテクスチャオブジェクト自体を取得する
     * @param[in] handle リソースハンドル
     * @return Textureオブジェクトのポインタ（無効な場合はnullptr）
     */
    const Texture* GetTextureObject(ResourceHandle handle) const;

    /**
     * @brief テクスチャ名からCPU側の画像データを取得
     * @param[in] name ファイルパスまたは識別名
     * @return ScratchImageへのポインタ
     */
    const DirectX::ScratchImage* GetScratchImage(const std::string& name) const;

    /**
     * @brief ロード済みのテクスチャ名一覧を取得
     */
    std::vector<std::string> GetTextureNames() const;

    /**
     * @brief デバッグ表示用にロード済みのテクスチャ名一覧を取得（キューブマップ除外、ソート済み）
     */
    std::vector<std::string> GetTextureNamesForDebug() const;

    /**
     * @brief 現在割り当てられているすべてのSRVハンドルを取得する（フリーリスト再構築用）
     */
    std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> GetAllAllocatedSrvHandles() const;

    /**
     * @brief デバッグ表示用にロード済みのキューブマップ名一覧を取得（キューブマップのみ、ソート済み）
     */
    std::vector<std::string> GetCubeMapNamesForDebug() const;

    /**
     * @brief フォールバック用のダミー白テクスチャを生成する
     */
    void CreateWhiteDummyTexture();

    /**
     * @brief フォールバック用のダミー白CubeMapを生成する
     */
    void CreateWhiteCubeMap();

    /**
     * @brief テクスチャのピクセルサイズを取得
     * @param[in] name 識別名
     * @param[out] outWidth 幅の出力先
     * @param[out] outHeight 高さの出力先
     * @return 取得成功なら true
     */
    bool GetTextureSize(const std::string& name, uint32_t& outWidth, uint32_t& outHeight) const;

    /**
     * @brief 白テクスチャのGPUハンドルを取得
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetWhiteTextureHandle() const {
        return whiteTextureHandle_;
    }

    /**
     * @brief [Bindless] 白テクスチャのSRVインデックスを取得
     */
    uint32_t GetWhiteTextureSrvIndex() const;

    /**
     * @brief 白CubeMapテクスチャのGPUハンドルを取得
     */
    D3D12_GPU_DESCRIPTOR_HANDLE GetWhiteCubeMapHandle() const {
        return whiteCubeMapHandle_;
    }

    /**
     * @brief [Bindless] 白CubeMapテクスチャのSRVインデックスを取得
     */
    uint32_t GetWhiteCubeMapSrvIndex() const;

    /**
     * @brief テクスチャのロード状態を取得する
     * @param[in] name ファイルパスまたは識別名
     * @return ロード状態。存在しない場合は Failed を返す
     */
    Texture::LoadingStatus GetTextureStatus(const std::string& name) const;

    /**
     * @brief キューブマップかどうかを取得
     */
    bool IsCubeMap(const std::string& name) const;

    /**
     * @brief すべてのロードタスク（背景タスクを含む）が完了しているかを取得
     */
    bool IsAllLoaded() const {
        return taskGroup_->IsAllDone() && backgroundTaskGroup_->IsAllDone();
    }

    /**
     * @brief 非同期タスクの実行（シーンの状態による自動判定）
     */
    template <class F, class... Args>
    auto EnqueueTask(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>> {
        bool isCritical = IsCurrentSceneInitializing();
        return EnqueueTask(isCritical, std::forward<F>(f), std::forward<Args>(args)...);
    }

    /**
     * @brief 優先度を指定して非同期タスクを実行
     */
    template <class F, class... Args>
    auto EnqueueTask(bool isCritical, F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>> {
        auto& group = isCritical ? taskGroup_ : backgroundTaskGroup_;
        return threadPool_->Enqueue(group, std::forward<F>(f), std::forward<Args>(args)...);
    }

    /**
     * @brief 白テクスチャのリソースを取得
     */
    ID3D12Resource* GetWhiteTextureResource() const {
        return whiteTextureResource_.Get();
    }

private:
    /**
     * @brief 現在のシーンが初期化中かどうかを判定する
     */
    bool IsCurrentSceneInitializing() const;
    DirectXCommon* dxCommon_ = nullptr;

    ResourceCachePool texturePool_;
    mutable std::unordered_map<std::string, ResourceHandle> nameToHandleMap_;
    std::vector<std::shared_ptr<Texture>> textureResources_;

    mutable std::mutex mutex_;

    std::unique_ptr<ThreadPool> threadPool_;
    std::shared_ptr<TaskGroup> taskGroup_;           ///< 重要タスク用
    std::shared_ptr<TaskGroup> backgroundTaskGroup_; ///< バックグラウンド用

    // フォールバック白テクスチャ
    std::unique_ptr<Texture> whiteTexture_;
    Microsoft::WRL::ComPtr<ID3D12Resource> whiteTextureResource_;
    D3D12_GPU_DESCRIPTOR_HANDLE whiteTextureHandle_{0};

    // フォールバック白CubeMap
    std::unique_ptr<Texture> whiteCubeMap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> whiteCubeMapResource_;
    D3D12_GPU_DESCRIPTOR_HANDLE whiteCubeMapHandle_{0};
};