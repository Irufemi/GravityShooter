#pragma once

#include <d3d12.h>
#include <wrl.h>
#include "RHI/DirectX12/RootSignatureConfig.h"

namespace Irufemi {

class DXRootSignatureManager {
public:
    DXRootSignatureManager() = default;
    ~DXRootSignatureManager() {
        Finalize();
    }

    /**
     * @brief 初期化処理。描画用およびコンピュート用の各種ルートシグネチャを生成
     * @param[in] device D3D12デバイス
     */
    void Initialize(ID3D12Device* device);

    /**
     * @brief 解放処理。保持しているルートシグネチャリソースをリセット
     */
    void Finalize();

    /**
     * @brief 描画用のルートシグネチャを取得
     */
    ID3D12RootSignature* GetGraphicsRootSignature() const {
        return graphicsRootSignature_.Get();
    }

    /**
     * @brief コンピュートシェーダ用のルートシグネチャを取得
     */
    ID3D12RootSignature* GetComputeRootSignature() const {
        return computeRootSignature_.Get();
    }

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> graphicsRootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_ = nullptr;
};

} // namespace Irufemi
