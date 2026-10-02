struct VertexShaderOutput
{
	float32_t4 position : SV_POSITION;
	float32_t3 texcoord : TEXCOORD0;
	float4 color : COLOR0;
};

struct TransformationMatrix
{
	float32_t4x4 WVP;
	float32_t4x4 World;
	float32_t4x4 WorldInverseTranspose;
};