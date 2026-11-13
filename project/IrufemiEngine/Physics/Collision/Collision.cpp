#include "Physics/Collision/Collision.h"
#include "Core/Math/Math.h"
#include "Core/Math/Geometry/AABB.h"
#include "Core/Math/Geometry/OBB.h"
#include "Core/Math/Geometry/Frustum.h"
#include "Core/Shape/Plane.h"
#include "Core/Shape/Sphere.h"
#include "Core/Shape/Triangle.h"
#include "Core/Shape/LinePrimitive.h"
#include "Core/Math/Matrix4x4.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <algorithm>

namespace Irufemi {
namespace Collision {

// Helper functions for min/max
static float MinFloat(float a, float b) {
    return a < b ? a : b;
}
static float MaxFloat(float a, float b) {
    return a > b ? a : b;
}

CollisionResult GetCollisionResult(const Sphere& a, const Sphere& b) {
    CollisionResult result;
    Vector3 diff = Math::Subtract(a.center, b.center);
    float dist = Math::Length(diff);
    float sumRadius = a.radius + b.radius;
    if (dist < sumRadius) {
        result.isHit = true;
        result.depth = sumRadius - dist;
        if (dist > 1e-5f) {
            result.normal = Math::Normalize(diff);
        } else {
            result.normal = {0.0f, 1.0f, 0.0f}; // 完全に重なった場合は上方向
        }
    }
    return result;
}

CollisionResult GetCollisionResult(const AABB& a, const AABB& b) {
    CollisionResult result;
    float overlapX = (std::min)(a.max.x, b.max.x) - (std::max)(a.min.x, b.min.x);
    float overlapY = (std::min)(a.max.y, b.max.y) - (std::max)(a.min.y, b.min.y);
    float overlapZ = (std::min)(a.max.z, b.max.z) - (std::max)(a.min.z, b.min.z);

    if (overlapX > 0 && overlapY > 0 && overlapZ > 0) {
        result.isHit = true;
        Vector3 centerA = {(a.min.x + a.max.x) * 0.5f, (a.min.y + a.max.y) * 0.5f, (a.min.z + a.max.z) * 0.5f};
        Vector3 centerB = {(b.min.x + b.max.x) * 0.5f, (b.min.y + b.max.y) * 0.5f, (b.min.z + b.max.z) * 0.5f};

        if (overlapX <= overlapY && overlapX <= overlapZ) {
            result.depth = overlapX;
            result.normal = (centerA.x > centerB.x) ? Vector3{1, 0, 0} : Vector3{-1, 0, 0};
        } else if (overlapY <= overlapX && overlapY <= overlapZ) {
            result.depth = overlapY;
            result.normal = (centerA.y > centerB.y) ? Vector3{0, 1, 0} : Vector3{0, -1, 0};
        } else {
            result.depth = overlapZ;
            result.normal = (centerA.z > centerB.z) ? Vector3{0, 0, 1} : Vector3{0, 0, -1};
        }
    }
    return result;
}

CollisionResult GetCollisionResult(const AABB& aabb, const Sphere& sphere) {
    CollisionResult result;
    Vector3 closestPoint = {Math::Clamp(sphere.center.x, aabb.min.x, aabb.max.x),
                            Math::Clamp(sphere.center.y, aabb.min.y, aabb.max.y),
                            Math::Clamp(sphere.center.z, aabb.min.z, aabb.max.z)};
    Vector3 diffSphere = Math::Subtract(sphere.center, closestPoint);
    float dist = Math::Length(diffSphere);

    if (dist <= sphere.radius) {
        result.isHit = true;
        Vector3 centerAABB = {(aabb.min.x + aabb.max.x) * 0.5f, (aabb.min.y + aabb.max.y) * 0.5f,
                              (aabb.min.z + aabb.max.z) * 0.5f};

        if (dist > 1e-5f) {
            result.depth = sphere.radius - dist;
            // aabbの押し出し方向は、球から遠ざかる方向（closestPointからAABB中心へ向かうおおよその方向、正しくは球中心->最近接点の逆）
            // 球の中心->最近接点 は、球から見たAABBへのベクトル。よってその逆がAABBの押し出し方向
            result.normal = Math::Multiply(-1.0f, Math::Normalize(diffSphere));
        } else {
            // 中心が完全に中にある場合
            float dx = (std::min)(sphere.center.x - aabb.min.x, aabb.max.x - sphere.center.x);
            float dy = (std::min)(sphere.center.y - aabb.min.y, aabb.max.y - sphere.center.y);
            float dz = (std::min)(sphere.center.z - aabb.min.z, aabb.max.z - sphere.center.z);
            result.depth = sphere.radius + (std::min)({dx, dy, dz});
            if (dx <= dy && dx <= dz) {
                result.normal = (centerAABB.x > sphere.center.x) ? Vector3{1, 0, 0} : Vector3{-1, 0, 0};
            } else if (dy <= dx && dy <= dz) {
                result.normal = (centerAABB.y > sphere.center.y) ? Vector3{0, 1, 0} : Vector3{0, -1, 0};
            } else {
                result.normal = (centerAABB.z > sphere.center.z) ? Vector3{0, 0, 1} : Vector3{0, 0, -1};
            }
        }
    }
    return result;
}

CollisionResult GetCollisionResult(const OBB& a, const OBB& b) {
    CollisionResult result;
    if ((a.size.x == 0.0f && a.size.y == 0.0f && a.size.z == 0.0f) ||
        (b.size.x == 0.0f && b.size.y == 0.0f && b.size.z == 0.0f)) {
        return result;
    }

    float R[3][3], AbsR[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            R[i][j] = Math::Dot(a.orientations[i], b.orientations[j]);
            AbsR[i][j] = std::abs(R[i][j]) + 1e-6f;
        }
    }

    Vector3 tWorld = Math::Subtract(b.center, a.center);
    Vector3 t = {Math::Dot(tWorld, a.orientations[0]), Math::Dot(tWorld, a.orientations[1]),
                 Math::Dot(tWorld, a.orientations[2])};

    float ra, rb;
    float minPenetration = (std::numeric_limits<float>::max)();
    Vector3 bestAxis = {0, 0, 0};
    float bestSign = 1.0f;

    auto testAxis = [&](const Vector3& axis, float overlap, float tProj) {
        if (overlap < 0.0f) {
            return false; // 分離軸が存在した
        }
        if (overlap < minPenetration) {
            minPenetration = overlap;
            bestAxis = axis;
            bestSign = (tProj < 0.0f) ? 1.0f : -1.0f; // aを遠ざける方向
        }
        return true;
    };

    // --- aの各軸 (3本) ---
    for (int i = 0; i < 3; i++) {
        ra = (i == 0) ? a.size.x : (i == 1) ? a.size.y : a.size.z;
        rb = b.size.x * AbsR[i][0] + b.size.y * AbsR[i][1] + b.size.z * AbsR[i][2];
        float tProj = (i == 0) ? t.x : (i == 1) ? t.y : t.z;
        if (!testAxis(a.orientations[i], ra + rb - std::abs(tProj), tProj)) {
            return result;
        }
    }

