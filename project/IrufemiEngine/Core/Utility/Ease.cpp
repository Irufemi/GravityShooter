#include "Core/Utility/Ease.h"

#include "Core/Math/Math.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Irufemi::Math;

// Irufemi::Quaternion 線形補間(最短経路を選び正規化して返す)
Irufemi::Quaternion Lerp(const Irufemi::Quaternion& q0, const Irufemi::Quaternion& q1, float t) {
    // 最短経路のため内積を計算し、負なら q1 を反転
    float dot = q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w;
    Irufemi::Quaternion q1s = q1;
    if (dot < 0.0f) {
        q1s.x = -q1s.x;
        q1s.y = -q1s.y;
        q1s.z = -q1s.z;
        q1s.w = -q1s.w;
    }

    Irufemi::Quaternion res;
    res.x = q0.x + t * (q1s.x - q0.x);
    res.y = q0.y + t * (q1s.y - q0.y);
    res.z = q0.z + t * (q1s.z - q0.z);
    res.w = q0.w + t * (q1s.w - q0.w);

    // 正規化して返す(NLERP)
    return Normalize(res);
}

// 球面線形補間
Irufemi::Vector3 Slerp(const Irufemi::Vector3& v1, const Irufemi::Vector3& v2, float t) {
    // --- ユーティリティ ---
    auto len = [](const Irufemi::Vector3& v) -> float { return std::sqrt(Dot(v, v)); };
    auto safeNormalize = [](const Irufemi::Vector3& v) -> Irufemi::Vector3 {
        float l = std::sqrt(Dot(v, v));
        if (l <= 1e-8f) {
            return Irufemi::Vector3{0, 0, 0};
        }
        return Multiply(1.0f / l, v);
    };

    // 長さ
    float l1 = len(v1);
    float l2 = len(v2);

    // どちらもゼロ長
    if (l1 <= 1e-8f && l2 <= 1e-8f) {
        return Irufemi::Vector3{0, 0, 0};
    }
    // 一方がゼロ長：もう片方の方向へ長さだけLerp
    if (l1 <= 1e-8f) {
        return Multiply(std::clamp(t, 0.0f, 1.0f) * l2, safeNormalize(v2));
    }
    if (l2 <= 1e-8f) {
        return Multiply((1.0f - std::clamp(t, 0.0f, 1.0f)) * l1, safeNormalize(v1));
    }

    // 単位方向
    Irufemi::Vector3 u1 = Multiply(1.0f / l1, v1);
    Irufemi::Vector3 u2 = Multiply(1.0f / l2, v2);

    // 角度
    float d = std::clamp(Dot(u1, u2), -1.0f, 1.0f);
    float theta = std::acos(d);
    float length = Lerp(l1, l2, t); // 長さは線形補間

    // 角度が極小なら nlerp 的に処理(sinθ ≒ 0 回避)
    if (theta < 1e-5f) {
        Irufemi::Vector3 dir = safeNormalize(Add(Multiply(1.0f - t, u1), Multiply(t, u2)));
        return Multiply(length, dir);
    }

    // 180°付近(無数の経路がある)対策：任意の直交方向を選んで回す
    if (std::abs(std::numbers::pi_v<float> - theta) < 1e-4f) {
        // u1 とほぼ平行でない基準軸を選ぶ
        Irufemi::Vector3 axis = (std::abs(u1.x) < 0.9f) ? Irufemi::Vector3{1, 0, 0} : Irufemi::Vector3{0, 1, 0};
        Irufemi::Vector3 ortho = safeNormalize(Cross(u1, axis));
        Irufemi::Vector3 vperp = safeNormalize(Cross(ortho, u1)); // u1 に直交する単位ベクトル
        Irufemi::Vector3 dir = Add(Multiply(std::cos(std::numbers::pi_v<float> * t), u1),
                                   Multiply(std::sin(std::numbers::pi_v<float> * t), vperp));
        return Multiply(length, dir);
    }

    // 通常のSlerp
    float sinTheta = std::sin(theta);
    float a = std::sin((1.0f - t) * theta) / sinTheta;
    float b = std::sin(t * theta) / sinTheta;
    Irufemi::Vector3 dir = Add(Multiply(a, u1), Multiply(b, u2));
    return Multiply(length, dir);
}

