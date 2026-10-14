#pragma once

#include "Renderer/Data/VertexData.h"
#include "Core/Type/PrimitiveType.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <d3d12.h>
#include <wrl.h>

struct PrimitiveData {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
};

struct PrimitiveResource {
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    uint32_t indexCount;
};

/**
 * @struct RingParams
 * @brief Ring（ドーナツ型・三日月型）形状生成のための詳細パラメータ構造体
 */
struct RingParams {
    float innerRadius = 0.2f;                               ///< 内径
    float startOuterRadius = 1.0f;                          ///< 開始地点の外径
    float endOuterRadius = 1.0f;                            ///< 終了地点の外径
    float startAngle = 0.0f;                                ///< 開始角度(度数法)
    float endAngle = 360.0f;                                ///< 終了角度(度数法)
    uint32_t segments = 32;                                 ///< 分割数
    bool verticalUV = false;                                ///< UVをV方向に変更するかどうか
    Irufemi::Vector4 innerColor = {1.0f, 1.0f, 1.0f, 1.0f}; ///< 内側の頂点カラー
    Irufemi::Vector4 outerColor = {1.0f, 1.0f, 1.0f, 1.0f}; ///< 外側の頂点カラー
    float startAlpha = 0.0f;                                ///< 開始地点のアルファ値（フェード用）
    float endAlpha = 0.0f;                                  ///< 終了地点のアルファ値（フェード用）
    float fadeRangeAngle = 0.0f;                            ///< フェードにかかる角度の範囲(度数法)
};

/**
 * @class PrimitiveManager
 * @brief プリミティブ形状（球、立方体、平面等）のメッシュデータを管理するクラス
 * @details 頻繁に使用される標準的な形状の CPU データおよび GPU リソース（頂点/インデックスバッファ）をキャッシュし、
 *          効率的な再利用を可能にします。
 */
class PrimitiveManager {

public:
    /** @name キャッシュデータの取得 */
    ///@{
    /**
     * @brief 指定した形状のプリミティブデータを取得する（CPUキャッシュ）
     * @param[in] type 形状のタイプ
     * @return 頂点とインデックスのリストを含む PrimitiveData
     */
    const PrimitiveData& GetPrimitiveData(Irufemi::PrimitiveType type);

    /**
     * @brief 指定した形状の頂点データのみを取得する
     */
    const std::vector<VertexData>& GetVertices(Irufemi::PrimitiveType type);

    /**
     * @brief 指定した形状の GPU リソース（BufferView）を取得する（GPUキャッシュ）
     * @details 標準設定（サイズ1.0等）のバッファを共有します。
     */
    const PrimitiveResource& GetStandardResource(Irufemi::PrimitiveType type);

    /**
     * @brief シリンダー形状専用の GPU リソースを取得する（蓋の有無を考慮したGPUキャッシュ）
     * @param[in] hasTop 上蓋を描画するか
     * @param[in] hasBottom 下蓋を描画するか
     * @return 蓋の有無に対応する PrimitiveResource
     */
    const PrimitiveResource& GetCylinderResource(bool hasTop, bool hasBottom);
    ///@}

    /** @name 形状生成メソッド（静的） */
    ///@{
    /**
     * @brief UV球（Sphere）メッシュデータを生成する
     * @param[in] radius 球の半径
     * @param[in] subdivision 緯度・経度方向の分割数
     * @return 生成された球の頂点およびインデックスデータ
     */
    static PrimitiveData CreateSphere(float radius, uint32_t subdivision);

    /**
     * @brief 直方体・立方体（Cube / Box）メッシュデータを生成する
     * @param[in] width X軸方向の幅
     * @param[in] height Y軸方向の高さ
     * @param[in] depth Z軸方向の奥行き
     * @return 生成された立方体の頂点およびインデックスデータ
     */
    static PrimitiveData CreateCube(float width, float height, float depth);

    /**
     * @brief 上下面の半径が異なる円柱・円錐台（Cylinder / Truncated Cone）メッシュデータを生成する
     * @param[in] bottomRadius 底面の半径
     * @param[in] topRadius 上面の半径
     * @param[in] height 円柱の高さ
     * @param[in] segments 円周方向の分割数
     * @param[in] hasTop 上面のフタを生成するかどうか
     * @param[in] hasBottom 底面のフタを生成するかどうか
     * @param[in] centered 原点を円柱の中心（高さの1/2）に配置するか（falseの場合は底面が原点）
     * @return 生成された円柱の頂点およびインデックスデータ
     */
    static PrimitiveData CreateCylinder(float bottomRadius, float topRadius, float height, uint32_t segments,
                                        bool hasTop = true, bool hasBottom = true, bool centered = true);

