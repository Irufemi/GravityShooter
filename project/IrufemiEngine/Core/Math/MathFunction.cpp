#define NOMINMAX
#include "Core/Math/MathFunction.h"
#include <cmath>
#include <algorithm>

#include "Core/Utility/Ease.h"
#include "Core/Math/Geometry/AABB.h"
#include "Core/Math/Geometry/OBB.h"
#include "Core/Shape/LinePrimitive.h"
#include "Core/Shape/Plane.h"
#include "Core/Shape/Sphere.h"
#include "Core/Shape/Triangle.h"

namespace Irufemi {
namespace Math {

#pragma region 2次元ベクトル関数

Vector2 Add(Vector2 a, Vector2 b) {
    return a + b;
}

Vector2 Subtract(Vector2 a, Vector2 b) {
    return a - b;
}

Vector2 Multiply(float scalar, Vector2 vector) {
    return scalar * vector;
}

float Dot(Vector2 a, Vector2 b) {
    return a.x * b.x + a.y * b.y;
}

float Length(Vector2 vector) {
    return std::sqrt(Dot(vector, vector));
}

Vector2 Normalize(Vector2 vector) {
    float len = Length(vector);
    if (len < 1e-6f) {
        return {0.0f, 0.0f};
    }
    return vector / len;
}

Vector2 ClosestPoint(Vector2 point, const Segment2D& segment) {
    Vector2 ab = segment.end - segment.origin;
    Vector2 ap = point - segment.origin;

    float sq = Dot(ab, ab);
    if (sq < 1e-6f) {
        return segment.origin;
    }

    float t = Dot(ap, ab) / sq;

    if (t < 0.0f) {
        return segment.origin;
    }
    if (t > 1.0f) {
        return segment.end;
    } else {
        return segment.origin + t * ab;
    }
}

#pragma endregion
#pragma region 3次元ベクトル関数

Vector3 Add(Vector3 a, Vector3 b) {
    return a + b;
}

Vector3 Subtract(Vector3 a, Vector3 b) {
    return a - b;
}

Vector3 Multiply(float scalar, Vector3 vector) {
    return scalar * vector;
}

float Dot(Vector3 a, Vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Length(Vector3 vector) {
    return std::sqrt(Dot(vector, vector));
}

Vector3 Normalize(Vector3 vector) {
    float len = Length(vector);
    if (len < 1e-6f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return vector / len;
}

Vector3 Cross(Vector3 a, Vector3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vector3 Project(Vector3 v1, Vector3 v2) {
    return (Dot(v1, v2) / Dot(v2, v2)) * v2;
}

Vector3 ClosestPoint(Vector3 point, const Segment& segment) {
    float diffSq = Dot(segment.diff, segment.diff);
    if (diffSq < 1e-6f) {
        return segment.origin;
    }
    Vector3 a = point - segment.origin;
    float t = Dot(a, segment.diff) / diffSq;
    t = Clamp(t, 0.0f, 1.0f);
    return segment.origin + t * segment.diff;
}

Vector3 ClosestPoint(Vector3 point, const Ray& ray) {
    float diffSq = Dot(ray.diff, ray.diff);
    if (diffSq < 1e-6f) {
        return ray.origin;
    }
    Vector3 a = point - ray.origin;
    float t = Dot(a, ray.diff) / diffSq;
    t = std::max(t, 0.0f);
    return ray.origin + t * ray.diff;
}

Vector3 ClosestPoint(Vector3 point, const Line& line) {
    float diffSq = Dot(line.diff, line.diff);
    if (diffSq < 1e-6f) {
        return line.origin;
    }
    Vector3 a = point - line.origin;
    float t = Dot(a, line.diff) / diffSq;
    return line.origin + t * line.diff;
}

Vector2 Bezier(Vector2 p0, Vector2 p1, Vector2 p2, float t) {
    Vector2 p0p1 = Lerp(p0, p1, t);
    Vector2 p1p2 = Lerp(p1, p2, t);
    return Lerp(p0p1, p1p2, t);
}

Vector2 CatmullRom(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float t) {
    Vector2 p;
    float t2 = t * t;
    float t3 = t2 * t;
    p.x = 0.5f * ((-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3 +
                  (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 + (-p0.x + p2.x) * t + 2.0f * p1.x);
    p.y = 0.5f * ((-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3 +
                  (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 + (-p0.y + p2.y) * t + 2.0f * p1.y);
    return p;
}

Vector3 Bezier(Vector3 p0, Vector3 p1, Vector3 p2, float t) {
    Vector3 p0p1 = Lerp(p0, p1, t);
    Vector3 p1p2 = Lerp(p1, p2, t);
    return Lerp(p0p1, p1p2, t);
}

Vector3 CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) {
    Vector3 p;
    float t2 = t * t;
    float t3 = t2 * t;
    p.x = 0.5f * ((-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3 +
                  (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 + (-p0.x + p2.x) * t + 2.0f * p1.x);
    p.y = 0.5f * ((-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3 +
                  (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 + (-p0.y + p2.y) * t + 2.0f * p1.y);
    p.z = 0.5f * ((-p0.z + 3.0f * p1.z - 3.0f * p2.z + p3.z) * t3 +
                  (2.0f * p0.z - 5.0f * p1.z + 4.0f * p2.z - p3.z) * t2 + (-p0.z + p2.z) * t + 2.0f * p1.z);
    return p;
}

#pragma endregion

#pragma region 4次元ベクトル関数
Vector4 Add(Vector4 a, Vector4 b) {
    return a + b;
}

Vector4 Subtract(Vector4 a, Vector4 b) {
    return a - b;
}

Vector4 Multiply(float s, Vector4 v) {
    return s * v;
}

float Dot(Vector4 a, Vector4 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

float Length(Vector4 v) {
    return std::sqrt(Dot(v, v));
}

Vector4 Normalize(Vector4 v) {
    float len = Length(v);
    if (len > 1e-6f) {
        return v / len;
    }
    return {0.0f, 0.0f, 0.0f, 0.0f};
}
#pragma endregion

#pragma region 4x4行列関数

Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2) {
    return m1 + m2;
}

Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2) {
    return m1 - m2;
}

Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
    return m1 * m2;
}

Matrix4x4 Inverse(const Matrix4x4& m) {
    Matrix4x4 inv{};

    const float* a = &m.m[0][0];
    float* o = &inv.m[0][0];

    o[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] +
           a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
    o[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] -
           a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
    o[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] -
           a[13] * a[3] * a[6];
    o[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] +
           a[9] * a[3] * a[6];
    o[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] -
           a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
    o[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] +
           a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
    o[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] -
           a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
    o[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] -
           a[8] * a[3] * a[6];
    o[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] +
           a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
    o[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] -
           a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
    o[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] +
            a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
    o[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] +
            a[8] * a[3] * a[5];
    o[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] -
            a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
    o[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] +
            a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
    o[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] -
            a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
    o[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] -
            a[8] * a[2] * a[5];

    float det = a[0] * o[0] + a[1] * o[4] + a[2] * o[8] + a[3] * o[12];

    if (std::abs(det) < 1e-12f) {
        return MakeIdentity4x4();
    }

    float invDet = 1.0f / det;
    for (int i = 0; i < 16; ++i) {
        o[i] *= invDet;
    }

    return inv;
}

Matrix4x4 Transpose(const Matrix4x4& m) {
    Matrix4x4 tMatrix{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            tMatrix.m[i][j] = m.m[j][i];
        }
    }
    return tMatrix;
}

Matrix4x4 MakeIdentity4x4() {
    // clang-format off
        return {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    // clang-format on
}

Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
    Matrix4x4 result = MakeIdentity4x4();
    result.m[3][0] = translate.x;
    result.m[3][1] = translate.y;
    result.m[3][2] = translate.z;
    return result;
}

Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
    Matrix4x4 result = MakeIdentity4x4();
    result.m[0][0] = scale.x;
    result.m[1][1] = scale.y;
    result.m[2][2] = scale.z;
    return result;
}

Vector3 Transform(const Vector3& vector, const Matrix4x4& m) {
    Vector3 result{};
    result.x = vector.x * m.m[0][0] + vector.y * m.m[1][0] + vector.z * m.m[2][0] + m.m[3][0];
    result.y = vector.x * m.m[0][1] + vector.y * m.m[1][1] + vector.z * m.m[2][1] + m.m[3][1];
    result.z = vector.x * m.m[0][2] + vector.y * m.m[1][2] + vector.z * m.m[2][2] + m.m[3][2];
    float w = vector.x * m.m[0][3] + vector.y * m.m[1][3] + vector.z * m.m[2][3] + m.m[3][3];
    if (std::abs(w) > 1e-12f) {
        result.x /= w;
        result.y /= w;
        result.z /= w;
    }
    return result;
}

Vector3 TransformNormal(const Vector3& vector, const Matrix4x4& m) {
    Vector3 result{};
    result.x = vector.x * m.m[0][0] + vector.y * m.m[1][0] + vector.z * m.m[2][0];
    result.y = vector.x * m.m[0][1] + vector.y * m.m[1][1] + vector.z * m.m[2][1];
    result.z = vector.x * m.m[0][2] + vector.y * m.m[1][2] + vector.z * m.m[2][2];
    return result;
}

Matrix4x4 MakeRotateXMatrix(float theta) {
    float c = std::cos(theta);
    float s = std::sin(theta);
    // clang-format off
        return {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, c,    s,    0.0f,
            0.0f, -s,   c,    0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    // clang-format on
}

Matrix4x4 MakeRotateYMatrix(float theta) {
    float c = std::cos(theta);
    float s = std::sin(theta);
    // clang-format off
        return {
            c,    0.0f, -s,   0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            s,    0.0f, c,    0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    // clang-format on
}

Matrix4x4 MakeRotateZMatrix(float theta) {
    float c = std::cos(theta);
    float s = std::sin(theta);
    // clang-format off
        return {
            c,    s,    0.0f, 0.0f,
            -s,   c,    0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    // clang-format on
}

Matrix4x4 MakeRotateXYZMatrix(const Vector3& rotate) {
    return MakeRotateXMatrix(rotate.x) * MakeRotateYMatrix(rotate.y) * MakeRotateZMatrix(rotate.z);
}

Matrix4x4 MakeRotateXYZMatrix(float x, float y, float z) {
    return MakeRotateXYZMatrix(Vector3{x, y, z});
}

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
    return MakeScaleMatrix(scale) * MakeRotateXYZMatrix(rotate) * Math::MakeTranslateMatrix(translate);
}

Matrix4x4 MakeAffineMatrix(const Vector3& scale, float rotateX, float rotateY, float rotateZ,
                           const Vector3& translate) {
    return MakeAffineMatrix(scale, Vector3{rotateX, rotateY, rotateZ}, translate);
}

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotateQuaternion, const Vector3& translate) {
    return MakeScaleMatrix(scale) * MakeRotateMatrix(rotateQuaternion) * MakeTranslateMatrix(translate);
}

Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
    float h = 1.0f / std::tan(fovY / 2.0f);
    float w = h / aspectRatio;
    float a = farClip / (farClip - nearClip);
    float b = (-nearClip * farClip) / (farClip - nearClip);
    // clang-format off
        return {
            w,    0.0f, 0.0f, 0.0f,
            0.0f, h,    0.0f, 0.0f,
            0.0f, 0.0f, a,    1.0f,
            0.0f, 0.0f, b,    0.0f
        };
    // clang-format on
}

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
    // clang-format off
        return {
            2.0f / (right - left),           0.0f,                            0.0f,                          0.0f,
            0.0f,                            2.0f / (top - bottom),           0.0f,                          0.0f,
            0.0f,                            0.0f,                            1.0f / (farClip - nearClip),   0.0f,
            (left + right) / (left - right), (top + bottom) / (bottom - top), nearClip / (nearClip - farClip), 1.0f
        };
    // clang-format on
}

Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
    // clang-format off
        return {
            width / 2.0f,          0.0f,           0.0f,                0.0f,
            0.0f,                  -height / 2.0f, 0.0f,                0.0f,
            0.0f,                  0.0f,           maxDepth - minDepth, 0.0f,
            left + width / 2.0f,   top + height / 2.0f, minDepth,       1.0f
        };
    // clang-format on
}

Ray ScreenPointToRay(Vector2 mousePos, float screenWidth, float screenHeight, const Matrix4x4& viewProjectionInverse) {
    // スクリーン座標を NDC (Normalized Device Coordinates) に変換
    // x: -1.0 ~ 1.0, y: 1.0 ~ -1.0 (DirectXのNDC系)
    float ndcX = (2.0f * mousePos.x) / screenWidth - 1.0f;
    float ndcY = 1.0f - (2.0f * mousePos.y) / screenHeight;

    // Nearクリップ面とFarクリップ面でのNDC座標
    Vector3 ndcNear = {ndcX, ndcY, 0.0f};
    Vector3 ndcFar = {ndcX, ndcY, 1.0f};

    // 逆行列を使ってワールド座標に変換
    Vector3 worldNear = Transform(ndcNear, viewProjectionInverse);
    Vector3 worldFar = Transform(ndcFar, viewProjectionInverse);

    // レイを生成 (始点はNear面、方向はNearからFarへ)
    Ray ray;
    ray.origin = worldNear;
    ray.diff = Normalize(worldFar - worldNear);

    return ray;
}

Matrix4x4 MakeRotateAxisAngle(Vector3 axis, float angle) {
    float c = std::cos(angle);
    float s = std::sin(angle);
    Vector3 n = Normalize(axis);
    // clang-format off
        return {
            n.x * n.x * (1 - c) + c,       n.x * n.y * (1 - c) + n.z * s, n.x * n.z * (1 - c) - n.y * s, 0.0f,
            n.x * n.y * (1 - c) - n.z * s, n.y * n.y * (1 - c) + c,       n.y * n.z * (1 - c) + n.x * s, 0.0f,
            n.x * n.z * (1 - c) + n.y * s, n.y * n.z * (1 - c) - n.x * s, n.z * n.z * (1 - c) + c,       0.0f,
            0.0f,                          0.0f,                          0.0f,                          1.0f
        };
    // clang-format on
}

Matrix4x4 DirectionToDirection(Vector3 from, Vector3 to) {
    Vector3 f = Normalize(from);
    Vector3 t = Normalize(to);
    float cosTheta = Dot(f, t);
    constexpr float kEpsilon = 1e-6f;

    if (cosTheta > 1.0f - kEpsilon) {
        return MakeIdentity4x4();
    }

    if (cosTheta < -1.0f + kEpsilon) {
        Vector3 axis = Perpendicular(f);
        return MakeRotateAxisAngle(axis, PI);
    }

    Vector3 axis = Cross(f, t);
    return MakeRotateAxisAngle(axis, std::acos(Clamp(cosTheta, -1.0f, 1.0f)));
}

Vector3 ExtractEulerFromMatrix(const Matrix4x4& matrix) {
    // 抽出前にスケールを除去する（各軸ベクトルを正規化）
    Vector3 lx = Normalize(Vector3{matrix.m[0][0], matrix.m[0][1], matrix.m[0][2]});
    Vector3 ly = Normalize(Vector3{matrix.m[1][0], matrix.m[1][1], matrix.m[1][2]});
    Vector3 lz = Normalize(Vector3{matrix.m[2][0], matrix.m[2][1], matrix.m[2][2]});

    Vector3 euler{};
    // MakeRotateXYZMatrix (Rx * Ry * Rz) に対応する抽出式
    float cy = std::sqrt(lx.x * lx.x + lx.y * lx.y); // 元の matrix.m[0][0], matrix.m[0][1]
    constexpr float kEpsilon = 1e-6f;

    if (cy > kEpsilon) {
        euler.x = std::atan2(ly.z, lz.z);
        euler.y = std::atan2(-lx.z, cy);
        euler.z = std::atan2(lx.y, lx.x);
    } else {
        euler.x = std::atan2(-lz.y, ly.y);
        euler.y = std::atan2(-lx.z, cy);
        euler.z = 0.0f;
    }
    return euler;
}

Vector3 LookRotation(Vector3 forward, Vector3 up) {
    if (forward.LengthSquared() < 0.0001f) {
        return {0.0f, 0.0f, 0.0f};
    }
    forward.Normalize();

    Vector3 right = Cross(up, forward);
    if (right.LengthSquared() < 0.0001f) {
        // 前方と上が平行な場合へのフォールバック
        right = Cross(Vector3{1.0f, 0.0f, 0.0f}, forward);
        if (right.LengthSquared() < 0.0001f) {
            right = Cross(Vector3{0.0f, 1.0f, 0.0f}, forward);
        }
    }
    right.Normalize();
    up = Cross(forward, right).GetNormalized();

    Matrix4x4 rotMat = MakeIdentity4x4();
    rotMat.m[0][0] = right.x;
    rotMat.m[0][1] = right.y;
    rotMat.m[0][2] = right.z;
    rotMat.m[1][0] = up.x;
    rotMat.m[1][1] = up.y;
    rotMat.m[1][2] = up.z;
    rotMat.m[2][0] = forward.x;
    rotMat.m[2][1] = forward.y;
    rotMat.m[2][2] = forward.z;

    return ExtractEulerFromMatrix(rotMat);
}

#pragma endregion

#pragma region Quaternion

Quaternion Multiply(const Quaternion& lhs, const Quaternion& rhs) {
    return lhs * rhs;
}

Quaternion IdentityQuaternion() {
    return {0.0f, 0.0f, 0.0f, 1.0f};
}

Quaternion Conjugate(const Quaternion& q) {
    return {-q.x, -q.y, -q.z, q.w};
}

float Norm(const Quaternion& q) {
    return std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
}

Quaternion Normalize(const Quaternion& q) {
    float n = Norm(q);
    if (n < 1e-6f) {
        return IdentityQuaternion();
    }
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}

Quaternion Inverse(const Quaternion& q) {
    float normSq = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (normSq < 1e-12f) {
        return IdentityQuaternion();
    }
    Quaternion conj = Conjugate(q);
    return {conj.x / normSq, conj.y / normSq, conj.z / normSq, conj.w / normSq};
}

Quaternion MakeRotateAxisAngleQuaternion(Vector3 axis, float angle) {
    Vector3 n = Math::Normalize(axis);
    float half = angle * 0.5f;
    float s = std::sin(half);
    return {n.x * s, n.y * s, n.z * s, std::cos(half)};
}

Vector3 RotateVector(const Vector3& vector, const Quaternion& quaternion) {
    Vector3 qv{quaternion.x, quaternion.y, quaternion.z};
    Vector3 uv = Cross(qv, vector);
    Vector3 uuv = Cross(qv, uv);
    return vector + ((uv * quaternion.w) + uuv) * 2.0f;
}

Matrix4x4 MakeRotateMatrix(const Quaternion& q) {
    float xx = q.x * q.x;
    float yy = q.y * q.y;
    float zz = q.z * q.z;
    float ww = q.w * q.w;
    float xy = q.x * q.y;
    float xz = q.x * q.z;
    float yz = q.y * q.z;
    float wx = q.w * q.x;
    float wy = q.w * q.y;
    float wz = q.w * q.z;

    // clang-format off
        return {
            ww + xx - yy - zz, 2.0f * (xy + wz),   2.0f * (xz - wy),   0.0f,
            2.0f * (xy - wz),   ww - xx + yy - zz, 2.0f * (yz + wx),   0.0f,
            2.0f * (xz + wy),   2.0f * (yz - wx),   ww - xx - yy + zz, 0.0f,
            0.0f,               0.0f,               0.0f,               1.0f
        };
    // clang-format on
}

Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t) {
    float dot = q0.x * q1.x + q0.y * q1.y + q0.z * q1.z + q0.w * q1.w;
    Quaternion target = q1;

    if (dot < 0.0f) {
        target = {-q1.x, -q1.y, -q1.z, -q1.w};
        dot = -dot;
    }

    if (dot > 0.9995f) {
        return Math::Normalize(Quaternion{q0.x + t * (target.x - q0.x), q0.y + t * (target.y - q0.y),
                                          q0.z + t * (target.z - q0.z), q0.w + t * (target.w - q0.w)});
    }

    float theta = std::acos(dot);
    float sinTheta = std::sin(theta);
    float scale0 = std::sin((1.0f - t) * theta) / sinTheta;
    float scale1 = std::sin(t * theta) / sinTheta;

    // clang-format off
        return {
            scale0 * q0.x + scale1 * target.x,
            scale0 * q0.y + scale1 * target.y,
            scale0 * q0.z + scale1 * target.z,
            scale0 * q0.w + scale1 * target.w
        };
    // clang-format on
}

Vector3 ToEuler(const Quaternion& q) {
    // MakeRotateMatrix(q) の要素と MakeRotateXYZMatrix の関係から直接導出
    // 行列アロケーションおよび 3 回のベクトル正規化（std::sqrt）を撤廃
    float m00 = q.w * q.w + q.x * q.x - q.y * q.y - q.z * q.z;
    float m01 = 2.0f * (q.x * q.y + q.w * q.z);
    float m02 = 2.0f * (q.x * q.z - q.w * q.y);
    float m12 = 2.0f * (q.y * q.z + q.w * q.x);
    float m22 = q.w * q.w - q.x * q.x - q.y * q.y + q.z * q.z;
    float m21 = 2.0f * (q.y * q.z - q.w * q.x);
    float m11 = q.w * q.w - q.x * q.x + q.y * q.y - q.z * q.z;

    float cy = std::sqrt(m00 * m00 + m01 * m01);
    constexpr float kEpsilon = 1e-6f;
    Vector3 euler{};

    if (cy > kEpsilon) {
        euler.x = std::atan2(m12, m22);
        euler.y = std::atan2(-m02, cy);
        euler.z = std::atan2(m01, m00);
    } else {
        // ジンバルロック時のフォールバック (pitch = ±90 deg)
        euler.x = std::atan2(-m21, m11);
        euler.y = std::atan2(-m02, cy);
        euler.z = 0.0f;
    }
    return euler;
}

Quaternion ToQuaternionFromEuler(Vector3 euler) {
    float cx = std::cos(euler.x * 0.5f);
    float sx = std::sin(euler.x * 0.5f);
    float cy = std::cos(euler.y * 0.5f);
    float sy = std::sin(euler.y * 0.5f);
    float cz = std::cos(euler.z * 0.5f);
    float sz = std::sin(euler.z * 0.5f);

    Quaternion q;
    // XYZ順 (Roll-Pitch-Yawの適用順) に合わせた直接計算
    q.w = cx * cy * cz + sx * sy * sz;
    q.x = sx * cy * cz - cx * sy * sz;
    q.y = cx * sy * cz + sx * cy * sz;
    q.z = cx * cy * sz - sx * sy * cz;

    return Normalize(q);
}

Quaternion ToQuaternionFromMatrix(const Matrix4x4& m) {
    // スケーリング成分を取り除き、純粋な回転行列成分を抽出する
    Vector3 xaxis = Normalize(Vector3{m.m[0][0], m.m[0][1], m.m[0][2]});
    Vector3 yaxis = Normalize(Vector3{m.m[1][0], m.m[1][1], m.m[1][2]});
    Vector3 zaxis = Normalize(Vector3{m.m[2][0], m.m[2][1], m.m[2][2]});

    float m00 = xaxis.x, m01 = xaxis.y, m02 = xaxis.z;
    float m10 = yaxis.x, m11 = yaxis.y, m12 = yaxis.z;
    float m20 = zaxis.x, m21 = zaxis.y, m22 = zaxis.z;

    Quaternion q;
    float trace = m00 + m11 + m22;
    if (trace > 0.0f) {
        float s = 0.5f / std::sqrt(trace + 1.0f);
        q.w = 0.25f / s;
        q.x = (m12 - m21) * s;
        q.y = (m20 - m02) * s;
        q.z = (m01 - m10) * s;
    } else {
        if (m00 > m11 && m00 > m22) {
            float s = 2.0f * std::sqrt(1.0f + m00 - m11 - m22);
            q.w = (m12 - m21) / s;
            q.x = 0.25f * s;
            q.y = (m01 + m10) / s;
            q.z = (m20 + m02) / s;
        } else if (m11 > m22) {
            float s = 2.0f * std::sqrt(1.0f + m11 - m00 - m22);
            q.w = (m20 - m02) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25f * s;
            q.z = (m12 + m21) / s;
        } else {
            float s = 2.0f * std::sqrt(1.0f + m22 - m00 - m11);
            q.w = (m01 - m10) / s;
            q.x = (m20 + m02) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25f * s;
        }
    }
    return Normalize(q);
}

Quaternion ExtractRotationSafe(const Matrix4x4& mat, const Vector3& scaleSign) {
    float sx = std::copysign(1.0f, scaleSign.x);
    float sy = std::copysign(1.0f, scaleSign.y);
    float sz = std::copysign(1.0f, scaleSign.z);

    Matrix4x4 pureRotMat = mat;
    pureRotMat.m[0][0] *= sx;
    pureRotMat.m[0][1] *= sx;
    pureRotMat.m[0][2] *= sx;
    pureRotMat.m[1][0] *= sy;
    pureRotMat.m[1][1] *= sy;
    pureRotMat.m[1][2] *= sy;
    pureRotMat.m[2][0] *= sz;
    pureRotMat.m[2][1] *= sz;
    pureRotMat.m[2][2] *= sz;

    return Normalize(ToQuaternionFromMatrix(pureRotMat));
}

void DecomposeAffineMatrixSafe(const Matrix4x4& mat, const Vector3& scaleSignHint, Vector3& outPosition,
                               Quaternion& outRotation, Vector3& outScale) {

    outPosition = {mat.m[3][0], mat.m[3][1], mat.m[3][2]};

    Vector3 xaxis = {mat.m[0][0], mat.m[0][1], mat.m[0][2]};
    Vector3 yaxis = {mat.m[1][0], mat.m[1][1], mat.m[1][2]};
    Vector3 zaxis = {mat.m[2][0], mat.m[2][1], mat.m[2][2]};

    // 3x3 行列式で反転状態を確認
    float det = xaxis.x * (yaxis.y * zaxis.z - yaxis.z * zaxis.y) - xaxis.y * (yaxis.x * zaxis.z - yaxis.z * zaxis.x) +
                xaxis.z * (yaxis.x * zaxis.y - yaxis.y * zaxis.x);

    float sx = std::copysign(1.0f, scaleSignHint.x);
    float sy = std::copysign(1.0f, scaleSignHint.y);
    float sz = std::copysign(1.0f, scaleSignHint.z);

    // 行列のフリップ状態とヒント符号の不整合がある場合、X軸の符号を反転して回転抽出の破綻を防ぐ
    if ((sx * sy * sz) * det < 0.0f) {
        sx = -sx;
    }

    outScale = {std::copysign(Length(xaxis), sx), std::copysign(Length(yaxis), sy), std::copysign(Length(zaxis), sz)};

    Vector3 finalSign = {sx, sy, sz};
    outRotation = ExtractRotationSafe(mat, finalSign);
}

#pragma endregion

Vector3 Perpendicular(const Vector3& vector) {
    if (std::abs(vector.x) > 1e-6f || std::abs(vector.y) > 1e-6f) {
        return Math::Normalize(Vector3{-vector.y, vector.x, 0.0f});
    }
    return Math::Normalize(Vector3{0.0f, -vector.z, vector.y});
}

float NormalizeAngle(float angle) {
    angle = fmod(angle + PI, 2.0f * PI);
    if (angle < 0.0f) {
        angle += 2.0f * PI;
    }
    return angle - PI;
}

LetterboxInfo CalculateLetterbox(float clientW, float clientH, float gameW, float gameH) {
    float aspectGame = gameW / gameH;
    float aspectClient = clientW / clientH;
    LetterboxInfo info{};
    if (aspectClient > aspectGame) {
        info.viewH = clientH;
        info.viewW = clientH * aspectGame;
        info.viewX = (clientW - info.viewW) * 0.5f;
        info.viewY = 0.0f;
    } else {
        info.viewW = clientW;
        info.viewH = clientW / aspectGame;
        info.viewX = 0.0f;
        info.viewY = (clientH - info.viewH) * 0.5f;
    }
    return info;
}

} // namespace Math

} // namespace Irufemi