    // --- bの各軸 (3本) ---
    for (int i = 0; i < 3; i++) {
        ra = a.size.x * AbsR[0][i] + a.size.y * AbsR[1][i] + a.size.z * AbsR[2][i];
        rb = (i == 0) ? b.size.x : (i == 1) ? b.size.y : b.size.z;
        float tProj = t.x * R[0][i] + t.y * R[1][i] + t.z * R[2][i];
        if (!testAxis(b.orientations[i], ra + rb - std::abs(tProj), Math::Dot(tWorld, b.orientations[i]))) {
            return result;
        }
    }

    // --- aの各軸 x bの各軸 の外積 (9本) ---
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            Vector3 axis = Math::Cross(a.orientations[i], b.orientations[j]);
            float len = Math::Length(axis);
            if (len > 1e-4f) {
                axis = Math::Normalize(axis);
                ra = a.size.x * std::abs(Math::Dot(a.orientations[0], axis)) +
                     a.size.y * std::abs(Math::Dot(a.orientations[1], axis)) +
                     a.size.z * std::abs(Math::Dot(a.orientations[2], axis));
                rb = b.size.x * std::abs(Math::Dot(b.orientations[0], axis)) +
                     b.size.y * std::abs(Math::Dot(b.orientations[1], axis)) +
                     b.size.z * std::abs(Math::Dot(b.orientations[2], axis));
                float tProj = Math::Dot(tWorld, axis);
                if (!testAxis(axis, ra + rb - std::abs(tProj), tProj)) {
                    return result;
                }
            }
        }
    }

    result.isHit = true;
    result.depth = minPenetration;
    result.normal = Math::Multiply(bestSign, bestAxis);
    return result;
}

CollisionResult GetCollisionResult(const OBB& obb, const Sphere& sphere) {
    CollisionResult result;
    if (obb.size.x == 0.0f && obb.size.y == 0.0f && obb.size.z == 0.0f) {
        return result;
    }

    Vector3 worldRelPos = Math::Subtract(sphere.center, obb.center);
    Vector3 localPos = {Math::Dot(worldRelPos, obb.orientations[0]), Math::Dot(worldRelPos, obb.orientations[1]),
                        Math::Dot(worldRelPos, obb.orientations[2])};

    Vector3 closestLocal = {Math::Clamp(localPos.x, -obb.size.x, obb.size.x),
                            Math::Clamp(localPos.y, -obb.size.y, obb.size.y),
                            Math::Clamp(localPos.z, -obb.size.z, obb.size.z)};

    float dist = Math::Length(Math::Subtract(localPos, closestLocal));

    if (dist <= sphere.radius) {
        result.isHit = true;
        if (dist > 1e-5f) {
            result.depth = sphere.radius - dist;
            Vector3 localNormal = Math::Normalize(Math::Subtract(closestLocal, localPos));
            result.normal = Math::Add(Math::Add(Math::Multiply(localNormal.x, obb.orientations[0]),
                                                Math::Multiply(localNormal.y, obb.orientations[1])),
                                      Math::Multiply(localNormal.z, obb.orientations[2]));
        } else {
            // 完全内包時
            float dx = obb.size.x - std::abs(localPos.x);
            float dy = obb.size.y - std::abs(localPos.y);
            float dz = obb.size.z - std::abs(localPos.z);
            result.depth = sphere.radius + (std::min)({dx, dy, dz});
            Vector3 localNormal = {0, 0, 0};
            if (dx <= dy && dx <= dz) {
                localNormal.x = (localPos.x < 0) ? 1.0f : -1.0f;
            } else if (dy <= dx && dy <= dz) {
                localNormal.y = (localPos.y < 0) ? 1.0f : -1.0f;
            } else {
                localNormal.z = (localPos.z < 0) ? 1.0f : -1.0f;
            }

            result.normal = Math::Add(Math::Add(Math::Multiply(localNormal.x, obb.orientations[0]),
                                                Math::Multiply(localNormal.y, obb.orientations[1])),
                                      Math::Multiply(localNormal.z, obb.orientations[2]));
        }
    }
    return result;
}

CollisionResult GetCollisionResult(const OBB& obb, const AABB& aabb) {
    OBB aabbAsObb;
    aabbAsObb.center = {(aabb.min.x + aabb.max.x) * 0.5f, (aabb.min.y + aabb.max.y) * 0.5f,
                        (aabb.min.z + aabb.max.z) * 0.5f};
    aabbAsObb.size = {(aabb.max.x - aabb.min.x) * 0.5f, (aabb.max.y - aabb.min.y) * 0.5f,
                      (aabb.max.z - aabb.min.z) * 0.5f};
    aabbAsObb.orientations[0] = {1.0f, 0.0f, 0.0f};
    aabbAsObb.orientations[1] = {0.0f, 1.0f, 0.0f};
    aabbAsObb.orientations[2] = {0.0f, 0.0f, 1.0f};
    return GetCollisionResult(obb, aabbAsObb);
}

// 球と球の衝突判定
bool IsCollision(const Vector3& s1_center, float s1_radius, const Vector3& s2_center, float s2_radius) {
    float distance = Math::Length(s2_center - s1_center);
    return distance <= s1_radius + s2_radius;
}

// 球と球の衝突判定
bool IsCollision(const Sphere& s1, const Sphere& s2) {
    float distance = Math::Length(s1.center - s2.center);
    return distance <= s1.radius + s2.radius;
}

// 平面と球の衝突判定
bool IsCollision(const Sphere& sphere, const Plane& plane) {
    float k = std::abs(Math::Dot(plane.normal, sphere.center) - plane.distance);
    return k <= sphere.radius;
}

// 線分と平面の衝突判定
bool IsCollision(const Segment& segment, const Plane& plane) {

    // まずは垂直判定を行うために、法線と線の内積を求める
    float dot = Math::Dot(plane.normal, segment.diff);

    // 垂直 = 平行であるので、衝突しているはずがない
    if (dot == 0.0f) {
        return false;
    }

    // tを求める
    float t = (plane.distance - Math::Dot(segment.origin, plane.normal)) / dot;

    // tの値と線の種類によって衝突しているかを判断する

    // segmentのため範囲は0.0f ~ 1.0f

    if (0.0f <= t && t <= 1.0f) {
        return true;
    } else {
        return false;
    }
}

// 半直線と平面の衝突判定
bool IsCollision(const Ray& ray, const Plane& plane) {

    // まずは垂直判定を行うために、法線と線の内積を求める
    float dot = Math::Dot(plane.normal, ray.diff);

    // 垂直 = 平行であるので、衝突しているはずがない
    if (dot == 0.0f) {
        return false;
    }

    // tを求める
    float t = (plane.distance - Math::Dot(ray.origin, plane.normal)) / dot;

    // tの値と線の種類によって衝突しているかを判断する

    // rayのため範囲は0.0f ~

    if (0.0f <= t) {
        return true;
    } else {
        return false;
    }
}

// 直線と平面の衝突判定
bool IsCollision(const Line& line, const Plane& plane) {

    // まずは垂直判定を行うために、法線と線の内積を求める
    float dot = Math::Dot(plane.normal, line.diff);

    // 垂直 = 平行であるので、衝突しているはずがない
    if (dot == 0.0f) {
        return false;
    }

    // lineのため範囲は無制限

    return true;
}

