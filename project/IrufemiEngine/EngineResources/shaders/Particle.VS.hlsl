#include "Particle.hlsli"
#include "VertexData.hlsli"

struct ParticleForGPU
{
	float32_t4x4 WVP;
	
	/*LambertianReflectance*/
	
	float32_t4x4 World;
	
	float32_t4 color;
};
StructuredBuffer<ParticleForGPU> gParticle : register(t0);

// struct VertexShaderInput は VertexData.hlsli で定義

#include "Camera.hlsli"
ConstantBuffer<Camera> gCamera : register(b2);

VertexShaderOutput main(VertexInput input, uint32_t instanced : SV_InstanceID)
{
	VertexShaderOutput output;
	
	float32_t4 worldPos = mul(input.position, gParticle[instanced].World);
	float4 viewPos = mul(worldPos, gCamera.view);
	output.position = mul(viewPos, gCamera.projection);
	output.texcoord = input.texcoord;
	output.color = input.color * gParticle[instanced].color;

	return output;
}

