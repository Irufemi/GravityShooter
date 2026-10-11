#pragma once

#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

/**
 * @class NebulaSeaFloorComponent
 * @brief 星雲ガス海（Nebula Sea Floor）3Dボリュメトリック描画コンポーネント
 * @details Y=0.0f を基準高度とし、タイトル星雲 (CosmicNebula) の5色グラデーション（漆黒・ティール・シアン・白・アメジスト）を継承した
 *          濃密な星雲雲海を3Dワールド空間に展開します。カメラの3D前進・旋回と完全同期するワールドXZデュアルフロー、
 *          3D Beer-Lambert 則による体積光吸収、柱などの環境物接地ソフト交差、およびスカイドーム大気フォグ同調を行います。
 */
class NebulaSeaFloorComponent : public Component {
public: // 定数バッファ構造体 (HLSL: register b6 / RootSlot::Special)
    struct alignas(256) NebulaSeaParams {
        float time = 0.0f;                      //!< 経過時間
        float railSpeed = 35.0f;                //!< レール前進速度
        float surfaceY = 0.0f;                  //!< 雲海上面高度基準（Y=0.0f / 環境物接地ライン）
        float volumeDepth = 35.0f;              //!< 雲海内部ボリューム深度（35.0f）

        float fadeStartDistance = 450.0f;       //!< 大気フォグ開始距離
        float fadeEndDistance = 2500.0f;        //!< 大気フォグ終了距離（地平線）
        float softIntersectionRange = 4.0f;     //!< 柱との深度ソフト交差幅
        float densityScale = 1.0f;              //!< 雲密度倍率

        Irufemi::Vector4 colDeepSpace{0.003f, 0.006f, 0.014f, 1.0f};  //!< 深宇宙の漆黒（深淵）
        Irufemi::Vector4 colTeal{0.03f, 0.40f, 0.44f, 1.0f};          //!< 澄んだディープティール
        Irufemi::Vector4 colCyan{0.12f, 0.78f, 0.88f, 1.0f};          //!< 鮮烈なエレクトリックシアン
        Irufemi::Vector4 colWhite{0.85f, 0.96f, 1.00f, 1.0f};         //!< 純白ハイライト光条
        Irufemi::Vector4 colAmethyst{0.24f, 0.08f, 0.35f, 1.0f};      //!< 幻想的なアメジストパープル
        Irufemi::Vector4 colHorizonSky{0.55f, 0.52f, 0.72f, 1.0f};    //!< 地平線スカイドーム色
        Irufemi::Vector4 cameraSnapXZ{0.0f, 0.0f, 0.0f, 0.0f};        //!< カメラXZ追従スナップ座標 (xy: XZ位置, zw: 予備)
    };

public: // 頂点構造体
    struct Vertex {
        Irufemi::Vector3 position;
        Irufemi::Vector3 normal;
        Irufemi::Vector2 texcoord;
    };

public: // メンバ関数
    NebulaSeaFloorComponent() = default;
    ~NebulaSeaFloorComponent() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;

    std::string GetComponentName() const override {
        return "NebulaSeaFloorComponent";
    }

    void OnRegisterProperties() override;

private:
    void CreateConstantBuffer();
    void CreateMeshGrid();

private: // メンバ変数
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    NebulaSeaParams* mappedParams_ = nullptr;
    NebulaSeaParams params_{};

    // カメラ追従グリッドメッシュリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    uint32_t indexCount_ = 0;

    float totalTime_ = 0.0f;
    Irufemi::Vector3 lastCameraPos_{0.0f, 15.0f, 0.0f};
};
