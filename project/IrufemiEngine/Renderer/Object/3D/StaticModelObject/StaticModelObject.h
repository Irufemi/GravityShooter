#pragma once
#include <d3d12.h>
#include <string>
#include "Renderer/Camera/Camera.h"
#include "Renderer/Data/TransformationMatrix.h"
#include <wrl.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include "Core/Math/Transform.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Matrix4x4.h"
#include "Resource/Model/Data/ObjModel.h"
#include "Renderer/System/Core/Object3DResource.h"
#include "Renderer/Data/Material.h"
#include "RHI/DirectX12/DynamicConstantBuffer.h"
#include "Renderer/System/Core/BaseModel.h"
#include "Resource/Model/Data/SkeletonData.h"
#include "Resource/Model/Data/SkeletonPose.h"
#include "Resource/Model/Data/SkinCluster.h"
#include "Renderer/Compute/IComputeTask.h"

namespace Irufemi {
class TextureManager;
class ModelManager;
class DrawManager;
}
class DebugUI;

//==========================
// objが配布されているサイト
// https://quaternius.com/
// 使用する場合はライセンスがCCOのものを利用する
// https://creativecommons.org/publicdomain/zero/1.0/deed.ja
//==========================

/**
 * @class StaticModelObject
 * @brief 3Dモデル（OBJ/GLTF等）のインスタンスを描画・管理するクラス
 * @details ModelManager から取得した共有モデルデータを参照し、個別の位置・回転・拡縮やマテリアル設定を保持します。
 * スキン付きモデルの場合は、バインドポーズによる静的スキニングコンピュートタスクを実行します。
 */
class StaticModelObject : public BaseModel, public Irufemi::IComputeTask {

private:
    /**
     * @brief モデル内の各ノードの名前と、そのノードのグローバル行列（ローカル行列の累積）をマッピングするキャッシュ
     */
    std::unordered_map<std::string, Irufemi::Matrix4x4> nodeGlobalTransforms_;

    /**
     * @brief ルートノードから再帰的に階層を辿り、各ノードのグローバル行列を計算・キャッシュする
     * @param node 現在処理中のノード
     * @param parentMatrix 親ノードのグローバル行列（初期呼び出し時は単位行列）
     */
    void CalculateNodeTransforms(const Irufemi::Node& node, const Irufemi::Matrix4x4& parentMatrix);

    /**
     * @brief ロード完了後にメッシュ等のリソースを構築する（遅延初期化）
     */
    void InitializeResources();

public: // メンバ関数
    /**
     * @brief デストラクタ
     */
    ~StaticModelObject() override;

    /**
     * @brief 初期化
     * @param[in] filename モデルファイル名（ModelManager経由でロード）
     */
    void Initialize(const std::string& filename = "plane.obj");

    /**
     * @brief 更新処理
     * @details ワールド行列の計算と定数バッファへの転送を行います。
     */
    void Update();

    /**
     * @brief 描画コマンドの積み込み
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
     * @brief デバッグ用UIの表示
     */
    void Debug(const char* objName = " ");

    /**
     * @brief デバッグ用タブの表示
     */
    void DebugTab();

    /**
     * @brief コンピュートシェーダーを用いたスキニング処理を実行します
     */
    void DispatchCompute() override;

private:
    Irufemi::SkeletonData skeletonData_;
    Irufemi::SkeletonPose skeletonPose_;
    Irufemi::SkinCluster skinCluster_;
    uint32_t lastSkinnedFrameIndex_ = 0;
    bool isResourceInitialized_ = false;
};
