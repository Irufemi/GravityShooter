/**
 * @file BossLightning.PS.hlsl
 * @brief ボス撃破演出用・表面電流オーバーレイ対応ピクセルシェーダー
 * @details Object3DのPBR/Phongライティングを完全維持しつつ、LightningUtility.hlsliによる3D電流を加算合成します。
 */

#include "Transform.hlsli"
#include "BasePassVertexOutput.hlsli"
#include "Lighting.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
#include "Bindless.hlsli"
#include "BasePassPixelOutput.hlsli"

SamplerState gSamplerWrap : register(s0);
SamplerState gSamplerPointClamp : register(s1);
SamplerState gSamplerClamp : register(s3);
SamplerState gSamplerWrapClamp : register(s4);
SamplerComparisonState gShadowSampler : register(s2);

ConstantBuffer<LightCommonData> gLightCommon : register(b1);

#include "PerFrame.hlsli"
ConstantBuffer<PerFrameData> gPerFrame : register(b2);

StructuredBuffer<PointLight> gPointLights : register(t2);
StructuredBuffer<SpotLight> gSpotLights : register(t3);
StructuredBuffer<AreaLight> gAreaLights : register(t4);
Texture2D<float32_t> gShadowMap : register(t5);

// RootSlot::Special (register b6) から動的電流パラメータを受け取る
#include "LightningUtility.hlsli"
ConstantBuffer<LightningConfig> gLightningCfg : register(b6);

PixelShaderOutput main(VertexShaderOutput input)
{
	PixelShaderOutput output;
	
	float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
	
	float32_t4 textureColor;
	if (gMaterial.useClampSampler == 1) {
		textureColor = gTextures[gMaterial.textureIndex].Sample(gSamplerClamp, transformedUV.xy);
	} else if (gMaterial.useClampSampler == 2) {
		textureColor = gTextures[gMaterial.textureIndex].Sample(gSamplerPointClamp, transformedUV.xy);
	} else if (gMaterial.useClampSampler == 3) {
		textureColor = gTextures[gMaterial.textureIndex].Sample(gSamplerWrapClamp, transformedUV.xy);
	} else {
		textureColor = gTextures[gMaterial.textureIndex].Sample(gSamplerWrap, transformedUV.xy);
	}
	
	if (textureColor.a <= gMaterial.alphaReference)
	{
		discard;
	}
	
	if (gMaterial.enableLighting != 0)
	{
		LightContext context;
		context.normal = normalize(input.normal);
		context.worldPosition = input.worldPosition;
		context.toEye = normalize(gPerFrame.cameraWorldPosition - input.worldPosition);

		float3 albedo = gMaterial.color.rgb * textureColor.rgb * input.color.rgb;
		if (gMaterial.lightingMode == 9)
		{
			float luminance = dot(albedo, float3(0.2125f, 0.7154f, 0.0721f));
			float3 gray = float3(luminance, luminance, luminance);
			albedo = lerp(gray, float3(1.0f, 1.0f, 1.0f), 0.5f);
		}
		float3 totalDiffuse = 0;
		float3 totalSpecular = 0;

		float shadowFactor = CalculateShadow(input.shadowPos, gShadowMap, gShadowSampler, context.normal, gLightCommon.directionalLight.direction);

		float3 dirDiffuse = 0;
		float3 dirSpecular = 0;
		ApplyDirectionalLight(gLightCommon.directionalLight, gMaterial, albedo, context, dirDiffuse, dirSpecular);
		totalDiffuse += dirDiffuse * shadowFactor;
		totalSpecular += dirSpecular * shadowFactor;

		for (uint32_t i = 0; i < gLightCommon.pointLightCount; ++i) {
			ApplyPointLight(gPointLights[i], gMaterial, albedo, context, totalDiffuse, totalSpecular);
		}
		for (uint32_t j = 0; j < gLightCommon.spotLightCount; ++j) {
			ApplySpotLight(gSpotLights[j], gMaterial, albedo, context, totalDiffuse, totalSpecular);
		}
		for (uint32_t k = 0; k < gLightCommon.areaLightCount; ++k) {
			ApplyAreaLight(gAreaLights[k], gMaterial, albedo, context, totalDiffuse, totalSpecular);
		}

		if (gMaterial.lightingMode == 0) {
			output.color.rgb = albedo;
		} else {
			float3 skyColor = float3(0.35f, 0.35f, 0.4f);
			float3 groundColor = float3(0.1f, 0.1f, 0.12f);
			float hemiFactor = context.normal.y * 0.5f + 0.5f;
			float3 ambientColor = lerp(groundColor, skyColor, hemiFactor);

			output.color.rgb = totalDiffuse + totalSpecular + (albedo * ambientColor);
		}
			
		float32_t3 reflectedVector = reflect(-context.toEye, context.normal);
		
		uint envWidth, envHeight, envMipLevels;
		gTextureCubes[gMaterial.envMapIndex].GetDimensions(0, envWidth, envHeight, envMipLevels);
		float mipLevel = gMaterial.roughness * float(envMipLevels - 1);
		
		float32_t4 environmentColor = gTextureCubes[gMaterial.envMapIndex].SampleLevel(gSamplerWrap, reflectedVector, mipLevel);
		
		float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, gMaterial.metallic);
		float3 F = FresnelSchlick(saturate(dot(context.normal, context.toEye)), F0);
		
		output.color.rgb += environmentColor.rgb * F * (1.0f - gMaterial.roughness) * gMaterial.environmentCoefficient;
		output.color.a = gMaterial.color.a * textureColor.a * input.color.a;
		
		if (output.color.a == 0.0)
		{
			discard;
		}
	}
	else
	{
		output.color = gMaterial.color * textureColor * input.color;
	}
	
    // --- 3D空間ノイズ電流（LightningUtility.hlsli）の加算合成 ---
    float3 toCam = normalize(gPerFrame.cameraWorldPosition - input.worldPosition);
    LightningSurfaceOutput lightningOut = Evaluate3DLightningCrawl(
        input.worldPosition,
        normalize(input.normal),
        toCam,
        gPerFrame.time,
        gLightningCfg
    );
    output.color.rgb += lightningOut.emissiveColor;

    // --- G-Buffer / Mask の通常出力 ---
    float effectType = gMaterial.customEffectType / 255.0f;
    float effectParam = gMaterial.customEffectParam;
    float enableMask = gMaterial.enableEffectMask ? 1.0f : 0.0f;

    if (input.customEffect.x > 0.0f || input.customEffect.z > 0.0f) {
        effectType = input.customEffect.x / 255.0f;
        effectParam = input.customEffect.y;
        enableMask = input.customEffect.z;
    }

    output.mask.r = effectType;
    output.mask.g = effectParam;
    output.mask.b = enableMask;
    output.mask.a = 1.0f;

    output.normal = float4(normalize(input.normal), 1.0f);
    output.material = float4(gMaterial.metallic, gMaterial.roughness, 0.0f, 1.0f);
    output.velocity = float2(0.0f, 0.0f);

    return output;
}
