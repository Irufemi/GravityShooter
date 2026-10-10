/**
 * @file Noise.hlsli
 * @brief ノイズ・ユーティリティ関数
 */

#pragma once
#ifndef NOISE_HLSLI
#define NOISE_HLSLI

static const float TAU = 6.28318530718;

// 擬似乱数 (2D -> 1D)
float rand(float2 n) { 
	return frac(sin(dot(n, float2(12.9898, 4.1414))) * 43758.5453);
}

// 擬似乱数 (3D -> 1D)
float rand(float3 n) {
    return frac(sin(dot(n, float3(12.9898, 78.233, 45.164))) * 43758.5453);
}

// シンプルなノイズ (2D)
float noise(float2 p) {
	float2 ip = floor(p);
	float2 u = frac(p);
	u = u * u * (3.0 - 2.0 * u);
	
	float res = lerp(
		lerp(rand(ip), rand(ip + float2(1.0, 0.0)), u.x),
		lerp(rand(ip + float2(0.0, 1.0)), rand(ip + float2(1.0, 1.0)), u.x), u.y);
	return res * res;
}

// シンプルなノイズ (3D)
float noise(float3 p) {
    float3 ip = floor(p);
    float3 u = frac(p);
    u = u * u * (3.0 - 2.0 * u);

    float n000 = rand(ip);
    float n100 = rand(ip + float3(1.0, 0.0, 0.0));
    float n010 = rand(ip + float3(0.0, 1.0, 0.0));
    float n110 = rand(ip + float3(1.0, 1.0, 0.0));
    float n001 = rand(ip + float3(0.0, 0.0, 1.0));
    float n101 = rand(ip + float3(1.0, 0.0, 1.0));
    float n011 = rand(ip + float3(0.0, 1.0, 1.0));
    float n111 = rand(ip + float3(1.0, 1.0, 1.0));

    float res = lerp(
        lerp(lerp(n000, n100, u.x), lerp(n010, n110, u.x), u.y),
        lerp(lerp(n001, n101, u.x), lerp(n011, n111, u.x), u.y), u.z);
    return res * res;
}

// Fractal Brownian Motion (2D)
float fBm(float2 p) {
	float f = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) {
		f += a * noise(p);
		p *= 2.0;
		a *= 0.5;
	}
	return f;
}

// Fractal Brownian Motion (3D)
float fBm(float3 p) {
    float f = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        f += a * noise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return f;
}

struct VoronoiResult {
    float2 seedPos;  // セル内のシード点（中心点）の絶対座標
    float minDist;   // そのシード点までの距離
};

// ボロノイ分割（Cellular Noise） (2D)
VoronoiResult Voronoi(float2 uv) {
    float2 baseCell = floor(uv);
    
    VoronoiResult res;
    res.minDist = 10.0f;
    res.seedPos = float2(0.0f, 0.0f);
    
    // 3x3の隣接セルを探索
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            float2 cellOffset = float2(x, y);
            float2 cellId = baseCell + cellOffset;
            
            // セルごとの固有ランダムオフセット (0.0 ~ 1.0)
            float2 seedOffset = float2(
                rand(cellId),
                rand(cellId + float2(13.5f, 41.2f))
            );
            
            float2 seedPos = cellId + seedOffset;
            float dist = distance(uv, seedPos);
            
            if (dist < res.minDist) {
                res.minDist = dist;
                res.seedPos = seedPos;
            }
        }
    }
    
    return res;
}

// ============================================================================
// Simplex Noise (2D) - 縦線・横線（グリッドアーティファクト）が出ない高品質ノイズ
// ============================================================================
float3 mod289(float3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
float2 mod289(float2 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
float3 permute(float3 x) { return mod289(((x * 34.0) + 1.0) * x); }

float snoise(float2 v) {
    const float4 C = float4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0
                            0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
                           -0.577350269189626,  // -1.0 + 2.0 * C.x
                            0.024390243902439); // 1.0 / 41.0
    // First corner
    float2 i  = floor(v + dot(v, C.yy));
    float2 x0 = v - i + dot(i, C.xx);

    // Other corners
    float2 i1 = (x0.x > x0.y) ? float2(1.0, 0.0) : float2(0.0, 1.0);
    float4 x12 = x0.xyxy + C.xxzz;
    x12.xy -= i1;

    // Permutations
    i = mod289(i);
    float3 p = permute(permute(i.y + float3(0.0, i1.y, 1.0))
                     + i.x + float3(0.0, i1.x, 1.0));

    float3 m = max(0.5 - float3(dot(x0, x0), dot(x12.xy, x12.xy), dot(x12.zw, x12.zw)), 0.0);
    m = m * m;
    m = m * m;

    // Gradients
    float3 x = 2.0 * frac(p * C.www) - 1.0;
    float3 h = abs(x) - 0.5;
    float3 ox = floor(x + 0.5);
    float3 a0 = x - ox;

    m *= 1.79284291400159 - 0.85373472095314 * (a0 * a0 + h * h);

    float3 g;
    g.x  = a0.x  * x0.x  + h.x  * x0.y;
    g.yz = a0.yz * x12.xz + h.yz * x12.yw;
    return 130.0 * dot(m, g); // 戻り値は約 -1.0 ~ 1.0
}

// Simplex Fractal Brownian Motion (2D)
float sFBm(float2 p) {
    float f = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        f += a * snoise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return f;
}

#endif // NOISE_HLSLI
