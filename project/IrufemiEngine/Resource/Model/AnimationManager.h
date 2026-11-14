#pragma once

#include <string>
#include "Resource/Model/Data/Animation.h"
#include "Resource/Model/Data/NodeAnimation.h"
#include "Resource/Model/Data/Node.h"
#include "Resource/Model/Data/SkeletonData.h"
#include "Resource/Model/Data/SkeletonPose.h"
#include "Resource/Model/Data/SkinCluster.h"
#include <optional>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>
#include <algorithm>
#include <wrl.h>
#include <d3d12.h>
#include "Core/System/DirectoryWatcher.h"

// 前方宣言
namespace Irufemi {
class DirectXCommon;
struct ObjModel;
}
struct ModelData;

/**
 * @class AnimationManager
 * @brief 3Dモデルのアニメーションリソースを統括管理するクラス
 * @details gltf等から読み込んだスケルトンやキーフレームデータを保持し、各フレームのアニメーション補間計算を担います。
 */
class AnimationManager {
public:
    AnimationManager() = default;
    ~AnimationManager() = default;

    // --- インスタンス機能 ---
    /**
     * @brief 初期化処理
     * @param[in] dxCommon DirectX基盤クラスのポインタ
     */
    void Initialize(Irufemi::DirectXCommon* dxCommon);

    /**
     * @brief アニメーション検索のルートディレクトリを設定する
     * @param[in] root ルートディレクトリのパス
     */
    void SetRootDirectory(std::string root);

    /**
     * @brief アニメーションファイルを読み込む（キャッシュ対応）
     * @param[in] filename 読み込むファイル名（.gltf など）
     * @return 読み込まれた Animation データへのポインタ
     */
    std::shared_ptr<Animation> LoadAnimationFile(const std::string& filename);

    /**
     * @brief スキンクラスター（GPUスキニング用のデータ）を生成する
     * @param[in] skeleton スケルトンデータ
     * @param[in] objModel モデルデータ
     * @return 生成された SkinCluster
     */
    SkinCluster CreateSkinCluster(const SkeletonData& skeleton, const Irufemi::ObjModel& objModel);

    // アニメーションファイルの列挙用
    /**
     * @brief 読み込み可能なアニメーションファイルの一覧を更新する
     */
    void RefreshAvailableAnimations();

