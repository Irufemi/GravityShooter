#pragma once

/**
 * @file PostProcessParameters.hlsli
 * @brief ポストプロセス用パラメータ構造体定義
 */

struct NoiseParams {
    float intensity;
    float time;
};

struct LightShaftsParams {
    float32_t2 lightScreenPos;
    float32_t density;
    float32_t decay;
    float32_t weight;
    float32_t exposure;
    int32_t samples;
    float32_t pad;
};

struct CustomEffectParams {
    float32_t4 color1;
    float32_t4 color2;
    float32_t param1;
    float32_t param2;
    float32_t param3;
    float32_t param4;
};

struct DualKawaseBlurParams {
    float blurRadius;
    float intensity;
    int iterationCount;
    float pad;
};

struct OutlineParams {
    float32_t intensity;
    float32_t3 pad;
    float32_t4x4 projectionInverse;
};

struct HSVParams {
    float32_t hue;
    float32_t saturation;
    float32_t value;
};

struct SlideParams {
    float32_t4 color;
    float32_t threshold;
};

struct FadeParams {
    float32_t4 color;
    float32_t intensity;
};

struct SmoothingParams {
    float32_t2 direction;   // ぼかし方向 ({1,0} or {0,1})
    int32_t kernelSize;
    float32_t pad;
};

struct GlitchParams {
    float intensity;
    float time;
    float edgeMaskStrength;
    float probability;

    float blockSizeX;
    float blockSizeY;
    float offsetBase;
    float offsetMax;

    float rgbShiftBase;
    float rgbShiftMax;
    float scanlineFreq;
    float scanlineIntensity;

    float32_t4 color;
};

struct BloomParams {
    float32_t2 direction;
    float32_t threshold;
    float32_t sigma;
    float32_t intensity;
    int32_t kernelSize;
};

struct DissolveParams {
    float32_t4 edgeColor;
    float32_t4 backgroundColor; // C++側構造体とのメモリレイアウト整合用
    float32_t threshold;
    float32_t edgeRange;
    int32_t noiseType;
};

struct PostProcessParams {
    int32_t effectCount;
    int32_t useMask;
    int32_t pad0_0;
    int32_t pad0_1;
    int4 effects[4];      // C++側の int32_t[16] と完全に一致させるため int4[4] に変更
    
    // Vignette
    float32_t4 vignetteColor;
    float32_t vignetteRadius;
    float32_t vignetteSoftness;
    float32_t2 pad1;
    
    // Noise
    float32_t noiseIntensity;
    float32_t noiseTime;
    
    // Dissolve
    float32_t4 dissolveEdgeColor;
    float32_t4 dissolveBackgroundColor;
    float32_t dissolveThreshold;
    float32_t dissolveEdgeRange;
    
    // HSV
    float32_t hsvHue;
    float32_t hsvSaturation;
    float32_t hsvValue;
    
    // ToneMapping
    float32_t toneMappingExposure;
    
    // Fade
    float32_t4 fadeColor;
    float32_t fadeIntensity;
    
    // Slide
    float32_t4 slideColor;
    float32_t slideThreshold;
    
    // Outline
    float32_t4x4 projectionInverse;
    float32_t outlineIntensity;
    int32_t maskOutlineMaxRadius;
    float32_t2 pad_outline;
    
    // RadialBlur
    float32_t2 radialBlurCenter;
    float32_t radialBlurWidth;
    int32_t radialBlurSamples;
    
    // Glitch
    float32_t glitchIntensity;
    float32_t glitchTime;
    float32_t glitchEdgeMaskStrength;
    float32_t glitchProbability;

    float32_t glitchBlockSizeX;
    float32_t glitchBlockSizeY;
    float32_t glitchOffsetBase;
    float32_t glitchOffsetMax;

    float32_t glitchRgbShiftBase;
    float32_t glitchRgbShiftMax;
    float32_t glitchScanlineFreq;
    float32_t glitchScanlineIntensity;
    float32_t4 glitchColor;
    
    // LuminanceBasedOutline
    float32_t4 luminanceOutlineColor;
    float32_t luminanceOutlineThreshold;
    float32_t3 pad_lumOutline;
    
    // Pixelation
    float32_t pixelationSize;
    float32_t3 pad_pixelation;
    
    // Pointillism
    float32_t pointillismStrokeSize;
    float32_t pointillismColorSteps;
    float32_t2 pad_pointillism;
    
    // Posterization
    float32_t posterizationSteps;
    float32_t3 pad_posterization;
    
    // NightVision
    float32_t nightVisionIntensity;
    float32_t nightVisionTime;
    float32_t2 pad_nightVision;
    
    // Kaleidoscope
    float32_t kaleidoscopeSegments;
    float32_t3 pad_kaleidoscope;

    // ChromaticAberration
    float32_t chromaticAberrationIntensity;
    float32_t3 pad_chromaticAberration;

    // DisplacementMap
    float32_t displacementMapIntensity;
    float32_t displacementMapTime;
    float32_t displacementMapTimeScale;
    float32_t pad_displacementMap;

    // DirectionalBlur
    float32_t2 directionalBlurDirection;
    float32_t directionalBlurStrength;
    int directionalBlurSamples;

    // Halftone
    float32_t halftoneScale;
    float32_t halftoneAngle;
    float32_t halftoneBlend;
    float32_t pad_halftone;

    // DepthOfField
    float32_t dofFocusDistance;
    float32_t dofFocusRange;
    float32_t dofBlurSize;
    int32_t dofSamples;

    // GravityDistortion
    float32_t2 gravityDistortionCenter;
    float32_t gravityDistortionRadius;
    float32_t gravityDistortionIntensity;
    float32_t gravityDistortionWaveWidth;
    float32_t gravityDistortionChromaticRatio;
    float32_t gravityDistortionTime;
    float32_t pad_gravityDistortion;
};

struct VignetteParams {
    float4 color;
    float radius;
    float softness;
    float2 pad;
};

struct GaussianParams {
    float32_t2 direction;
    float32_t sigma;
    int32_t kernelSize;
};

struct RadialBlurParams {
    float32_t2 center;      // 中心点 (0.5, 0.5 等)
    float32_t blurWidth;    // ぼかしの幅 (0.01 等)
    int32_t numSamples;     // サンプリング数 (10 等)
};

struct ToneMappingParams {
    float32_t exposure; // 露出補正
    float32_t3 padding;
};