// 三角形と線分の衝突判定
bool IsCollision(const Triangle& triangle, const Segment& segment) {

    // 各辺を結んだベクトルと、頂点と衝突点pを結んだベクトルのクロス積を求める
    Vector3 normal = Math::Cross(Math::Subtract(triangle.vertices_[1], triangle.vertices_[0]),
                                 Math::Subtract(triangle.vertices_[2], triangle.vertices_[0]));

    // 平面と線分の内積(垂直＝平行チェック)
    float dot = Math::Dot(normal, segment.diff);
    if (dot == 0.0f) {
        return false; // 平行なので交差しない
    }

    // 平面と線分の交点を求める
    float t = Math::Dot(normal, Math::Subtract(triangle.vertices_[0], segment.origin)) / dot;
    // t が [0,1] にないなら線分上に交点がない
    if (t < 0.0f || t > 1.0f) {
        return false;
    }

    // 交点を求める
    Vector3 p = Math::Add(segment.origin, Math::Multiply(t, segment.diff));

    // 各辺を結んだベクトルと、頂点と衝突点pを結んだベクトルのクロス積を取る
    Vector3 v01 = Math::Subtract(triangle.vertices_[1], triangle.vertices_[0]);
    Vector3 v1p = Math::Subtract(p, triangle.vertices_[1]);
    Vector3 cross01 = Math::Cross(v01, v1p);
    Vector3 v12 = Math::Subtract(triangle.vertices_[2], triangle.vertices_[1]);
    Vector3 v2p = Math::Subtract(p, triangle.vertices_[2]);
    Vector3 cross12 = Math::Cross(v12, v2p);
    Vector3 v20 = Math::Subtract(triangle.vertices_[0], triangle.vertices_[2]);
    Vector3 v0p = Math::Subtract(p, triangle.vertices_[0]);
    Vector3 cross20 = Math::Cross(v20, v0p);
    // すべての小三角形のクロス積と法線が同じ方向を向いていたら衝突
    if (Math::Dot(cross01, normal) >= 0.0f && Math::Dot(cross12, normal) >= 0.0f &&
        Math::Dot(cross20, normal) >= 0.0f) {
        // 衝突
        return true;
    }
    return false;
}

// AABBとAABBの衝突判定
bool IsCollision(const AABB& a, const AABB& b) {

    if ((a.min.x <= b.max.x && a.max.x >= b.min.x) && // x軸
        (a.min.y <= b.max.y && a.max.y >= b.min.y) && // y軸
        (a.min.z <= b.max.z && a.max.z >= b.min.z)    // z軸
    ) {
        return true;
    }

    return false;
}

// AABBと球の衝突判定
bool IsCollision(const AABB& aabb, const Sphere& sphere) {

    // 最近接点を求める
    Vector3 closestPoint{Math::Clamp(sphere.center.x, aabb.min.x, aabb.max.x),
                         Math::Clamp(sphere.center.y, aabb.min.y, aabb.max.y),
                         Math::Clamp(sphere.center.z, aabb.min.z, aabb.max.z)};
    // 最近接点と球の中心との距離を求める
    float distance = Math::Length(Math::Subtract(closestPoint, sphere.center));
    // 距離が半径よりも小さければ衝突
    if (distance <= sphere.radius) {
        // 衝突
        return true;
    }

    return false;
}

// AABBと線分の衝突判定
bool IsCollision(const AABB& aabb, const Segment& segment) {

    float tMin = 0.0f;
    float tMax = 1.0f;

    // x軸
    if (segment.diff.x != 0.0f) {
        float tx1 = (aabb.min.x - segment.origin.x) / segment.diff.x;
        float tx2 = (aabb.max.x - segment.origin.x) / segment.diff.x;
        float tNearX = (std::min)(tx1, tx2);
        float tFarX = (std::max)(tx1, tx2);
        tMin = (std::max)(tMin, tNearX);
        tMax = (std::min)(tMax, tFarX);
    } else {
        if (segment.origin.x < aabb.min.x || segment.origin.x > aabb.max.x) {
            return false;
        }
    }

    // y軸
    if (segment.diff.y != 0.0f) {
        float ty1 = (aabb.min.y - segment.origin.y) / segment.diff.y;
        float ty2 = (aabb.max.y - segment.origin.y) / segment.diff.y;
        float tNearY = (std::min)(ty1, ty2);
        float tFarY = (std::max)(ty1, ty2);
        tMin = (std::max)(tMin, tNearY);
        tMax = (std::min)(tMax, tFarY);
    } else {
        if (segment.origin.y < aabb.min.y || segment.origin.y > aabb.max.y) {
            return false;
        }
    }

    // z軸
    if (segment.diff.z != 0.0f) {
        float tz1 = (aabb.min.z - segment.origin.z) / segment.diff.z;
        float tz2 = (aabb.max.z - segment.origin.z) / segment.diff.z;
        float tNearZ = (std::min)(tz1, tz2);
        float tFarZ = (std::max)(tz1, tz2);
        tMin = (std::max)(tMin, tNearZ);
        tMax = (std::min)(tMax, tFarZ);
    } else {
        if (segment.origin.z < aabb.min.z || segment.origin.z > aabb.max.z) {
            return false;
        }
    }

    // 衝突
    if (tMin <= tMax) {
        return true;
    }

    return false;
}

// AABBと半直線の衝突判定
bool IsCollision(const AABB& aabb, const Ray& ray) {
    float tMin = 0.0f;
    float tMax = (std::numeric_limits<float>::max)(); // 無限遠まで判定する

    // x軸
    if (ray.diff.x != 0.0f) {
        float tx1 = (aabb.min.x - ray.origin.x) / ray.diff.x;
        float tx2 = (aabb.max.x - ray.origin.x) / ray.diff.x;
        float tNearX = (std::min)(tx1, tx2);
        float tFarX = (std::max)(tx1, tx2);
        tMin = (std::max)(tMin, tNearX);
        tMax = (std::min)(tMax, tFarX);
    } else {
        // x軸が0ならRayはX方向に進まない ⇒ AABBのX範囲にoriginがないなら衝突なし
        if (ray.origin.x < aabb.min.x || ray.origin.x > aabb.max.x) {
            return false;
        }
    }

    // y軸
    if (ray.diff.y != 0.0f) {
        float ty1 = (aabb.min.y - ray.origin.y) / ray.diff.y;
        float ty2 = (aabb.max.y - ray.origin.y) / ray.diff.y;
        float tNearY = (std::min)(ty1, ty2);
        float tFarY = (std::max)(ty1, ty2);
        tMin = (std::max)(tMin, tNearY);
        tMax = (std::min)(tMax, tFarY);
    } else {
        if (ray.origin.y < aabb.min.y || ray.origin.y > aabb.max.y) {
            return false;
        }
    }

    // z軸
    if (ray.diff.z != 0.0f) {
        float tz1 = (aabb.min.z - ray.origin.z) / ray.diff.z;
        float tz2 = (aabb.max.z - ray.origin.z) / ray.diff.z;
        float tNearZ = (std::min)(tz1, tz2);
        float tFarZ = (std::max)(tz1, tz2);
        tMin = (std::max)(tMin, tNearZ);
        tMax = (std::min)(tMax, tFarZ);
    } else {
        if (ray.origin.z < aabb.min.z || ray.origin.z > aabb.max.z) {
            return false;
        }
    }

    // 衝突判定：tMin が tMax 以下 かつ tMax が正方向
    if ((tMin <= tMax) && (tMax >= 0.0f)) {
        return true;
    }

    return false;
}

