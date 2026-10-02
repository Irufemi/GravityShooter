#include "Renderer/ScreenCaptureManager.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "RHI/DirectX12/RenderTexture.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"
#include "Core/Utility/Log.h"
#include <iostream>

// DirectXTex
#include "../../externals/DirectXTex/DirectXTex.h"
#include "../../externals/DirectXTex/d3dx12.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include "Core/Utility/ErrorUtility.h"

#pragma comment(lib, "Pathcch.lib")

ScreenCaptureManager::ScreenCaptureManager() {}

ScreenCaptureManager::~ScreenCaptureManager() {
    Finalize();
}

void ScreenCaptureManager::Initialize(DirectXCommon* dxCommon, ThreadPool* threadPool) {
    dxCommon_ = dxCommon;
    threadPool_ = threadPool;
    isEncoding_ = false;
}

void ScreenCaptureManager::Finalize() {
    // スレッドプールのタスクが完了するのを待つ (簡易的)
    while (isEncoding_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    colorCopyBuffer_.Reset();
    depthCopyBuffer_.Reset();
}

void ScreenCaptureManager::Update() {
    // 毎フレーム呼ばれるが、現在特にメインスレッドで非同期完了を監視する必要はない
    // (ThreadPool のタスク内で完結させているため)
}

bool ScreenCaptureManager::RequestCapture(const std::wstring& filePath, ScreenCaptureType type,
                                          std::function<void()> onComplete) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    if (isEncoding_) {
        return false; // 処理中はリクエストを破棄または待機 (ここでは簡単のため破棄)
    }

    ScreenCaptureRequest req;
    req.filePath = filePath;
    req.type = type;
    req.isMetadataRequested = false;
    req.isAlphaRequested = false;
    req.isDepthRequested = false;
    req.onComplete = onComplete;

    pendingRequests_.push_back(req);
    return true;
}

bool ScreenCaptureManager::RequestCaptureWithMetadata(const std::wstring& filePath, ScreenCaptureType type,
                                                      std::function<void()> onComplete) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    if (isEncoding_) {
        return false;
    }

    ScreenCaptureRequest req;
    req.filePath = filePath;
    req.type = type;
    req.isMetadataRequested = true;
    req.isAlphaRequested = false;
    req.isDepthRequested = false;
    req.onComplete = onComplete;

    pendingRequests_.push_back(req);
    return true;
}

bool ScreenCaptureManager::RequestCaptureWithAlpha(const std::wstring& filePath, std::function<void()> onComplete) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    if (isEncoding_) {
        return false;
    }

    ScreenCaptureRequest req;
    req.filePath = filePath;
    req.type = ScreenCaptureType::SceneOnly;
    req.isMetadataRequested = false;
    req.isAlphaRequested = true;
    req.isDepthRequested = false;
    req.onComplete = onComplete;

    pendingRequests_.push_back(req);
    return true;
}

bool ScreenCaptureManager::RequestCaptureDepth(const std::wstring& filePath, std::function<void()> onComplete) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    if (isEncoding_) {
        return false;
    }

    ScreenCaptureRequest req;
    req.filePath = filePath;
    req.type = ScreenCaptureType::SceneOnly;
    req.isMetadataRequested = false;
    req.isAlphaRequested = false;
    req.isDepthRequested = true;
    req.onComplete = onComplete;

    pendingRequests_.push_back(req);
    return true;
}

bool ScreenCaptureManager::IsCaptureWithAlphaRequested() const {
    for (const auto& req : pendingRequests_) {
        if (req.isAlphaRequested) {
            return true;
        }
    }
    return false;
}