    /**
     * @brief 上下面の半径が等しい等径円柱（Cylinder）メッシュデータを生成する
     * @param[in] radius 円柱の半径
     * @param[in] height 円柱の高さ
     * @param[in] segments 円周方向の分割数
     * @param[in] hasTop 上面のフタを生成するかどうか
     * @param[in] hasBottom 底面のフタを生成するかどうか
     * @return 生成された円柱の頂点およびインデックスデータ
     */
    static PrimitiveData CreateCylinder(float radius, float height, uint32_t segments, bool hasTop = true,
                                        bool hasBottom = true);

    /**
     * @brief 円錐（Cone）メッシュデータを生成する
     * @param[in] radius 底面の半径
     * @param[in] height 円錐の高さ
     * @param[in] segments 円周方向の分割数
     * @return 生成された円錐の頂点およびインデックスデータ
     */
    static PrimitiveData CreateCone(float radius, float height, uint32_t segments);

    /**
     * @brief ドーナツ型（Torus）メッシュデータを生成する
     * @param[in] majorRadius トーラス中心からチューブ中心までの大半径
     * @param[in] minorRadius チューブ自体の小半径
     * @param[in] majorSegments 大円周方向の分割数
     * @param[in] minorSegments 小円周方向の分割数
     * @return 生成されたトーラスの頂点およびインデックスデータ
     */
    static PrimitiveData CreateTorus(float majorRadius, float minorRadius, uint32_t majorSegments,
                                     uint32_t minorSegments);

    /**
     * @brief 正二十面体分割による均一球（IcoSphere）メッシュデータを生成する
     * @param[in] radius 球の半径
     * @param[in] subdivision 再帰分割レベル
     * @return 生成されたアイコスフィアの頂点およびインデックスデータ
     */
    static PrimitiveData CreateIcoSphere(float radius, uint32_t subdivision);

    /**
     * @brief 格子状の平面グリッド（Grid）メッシュデータを生成する
     * @param[in] width X軸方向の幅
     * @param[in] height Z軸方向の奥行き/高さ
     * @param[in] xSegments X方向の分割数
     * @param[in] ySegments Y/Z方向の分割数
     * @return 生成されたグリッドの頂点およびインデックスデータ
     */
    static PrimitiveData CreateGrid(float width, float height, uint32_t xSegments, uint32_t ySegments);

    /**
     * @brief パラメータ構造体を指定して円環（Ring）メッシュデータを生成する
     * @param[in] params 円環生成パラメータ（内径・外径・開始/終了角・分割数等）
     * @return 生成された円環の頂点およびインデックスデータ
     */
    static PrimitiveData CreateRing(const RingParams& params);

    /**
     * @brief パラメータを個別に指定して円環・扇形（Ring / Arc）メッシュデータを生成する
     * @param[in] innerRadius 内半径
     * @param[in] outerRadius 外半径
     * @param[in] startAngle 開始角度（ラジアン）
     * @param[in] endAngle 終了角度（ラジアン）
     * @param[in] segments 円周方向の分割数
     * @param[in] verticalUV UV座標を垂直方向にマッピングするかどうか
     * @return 生成された円環の頂点およびインデックスデータ
     */
    static PrimitiveData CreateRing(float innerRadius, float outerRadius, float startAngle, float endAngle,
                                    uint32_t segments, bool verticalUV);

    /**
     * @brief 単純な四角形平面（Plane / Quad）メッシュデータを生成する
     * @param[in] width X軸方向の幅
     * @param[in] height Y軸方向の高さ
     * @return 生成された平面の頂点およびインデックスデータ
     */
    static PrimitiveData CreatePlane(float width = 1.0f, float height = 1.0f);

    /**
     * @brief 正三角形（Triangle）メッシュデータを生成する
     * @return 生成された三角形の頂点およびインデックスデータ
     */
    static PrimitiveData CreateTriangle();

    /**
     * @brief 正四面体（Tetrahedron）メッシュデータを生成する
     * @return 生成された四面体の頂点およびインデックスデータ
     */
    static PrimitiveData CreateTetra();

    /**
     * @brief 2D円盤（Circle）メッシュデータを生成する
     * @param[in] radius 円の半径
     * @param[in] segments 円周方向の分割数
     * @return 生成された円盤の頂点およびインデックスデータ
     */
    static PrimitiveData CreateCircle(float radius, uint32_t segments);