// AABBと直線の衝突判定
bool IsCollision(const AABB& aabb, const Line& line) {
    float tMin = -(std::numeric_limits<float>::max)(); // 無限負方向
    float tMax = (std::numeric_limits<float>::max)();  // 無限正方向

    // x軸
    if (line.diff.x != 0.0f) {
        float tx1 = (aabb.min.x - line.origin.x) / line.diff.x;
        float tx2 = (aabb.max.x - line.origin.x) / line.diff.x;
        float tNearX = (std::min)(tx1, tx2);
        float tFarX = (std::max)(tx1, tx2);
        tMin = (std::max)(tMin, tNearX);
        tMax = (std::min)(tMax, tFarX);
    } else {
        if (line.origin.x < aabb.min.x || line.origin.x > aabb.max.x) {
            return false;
        }
    }

    // y軸
    if (line.diff.y != 0.0f) {
        float ty1 = (aabb.min.y - line.origin.y) / line.diff.y;
        float ty2 = (aabb.max.y - line.origin.y) / line.diff.y;
        float tNearY = (std::min)(ty1, ty2);
        float tFarY = (std::max)(ty1, ty2);
        tMin = (std::max)(tMin, tNearY);
        tMax = (std::min)(tMax, tFarY);
    } else {
        if (line.origin.y < aabb.min.y || line.origin.y > aabb.max.y) {
            return false;
        }
    }

    // z軸
    if (line.diff.z != 0.0f) {
        float tz1 = (aabb.min.z - line.origin.z) / line.diff.z;
        float tz2 = (aabb.max.z - line.origin.z) / line.diff.z;
        float tNearZ = (std::min)(tz1, tz2);
        float tFarZ = (std::max)(tz1, tz2);
        tMin = (std::max)(tMin, tNearZ);
        tMax = (std::min)(tMax, tFarZ);
    } else {
        if (line.origin.z < aabb.min.z || line.origin.z > aabb.max.z) {
            return false;
        }
    }

    return tMin <= tMax;
}

// AABBと頂点の衝突判定
bool IsCollision(const AABB& aabb, const Vector3& point) {

    if ((aabb.min.x <= point.x && aabb.max.x >= point.x) && // x軸
        (aabb.min.y <= point.y && aabb.max.y >= point.y) && // y軸
        (aabb.min.z <= point.z && aabb.max.z >= point.z)    // z軸
    ) {
        return true;
    }

    return false;
}

// OBBと球の衝突判定
bool IsCollision(const OBB& obb, const Sphere& sphere) {
    if (obb.size.x == 0.0f && obb.size.y == 0.0f && obb.size.z == 0.0f) {
        return false;
    }

    // 1. 球の中心点をOBBのローカル空間に変換する
    // OBBの中心から球の中心へのベクトル
    Vector3 worldRelPos = Math::Subtract(sphere.center, obb.center);

    // 各軸に射影してローカル座標を求める
    Vector3 localPos = {Math::Dot(worldRelPos, obb.orientations[0]), Math::Dot(worldRelPos, obb.orientations[1]),
                        Math::Dot(worldRelPos, obb.orientations[2])};

    // 2. ローカル空間での「最近接点」を求める
    // OBBのローカル空間では、OBBは原点中心のAABBとして扱える
    // 範囲は [-size, size]
    Vector3 closestPoint = {Math::Clamp(localPos.x, -obb.size.x, obb.size.x),
                            Math::Clamp(localPos.y, -obb.size.y, obb.size.y),
                            Math::Clamp(localPos.z, -obb.size.z, obb.size.z)};

    // 3. ローカル空間での最近接点と球の中心(localPos)の距離を判定
    float distance = Math::Length(Math::Subtract(localPos, closestPoint));

    return distance <= sphere.radius;
}

// OBBと線分の衝突判定
bool IsCollision(const OBB& obb, const Segment& segment) {
    // 1. 線分をOBBのローカル空間に変換する
    // OBBの中心からの相対座標
    Vector3 worldOriginRel = Math::Subtract(segment.origin, obb.center);

    // OBBの各軸(orientations)への射影を行い、ローカル空間の線分を作る
    Segment localSegment;
    localSegment.origin = {Math::Dot(worldOriginRel, obb.orientations[0]),
                           Math::Dot(worldOriginRel, obb.orientations[1]),
                           Math::Dot(worldOriginRel, obb.orientations[2])};
    localSegment.diff = {Math::Dot(segment.diff, obb.orientations[0]), Math::Dot(segment.diff, obb.orientations[1]),
                         Math::Dot(segment.diff, obb.orientations[2])};

    // 2. ローカル空間でのAABBを作成
    // OBBはローカル空間では原点中心、サイズは obb.size の AABB となる
    AABB localAABB;
    localAABB.min = {-obb.size.x, -obb.size.y, -obb.size.z};
    localAABB.max = {obb.size.x, obb.size.y, obb.size.z};

    // 3. 既存の AABB と Segment の判定関数を呼び出す
    return IsCollision(localAABB, localSegment);
}

// OBBとRay(半直線)の判定
bool IsCollision(const OBB& obb, const Ray& ray) {
    // 1. RayをOBBのローカル空間に変換
    Vector3 worldRelPos = Math::Subtract(ray.origin, obb.center);
    Vector3 localOrigin = {Math::Dot(worldRelPos, obb.orientations[0]), Math::Dot(worldRelPos, obb.orientations[1]),
                           Math::Dot(worldRelPos, obb.orientations[2])};
    Vector3 localDiff = {Math::Dot(ray.diff, obb.orientations[0]), Math::Dot(ray.diff, obb.orientations[1]),
                         Math::Dot(ray.diff, obb.orientations[2])};

    // 2. スラブ法による判定
    float tMin = 0.0f; // Rayなので0以上
    float tMax = std::numeric_limits<float>::infinity();

    const float* originArr = &localOrigin.x;
    const float* diffArr = &localDiff.x;
    const float* sizeArr = &obb.size.x;

    for (int i = 0; i < 3; ++i) {
        // diffがほぼ0(線がこの軸に対して動いていない)場合
        if (std::abs(diffArr[i]) < 1e-6f) {
            // 始点がOBBの外側なら、平行なので一生当たらない
            if (std::abs(originArr[i]) > sizeArr[i]) {
                return false;
            }
        } else {
            // 各軸のスラブ(壁)との交差距離tを計算
            float t1 = (-sizeArr[i] - originArr[i]) / diffArr[i];
            float t2 = (sizeArr[i] - originArr[i]) / diffArr[i];

            float tNear = (std::min)(t1, t2);
            float tFar = (std::max)(t1, t2);

            tMin = (std::max)(tMin, tNear);
            tMax = (std::min)(tMax, tFar);
        }
    }

    // 最終的に重なった範囲があれば衝突
    return tMin <= tMax && tMax >= 0.0f;
}

