struct Particle
{
	float3 translate;
	uint type; // 0: 親, 1: Trail, 2: Death
	float3 scale;
	float lifeTime;
	float3 velocity;
	float currentTime;
	float4 color;

	// 拡張パラメータ
	float3 rotation;
	float trailTimer; // Trail用タイマー
	float3 rotateSpeed;
	uint emitterIndex;
	float3 startScale;
	uint billboardMode;
	float3 endScale;
	uint atlasSize;
	float4 startColor;
	float4 endColor;
	float4 midColor;
	float3 midScale;
	float midPoint;
};

struct PerView
{
	float4x4 viewProjection;
	float4x4 billboardMatrix;
	float3 worldPosition;
	float cameraNear;
	float cameraFar;
	float3 pad;
};

struct ParticleSortData
{
	float depth;
	uint particleIndex;
};

struct VertexShaderOutput
{
	float4 position : SV_POSITION;
	float4 texcoord : TEXCOORD0;
	float4 color : COLOR0;
	float timeRatio : TEXCOORD1;
	float cameraNear : TEXCOORD2;
	float cameraFar : TEXCOORD3;
};

struct GPUParticleEmitter
{
	// float4 x 1
	uint type;          // 0: Sphere, 1: Beam, 2: Ring, 3: Cylinder
	float3 translate;   // 位置

	// float4 x 2
	float emissionRate;     // 1秒あたりの連続放出数
	float emissionResidue;  // 端数繰り越し用
	float padFreqTime;      // タイマー(パディング)
	int emit;               // 放出フラグ

	// float4 x 3
	float radius;       // Sphere/Ring/Cylinder用: 半径
	float3 direction;   // Beam用: 方向

	// float4 x 4
	float spread;       // Beam用: 広がり
	float velocity;     // Beam用: 速度
	float minLife;      // 最小寿命
	float maxLife;      // 最大寿命

	// float4 x 5
	float3 startScaleMin; // 開始スケール最小
	float pad0;
	// float4 x 6
	float3 startScaleMax; // 開始スケール最大
	float pad1;
	// float4 x 7
	float3 endScaleMin;   // 終了スケール最小
	float pad2;
	// float4 x 8
	float3 endScaleMax;   // 終了スケール最大
	float pad3;

	// float4 x 9
	float4 startColorMin; // 開始色最小
	// float4 x 10
	float4 startColorMax; // 開始色最大
	// float4 x 11
	float4 endColorMin;   // 終了色最小
	// float4 x 12
	float4 endColorMax;   // 終了色最大

	// float4 x 13
	uint colorMode;       // カラーモード
	float gravity;        // 重力
	float damping;        // 空気抵抗
	
	// --- ビルボード（カメラ向け板ポリゴン）の描画モード ---
	// 0: None             (ビルボード無効。通常の3D回転を適用)
	// 1: CameraBillboard  (常にカメラの正面を向く。光の玉や煙などに使用)
	// 2: VelocityBillboard(進行方向に沿って板が向き、伸びる。火の粉の軌跡や雨粒などに使用)
	uint billboardMode;

	// float4 x 14
	uint burstCount;      // そのフレームの追加放出数
	float jitter;         // 座標のゆらぎ
	uint atlasRows;
	uint atlasCols;

	// float4 x 15
	float groundHeight;
	float bounce;
	float attractorStrength;
	uint randomSeed;

	// float4 x 16
	float3 attractorPos;
	uint enableRandomRotation;

	// float4 x 17
	float3 areaSize;
	uint enableTrail;

	// float4 x 18
	uint enableDeathEmit;
	float trailFrequency;
	uint enableLighting;
	float pad8;

	// float4 x 19
	float4 midColorMin;
	// float4 x 20
	float4 midColorMax;
	// float4 x 21
	float3 midScaleMin;
	float pad9;
	// float4 x 22
	float3 midScaleMax;
	float midPoint;

	// float4 x 23
	float3 prevTranslate;
	float pad10;
};

struct ParticleField
{
	uint type; // 0: Directional (Gravity/Wind), 1: Point Attractor, 2: Vortex
	float strength;
	float pad0;
	float pad1;
	
	float3 position;
	float range;
	
	float3 direction;
	float falloff;
	
	float3 axis;
	float pad2;
};