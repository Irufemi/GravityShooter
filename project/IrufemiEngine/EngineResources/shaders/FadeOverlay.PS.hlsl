struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

cbuffer FadeParamsCB : register(b0) {
    float4 gColor;
    float gIntensity;
    float3 gPadding;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    output.color = float4(gColor.rgb, gIntensity);
    return output;
}
