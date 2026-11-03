#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>
#include <algorithm>
#include <map>
#include <fstream>
#include <sstream>
#include <wrl.h>
#include <future>
#include <type_traits>
#include "Core/System/ThreadPool.h"
#include <d3d12.h>
#include "Core/System/ResourceHandle.h"
#include "Core/System/ResourceCachePool.h"
#include "Resource/Model/Data/ObjModel.h"
#include "Resource/Model/Data/MaterialData.h"
#include "Resource/Model/Data/VoxelizedModel.h"
#include "Core/Math/Vector3Int.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Math.h"
#include <atomic>
#include "Core/System/DirectoryWatcher.h"

struct Node;
class DirectXCommon;
class TextureManager;

/**
 * @struct GpuMesh
 * @brief GPU上に転送されたメッシュリソースを保持する構造体
 */
struct GpuMesh {
    GpuMesh() = default;
    ~GpuMesh();
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView{};
    UINT vertexCount = 0;
    UINT indexCount = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE vertexSrvHandle{};
    uint32_t srvIndex = 0xFFFFFFFF;

    static DirectXCommon* dxCommon_;
};

/**
 * @struct GpuMaterial
 * @brief GPU上で使用されるマテリアルリソースを保持する構造体
 */
struct GpuMaterial {
    GpuMaterial() = default;
    explicit GpuMaterial(TextureManager* tm) : textureManager_(tm) {}
    ~GpuMaterial();
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
    ResourceHandle textureHandle;
    TextureManager* textureManager_ = nullptr;
};

/**
 * @struct ManagedModel
 * @brief CPU/GPU両方のデータを統合して管理する単位
 */
struct ManagedModel {
    enum class LoadingStatus { Pending = 0, Loading = 1, Loaded = 2, Failed = 3 };

    std::shared_ptr<ObjModel> cpuModel;
    std::vector<std::shared_ptr<GpuMesh>> gpuMeshes;
    std::vector<std::shared_ptr<GpuMaterial>> gpuMaterials;

    /**
     * @brief ボクセル化済みモデルのキャッシュリスト（解像度別）
     * 複数インスタンス間で共有してメモリと初期化時間を節約する
     */
    std::vector<std::shared_ptr<VoxelizedModel>> cachedVoxelModels;

    /** @brief ボクセルキャッシュアクセス用の排他制御ミューテックス */
    std::mutex voxelMutex;

    std::atomic<LoadingStatus> status = LoadingStatus::Pending;
    uint64_t lastLoadTime = 0;  // ホットリロード用のタイムスタンプ
    std::string sourceFilePath; // ロード元のファイルパス
};

/**
 * @class ModelManager
 * @brief モデルリソース（OBJ, GLTF等）のロード、管理、キャッシュを行うマネージャクラス
 *
 * 設計思想:
 * - 読み込み済みのモデルをファイルパス（またはファイル名）でキャッシュし、同一リソースの重複ロードを防ぎます。
 * - std::weak_ptr を用いたキャッシュ管理により、不要になったリソースの自動的な解放を支援します。
 * - Assimpライブラリを使用して多様な3Dモデルフォーマットに対応します。
 *
 * 使い方:
 * 1. Initialize() でエンジン共通コンポーネントを登録します。
 * 2. SetRootDirectory() でリソースのベースパスを設定します（デフォルトは "resources/model"）。
 * 3. GetModel("filename.obj") でモデルを取得します。初回呼び出し時にロードが行われます。
 */
class ModelManager {
public:
    ModelManager();
    ~ModelManager();

    /**
     * @brief マネージャの解放・シャットダウン
     */
    void Finalize();

    /**
     * @brief マネージャの初期化
     * @param dxCommon DirectXコンポーネントへのポインタ
     * @param textureManager テクスチャマネージャへのポインタ（マテリアルのテクスチャロードに使用）
     */
    void Initialize(DirectXCommon* dxCommon, TextureManager* textureManager);

    /**
     * @brief モデル検索のルートディレクトリを設定
     * @param root ルートディレクトリのパス
     */
    void SetRootDirectory(std::string root);

    /**
     * @brief モデルを取得する。キャッシュにあればそれを返し、なければロードする。
     * @param filename ファイル名（または相対パス）。拡張子を含む。
     * @return 取得した ResourceHandle
     */
    ResourceHandle LoadModel(const std::string& filename);

    /**
     * @brief モデルのリソースハンドルを解放する（参照カウントを減らす）
     * @param handle リソースハンドル
     */
    void ReleaseModel(ResourceHandle handle);

    /**
     * @brief リソースハンドルからモデルオブジェクト本体を取得する
     * @param handle リソースハンドル
     * @return ManagedModelオブジェクトのポインタ（無効な場合はnullptr）
     */
    ManagedModel* Resolve(ResourceHandle handle) const;

    /**
     * @brief 指定したモデルと解像度のボクセル化データを取得する（キャッシュ対応）
     * @details 既に同じ解像度でボクセル化されていればキャッシュを返し、なければ新規計算してキャッシュする。
     * @param filename ボクセル化の元となるモデルファイル名
     * @param resolution ボクセルの分割数（解像度）
     * @return 共有される VoxelizedModel へのポインタ（失敗時は nullptr）
     */
    std::shared_ptr<VoxelizedModel> GetVoxelizedModel(const std::string& filename,
                                                      const Irufemi::Vector3Int& resolution);

