/**
 * @file NebulaSeaFloor.VS.hlsl
 * @brief 星雲ガス海（Nebula Sea Floor）3Dワールド空間 頂点シェーダー
 * @details Y=0.0f を基準高度とし、カメラ追従グリッドを3Dワールド空間へ投影・変換します。
 */

#include "PerFrame.hlsli"

ConstantBuffer<PerFrameData> gPerFrame : register(b2);

struct NebulaSeaParams {
    float time;
    float railSpeed;
    float surfaceY;
    float volumeDepth;
    float fadeStartDistance;
    float fadeEndDistance;
    float softIntersectionRange;
    float densityScale;
    float4 colDeepSpace;
    float4 colTeal;
    float4 colCyan;
    float4 colWhite;
    float4 colAmethyst;
    float4 colHorizonSky;
    float4 cameraSnapXZ;
};
ConstantBuffer<NebulaSeaParams> gNebulaSea : register(b6);

struct VertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
};

struct VertexOutput {
    float4 svPosition : SV_Position;
    float3 worldPosition : WORLD_POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
    float3 viewRay : VIEW_RAY;
};

VertexOutput main(VertexInput input) {
    VertexOutput output;

    // カメラXZ追従スナップを加算したワールド座標
    float3 worldPos = input.position;
    worldPos.x += gNebulaSea.cameraSnapXZ.x;
    worldPos.z += gNebulaSea.cameraSnapXZ.y;
    worldPos.y = gNebulaSea.surfaceY; // Y=0.0f (接地基準面)

    output.worldPosition = worldPos;
    output.normal = float3(0.0f, 1.0f, 0.0f);
    output.texcoord = input.texcoord;

    // カメラ位置からの3Dワールド視線ベクトル
    output.viewRay = worldPos - gPerFrame.cameraWorldPosition;

    // ビュープロジェクション変換
    float4 viewPos = mul(float4(worldPos, 1.0f), gPerFrame.view);
    output.svPosition = mul(viewPos, gPerFrame.projection);

    return output;
}
