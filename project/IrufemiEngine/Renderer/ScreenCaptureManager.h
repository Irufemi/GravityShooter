#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>
#include "Core/System/ThreadPool.h"
#include "Core/Utility/Synchronized.h"

// 前方宣言
class DirectXCommon;
class IrufemiEngine;
class RenderTexture;

enum class ScreenCaptureType {
    SceneOnly, // ポストプロセス後、UI描画前の純粋なシーン
    WithUI     // ImGui等を含む最終的な画面
};

struct ScreenCaptureRequest {
    std::wstring filePath;
    ScreenCaptureType type;
    bool isMetadataRequested;
    bool isAlphaRequested;
    bool isDepthRequested;
    std::function<void()> onComplete;
};

class ScreenCaptureManager {
public:
    ScreenCaptureManager();
    ~ScreenCaptureManager();

    /**
     * @brief Initialize を実行する。
     */
    void Initialize(DirectXCommon* dxCommon, ThreadPool* threadPool);
    /**
     * @brief Finalize を実行する。
     */
    void Finalize();

    /**
     * @brief Update を実行する。
     */
    void Update(); // 毎フレームの完了チェックなど

    // キャプチャリクエストAPI
    bool RequestCapture(const std::wstring& filePath, ScreenCaptureType type,
                        std::function<void()> onComplete = nullptr);
    bool RequestCaptureWithMetadata(const std::wstring& filePath, ScreenCaptureType type,
                                    std::function<void()> onComplete = nullptr);
    bool RequestCaptureWithAlpha(const std::wstring& filePath, std::function<void()> onComplete = nullptr);
    bool RequestCaptureDepth(const std::wstring& filePath, std::function<void()> onComplete = nullptr);

    // 内部用のフック呼び出し（描画パイプラインから呼ばれる）
    /**
     * @brief OnPreUIDraw を実行する。
     */
    void OnPreUIDraw(ID3D12GraphicsCommandList* commandList, RenderTexture* mainRenderTexture);
    /**
     * @brief OnPostUIDraw を実行する。
     */
    void OnPostUIDraw(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer);
    /**
     * @brief OnPostDepthDraw を実行する。
     */
    void OnPostDepthDraw(ID3D12GraphicsCommandList* commandList, ID3D12Resource* depthBuffer);

    // スカイボックス等を除外するためのフラグ取得
    /**
     * @brief IsCaptureWithAlphaRequested かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsCaptureWithAlphaRequested() const;

    // メタデータ生成用
    /**
     * @brief RecordMetadata を実行する。
     */
    void RecordMetadata(IrufemiEngine* engine);

private:
    /**
     * @brief ExecuteCopyTask を実行する。
     */
    void ExecuteCopyTask(ID3D12Resource* sourceResource, D3D12_RESOURCE_STATES currentState,
                         const ScreenCaptureRequest& req);
    /**
     * @brief GenerateMetadataJson を実行する。
     */
    void GenerateMetadataJson(const std::wstring& imagePath);

    /**
     * @brief コピー用バッファが存在し、コピー元と一致しているか確認し、必要なら再生成する。
     */
    bool EnsureCopyBuffer(Microsoft::WRL::ComPtr<ID3D12Resource>& buffer, ID3D12Resource* sourceResource,
                          DXGI_FORMAT overrideFormat = DXGI_FORMAT_UNKNOWN);

private:
    DirectXCommon* dxCommon_ = nullptr;
    ThreadPool* threadPool_ = nullptr;

    Irufemi::Synchronized<std::vector<ScreenCaptureRequest>> pendingRequests_;

    // キャプチャ中の多重実行を防ぐためのフラグやバッファ管理
    std::atomic<bool> isEncoding_{false};

    // 中間コピー用バッファ (解像度やタイミングが異なるため分離管理)
    Microsoft::WRL::ComPtr<ID3D12Resource> colorCopyBufferWithoutUI_;
    Microsoft::WRL::ComPtr<ID3D12Resource> colorCopyBufferWithUI_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthCopyBuffer_;

    // 一時的に記録しておくメタデータ内容
    std::string currentMetadataJson_;
};
