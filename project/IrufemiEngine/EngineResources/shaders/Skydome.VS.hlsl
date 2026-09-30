struct SkydomeVertexOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

static const uint kNumVertex = 3;
static const float4 kPositions[kNumVertex] = {
    {-1.0f,  1.0f, 1.0f, 1.0f}, // 左上 (Z=1.0 最奥)
    { 3.0f,  1.0f, 1.0f, 1.0f}, // 右上
    {-1.0f, -3.0f, 1.0f, 1.0f}, // 左下
};
static const float2 kTexcoords[kNumVertex] = {
    {0.0f, 0.0f}, // 左上
    {2.0f, 0.0f}, // 右上
    {0.0f, 2.0f}, // 左下
};

/**
 * @brief プロシージャルSkydome頂点シェーダー
 * @details 頂点バッファを一切バインドせず、SV_VertexIDから全画面を覆う三角形を生成し最奥深度に固定する。
 */
SkydomeVertexOutput main(uint vertexId : SV_VertexID)
{
    SkydomeVertexOutput output;
    output.position = kPositions[vertexId];
    output.texcoord = kTexcoords[vertexId];
    return output;
}