    /**
     * @brief 指定したフォルダ以下のモデルをすべて先行ロードする
     * @param relativeFolder ルートディレクトリからの相対パス
     */
    void PreloadAllUnder(const std::string& relativeFolder);

    /**
     * @brief 現在キャッシュされているモデルのキー（パス）一覧を取得
     * @return キーのリスト
     */
    std::vector<std::string> GetCachedKeys() const;

    /**
     * @brief ルートディレクトリ以下のすべての有効なモデルファイル名を取得する（キャッシュ対応）
     * @return ファイル名のリスト
     */
    std::vector<std::string> GetAvailableModels() const;

    /**
     * @brief 利用可能なモデルファイルのリストを再スキャンして更新する
     */
    void RefreshAvailableModels();

    /**
     * @brief 現在の非同期ロードタスクの数を取得
     */
    uint32_t GetPendingTaskCount() const {
        return taskGroup_->GetPendingCount();
    }

    /**
     * @brief すべてのロードタスクが完了したかを取得
     */
    bool IsAllLoaded() const {
        return taskGroup_->IsAllDone() && backgroundTaskGroup_->IsAllDone();
    }

    /**
     * @brief 汎用的な非同期タスクをキューに追加し、判定フラグに基づいてリソースの待機対象にするかを決定する
     * @details シーンの Initialize 中であれば Critical、Update 中であれば Background
     * として扱います（引数で明示指定も可能）。
     * @tparam F 関数型
     * @tparam Args 引数型
     * @param f 実行する関数
     * @param args 関数の引数
     * @return 実行結果を取得するための std::future
     */
    template <class F, class... Args>
    auto EnqueueTask(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>> {
        bool isCritical = IsCurrentSceneInitializing();
        return EnqueueTask(isCritical, std::forward<F>(f), std::forward<Args>(args)...);
    }

    /**
     * @brief 優先度を指定して汎用的な非同期タスクをキューに追加する
     * @param isCritical true の場合、完了するまで SceneManager はシーンの更新・描画を待機します。
     */
    template <class F, class... Args>
    auto EnqueueTask(bool isCritical, F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>> {
        auto& group = isCritical ? taskGroup_ : backgroundTaskGroup_;
        return threadPool_->Enqueue(group, std::forward<F>(f), std::forward<Args>(args)...);
    }

    /**
     * @brief すべてのキャッシュを破棄する
     */
    void ClearAll();

    // --- ロード関数群 (内部的または特殊用途で使用) ---

    /**
     * @brief ヴォクセル化モデルを生成する
     */
    static VoxelizedModel VoxelizeModel(const ObjModel& model, const Irufemi::Vector3Int& resolution,
                                        TextureManager* textureManager);

private:
    /**
     * @brief ファイルパスを正規化し、解決する
     */
    std::string NormalizeAndResolve(const std::string& filename) const;

    /**
     * @brief 文字列が特定の接頭辞で始まっているか判定
     */
    static bool StartsWith(const std::string& s, const std::string& prefix);

    /**
     * @brief フルパスをディレクトリとファイル名に分割する
     */
    static std::pair<std::string, std::string> SplitDirectoryAndFile(const std::string& full);

    /**
     * @brief ロード状況のデバッグログ出力
     */
    void DebugLogLoad(const std::string& key, size_t meshCount);

    /**
     * @brief モデルファイルを再帰的に検索してパスを返す
     */
    std::string FindFileRecursive(const std::string& filename) const;

    /**
     * @brief モデルの読み込み実体（内部用）
     */
    void LoadInternal(std::shared_ptr<ManagedModel> model, const std::string& fullPath);

    /**
     * @brief 現在のシーンが初期化中かどうかを判定する
     */
    bool IsCurrentSceneInitializing() const;

    // --- 内部ユーティリティ ---

private:
    DirectXCommon* dxCommon_ = nullptr;
    TextureManager* textureManager_ = nullptr;
    std::string rootDir_;
    mutable std::recursive_mutex mutex_;
    ResourceCachePool modelPool_;
    mutable std::unordered_map<std::string, ResourceHandle> nameToHandleMap_;
    std::vector<std::shared_ptr<ManagedModel>> managedModels_;
    mutable std::unordered_map<std::string, std::string> filePathCache_;

    mutable std::vector<std::string> availableModelsCache_;
    mutable bool isAvailableModelsCached_ = false;
    std::unique_ptr<ThreadPool> threadPool_;
    std::shared_ptr<TaskGroup> taskGroup_;           ///< 重要タスク用（シーンを止める）
    std::shared_ptr<TaskGroup> backgroundTaskGroup_; ///< バックグラウンド用（シーンを止めない）

    std::unique_ptr<DirectoryWatcher> directoryWatcher_;
    /**
     * @brief モデルアセットディレクトリの変更（追加・更新・削除）を検知した際のハンドラー
     */
    void OnDirectoryChanged();
};