void ScreenCaptureManager::RecordMetadata(IrufemiEngine* engine) {
    if (!engine) {
        return;
    }

    std::stringstream ss;
    ss << "{\n";

    if (auto camManager = engine->GetCameraManager()) {
        if (auto cam = camManager->GetActiveCamera()) {
            Irufemi::Vector3 pos = cam->GetTranslate();
            Irufemi::Vector3 rot = cam->GetRotate();
            ss << "  \"camera\": {\n";
            ss << "    \"position\": [" << pos.x << ", " << pos.y << ", " << pos.z << "],\n";
            ss << "    \"rotation\": [" << rot.x << ", " << rot.y << ", " << rot.z << "]\n";
            ss << "  },\n";
        }
    }

    ss << "  \"engine\": {\n";
    ss << "    \"gameTime\": " << engine->GetGameTime() << ",\n";
    ss << "    \"resolution\": [" << engine->GetClientWidth() << ", " << engine->GetClientHeight() << "]\n";
    ss << "  }\n";
    ss << "}\n";

    currentMetadataJson_ = ss.str();
}

void ScreenCaptureManager::OnPreUIDraw(ID3D12GraphicsCommandList* commandList, RenderTexture* mainRenderTexture) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    if (pendingRequests_.empty() || isEncoding_) {
        return;
    }

    for (size_t i = 0; i < pendingRequests_.size(); ++i) {
        auto& req = pendingRequests_[i];
        if (req.type == ScreenCaptureType::SceneOnly && !req.isDepthRequested) {

            if (!colorCopyBuffer_) {
                auto desc = mainRenderTexture->GetResource()->GetDesc();
                desc.Flags = D3D12_RESOURCE_FLAG_NONE;
                CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
                HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                                             D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                                             IID_PPV_ARGS(&colorCopyBuffer_));
                IRUFEMI_ASSERT(SUCCEEDED(hr) && "Failed to create color copy buffer for screen capture!");
                if (FAILED(hr)) {
                    return;
                }
            }

            DirectXUtils::TransitionBarrier(commandList, mainRenderTexture->GetResource(),
                                            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
            DirectXUtils::TransitionBarrier(commandList, colorCopyBuffer_.Get(), D3D12_RESOURCE_STATE_COMMON,
                                            D3D12_RESOURCE_STATE_COPY_DEST);

            commandList->CopyResource(colorCopyBuffer_.Get(), mainRenderTexture->GetResource());

            DirectXUtils::TransitionBarrier(commandList, colorCopyBuffer_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                            D3D12_RESOURCE_STATE_COMMON);
            DirectXUtils::TransitionBarrier(commandList, mainRenderTexture->GetResource(),
                                            D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);

            if (req.isMetadataRequested) {
                RecordMetadata(dxCommon_->GetEngine());
            }

            ExecuteCopyTask(colorCopyBuffer_.Get(), D3D12_RESOURCE_STATE_COMMON, req);

            pendingRequests_.erase(pendingRequests_.begin() + i);
            break;
        }
    }
}

void ScreenCaptureManager::OnPostUIDraw(ID3D12GraphicsCommandList* commandList, ID3D12Resource* backBuffer) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    if (pendingRequests_.empty() || isEncoding_) {
        return;
    }

    for (size_t i = 0; i < pendingRequests_.size(); ++i) {
        auto& req = pendingRequests_[i];
        if (req.type == ScreenCaptureType::WithUI) {

            if (!colorCopyBuffer_) {
                auto desc = backBuffer->GetDesc();
                desc.Flags = D3D12_RESOURCE_FLAG_NONE;
                D3D12_HEAP_PROPERTIES heapProps{};
                heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
                dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                                D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                                IID_PPV_ARGS(&colorCopyBuffer_));
            }

            DirectXUtils::TransitionBarrier(commandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                            D3D12_RESOURCE_STATE_COPY_SOURCE);
            DirectXUtils::TransitionBarrier(commandList, colorCopyBuffer_.Get(), D3D12_RESOURCE_STATE_COMMON,
                                            D3D12_RESOURCE_STATE_COPY_DEST);

            commandList->CopyResource(colorCopyBuffer_.Get(), backBuffer);

            DirectXUtils::TransitionBarrier(commandList, colorCopyBuffer_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                            D3D12_RESOURCE_STATE_COMMON);
            DirectXUtils::TransitionBarrier(commandList, backBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE,
                                            D3D12_RESOURCE_STATE_RENDER_TARGET);

            if (req.isMetadataRequested) {
                RecordMetadata(dxCommon_->GetEngine());
            }

            ExecuteCopyTask(colorCopyBuffer_.Get(), D3D12_RESOURCE_STATE_COMMON, req);

            pendingRequests_.erase(pendingRequests_.begin() + i);
            break;
        }
    }
}

