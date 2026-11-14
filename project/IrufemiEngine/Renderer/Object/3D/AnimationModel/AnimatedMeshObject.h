#pragma once
#include "Renderer/System/Core/IRenderable.h"
#include "Renderer/System/Core/BaseModel.h"
#include "Resource/Model/Data/SkeletonData.h"
#include "Resource/Model/Data/SkeletonPose.h"
#include "Resource/Model/Data/SkinCluster.h"
#include "Renderer/Compute/IComputeTask.h"
#include <string>
#include <cstdint>
#include <memory>
#include <vector>

class PrimitiveBatch;

/**
 * @class AnimatedMeshObject
 * @brief スキニングアニメーション付きのメッシュ描画に特化した純粋な低レイヤークラス。
 * GameObjectに依存せず、外部から与えられたポーズ（SkeletonPose）を元に描画・コンピュート処理を行います。
 */
class AnimatedMeshObject : public IComputeTask, public BaseModel {
public:
    AnimatedMeshObject();
    ~AnimatedMeshObject() override;

    /**
     * @brief DispatchCompute を実行する。
     */
    void DispatchCompute() override;
    /**
     * @brief Initialize を実行する。
     */
    void Initialize(const std::string& filename);

    /// @brief 外部からポーズを渡して更新。nullptrの場合は内部のバインドポーズまたは前回状態を維持。
    void Update(const Irufemi::SkeletonPose* externalPose = nullptr);

    /**
     * @brief SyncBeforeDraw を実行する。
     */
    void SyncBeforeDraw() override;
    /**
     * @brief Draw を実行する。
     */
    void Draw() override;
    /**
     * @brief DrawOutlineMask を実行する。
     */
    void DrawOutlineMask() override;
    /**
     * @brief Debug を実行する。
     */
    void Debug(const char* objName = " ");

    /**
     * @brief SkeletonData を取得する。
     * @return 取得された SkeletonData
     */
    const Irufemi::SkeletonData* GetSkeletonData() const;
    /**
     * @brief InternalSkeletonPose を取得する。
     * @return 取得された InternalSkeletonPose
     */
    Irufemi::SkeletonPose* GetInternalSkeletonPose(); // プログラマの手動制御用

private:
    /**
     * @brief InitializeResources を実行する。
     */
    void InitializeResources();

private:
    Irufemi::SkeletonData skeletonData_;
    Irufemi::SkeletonPose internalPose_;
    const Irufemi::SkeletonPose* currentPose_ = nullptr; // 描画に使用するポーズ

    Irufemi::SkinCluster skinCluster_;
    std::vector<D3D12_VERTEX_BUFFER_VIEW> drawVbvs_;
    std::vector<D3D12_VERTEX_BUFFER_VIEW> outlineVbvs_;

    Irufemi::Matrix4x4 localMatrix_;
    Irufemi::Matrix4x4 worldMatrix_;

    uint32_t lastSkinnedFrameIndex_ = 0;
    std::string filename_;
    bool isCulled_ = false;
};