float EaseInSine(float num) {
    return 1.0f - std::cosf((num * std::numbers::pi_v<float> / 2.0f));
}

float EaseOutSine(float num) {
    return std::sinf(num * std::numbers::pi_v<float> / 2.0f);
}

float EaseInOutSine(float num) {
    return -(std::cosf(std::numbers::pi_v<float> * num) - 1.0f) / 2.0f;
}

float EaseInQuad(float num) {
    return num * num;
}

float EaseOutQuad(float num) {
    return 1.0f - (1.0f - num) * (1.0f - num);
}

float EaseInOutQuad(float num) {
    if (num < 0.5f) {
        return 2.0f * num * num;
    }
    float p = -2.0f * num + 2.0f;
    return 1.0f - (p * p) * 0.5f;
}

float EaseInCubic(float num) {
    return num * num * num;
}

float EaseOutCubic(float num) {
    float inv = 1.0f - num;
    return 1.0f - inv * inv * inv;
}

float EaseInOutCubic(float num) {
    if (num < 0.5f) {
        return 4.0f * num * num * num;
    }
    float p = -2.0f * num + 2.0f;
    return 1.0f - (p * p * p) * 0.5f;
}

float EaseInQuart(float num) {
    float num2 = num * num;
    return num2 * num2;
}

float EaseOutQuart(float num) {
    float inv = 1.0f - num;
    float inv2 = inv * inv;
    return 1.0f - inv2 * inv2;
}

float EaseInOutQuart(float num) {
    if (num < 0.5f) {
        float num2 = num * num;
        return 8.0f * num2 * num2;
    }
    float p = -2.0f * num + 2.0f;
    float p2 = p * p;
    return 1.0f - (p2 * p2) * 0.5f;
}

float EaseInQuint(float num) {
    float num2 = num * num;
    return num2 * num2 * num;
}

float EaseOutQuint(float num) {
    float inv = 1.0f - num;
    float inv2 = inv * inv;
    return 1.0f - inv2 * inv2 * inv;
}

float EaseInOutQuint(float num) {
    if (num < 0.5f) {
        float num2 = num * num;
        return 16.0f * num2 * num2 * num;
    }
    float p = -2.0f * num + 2.0f;
    float p2 = p * p;
    return 1.0f - (p2 * p2 * p) * 0.5f;
}

float EvaluateEase(EaseType type, float t) {
    switch (type) {
    case EaseType::Linear:
        return t;
    case EaseType::EaseInSine:
        return EaseInSine(t);
    case EaseType::EaseOutSine:
        return EaseOutSine(t);
    case EaseType::EaseInOutSine:
        return EaseInOutSine(t);
    case EaseType::EaseInQuad:
        return EaseInQuad(t);
    case EaseType::EaseOutQuad:
        return EaseOutQuad(t);
    case EaseType::EaseInOutQuad:
        return EaseInOutQuad(t);
    case EaseType::EaseInCubic:
        return EaseInCubic(t);
    case EaseType::EaseOutCubic:
        return EaseOutCubic(t);
    case EaseType::EaseInOutCubic:
        return EaseInOutCubic(t);
    case EaseType::EaseInQuart:
        return EaseInQuart(t);
    case EaseType::EaseOutQuart:
        return EaseOutQuart(t);
    case EaseType::EaseInOutQuart:
        return EaseInOutQuart(t);
    case EaseType::EaseInQuint:
        return EaseInQuint(t);
    case EaseType::EaseOutQuint:
        return EaseOutQuint(t);
    case EaseType::EaseInOutQuint:
        return EaseInOutQuint(t);
    default:
        return t;
    }
}