void ScreenCaptureManager::OnPostDepthDraw(ID3D12GraphicsCommandList* commandList, ID3D12Resource* depthBuffer) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    if (pendingRequests_.empty() || isEncoding_) {
        return;
    }

    for (size_t i = 0; i < pendingRequests_.size(); ++i) {
        auto& req = pendingRequests_[i];
        if (req.isDepthRequested) {

            if (!depthCopyBuffer_) {
                auto desc = depthBuffer->GetDesc();
                desc.Flags = D3D12_RESOURCE_FLAG_NONE;
                desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
                D3D12_HEAP_PROPERTIES heapProps{};
                heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
                dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                                D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                                IID_PPV_ARGS(&depthCopyBuffer_));
            }

            DirectXUtils::TransitionBarrier(commandList, depthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                            D3D12_RESOURCE_STATE_COPY_SOURCE);
            DirectXUtils::TransitionBarrier(commandList, depthCopyBuffer_.Get(), D3D12_RESOURCE_STATE_COMMON,
                                            D3D12_RESOURCE_STATE_COPY_DEST);

            commandList->CopyResource(depthCopyBuffer_.Get(), depthBuffer);

            DirectXUtils::TransitionBarrier(commandList, depthCopyBuffer_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                            D3D12_RESOURCE_STATE_COMMON);
            DirectXUtils::TransitionBarrier(commandList, depthBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE,
                                            D3D12_RESOURCE_STATE_DEPTH_WRITE);

            ExecuteCopyTask(depthCopyBuffer_.Get(), D3D12_RESOURCE_STATE_COMMON, req);

            pendingRequests_.erase(pendingRequests_.begin() + i);
            break;
        }
    }
}

void ScreenCaptureManager::ExecuteCopyTask(ID3D12Resource* sourceResource, D3D12_RESOURCE_STATES currentState,
                                           const ScreenCaptureRequest& req) {
    isEncoding_ = true;

    threadPool_->Enqueue([this, sourceResource, currentState, req]() {
        std::filesystem::path path(req.filePath);
        if (path.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
        }

        DirectX::ScratchImage image;
        HRESULT hr = DirectX::CaptureTexture(dxCommon_->GetCommandQueue(), sourceResource, false, image, currentState,
                                             currentState);

        if (SUCCEEDED(hr)) {
            DirectX::SaveToWICFile(image.GetImages(), image.GetImageCount(), DirectX::WIC_FLAGS_NONE,
                                   DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), req.filePath.c_str());

            if (req.isMetadataRequested && !currentMetadataJson_.empty()) {
                GenerateMetadataJson(req.filePath);
            }

            if (req.onComplete) {
                req.onComplete();
            }
        } else {
            /**
             * @brief エディタのコンソールパネルにも出力するため、Log::OutPutLog を使用
             */
            Log::OutPutLog(std::cerr, "[ScreenCaptureManager] Failed to capture texture.");
        }

        isEncoding_ = false;
    });
}

void ScreenCaptureManager::GenerateMetadataJson(const std::wstring& imagePath) {
    // 拡張子を .json に変更
    std::wstring jsonPath = imagePath;
    size_t dotPos = jsonPath.find_last_of(L".");
    if (dotPos != std::wstring::npos) {
        jsonPath = jsonPath.substr(0, dotPos) + L".json";
    } else {
        jsonPath += L".json";
    }

    std::ofstream ofs(jsonPath);
    if (ofs) {
        ofs << currentMetadataJson_;
        ofs.close();
    }
}