// OBBとLine(直線)の判定
bool IsCollision(const OBB& obb, const Line& line) {
    // 1. LineをOBBのローカル空間に変換(Rayと同様)
    Vector3 worldRelPos = Math::Subtract(line.origin, obb.center);
    Vector3 localOrigin = {Math::Dot(worldRelPos, obb.orientations[0]), Math::Dot(worldRelPos, obb.orientations[1]),
                           Math::Dot(worldRelPos, obb.orientations[2])};
    Vector3 localDiff = {Math::Dot(line.diff, obb.orientations[0]), Math::Dot(line.diff, obb.orientations[1]),
                         Math::Dot(line.diff, obb.orientations[2])};

    // 2. スラブ法(範囲制限なし)
    float tMin = -std::numeric_limits<float>::infinity();
    float tMax = std::numeric_limits<float>::infinity();

    const float* originArr = &localOrigin.x;
    const float* diffArr = &localDiff.x;
    const float* sizeArr = &obb.size.x;

    for (int i = 0; i < 3; ++i) {
        if (std::abs(diffArr[i]) < 1e-6f) {
            if (std::abs(originArr[i]) > sizeArr[i]) {
                return false;
            }
        } else {
            float t1 = (-sizeArr[i] - originArr[i]) / diffArr[i];
            float t2 = (sizeArr[i] - originArr[i]) / diffArr[i];
            tMin = (std::max)(tMin, (std::min)(t1, t2));
            tMax = (std::min)(tMax, (std::max)(t1, t2));
        }
    }

    return tMin <= tMax;
}

// OBBとOBBの衝突判定
bool IsCollision(const OBB& a, const OBB& b) {
    if ((a.size.x == 0.0f && a.size.y == 0.0f && a.size.z == 0.0f) ||
        (b.size.x == 0.0f && b.size.y == 0.0f && b.size.z == 0.0f)) {
        return false;
    }

    // 2つのOBBの各軸(計6本)と、それらの外積(3x3=9本)の計15本を調べる

    // 1. 準備：回転行列(相対方向)と中心差分の計算
    float R[3][3], AbsR[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            // aの軸iとbの軸jの内積
            R[i][j] = Math::Dot(a.orientations[i], b.orientations[j]);
            // 絶対値(浮動小数点の誤差対策で僅かな値を加算)
            AbsR[i][j] = std::abs(R[i][j]) + 1e-6f;
        }
    }

    // 中心間の距離ベクトル
    Vector3 tWorld = Math::Subtract(b.center, a.center);
    // aのローカル座標系に変換
    Vector3 t = {Math::Dot(tWorld, a.orientations[0]), Math::Dot(tWorld, a.orientations[1]),
                 Math::Dot(tWorld, a.orientations[2])};

    float ra, rb;

    // 2. 分離軸の判定(全15パターン)

    // --- aの各軸 (3本) ---
    for (int i = 0; i < 3; i++) {
        const float sizeA[] = {a.size.x, a.size.y, a.size.z};
        const float sizeB[] = {b.size.x, b.size.y, b.size.z};
        const float tArr[] = {t.x, t.y, t.z};

        ra = sizeA[i];
        rb = sizeB[0] * AbsR[i][0] + sizeB[1] * AbsR[i][1] + sizeB[2] * AbsR[i][2];
        if (std::abs(tArr[i]) > ra + rb) {
            return false;
        }
    }

    // --- bの各軸 (3本) ---
    for (int i = 0; i < 3; i++) {
        ra = a.size.x * AbsR[0][i] + a.size.y * AbsR[1][i] + a.size.z * AbsR[2][i];
        rb = (i == 0) ? b.size.x : (i == 1) ? b.size.y : b.size.z;
        float tRel = t.x * R[0][i] + t.y * R[1][i] + t.z * R[2][i];
        if (std::abs(tRel) > ra + rb) {
            return false;
        }
    }

    // --- aの各軸 x bの各軸 の外積 (9本) ---
    // L = A0 x B0
    ra = a.size.y * AbsR[2][0] + a.size.z * AbsR[1][0];
    rb = b.size.y * AbsR[0][2] + b.size.z * AbsR[0][1];
    if (std::abs(t.z * R[1][0] - t.y * R[2][0]) > ra + rb) {
        return false;
    }

    // L = A0 x B1
    ra = a.size.y * AbsR[2][1] + a.size.z * AbsR[1][1];
    rb = b.size.x * AbsR[0][2] + b.size.z * AbsR[0][0];
    if (std::abs(t.z * R[1][1] - t.y * R[2][1]) > ra + rb) {
        return false;
    }

    // L = A0 x B2
    ra = a.size.y * AbsR[2][2] + a.size.z * AbsR[1][2];
    rb = b.size.x * AbsR[0][1] + b.size.y * AbsR[0][0];
    if (std::abs(t.z * R[1][2] - t.y * R[2][2]) > ra + rb) {
        return false;
    }

    // L = A1 x B0
    ra = a.size.x * AbsR[2][0] + a.size.z * AbsR[0][0];
    rb = b.size.y * AbsR[1][2] + b.size.z * AbsR[1][1];
    if (std::abs(t.x * R[2][0] - t.z * R[0][0]) > ra + rb) {
        return false;
    }

    // L = A1 x B1
    ra = a.size.x * AbsR[2][1] + a.size.z * AbsR[0][1];
    rb = b.size.x * AbsR[1][2] + b.size.z * AbsR[1][0];
    if (std::abs(t.x * R[2][1] - t.z * R[0][1]) > ra + rb) {
        return false;
    }

    // L = A1 x B2
    ra = a.size.x * AbsR[2][2] + a.size.z * AbsR[0][2];
    rb = b.size.x * AbsR[1][1] + b.size.y * AbsR[1][0];
    if (std::abs(t.x * R[2][2] - t.z * R[0][2]) > ra + rb) {
        return false;
    }

    // L = A2 x B0
    ra = a.size.x * AbsR[1][0] + a.size.y * AbsR[0][0];
    rb = b.size.y * AbsR[2][2] + b.size.z * AbsR[2][1];
    if (std::abs(t.y * R[0][0] - t.x * R[1][0]) > ra + rb) {
        return false;
    }

    // L = A2 x B1
    ra = a.size.x * AbsR[1][1] + a.size.y * AbsR[0][1];
    rb = b.size.x * AbsR[2][2] + b.size.z * AbsR[2][0];
    if (std::abs(t.y * R[0][1] - t.x * R[1][1]) > ra + rb) {
        return false;
    }

    // L = A2 x B2
    ra = a.size.x * AbsR[1][2] + a.size.y * AbsR[0][2];
    rb = b.size.x * AbsR[2][1] + b.size.y * AbsR[2][0];
    if (std::abs(t.y * R[0][2] - t.x * R[1][2]) > ra + rb) {
        return false;
    }

    // すべての軸で重なっていたら衝突
    return true;
}

