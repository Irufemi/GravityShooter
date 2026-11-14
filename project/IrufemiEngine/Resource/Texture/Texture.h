#pragma once

#include <d3d12.h>
#include <string>
#include <wrl.h>
#include <cstdint>
#include <atomic>
#include "../../../externals/DirectXTex/DirectXTex.h"

namespace Irufemi {
class DirectXCommon;
class DescriptorPool;
}

/**
 * @class Texture
 * @brief 個別のテクスチャリソースを管理するクラス
 * @details DirectX 12 のリソース（ID3D12Resource）と SRV ハンドルを保持し、データのロードと初期化を行います。
 */
class Texture {
public:
    enum class LoadingStatus { Pending, Loading, Loaded, Failed };

    /** @name 静的メンバ設定 */
    ///@{
    static void SetDirectXCommon(Irufemi::DirectXCommon* dxCommon) {
        dxCommon_ = dxCommon;
    }
    /**
     * @brief SRV用ディスクリプタプールを設定する
     * @param[in] pool 設定する DescriptorPool のポインタ
     */
    static void SetDescriptorPool(Irufemi::DescriptorPool* pool) {
        srvPool_ = pool;
    }
    /**
     * @brief SRV用ディスクリプタプールを取得する
     * @return 取得された DescriptorPool ポインタ
     */
    static Irufemi::DescriptorPool* GetDescriptorPool() {
        return srvPool_;
    }
    /**
     * @brief デフォルト白テクスチャリソースを設定する
     * @param[in] resource 設定する ID3D12Resource ポインタ
     */
    static void SetWhiteTextureResource(ID3D12Resource* resource) {
        whiteResource_ = resource;
    }
    ///@}

    /**
     * @brief コンストラクタ
     */
    Texture();

    /**
     * @brief デストラクタ
     */
    ~Texture();

    /**
     * @brief ロード状態の取得
     */
    LoadingStatus GetStatus() const {
        return status_.load();
    }

    /**
     * @brief キューブマップかどうか
     */
    bool IsCubemap() const {
        return isCubemap_;
    }

    /**
     * @brief ファイルからテクスチャを初期化
     * @param[in] filePath 画像ファイルのパス
     */
    void Initialize(const std::string& filePath);

    /**
     * @brief メモリ上のピクセルデータからテクスチャを初期化
     * @param[in] name 識別名
     * @param[in] pixels ピクセルデータ（RGBA8想定）
     * @param[in] width 横幅
     * @param[in] height 縦幅
     */
    void InitializeFromMemory(const std::string& name, const uint32_t* pixels, uint32_t width, uint32_t height);

    /**
     * @brief メモリ上のピクセルデータからキューブマップを初期化
     * @param[in] name 識別名
     * @param[in] pixels ピクセルデータ（RGBA8想定、6面分連続）
     * @param[in] width 横幅
     * @param[in] height 縦幅
     */
    void InitializeCubeFromMemory(const std::string& name, const uint32_t* pixels, uint32_t width, uint32_t height);

    /**
     * @brief 外部で作成されたテクスチャリソースから初期化
     * @param[in] name 識別名
     * @param[in] resource ID3D12Resource
     * @param[in] srvIndex 割り当て済みのSRVインデックス
     * @param[in] srvHandle GPUディスクリプタハンドル
     */
    void InitializeFromExternalResource(const std::string& name, Microsoft::WRL::ComPtr<ID3D12Resource> resource,
                                        uint32_t srvIndex, D3D12_GPU_DESCRIPTOR_HANDLE srvHandle);

    /**
     * @brief GPU側のSRVハンドルを取得
     */
    const D3D12_GPU_DESCRIPTOR_HANDLE& GetTextureSrvHandleGPU() const {
        return textureSrvHandleGPU_;
    }

    /**
     * @brief [Bindless] DescriptorPool における SRV のインデックスを取得
     */
    uint32_t GetSrvIndex() const {
        return srvIndex_;
    }

    /**
     * @brief ScratchImage（CPU側の画像データ）を取得
     */
    const DirectX::ScratchImage* GetScratchImage() const {
        return &mipImages_;
    }

    /** @name サイズ取得・設定 */
    ///@{
    uint32_t GetWidth() const {
        return width_;
    }
    /**
     * @brief Height を取得する。
     * @return 取得された Height
     */
    uint32_t GetHeight() const {
        return height_;
    }
    /**
     * @brief Size を設定する。
     * @param[in] width 設定する Size の値
     * @param[in] height 設定する Size の値
     */
    void SetSize(uint32_t width, uint32_t height) {
        width_ = width;
        height_ = height;
    }
    ///@}

    /** @name 下位互換用（段階移行用） */
    ///@{
    static uint32_t GetStaticSRVIndex() {
        return index_;
    }
    /**
     * @brief AddStaticSRVIndex を実行する。
     */
    static void AddStaticSRVIndex() {
        index_++;
    }
    ///@}

protected:
    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU_{};
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU_{};
    std::string filePath_;
    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource_ = nullptr;
    DirectX::ScratchImage mipImages_;

    static uint32_t index_;          // 互換用
    uint32_t srvIndex_ = UINT32_MAX; // allocator で確保した index

    std::atomic<LoadingStatus> status_{LoadingStatus::Pending};

    bool isCubemap_ = false;

    static Irufemi::DirectXCommon* dxCommon_;
    static Irufemi::DescriptorPool* srvPool_;
    static ID3D12Resource* whiteResource_;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
};
