#include "ParticleGPU.hlsli"
#include "VertexData.hlsli"
#include "CullingUtility.hlsli"
#include "MathUtility.hlsli"

StructuredBuffer<Particle> gParticles : register(t0);
StructuredBuffer<ParticleSortData> gSortList : register(t1);
ConstantBuffer<PerView> gPerView : register(b0);

VertexShaderOutput main(VertexInput input, uint instanceId : SV_InstanceID) 
{
	VertexShaderOutput output;
    
    ParticleSortData sortData = gSortList[instanceId];
	Particle particle = gParticles[sortData.particleIndex];
    
    // ソート時に付与したdepthが負の場合は消滅済みパーティクルのためカリング
    if (sortData.depth < 0.0f)
    {
        CullInstanceByScale(particle.scale);
    }
	
    // ローカル座標にスケールを適用
    float3 localPos = input.position.xyz * particle.scale;
    float3 worldPos = float3(0, 0, 0);
    
    if (particle.billboardMode == 1) // Billboard
    {
        // Z軸回転を適用
        float4x4 rotZ = MakeRotateZMatrix(particle.rotation.z);
        float3 rotPos = mul(float4(localPos, 1.0f), rotZ).xyz;
        
        // ビルボード行列を適用して平行移動
        worldPos = mul(float4(rotPos, 1.0f), gPerView.billboardMatrix).xyz + particle.translate;
    }
    else if (particle.billboardMode == 2) // Velocity Billboard
    {
        // 速度方向ビルボード (Velocity Billboard)
        float3 dir = particle.velocity;
        float len = length(dir);
        if (len < 0.0001f)
        {
            dir = float3(0.0f, 1.0f, 0.0f);
        }
        else
        {
            dir /= len;
        }

        // カメラからパーティクルへの方向ベクトル
        float3 viewDir = normalize(particle.translate - gPerView.worldPosition);

        // パーティクルの右方向（進行方向と視線ベクトルの外積）
        float3 right = cross(dir, viewDir);
        float lenR = length(right);
        if (lenR < 0.0001f)
        {
            // 進行方向と視線が平行な場合は、任意の右方向を定義
            float3 upVec = abs(dir.y) < 0.999f ? float3(0,1,0) : float3(1,0,0);
            right = normalize(cross(dir, upVec));
        }
        else
        {
            right /= lenR;
        }

        // パーティクルの手前（法線）方向
        float3 normal = cross(right, dir);

        // 速度方向ビルボード回転行列
        // Y軸が進行方向(dir)、X軸が右(right)、Z軸が手前(normal)に整列
        float3x3 rotMatrix = {
            right.x,  right.y,  right.z,
            dir.x,    dir.y,    dir.z,
            normal.x, normal.y, normal.z
        };

        worldPos = mul(localPos, rotMatrix) + particle.translate;
    }
    else // 3D Rotation
    {
        // 3D回転 (SRT)
        float4x4 rotateMatrix = MakeRotateXYZMatrix(particle.rotation);
        worldPos = mul(float4(localPos, 1.0f), rotateMatrix).xyz + particle.translate;
    }
    
    // ViewProjectionを適用して最終的な頂点座標を計算
	output.position = mul(float4(worldPos, 1.0f), gPerView.viewProjection);
	
    // UV アニメーション (テクスチャアトラス)
    float2 uv = input.texcoord;
    uint atlasRows = (particle.atlasSize >> 16) & 0xFFFF;
    uint atlasCols = particle.atlasSize & 0xFFFF;
    uint totalFrames = max(1, atlasRows * atlasCols);
    if (totalFrames > 1)
    {
        float t = saturate(particle.currentTime / particle.lifeTime);
        uint frameIndex = (uint)(t * (float)totalFrames);
        frameIndex = min(frameIndex, totalFrames - 1);
        
        uint row = frameIndex / max(1, atlasCols);
        uint col = frameIndex % max(1, atlasCols);
        
        float2 frameSize = 1.0f / float2(max(1, atlasCols), max(1, atlasRows));
        uv = (uv + float2(col, row)) * frameSize;
    }
    output.texcoord = float4(uv, particle.translate.xy);
    output.timeRatio = saturate(particle.currentTime / max(particle.lifeTime, 0.0001f));
	output.color = input.color * particle.color;
	output.cameraNear = gPerView.cameraNear;
	output.cameraFar = gPerView.cameraFar;
	return output;
}
