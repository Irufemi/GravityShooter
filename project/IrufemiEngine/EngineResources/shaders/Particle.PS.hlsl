#include "Particle.hlsli"
#include "Material.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
#include "Bindless.hlsli"

#include "BasePassPixelOutput.hlsli"

SamplerState gSamplerWrap : register(s0);
SamplerState gSamplerClamp : register(s1);

/*LambertianReflectance*/

#include "Lighting.hlsli"
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

PixelShaderOutput main(VertexShaderOutput input)
{
	PixelShaderOutput output;
	
	/*UVTransform*/
	
	///Materialを拡張する
	
	float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
	float32_t4 textureColor;
	if (gMaterial.useClampSampler != 0)
	{
		textureColor = gTextures[gMaterial.textureIndex].Sample(gSamplerClamp, transformedUV.xy);
	}
	else
	{
		textureColor = gTextures[gMaterial.textureIndex].Sample(gSamplerWrap, transformedUV.xy);
	}
	output.color = gMaterial.color * textureColor * input.color;
	
	/*2値抜き*/
		
	/// disxard
		
	// アルファテスト
	if (output.color.a == 0.0) {
		discard;
	}
	
	output.mask.r = gMaterial.customEffectType / 255.0f;
	output.mask.g = gMaterial.customEffectParam;
	output.mask.b = gMaterial.enableEffectMask ? 1.0f : 0.0f;
	output.mask.a = 1.0f;

	// パーティクルの法線はビルボードなのでZ手前固定
	output.normal = float4(0.0f, 0.0f, -1.0f, 1.0f); 
	
	// パーティクルのマテリアルは仮の値
	output.material = float4(0.0f, 1.0f, 0.0f, 1.0f);
	
	// ベロシティは仮
	output.velocity = float2(0.0f, 0.0f);

	return output;
}