#include "ParticleGPU.hlsli"
#include "DepthFade.hlsli"
#include "Material.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);

#include "Lighting.hlsli"
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

#include "Bindless.hlsli"

#include "BasePassPixelOutput.hlsli"

Texture2D<float> gDepthTexture : register(t6); // ソフトパーティクル用深度テクスチャ

SamplerState gSamplerWrap : register(s0);
SamplerState gSamplerClamp : register(s1);
SamplerState gSamplerWrapClamp : register(s4); // U:Wrap, V:Clamp

PixelShaderOutput main(VertexShaderOutput input)
{
	PixelShaderOutput output;
	
	/*UVTransform*/
	
	///Materialを拡張する
	
	float4 transformedUV = mul(float32_t4(input.texcoord.xy, 0.0f, 1.0f), gMaterial.uvTransform);
	float32_t4 textureColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

	// パーティクルは基本的にラップではなくクランプを使用する
	// (境界部分での1ピクセルの回り込みによる矩形ノイズを防ぐため)
	if (gMaterial.useClampSampler == 3)
	{
		textureColor = gTextures[gMaterial.textureIndex].Sample(gSamplerWrapClamp, transformedUV.xy);
	}
	else if (gMaterial.useClampSampler == 1) // 1: Wrap を明示的に指定した場合
	{
		textureColor = gTextures[gMaterial.textureIndex].Sample(gSamplerWrap, transformedUV.xy);
	}
	else // デフォルト(0) は Clamp
	{
		textureColor = gTextures[gMaterial.textureIndex].Sample(gSamplerClamp, transformedUV.xy);
	}

	// ライトの影響を適用
	float3 litColor = textureColor.rgb * input.color.rgb;
	if (gMaterial.enableLighting != 0) {
		// シンプルにライトカラーと強度を乗算（パーティクルの性質上、環境光的に全体に影響させる）
		litColor *= (gDirectionalLight.color.rgb * gDirectionalLight.intensity);
	}

	output.color = float4(gMaterial.color.rgb * litColor, gMaterial.color.a * textureColor.a * input.color.a);
	
	/*2値抜き*/
		
	/// disxard
		
	// output.aolorのα値が0の時にPixelを棄却
	if (output.color.a == 0.0)
	{
		discard;
	}
	
	/*ソフトパーティクル計算*/
	
	float softScale = 1.0f; // TODO: 必要な場合はマテリアルやエミッターパラメータに出す
	// float fade = CalculateDepthFade(gDepthTexture, input.position, input.cameraNear, input.cameraFar, softScale);
	
	// αにフェードを適用 (Zファイト/めり込み防止だが、HitEffectが表面で発生すると見えなくなるため一時無効化)
	// output.color.a *= fade;
	
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