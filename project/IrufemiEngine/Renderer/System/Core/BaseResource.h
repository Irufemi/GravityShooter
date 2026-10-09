#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <memory>

#include "Renderer/System/Core/MultiBufferSyncState.h"

class DirectXCommon;

class BaseResource : public MultiBufferSyncState {
public:
    BaseResource() = default;
    explicit BaseResource(DirectXCommon* dxCommon) : dxCommon_(dxCommon) {}
    virtual ~BaseResource() = default;

    /**
     * @brief 個別インスタンス用の DirectXCommon を設定する
     * @param[in] dxCommon 設定する DirectXCommon インスタンスのポインタ
     */
    void SetDirectXCommonInstance(DirectXCommon* dxCommon) {
        dxCommon_ = dxCommon;
    }

    /**
     * @brief DirectXCommon を取得する（個別インスタンス優先、未設定時はデフォルトの静的インスタンスへフォールバック）
     * @return 利用可能な DirectXCommon のポインタ
     */
    DirectXCommon* GetDxCommon() const {
        return dxCommon_ ? dxCommon_ : defaultDxCommon_;
    }

    /**
     * @brief 全リソース共通のデフォルト DirectXCommon を設定する
     * @param[in] dxCommon 設定する DirectXCommon インスタンスのポインタ
     */
    static void SetDirectXCommon(DirectXCommon* dxCommon) {
        defaultDxCommon_ = dxCommon;
    }

    /**
     * @brief 全リソース共通のデフォルト DirectXCommon を取得する
     * @return デフォルトの DirectXCommon ポインタ
     */
    static DirectXCommon* GetDirectXCommon() {
        return defaultDxCommon_;
    }

    /**
     * @brief GPU リソース（頂点・インデックス・定数バッファ等）を生成・初期化する純粋仮想関数
     */
    virtual void CreateResource() = 0;

    /**
     * @brief GPU リソースを CPU メモリアドレス空間にマップする純粋仮想関数
     */
    virtual void Map() = 0;

    /**
     * @brief GPU リソースの CPU マッピングを解除する純粋仮想関数
     */
    virtual void Unmap() = 0;

protected:
    DirectXCommon* dxCommon_ = nullptr;
    static DirectXCommon* defaultDxCommon_;
};
