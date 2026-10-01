#include "SkinningObject3D.hlsli"
#include "Lighting.hlsli"

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<LightCommonData> gLightCommonData : register(b1);

/*Skinning*/

StructuredBuffer<Well> gMatrixPalette : register(t0);

struct Skinned
{
	float32_t4 position;
	float32_t3 normal;
};

struct VertexShaderInput
{
	float32_t4 position : POSITION0;
	float32_t2 texcoord : TEXCOORD0;
	float32_t3 normal : NORMAL0;
	
	float32_t4 color : COLOR0;
	
	float32_t4 weight : WEIGHT0;
	int32_t4 index : INDEX0;
	
};

Skinned Skinning(VertexShaderInput input)
{
	Skinned skinned;
	// なんやかんやSkinningの処理をする
	
	// 位置の変換
	skinned.position = mul(input.position, gMatrixPalette[input.index.x].skeletonSpaceMatrix) * input.weight.x;
	skinned.position += mul(input.position, gMatrixPalette[input.index.y].skeletonSpaceMatrix) * input.weight.y;
	skinned.position += mul(input.position, gMatrixPalette[input.index.z].skeletonSpaceMatrix) * input.weight.z;
	skinned.position += mul(input.position, gMatrixPalette[input.index.w].skeletonSpaceMatrix) * input.weight.w;
	skinned.position.w = 1.0f; // 確実に1を入れる
	// 法線の変換
	skinned.normal = mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.x].skeletonInverseTransposeMatrix) * input.weight.x;
	skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.y].skeletonInverseTransposeMatrix) * input.weight.y;
	skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.z].skeletonInverseTransposeMatrix) * input.weight.z;
	skinned.normal += mul(input.normal, (float32_t3x3) gMatrixPalette[input.index.w].skeletonInverseTransposeMatrix) * input.weight.w;
	skinned.normal = normalize(skinned.normal); // 正規化して戻してあげる
	
	return skinned;
}

#include "Camera.hlsli"
ConstantBuffer<Camera> gCamera : register(b2);

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	
	/*Skinning*/
	Skinned skinned = Skinning(input); // まずSkinning計算を行って、Skinning後の頂点情報を手に入れる。ここでの頂点もSkeletonSpace
	// Skinning結果を使って変換
	float4 worldPos = mul(skinned.position, gTransformationMatrix.World);
	float4 viewPos = mul(worldPos, gCamera.view);
	output.position = mul(viewPos, gCamera.projection);
	output.worldPosition = worldPos.xyz;
	output.texcoord = input.texcoord;
	output.normal = normalize(mul(skinned.normal, (float32_t3x3) gTransformationMatrix.WorldInverseTranspose));

	// シャドウマッピング用の座標変換
	output.shadowPos = mul(worldPos, gLightCommonData.viewProjection);
	
	output.color = input.color;
	output.customEffect = float32_t4(0, 0, 0, 0);
	
	return output;
}