#pragma once
#include "Core/Math/Math.h"

namespace Irufemi {
// 前方宣言
struct Sphere;
struct Plane;
struct Segment;
struct Ray;
struct Line;
struct Triangle;
struct AABB;
struct OBB;
struct Frustum;

/**
 * @namespace Collision
 * @brief プリミティブ形状（Sphere, AABB, OBB, Frustum等）の幾何学的交差判定およびめり込み押し出し計算を提供する名前空間
 */
namespace Collision {

/// <summary>
/// 押し出し(Kinematic Resolution)用の詳細な衝突結果構造体
/// </summary>
struct CollisionResult {
    bool isHit = false;
    Vector3 normal = {0.0f, 0.0f, 0.0f}; // 第1引数のオブジェクトを押し出す(反発する)方向の正規化ベクトル
    float depth = 0.0f;                  // めり込み量
};

// --- 押し出し対応の判定関数群 ---
/**
 * @brief CollisionResult を取得する。
 * @return 取得された CollisionResult
 */
CollisionResult GetCollisionResult(const Sphere& a, const Sphere& b);
/**
 * @brief CollisionResult を取得する。
 * @return 取得された CollisionResult
 */
CollisionResult GetCollisionResult(const AABB& a, const AABB& b);
/**
 * @brief CollisionResult を取得する。
 * @return 取得された CollisionResult
 */
CollisionResult GetCollisionResult(const AABB& aabb, const Sphere& sphere);
/**
 * @brief CollisionResult を取得する。
 * @return 取得された CollisionResult
 */
CollisionResult GetCollisionResult(const OBB& a, const OBB& b);
/**
 * @brief CollisionResult を取得する。
 * @return 取得された CollisionResult
 */
CollisionResult GetCollisionResult(const OBB& obb, const Sphere& sphere);
/**
 * @brief CollisionResult を取得する。
 * @return 取得された CollisionResult
 */
CollisionResult GetCollisionResult(const OBB& obb, const AABB& aabb);

/// <summary>
/// 球と球の衝突判定
/// </summary>
/// <param name="s1_center"></param>
/// <param name="s1_radius"></param>
/// <param name="s2_center"></param>
/// <param name="s2_radius"></param>
/// <returns></returns>
bool IsCollision(const Vector3& s1_center, const float& s1_radius, const Vector3& s2_center, const float& s2_radius);

/// <summary>
/// 球と球の衝突判定
/// </summary>
/// <param name="s1"></param>
/// <param name="s2"></param>
/// <returns></returns>
bool IsCollision(const Sphere& s1, const Sphere& s2);

/// <summary>
/// 球と平面の衝突判定
/// </summary>
/// <param name="sphere"></param>
/// <param name="plane"></param>
/// <returns></returns>
bool IsCollision(const Sphere& sphere, const Plane& plane);

/// <summary>
/// 線分と平面の衝突判定
/// </summary>
/// <param name="segment"></param>
/// <param name="plane"></param>
/// <returns></returns>
bool IsCollision(const Segment& segment, const Plane& plane);

/// <summary>
/// 半直線と平面の衝突判定
/// </summary>
/// <param name="ray"></param>
/// <param name="plane"></param>
/// <returns></returns>
bool IsCollision(const Ray& ray, const Plane& plane);

/// <summary>
/// 直線と平面の衝突判定
/// </summary>
/// <param name="line"></param>
/// <param name="plane"></param>
/// <returns></returns>
bool IsCollision(const Line& line, const Plane& plane);

/// <summary>
/// 三角形と線分の衝突判定
/// </summary>
/// <param name="triangle"></param>
/// <param name="segment"></param>
/// <returns></returns>
bool IsCollision(const Triangle& triangle, const Segment& segment);

/// <summary>
/// AABBとAABBの衝突判定
/// </summary>
/// <param name="a"></param>
/// <param name="b"></param>
/// <returns></returns>
bool IsCollision(const AABB& aabb1, const AABB& aabb2);

/// <summary>
/// レイとAABBの衝突判定
/// </summary>
bool IsCollision(const Ray& ray, const AABB& aabb, float& outDistance);

/// <summary>
/// レイと球の衝突判定
/// </summary>
bool IsCollision(const Ray& ray, const Sphere& sphere, float& outDistance);

/// <summary>
/// レイとOBBの衝突判定
/// </summary>
bool IsCollision(const Ray& ray, const OBB& obb, float& outDistance);

/// <summary>
/// AABBと球の衝突判定
/// </summary>
/// <param name="aabb"></param>
/// <param name="sphere"></param>
/// <returns></returns>
bool IsCollision(const AABB& aabb, const Sphere& sphere);

/// <summary>
/// AABBと線分の衝突判定
/// </summary>
/// <param name="aabb"></param>
/// <param name="plaane"></param>
/// <returns></returns>
bool IsCollision(const AABB& aabb, const Segment& segment);

/// <summary>
/// AABBと半直線の衝突判定
/// </summary>
/// <param name="aabb"></param>
/// <param name="plaane"></param>
/// <returns></returns>
bool IsCollision(const AABB& aabb, const Ray& ray);

/// <summary>
/// AABBと直線の衝突判定
/// </summary>
/// <param name="aabb"></param>
/// <param name="plaane"></param>
/// <returns></returns>
bool IsCollision(const AABB& aabb, const Line& line);

/// <summary>
/// AABBと頂点の衝突判定
/// </summary>
/// <param name="aabb"></param>
/// <param name="point"></param>
/// <returns></returns>
bool IsCollision(const AABB& aabb, const Vector3& point);

/// <summary>
/// OBBと球の衝突判定
/// </summary>
bool IsCollision(const OBB& obb, const Sphere& sphere);

/// <summary>
/// OBBと線分の衝突判定
/// </summary>
bool IsCollision(const OBB& obb, const Segment& segment);

/// <summary>
/// OBBと半直線の衝突判定
/// </summary>
bool IsCollision(const OBB& obb, const Ray& ray);

/// <summary>
/// OBBと直線の衝突判定
/// </summary>
bool IsCollision(const OBB& obb, const Line& line);

/// <summary>
/// OBBとOBBの衝突判定
/// </summary>
bool IsCollision(const OBB& a, const OBB& b);

/// <summary>
/// OBBとAABBの衝突判定
/// </summary>
bool IsCollision(const OBB& obb, const AABB& aabb);

/// <summary>
/// 球と球の衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsSphereCollision(const Sphere& s1, const Sphere& s2);

/// <summary>
/// 球と平面の衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsSpherePlaneCollision(const Sphere& sphere, const Plane& plane);

/// <summary>
/// 線分と平面の衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsSegmentPlaneCollision(const Segment& segment, const Plane& plane);

/// <summary>
/// 半直線と平面の衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsRayPlaneCollision(const Ray& ray, const Plane& plane);

/// <summary>
/// 直線と平面の衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsLinePlaneCollision(const Line& line, const Plane& plane);

/// <summary>
/// 三角形と線分の衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsTriangleSegmentCollision(const Triangle& triangle, const Segment& segment);

/// <summary>
/// AABBとAABBの衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsAABBCollision(const AABB& a, const AABB& b);

/// <summary>
/// AABBと球の衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsAABBSphereCollision(const AABB& aabb, const Sphere& sphere);

/// <summary>
/// AABBと線分の衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsAABBSegmentCollision(const AABB& aabb, const Segment& segment);

/// <summary>
/// AABBと半直線の衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsAABBRayCollision(const AABB& aabb, const Ray& ray);

/// <summary>
/// AABBと直線の衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsAABBLineCollision(const AABB& aabb, const Line& line);

/// <summary>
/// AABBと点の衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsAABBPointCollision(const AABB& aabb, const Vector3& point);

/// <summary>
/// OBBと球の衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsOBBSphereCollision(const OBB& obb, const Sphere& sphere);

/// <summary>
/// OBBと線分の衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsOBBSegmentCollision(const OBB& obb, const Segment& segment);

/// </summary>
/// OBBとRay(半直線)の判定
/// </summary>
/**
 * @brief IsOBBRayCollision かどうかを判定する。
 * @return 判定結果 (true/false)
 */
[[deprecated("Use IsCollision instead")]]
bool IsOBBRayCollision(const OBB& obb, const Ray& ray);

/// </summary>
/// OBBとLine(直線)の判定
/// </summary>
/**
 * @brief IsOBBLineCollision かどうかを判定する。
 * @return 判定結果 (true/false)
 */
[[deprecated("Use IsCollision instead")]]
bool IsOBBLineCollision(const OBB& obb, const Line& line);

/// <summary>
/// OBBとOBBの衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsOBBCollision(const OBB& a, const OBB& b);

/// <summary>
/// OBBとAABBの衝突判定
/// </summary>
[[deprecated("Use IsCollision instead")]]
bool IsOBBAABBCollision(const OBB& obb, const AABB& aabb);

/// <summary>
/// 視錐台と球の衝突判定（カリング用）
/// </summary>
bool IsCollision(const Frustum& frustum, const Sphere& sphere);

/// <summary>
/// OBB上の、球に最も近い最近接点（ワールド座標）を求め、必要に応じて法線方向に押し出す
/// </summary>
Vector3 GetOBBSphereClosestPoint(const OBB& obb, const Sphere& sphere, float offset = 0.0f);

/// <summary>
/// OBBと線分（移動軌跡）の最初の交点（ワールド座標）を求める
/// </summary>
bool GetOBBSegmentIntersection(const OBB& obb, const Segment& segment, Vector3& outIntersection);

} // namespace Collision
} // namespace Irufemi
