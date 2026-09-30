#include "ParticleGPU.hlsli"
#include "RandomGenerator.hlsli"
#include "PerFrame.hlsli"

static const uint kMaxParticles = 32768;

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<int> gFreeList : register(u2);
StructuredBuffer<GPUParticleEmitter> gEmitters : register(t0);

struct ObjectVertex {
    float4 position;
    float2 texcoord;
    float3 normal;
    float4 color;
};
StructuredBuffer<ObjectVertex> gMeshVertices : register(t3);

cbuffer EmitConstants : register(b2)
{
    uint gEmitterIndex;
};
ConstantBuffer<PerFrame> gPerFrame : register(b1);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // 放出数の計算（通常放出 + バースト放出）
    GPUParticleEmitter emitter = gEmitters[gEmitterIndex];
    int emitCount = (int)emitter.burstCount;

    if (emitCount <= 0) return;

    int i = (int)DTid.x;
    if (i >= emitCount) return;

    int freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);

        if (freeListIndex >= 0 && freeListIndex < (int)kMaxParticles)
        {
            uint particleIndex = (uint)gFreeList[freeListIndex];
            
            // 乱数生成器の初期化
            // iとparticleIndexが連動して相殺するのを防ぐため素数を掛ける。さらにエミッターごとのシードを加味して完全一致を防ぐ。
            uint seedValue = (uint)gPerFrame.time * 100000 + (i * 1337) + particleIndex + (emitter.randomSeed * 77777);
            RandomGenerator rng;
            rng.seed = uint3(seedValue, seedValue + 111, seedValue + 222);

            float r_life  = rng.Generate1d();
            float r_scale = rng.Generate1d();
            float r_color = rng.Generate1d();
            float3 r_pos  = rng.Generate3d();
            float r_vel   = rng.Generate1d();

            gParticles[particleIndex].currentTime = 0.0f;
            gParticles[particleIndex].lifeTime = max(lerp(emitter.minLife, emitter.maxLife, r_life), 0.0001f);
            gParticles[particleIndex].type = 0; // 親として初期化
            gParticles[particleIndex].trailTimer = 0.0f;
            gParticles[particleIndex].emitterIndex = gEmitterIndex;
            gParticles[particleIndex].billboardMode = emitter.billboardMode;
            gParticles[particleIndex].atlasSize = (emitter.atlasRows << 16) | (emitter.atlasCols & 0xFFFF);

            // フレーム間スポーン補間 (Sub-frame Spawning Interpolation)
            // 高速移動時でもフレーム間の隙間を埋め、連続した光条・軌跡を形成する
            float subframeT = (emitCount > 1) ? ((float)i / (float)(emitCount - 1)) : 1.0f;
            float3 basePos = lerp(emitter.prevTranslate, emitter.translate, subframeT);

            // 放出形状別の初期位置・速度設定
            if (emitter.type == 0) // Sphere
            {
                float phi = r_pos.x * 2.0f * 3.141592f;
                float theta = r_pos.y * 3.141592f;
                float3 offset = float3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi)) * (r_pos.z * emitter.radius);
                gParticles[particleIndex].translate = basePos + offset;
                float3 radialDir = (length(offset) > 0.0001f) ? normalize(offset) : normalize(rng.Generate3d() * 2.0f - 1.0f);
                gParticles[particleIndex].velocity = (emitter.direction + radialDir * emitter.spread) * emitter.velocity;
            }
            else if (emitter.type == 1) // Beam
            {
                float3 L = normalize(emitter.direction);
                float3 up = abs(L.y) < 0.999f ? float3(0,1,0) : float3(1,0,0);
                float3 side = normalize(cross(up, L));
                float3 upVec = cross(L, side);

                float angle = r_pos.x * 2.0f * 3.141592f;
                // 表面付近(0.9~1.0)に完全に集中させる
                float dist = (0.9f + r_pos.y * 0.1f) * emitter.radius;
                float3 offset = (side * cos(angle) + upVec * sin(angle)) * dist;

                gParticles[particleIndex].translate = basePos + offset;
                
                // パーティクルが外側へ広がらないよう、接線方向への初速を削除。
                // 完全にビームの進行方向(L)に沿って直進させることで、太さを一定に保つ。
                // 僅かな揺らぎ(ノイズ)として、極めて微小なランダム方向のみを加算。
                float3 randomDir = normalize(side * (r_pos.x * 2 - 1) + upVec * (r_pos.y * 2 - 1));
                float3 straightDir = normalize(L + randomDir * (emitter.spread * 0.1f));
                
                gParticles[particleIndex].velocity = straightDir * (emitter.velocity * (0.8f + r_vel * 0.4f));
            }
            else if (emitter.type == 2) // Ring
            {
                float angle = rng.Generate1d() * 2.0f * 3.141592f;
                // radius: 外径, 厚みは既存の計算でspreadを流用していたが、放射強度のspreadと被るのでここでは固定値の0.1などに固定するか、そのまま使う
                float r = emitter.radius - (rng.Generate1d() * 0.1f);
                float3 offset = float3(cos(angle), 0, sin(angle)) * r;
                
                gParticles[particleIndex].translate = basePos + offset;
                float3 radialDir = (length(offset) > 0.0001f) ? normalize(offset) : normalize(rng.Generate3d() * 2.0f - 1.0f);
                gParticles[particleIndex].velocity = (emitter.direction + radialDir * emitter.spread) * emitter.velocity;
            }
            else if (emitter.type == 3) // Cylinder
            {
                float angle = rng.Generate1d() * 2.0f * 3.141592f;
                // 円周上（半径のフチ 90%〜100% の範囲）にのみ粒子を生成し、中空にする
                float r = emitter.radius - (rng.Generate1d() * 0.1f * emitter.radius);
                float h = (rng.Generate1d() * 2.0f - 1.0f) * (emitter.velocity * 0.5f); // velocityを高さとして流用
                
                float3 L = normalize(emitter.direction);
                float3 up = abs(L.y) < 0.999f ? float3(0,1,0) : float3(1,0,0);
                float3 side = normalize(cross(up, L));
                float3 upVec = cross(L, side);
                
                float3 offset = (side * cos(angle) + upVec * sin(angle)) * r + L * h;
                gParticles[particleIndex].translate = basePos + offset;
                gParticles[particleIndex].velocity = L * 0.05f;
            }
            else if (emitter.type == 4) // Box
            {
                float3 offset = (r_pos - float3(0.5f, 0.5f, 0.5f)) * emitter.areaSize;
                gParticles[particleIndex].translate = basePos + offset;
                float3 radialDir = (length(offset) > 0.0001f) ? normalize(offset) : normalize(rng.Generate3d() * 2.0f - 1.0f);
                gParticles[particleIndex].velocity = (emitter.direction + radialDir * emitter.spread) * emitter.velocity;
            }
            else if (emitter.type == 5) // Hemisphere (Burst)
            {
                // r_pos.x: 方位角 (0~2pi), r_pos.y: 仰角 (0~pi/2で上半分のみ)
                float phi = r_pos.x * 2.0f * 3.141592f;
                float theta = r_pos.y * (3.141592f * 0.5f); // 90度（上半分）までに制限
                
                // radiusにr_pos.zをかけて中身を埋めるか、表面だけにするか
                // ここではバースト用に球体内部からも放出（r_pos.z）
                float r = pow(r_pos.z, 1.0f/3.0f) * emitter.radius; 
                
                float3 offset = float3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi)) * r;
                // Y軸方向の高さを少し潰して、横に広いドーム状にする
                offset.y *= 0.5f;
                gParticles[particleIndex].translate = basePos + offset;
                
                // 放射状に広がる速度
                float3 radialDir = (length(offset) > 0.0001f) ? normalize(offset) : normalize(rng.Generate3d() * 2.0f - 1.0f);
                // spreadパラメータを横方向への押し出し係数として利用する
                radialDir.xz *= (1.0f + emitter.spread);
                radialDir = normalize(radialDir);

                // 上方向固定バイアスを無くし、純粋な放射状（横に広がる）速度にする
                gParticles[particleIndex].velocity = radialDir * emitter.velocity * (0.5f + r_vel * 0.5f);
            }
            else if (emitter.type == 6)
            {
                // Mesh Emitter
                uint vertexCount = (uint)emitter.padFreqTime;
                if (vertexCount > 0) {
                    uint vertexIndex = (uint)(rng.Generate1d() * vertexCount);
                    vertexIndex = min(vertexIndex, vertexCount - 1);
                    ObjectVertex v = gMeshVertices[vertexIndex];
                    
                    float3 emitPos = v.position.xyz + basePos;
                    float3 emitDir = normalize(v.normal);
                    
                    gParticles[particleIndex].translate = emitPos;
                    gParticles[particleIndex].velocity = emitDir * max(emitter.velocity + (rng.Generate1d() - 0.5f) * emitter.spread, 0.0f);
                } else {
                    gParticles[particleIndex].translate = emitter.translate;
                    gParticles[particleIndex].velocity = float3(0, 0, 0);
                }
            }

            // スケール初期化
            gParticles[particleIndex].startScale = lerp(emitter.startScaleMin, emitter.startScaleMax, r_scale);
            gParticles[particleIndex].midScale = lerp(emitter.midScaleMin, emitter.midScaleMax, r_scale);
            gParticles[particleIndex].endScale = lerp(emitter.endScaleMin, emitter.endScaleMax, r_scale);
            gParticles[particleIndex].scale = gParticles[particleIndex].startScale;

            // カラー初期化
            gParticles[particleIndex].startColor = lerp(emitter.startColorMin, emitter.startColorMax, r_color);
            gParticles[particleIndex].midColor = lerp(emitter.midColorMin, emitter.midColorMax, r_color);
            gParticles[particleIndex].endColor = lerp(emitter.endColorMin, emitter.endColorMax, r_color);
            gParticles[particleIndex].color = gParticles[particleIndex].startColor;

            gParticles[particleIndex].midPoint = emitter.midPoint;

            // 回転初期化
            if (emitter.enableRandomRotation != 0) {
                gParticles[particleIndex].rotation = rng.Generate3d() * 2.0f * 3.141592f;
                gParticles[particleIndex].rotateSpeed = (rng.Generate3d() * 2.0f - 1.0f) * 3.141592f;
            } else {
                gParticles[particleIndex].rotation = float3(0.0f, 0.0f, 0.0f);
                gParticles[particleIndex].rotateSpeed = float3(0.0f, 0.0f, 0.0f);
            }
        }
        else
    {
        InterlockedAdd(gFreeListIndex[0], 1);
    }
}