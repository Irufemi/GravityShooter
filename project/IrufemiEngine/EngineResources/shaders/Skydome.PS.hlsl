#include "BasePassPixelOutput.hlsli"
#include "Material.hlsli"
#include "PerFrame.hlsli"
#include "Bindless.hlsli"

struct SkydomeVertexOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<PerFrameData> gPerFrame : register(b2);
SamplerState gSamplerWrap : register(s0);

static const float PI = 3.14159265358979323846f;

PixelShaderOutput main(SkydomeVertexOutput input)
{
    PixelShaderOutput output;

    // スクリーンUVからNDC座標 [-1, 1] を算出
    float2 ndc = input.texcoord * 2.0f - 1.0f;
    ndc.y = -ndc.y; // DirectX UV (上0, 下1) から NDC (上+1, 下-1) へ反転

    // プロジェクション対角成分からビュー空間の方向レイを復元
    float tanHalfFovX = 1.0f / gPerFrame.projection[0][0];
    float tanHalfFovY = 1.0f / gPerFrame.projection[1][1];
    float3 viewRay = float3(ndc.x * tanHalfFovX, ndc.y * tanHalfFovY, 1.0f);

    // ビュー行列の回転成分の逆行列（転置行列）を掛けてワールド視線レイを算出
    float3x3 invViewRot = transpose((float3x3)gPerFrame.view);
    float3 worldRay = normalize(mul(viewRay, invViewRot));

    // 球面UVマッピング (緯度経度)
    // 方位角 (経度): atan2(x, z) で [-PI, PI] -> [0, 1]
    float u = atan2(worldRay.x, worldRay.z) / (2.0f * PI) + 0.5f;
    // 仰角 (緯度): asin(y) で [-PI/2, PI/2] -> [1, 0] (天頂が0, 地平線が0.5, 底面が1)
    float v = -asin(clamp(worldRay.y, -1.0f, 1.0f)) / PI + 0.5f;

    // UV変換
    float4 transformedUV = mul(float4(u, v, 0.0f, 1.0f), gMaterial.uvTransform);

    // 2Dテクスチャサンプリング
    float4 textureColor = gTextures[gMaterial.textureIndex].Sample(gSamplerWrap, transformedUV.xy);

    // Unlit（テクスチャカラー * マテリアルカラー）を出力
    output.color = textureColor * gMaterial.color;
    output.mask = float4(0.0f, 0.0f, 0.0f, 0.0f);     // マスクなし
    output.normal = float4(0.0f, 0.0f, 0.0f, 0.0f);   // 法線なし（背景）
    output.material = float4(0.0f, 0.0f, 0.0f, 0.0f); // ラフネス/メタリックなし
    output.velocity = float2(0.0f, 0.0f);

    return output;
}