// OBBとAABBの衝突判定
bool IsCollision(const OBB& obb, const AABB& aabb) {
    // AABBをOBBに変換して判定
    OBB aabbAsObb;
    aabbAsObb.center = {(aabb.min.x + aabb.max.x) * 0.5f, (aabb.min.y + aabb.max.y) * 0.5f,
                        (aabb.min.z + aabb.max.z) * 0.5f};
    aabbAsObb.size = {(aabb.max.x - aabb.min.x) * 0.5f, (aabb.max.y - aabb.min.y) * 0.5f,
                      (aabb.max.z - aabb.min.z) * 0.5f};
    aabbAsObb.orientations[0] = {1.0f, 0.0f, 0.0f};
    aabbAsObb.orientations[1] = {0.0f, 1.0f, 0.0f};
    aabbAsObb.orientations[2] = {0.0f, 0.0f, 1.0f};

    return IsCollision(obb, aabbAsObb);
}

// 視錐台と球の衝突判定（カリング用）
bool IsCollision(const Frustum& frustum, const Sphere& sphere) {
    // すべての平面に対して、球が外面（法線と反対側）に完全に出ていないかチェックする
    for (const auto& plane : frustum.planes) {
        // 平面方程式は Dot(N, P) - D = 0 (D = plane.distance)
        // 点Pの平面からの距離は Dot(N, P) - D
        // これが -sphere.radius より小さければ、球は平面の外側（法線と反対側）に完全にある
        if (Math::Dot(plane.normal, sphere.center) - plane.distance < -sphere.radius) {
            return false; // 1つでも平面の外側にあれば、視錐台の外
        }
    }
    return true; // すべての平面の内側、または境界と重なっている
}

bool IsCollision(const Ray& ray, const AABB& aabb, float& outDistance) {
    Vector3 dir = Math::Normalize(ray.diff);
    Vector3 invDir = {dir.x != 0.0f ? 1.0f / dir.x : 0.0f, dir.y != 0.0f ? 1.0f / dir.y : 0.0f,
                      dir.z != 0.0f ? 1.0f / dir.z : 0.0f};

    float tmin = 0.0f;
    float tmax = 10000.0f; // 十分大きな値

    for (int i = 0; i < 3; ++i) {
        float origin = i == 0 ? ray.origin.x : (i == 1 ? ray.origin.y : ray.origin.z);
        float minVal = i == 0 ? aabb.min.x : (i == 1 ? aabb.min.y : aabb.min.z);
        float maxVal = i == 0 ? aabb.max.x : (i == 1 ? aabb.max.y : aabb.max.z);
        float invD = i == 0 ? invDir.x : (i == 1 ? invDir.y : invDir.z);

        if (invD != 0.0f) {
            float t1 = (minVal - origin) * invD;
            float t2 = (maxVal - origin) * invD;

            tmin = MaxFloat(tmin, MinFloat(t1, t2));
            tmax = MinFloat(tmax, MaxFloat(t1, t2));
        } else if (origin < minVal || origin > maxVal) {
            return false;
        }
    }

    if (tmax >= tmin && tmin >= 0.0f) {
        outDistance = tmin;
        return true;
    }

    return false;
}

bool IsCollision(const Ray& ray, const Sphere& sphere, float& outDistance) {
    Vector3 dir = Math::Normalize(ray.diff);
    Vector3 m = ray.origin - sphere.center;
    float b = Math::Dot(m, dir);
    float c = Math::Dot(m, m) - sphere.radius * sphere.radius;

    // 始点がすでに球の中にある場合
    if (c > 0.0f && b > 0.0f) {
        return false;
    }

    float discr = b * b - c;
    if (discr < 0.0f) {
        return false;
    }

    float t = -b - std::sqrt(discr);
    if (t < 0.0f) {
        t = 0.0f; // 内部から開始した場合
    }

    outDistance = t;
    return true;
}

bool IsCollision(const Ray& ray, const OBB& obb, float& outDistance) {
    // OBBのローカル空間にRayを変換してAABBとの判定に帰着させる
    Vector3 dir = Math::Normalize(ray.diff);
    Vector3 p = obb.center - ray.origin;

    float tmin = 0.0f;
    float tmax = 10000.0f;

    Vector3 orientations[3] = {obb.orientations[0], obb.orientations[1], obb.orientations[2]};
    float size[3] = {obb.size.x, obb.size.y, obb.size.z};

    for (int i = 0; i < 3; ++i) {
        float e = Math::Dot(orientations[i], p);
        float f = Math::Dot(orientations[i], dir);

        if (std::abs(f) > 0.0001f) {
            float t1 = (e + size[i]) / f;
            float t2 = (e - size[i]) / f;

            if (t1 > t2) {
                std::swap(t1, t2);
            }

            tmin = MaxFloat(tmin, t1);
            tmax = MinFloat(tmax, t2);

            if (tmin > tmax) {
                return false;
            }
            if (tmax < 0.0f) {
                return false;
            }
        } else if (-e - size[i] > 0.0f || -e + size[i] < 0.0f) {
            return false;
        }
    }

    outDistance = tmin > 0.0f ? tmin : tmax;
    return true;
}

// 球と球の衝突判定
bool IsSphereCollision(const Sphere& s1, const Sphere& s2) {
    // 2つの球の中心点間の距離を求める
    float distance = Math::Length(Math::Subtract(s2.center, s1.center));
    // 半径の合計よりも短ければ衝突
    if (distance <= s1.radius + s2.radius) {
        return true;
    }
    return false;
}

// 平面と球の衝突判定
bool IsSpherePlaneCollision(const Sphere& sphere, const Plane& plane) {
    float k = std::fabs(Math::Dot(plane.normal, sphere.center) - plane.distance);
    if (k <= sphere.radius) {
        return true;
    }
    return false;
}

// 線分と平面の衝突判定
bool IsSegmentPlaneCollision(const Segment& segment, const Plane& plane) {
    // まずは垂直判定を行うために、法線と線の内積を求める
    float dot = Math::Dot(plane.normal, segment.diff);
    // 垂直 = 平行であるので、衝突しているはずがない
    if (dot == 0.0f) {
        return false;
    }
    // tを求める
    float t = (plane.distance - Math::Dot(segment.origin, plane.normal)) / dot;
    // tの値と線の種類によって衝突しているかを判断する
    if (0.0f <= t && t <= 1.0f) {
        return true;
    }
    return false;
}

// 半直線と平面の衝突判定
bool IsRayPlaneCollision(const Ray& ray, const Plane& plane) {
    // まずは垂直判定を行うために、法線と線の内積を求める
    float dot = Math::Dot(plane.normal, ray.diff);
    // 垂直 = 平行であるので、衝突しているはずがない
    if (dot == 0.0f) {
        return false;
    }
    // tを求める
    float t = (plane.distance - Math::Dot(ray.origin, plane.normal)) / dot;
    // tの値と線の種類によって衝突しているかを判断する
    if (0.0f <= t) {
        return true;
    }
    return false;
}

// 直線と平面の衝突判定
bool IsLinePlaneCollision(const Line& line, const Plane& plane) {
    // まずは垂直判定を行うために、法線と線の内積を求める
    float dot = Math::Dot(plane.normal, line.diff);
    // 垂直 = 平行であるので、衝突しているはずがない
    if (dot == 0.0f) {
        return false;
    }
    return true;
}