    /**
     * @brief 正八面体（Octahedron）メッシュデータを生成する
     * @return 生成された八面体の頂点およびインデックスデータ
     */
    static PrimitiveData CreateOctahedron();
    ///@}

public:
    PrimitiveManager() = default;
    ~PrimitiveManager() = default;
    PrimitiveManager(const PrimitiveManager&) = delete;
    PrimitiveManager& operator=(const PrimitiveManager&) = delete;

public:
    /**
     * @brief GPUリソースの生成補助
     */
    void CreateGPUResource(const PrimitiveData& data, PrimitiveResource& resource);

private:
    // --- 頂点生成・インデックス生成の分割ヘルパー ---
    /**
     * @brief 球体（UV Sphere）の頂点データ（座標・法線・UV）を生成する
     * @param[out] data 生成結果を格納するプリミティブデータ
     * @param[in] radius 球の半径
     * @param[in] subdivision 経度・緯度の分割数
     */
    static void GenerateSphereVertices(PrimitiveData& data, float radius, uint32_t subdivision);
    /**
     * @brief 球体（UV Sphere）のインデックスバッファを生成する
     * @param[out] data 生成結果を格納するプリミティブデータ
     * @param[in] subdivision 経度・緯度の分割数
     */
    static void GenerateSphereIndices(PrimitiveData& data, uint32_t subdivision);

    /**
     * @brief 円柱（Cylinder / Cone / Frustum）の頂点データを生成する
     * @param[out] data 生成結果を格納するプリミティブデータ
     * @param[in] bottomRadius 底面の半径
     * @param[in] topRadius 上面の半径
     * @param[in] height 円柱の高さ
     * @param[in] segments 円周方向の分割数
     * @param[in] hasTop 上蓋のポリゴンを生成するかどうか
     * @param[in] hasBottom 底蓋のポリゴンを生成するかどうか
     * @param[in] centered Y軸中心を原点とするか（falseの場合は底面が原点）
     */
    static void GenerateCylinderVertices(PrimitiveData& data, float bottomRadius, float topRadius, float height,
                                         uint32_t segments, bool hasTop, bool hasBottom, bool centered);
    /**
     * @brief 円柱のインデックスバッファを生成する
     * @param[out] data 生成結果を格納するプリミティブデータ
     * @param[in] segments 円周方向の分割数
     * @param[in] hasTop 上蓋のインデックスを生成するかどうか
     * @param[in] hasBottom 底蓋のインデックスを生成するかどうか
     */
    static void GenerateCylinderIndices(PrimitiveData& data, uint32_t segments, bool hasTop, bool hasBottom);

    /**
     * @brief 扇形リング（AOE予告円環・扇形）の頂点データを生成する
     * @param[out] data 生成結果を格納するプリミティブデータ
     * @param[in] params 内径・外径・中心角などのリングパラメータ
     */
    static void GenerateRingVertices(PrimitiveData& data, const RingParams& params);
    /**
     * @brief 扇形リングのインデックスバッファを生成する
     * @param[out] data 生成結果を格納するプリミティブデータ
     * @param[in] segments 円弧方向の分割数
     */
    static void GenerateRingIndices(PrimitiveData& data, uint32_t segments);

    /**
     * @brief トーラス（ドーナツ型円環面）の頂点データを生成する
     * @param[out] data 生成結果を格納するプリミティブデータ
     * @param[in] majorRadius 大半径（ドーナツ中心からチューブ中心までの距離）
     * @param[in] minorRadius 小半径（チューブの太さ半径）
     * @param[in] majorSegments 大円の分割数
     * @param[in] minorSegments チューブ円周の分割数
     */
    static void GenerateTorusVertices(PrimitiveData& data, float majorRadius, float minorRadius, uint32_t majorSegments,
                                      uint32_t minorSegments);
    /**
     * @brief トーラスのインデックスバッファを生成する
     * @param[out] data 生成結果を格納するプリミティブデータ
     * @param[in] majorSegments 大円の分割数
     * @param[in] minorSegments チューブ円周の分割数
     */
    static void GenerateTorusIndices(PrimitiveData& data, uint32_t majorSegments, uint32_t minorSegments);

private:
    std::unordered_map<Irufemi::PrimitiveType, PrimitiveData> cpuCache_;
    std::unordered_map<Irufemi::PrimitiveType, PrimitiveResource> gpuCache_;
    std::unordered_map<uint32_t, PrimitiveResource> cylinderGpuCache_;
};