    /**
     * @brief 読み込み可能なアニメーションファイルの一覧を取得する
     * @return ファイル名のリスト
     */
    std::vector<std::string> GetAvailableAnimations() const;

public: // 静的ヘルパ
    /**
     * @brief 任意の時刻の Vector3 の値を取得する
     * @param[in] keyframes キーフレームのリスト
     * @param[in] time 取得するアニメーションの時刻
     * @return 補間された Vector3 の値
     */
    static Irufemi::Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);

    /**
     * @brief 任意の時刻の Quaternion の値を取得する
     * @param[in] keyframes キーフレームのリスト
     * @param[in] time 取得するアニメーションの時刻
     * @return 補間された Quaternion の値
     */
    static Irufemi::Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

    /**
     * @brief 任意の時刻の Vector3 の値を取得する (AnimationCurve対応版)
     * @param[in] keyframes キーフレームカーブ
     * @param[in] time 取得するアニメーションの時刻
     * @return 補間された Vector3 の値
     */
    static Irufemi::Vector3 CalculateValue(const AnimationCurve<Irufemi::Vector3>& keyframes, float time);

    /**
     * @brief 任意の時刻の Quaternion の値を取得する (AnimationCurve対応版)
     * @param[in] keyframes キーフレームカーブ
     * @param[in] time 取得するアニメーションの時刻
     * @return 補間された Quaternion の値
     */
    static Irufemi::Quaternion CalculateValue(const AnimationCurve<Irufemi::Quaternion>& keyframes, float time);

    /**
     * @brief 任意の時刻のオイラー角 (Vector3) の値を取得する
     * @param[in] keyframes クォータニオンのキーフレームカーブ
     * @param[in] time 取得するアニメーションの時刻
     * @return 補間されオイラー角に変換された値
     */
    static Irufemi::Vector3 CalculateValueAsEuler(const AnimationCurve<Irufemi::Quaternion>& keyframes, float time);

    /**
     * @brief Nodeの階層構造から静的なSkeletonDataを生成する
     * @param[in] rootNode 階層のルートとなるNode
     * @return 生成された SkeletonData
     */
    static SkeletonData CreateSkeletonData(const Node& rootNode);

    /**
     * @brief SkeletonDataからインスタンスごとのSkeletonPoseを生成する
     * @param[in] data 共有される静的なSkeletonData
     * @return 姿勢計算用の SkeletonPose
     */
    static SkeletonPose CreateSkeletonPose(const SkeletonData* data);

    /**
     * @brief NodeからJointDataを生成する
     * @param[in] node 対象のノード
     * @param[in] parent 親ジョイントのインデックス（存在しない場合はnullopt）
     * @param[out] joints 追加先のジョイント配列
     * @return 生成されたジョイントのインデックス
     */
    static int32_t CreateJointData(const Node& node, const std::optional<int32_t>& parent,
                                   std::vector<JointData>& joints);

    /**
     * @brief SkeletonPoseのワールド行列などを更新する
     * @param[in,out] skeleton 更新対象の SkeletonPose
     */
    static void SkeletonUpdate(SkeletonPose& skeleton);

    /**
     * @brief SkeletonPoseに対して単一のAnimationを適用する
     * @param[in,out] skeleton 適用対象の SkeletonPose
     * @param[in] animation 適用するアニメーションデータ
     * @param[in] animationTime 再生時刻
     * @param[in] applyRootTranslation Rootボーンの移動を適用するかどうか（Root Motion抽出時はfalse）
     */
    static void ApplyAnimation(SkeletonPose& skeleton, const Animation& animation, float animationTime,
                               bool applyRootTranslation = true);

    /**
     * @brief 2つのAnimationをブレンドしてSkeletonPoseに適用する
     * @param[in,out] skeleton 適用対象の SkeletonPose
     * @param[in] animA アニメーションA
     * @param[in] timeA アニメーションAの再生時刻
     * @param[in] animB アニメーションB
     * @param[in] timeB アニメーションBの再生時刻
     * @param[in] weight animBの重み (0.0 ~ 1.0)
     * @param[in] applyRootTranslation Rootボーンの移動を適用するかどうか
     */
    static void BlendAnimation(SkeletonPose& skeleton, const Animation& animA, float timeA, const Animation& animB,
                               float timeB, float weight, bool applyRootTranslation = true);

    /**
     * @brief SkinCluster（行列パレット）を更新してGPUバッファに書き込む
     * @param[in,out] skinCluster 更新対象の SkinCluster
     * @param[in] skeleton 現在の姿勢を持つ SkeletonPose
     * @param[in] frameIndex スワップチェーンのフレームインデックス (ダブル/トリプルバッファリング用)
     */
    static void SkinClusterUpdate(SkinCluster& skinCluster, const SkeletonPose& skeleton, uint32_t frameIndex);

private: // 内部ヘルパ
    /**
     * @brief NormalizeAndResolve を実行する。
     */
    std::string NormalizeAndResolve(const std::string& filename) const;
    /**
     * @brief StartsWith を実行する。
     */
    static bool StartsWith(const std::string& s, const std::string& prefix);
    static std::pair<std::string, std::string> SplitDirectoryAndFile(const std::string& full);
    /**
     * @brief FindFileRecursive を実行する。
     */
    std::string FindFileRecursive(const std::string& filename) const;

private:
    Irufemi::DirectXCommon* dxCommon_ = nullptr;
    std::string rootDir_;
    mutable std::mutex mutex_;

    struct CachedAnimation {
        std::shared_ptr<Animation> animation;
        uint64_t lastLoadTime = 0;
        std::string sourceFilePath;
    };
    std::unordered_map<std::string, CachedAnimation> cache_;
    mutable std::unordered_map<std::string, std::string> filePathCache_;
    std::vector<std::string> availableAnimations_;

    std::unique_ptr<DirectoryWatcher> directoryWatcher_;
    /**
     * @brief OnDirectoryChanged を実行する。
     */
    void OnDirectoryChanged();
};