// 三角形と線分の衝突判定
bool IsTriangleSegmentCollision(const Triangle& triangle, const Segment& segment) {
    Vector3 normal = Math::Cross(Math::Subtract(triangle.vertices_[1], triangle.vertices_[0]),
                                 Math::Subtract(triangle.vertices_[2], triangle.vertices_[0]));
    float dot = Math::Dot(normal, segment.diff);
    if (dot == 0.0f) {
        return false;
    }
    float t = Math::Dot(normal, Math::Subtract(triangle.vertices_[0], segment.origin)) / dot;
    if (t < 0.0f || t > 1.0f) {
        return false;
    }
    Vector3 p = Math::Add(segment.origin, Math::Multiply(t, segment.diff));
    Vector3 v01 = Math::Subtract(triangle.vertices_[1], triangle.vertices_[0]);
    Vector3 v1p = Math::Subtract(p, triangle.vertices_[1]);
    Vector3 cross01 = Math::Cross(v01, v1p);
    Vector3 v12 = Math::Subtract(triangle.vertices_[2], triangle.vertices_[1]);
    Vector3 v2p = Math::Subtract(p, triangle.vertices_[2]);
    Vector3 cross12 = Math::Cross(v12, v2p);
    Vector3 v20 = Math::Subtract(triangle.vertices_[0], triangle.vertices_[2]);
    Vector3 v0p = Math::Subtract(p, triangle.vertices_[0]);
    Vector3 cross20 = Math::Cross(v20, v0p);
    if (Math::Dot(cross01, normal) >= 0.0f && Math::Dot(cross12, normal) >= 0.0f &&
        Math::Dot(cross20, normal) >= 0.0f) {
        return true;
    }
    return false;
}

// AABBとAABBの衝突判定
bool IsAABBCollision(const AABB& a, const AABB& b) {
    if ((a.min.x <= b.max.x && a.max.x >= b.min.x) && // x軸
        (a.min.y <= b.max.y && a.max.y >= b.min.y) && // y軸
        (a.min.z <= b.max.z && a.max.z >= b.min.z)    // z軸
    ) {
        return true;
    }
    return false;
}

// AABBと球の衝突判定
bool IsAABBSphereCollision(const AABB& aabb, const Sphere& sphere) {
    Vector3 closestPoint{Math::Clamp(sphere.center.x, aabb.min.x, aabb.max.x),
                         Math::Clamp(sphere.center.y, aabb.min.y, aabb.max.y),
                         Math::Clamp(sphere.center.z, aabb.min.z, aabb.max.z)};
    float distance = Math::Length(Math::Subtract(closestPoint, sphere.center));
    if (distance <= sphere.radius) {
        return true;
    }
    return false;
}

// AABBと線分の衝突判定
bool IsAABBSegmentCollision(const AABB& aabb, const Segment& segment) {
    float tMin = 0.0f;
    float tMax = 1.0f;

    // x軸
    if (segment.diff.x != 0.0f) {
        float tx1 = (aabb.min.x - segment.origin.x) / segment.diff.x;
        float tx2 = (aabb.max.x - segment.origin.x) / segment.diff.x;
        float tNearX = (std::min)(tx1, tx2);
        float tFarX = (std::max)(tx1, tx2);
        tMin = (std::max)(tMin, tNearX);
        tMax = (std::min)(tMax, tFarX);
    } else {
        if (segment.origin.x < aabb.min.x || segment.origin.x > aabb.max.x) {
            return false;
        }
    }

    // y軸
    if (segment.diff.y != 0.0f) {
        float ty1 = (aabb.min.y - segment.origin.y) / segment.diff.y;
        float ty2 = (aabb.max.y - segment.origin.y) / segment.diff.y;
        float tNearY = (std::min)(ty1, ty2);
        float tFarY = (std::max)(ty1, ty2);
        tMin = (std::max)(tMin, tNearY);
        tMax = (std::min)(tMax, tFarY);
    } else {
        if (segment.origin.y < aabb.min.y || segment.origin.y > aabb.max.y) {
            return false;
        }
    }

    // z軸
    if (segment.diff.z != 0.0f) {
        float tz1 = (aabb.min.z - segment.origin.z) / segment.diff.z;
        float tz2 = (aabb.max.z - segment.origin.z) / segment.diff.z;
        float tNearZ = (std::min)(tz1, tz2);
        float tFarZ = (std::max)(tz1, tz2);
        tMin = (std::max)(tMin, tNearZ);
        tMax = (std::min)(tMax, tFarZ);
    } else {
        if (segment.origin.z < aabb.min.z || segment.origin.z > aabb.max.z) {
            return false;
        }
    }

    return tMin <= tMax;
}

// AABBと半直線の衝突判定
bool IsAABBRayCollision(const AABB& aabb, const Ray& ray) {
    float tMin = 0.0f;
    float tMax = (std::numeric_limits<float>::max)();

    // x軸
    if (ray.diff.x != 0.0f) {
        float tx1 = (aabb.min.x - ray.origin.x) / ray.diff.x;
        float tx2 = (aabb.max.x - ray.origin.x) / ray.diff.x;
        float tNearX = (std::min)(tx1, tx2);
        float tFarX = (std::max)(tx1, tx2);
        tMin = (std::max)(tMin, tNearX);
        tMax = (std::min)(tMax, tFarX);
    } else {
        if (ray.origin.x < aabb.min.x || ray.origin.x > aabb.max.x) {
            return false;
        }
    }

    // y軸
    if (ray.diff.y != 0.0f) {
        float ty1 = (aabb.min.y - ray.origin.y) / ray.diff.y;
        float ty2 = (aabb.max.y - ray.origin.y) / ray.diff.y;
        float tNearY = (std::min)(ty1, ty2);
        float tFarY = (std::max)(ty1, ty2);
        tMin = (std::max)(tMin, tNearY);
        tMax = (std::min)(tMax, tFarY);
    } else {
        if (ray.origin.y < aabb.min.y || ray.origin.y > aabb.max.y) {
            return false;
        }
    }

    // z軸
    if (ray.diff.z != 0.0f) {
        float tz1 = (aabb.min.z - ray.origin.z) / ray.diff.z;
        float tz2 = (aabb.max.z - ray.origin.z) / ray.diff.z;
        float tNearZ = (std::min)(tz1, tz2);
        float tFarZ = (std::max)(tz1, tz2);
        tMin = (std::max)(tMin, tNearZ);
        tMax = (std::min)(tMax, tFarZ);
    } else {
        if (ray.origin.z < aabb.min.z || ray.origin.z > aabb.max.z) {
            return false;
        }
    }

    return (tMin <= tMax) && (tMax >= 0.0f);
}

// AABBと直線の衝突判定
bool IsAABBLineCollision(const AABB& aabb, const Line& line) {
    float tMin = -(std::numeric_limits<float>::max)();
    float tMax = (std::numeric_limits<float>::max)();

    // x軸
    if (line.diff.x != 0.0f) {
        float tx1 = (aabb.min.x - line.origin.x) / line.diff.x;
        float tx2 = (aabb.max.x - line.origin.x) / line.diff.x;
        float tNearX = (std::min)(tx1, tx2);
        float tFarX = (std::max)(tx1, tx2);
        tMin = (std::max)(tMin, tNearX);
        tMax = (std::min)(tMax, tFarX);
    } else {
        if (line.origin.x < aabb.min.x || line.origin.x > aabb.max.x) {
            return false;
        }
    }

    // y軸
    if (line.diff.y != 0.0f) {
        float ty1 = (aabb.min.y - line.origin.y) / line.diff.y;
        float ty2 = (aabb.max.y - line.origin.y) / line.diff.y;
        float tNearY = (std::min)(ty1, ty2);
        float tFarY = (std::max)(ty1, ty2);
        tMin = (std::max)(tMin, tNearY);
        tMax = (std::min)(tMax, tFarY);
    } else {
        if (line.origin.y < aabb.min.y || line.origin.y > aabb.max.y) {
            return false;
        }
    }

    // z軸
    if (line.diff.z != 0.0f) {
        float tz1 = (aabb.min.z - line.origin.z) / line.diff.z;
        float tz2 = (aabb.max.z - line.origin.z) / line.diff.z;
        float tNearZ = (std::min)(tz1, tz2);
        float tFarZ = (std::max)(tz1, tz2);
        tMin = (std::max)(tMin, tNearZ);
        tMax = (std::min)(tMax, tFarZ);
    } else {
        if (line.origin.z < aabb.min.z || line.origin.z > aabb.max.z) {
            return false;
        }
    }

    return tMin <= tMax;
}

// AABBと点の衝突判定
bool IsAABBPointCollision(const AABB& aabb, const Vector3& point) {
    if ((aabb.min.x <= point.x && aabb.max.x >= point.x) && // x軸
        (aabb.min.y <= point.y && aabb.max.y >= point.y) && // y軸
        (aabb.min.z <= point.z && aabb.max.z >= point.z)    // z軸
    ) {
        return true;
    }
    return false;
}

// OBBと球の衝突判定
bool IsOBBSphereCollision(const OBB& obb, const Sphere& sphere) {
    return IsCollision(obb, sphere);
}

// OBBと線分の衝突判定
bool IsOBBSegmentCollision(const OBB& obb, const Segment& segment) {
    return IsCollision(obb, segment);
}

// OBBとRay(半直線)の判定
bool IsOBBRayCollision(const OBB& obb, const Ray& ray) {
    return IsCollision(obb, ray);
}

// OBBとLine(直線)の判定
bool IsOBBLineCollision(const OBB& obb, const Line& line) {
    return IsCollision(obb, line);
}

// OBBとOBBの衝突判定
bool IsOBBCollision(const OBB& a, const OBB& b) {
    return IsCollision(a, b);
}

// OBBとAABB stumbling 判定
bool IsOBBAABBCollision(const OBB& obb, const AABB& aabb) {
    return IsCollision(obb, aabb);
}

Vector3 GetOBBSphereClosestPoint(const OBB& obb, const Sphere& sphere, float offset) {
    // 1. 球の中心点をOBBのローカル空間に変換する
    Vector3 worldRelPos = Math::Subtract(sphere.center, obb.center);
    Vector3 localPos = {Math::Dot(worldRelPos, obb.orientations[0]), Math::Dot(worldRelPos, obb.orientations[1]),
                        Math::Dot(worldRelPos, obb.orientations[2])};

    // 2. ローカル空間での最近接点を求める (AABBと同じ Clamp)
    Vector3 localClosest = {Math::Clamp(localPos.x, -obb.size.x, obb.size.x),
                            Math::Clamp(localPos.y, -obb.size.y, obb.size.y),
                            Math::Clamp(localPos.z, -obb.size.z, obb.size.z)};

    // 3. ローカル最近接点をワールド空間に戻す
    Vector3 worldClosest = obb.center;
    worldClosest = Math::Add(worldClosest, Math::Multiply(localClosest.x, obb.orientations[0]));
    worldClosest = Math::Add(worldClosest, Math::Multiply(localClosest.y, obb.orientations[1]));
    worldClosest = Math::Add(worldClosest, Math::Multiply(localClosest.z, obb.orientations[2]));

    // 4. OBB表面から弾丸の中心（外側）へ向かう法線ベクトル方向にオフセット押し出し
    if (offset > 0.0f) {
        Vector3 pushDir = Math::Subtract(sphere.center, worldClosest);
        float len = Math::Length(pushDir);
        if (len > 0.001f) {
            pushDir = Math::Normalize(pushDir);
            worldClosest = Math::Add(worldClosest, Math::Multiply(offset, pushDir));
        } else {
            // 万が一中心が完全に一致していた場合は、OBBのY軸正方向（上空側）に逃がす
            worldClosest = Math::Add(worldClosest, Math::Multiply(offset, obb.orientations[1]));
        }
    }

    return worldClosest;
}

bool GetOBBSegmentIntersection(const OBB& obb, const Segment& segment, Vector3& outIntersection) {
    // 1. 線分をOBBのローカル空間に変換する
    Vector3 worldOriginRel = Math::Subtract(segment.origin, obb.center);
    Vector3 localOrigin = {Math::Dot(worldOriginRel, obb.orientations[0]),
                           Math::Dot(worldOriginRel, obb.orientations[1]),
                           Math::Dot(worldOriginRel, obb.orientations[2])};
    Vector3 localDiff = {Math::Dot(segment.diff, obb.orientations[0]), Math::Dot(segment.diff, obb.orientations[1]),
                         Math::Dot(segment.diff, obb.orientations[2])};

    float tMin = 0.0f;
    float tMax = 1.0f;

    const float* originArr = &localOrigin.x;
    const float* diffArr = &localDiff.x;
    const float* sizeArr = &obb.size.x;

    for (int i = 0; i < 3; ++i) {
        if (std::abs(diffArr[i]) < 1e-6f) {
            if (std::abs(originArr[i]) > sizeArr[i]) {
                return false;
            }
        } else {
            float t1 = (-sizeArr[i] - originArr[i]) / diffArr[i];
            float t2 = (sizeArr[i] - originArr[i]) / diffArr[i];

            float tNear = (std::min)(t1, t2);
            float tFar = (std::max)(t1, t2);

            tMin = (std::max)(tMin, tNear);
            tMax = (std::min)(tMax, tFar);
        }
    }

    // 交差しているか判定
    if (tMin <= tMax && tMin >= 0.0f && tMin <= 1.0f) {
        // ローカル空間での交点を求める
        Vector3 localIntersection = Math::Add(localOrigin, Math::Multiply(tMin, localDiff));

        // ワールド空間に戻す
        Vector3 worldIntersection = obb.center;
        worldIntersection = Math::Add(worldIntersection, Math::Multiply(localIntersection.x, obb.orientations[0]));
        worldIntersection = Math::Add(worldIntersection, Math::Multiply(localIntersection.y, obb.orientations[1]));
        worldIntersection = Math::Add(worldIntersection, Math::Multiply(localIntersection.z, obb.orientations[2]));

        outIntersection = worldIntersection;
        return true;
    }

    return false;
}

} // namespace Collision
} // namespace Irufemi
