#pragma once

#include "Renderer/System/Core/BaseBatch.h"
#include "Resource/Model/Data/ObjModel.h"

struct ManagedModel;
struct GpuMesh;
class ModelManager;

/**
 * @class ModelBatch
 * @brief 外部モデルデータ（.obj, .gltf）を大量インスタンシング描画するための領域クラス
 * @note 単一メッシュ（先頭メッシュ）・単一マテリアル専用の高速バッチクラスです。
 *       複数メッシュを内包するモデルの場合は先頭メッシュのみが描画されます。
 */
class ModelBatch : public BaseBatch {
public:
    ModelBatch() = default;
    ~ModelBatch() override = default;

    /**
     * @brief バッチ描画で使用する ModelManager を設定する
     * @param[in] mm 設定する ModelManager のポインタ
     */
    static void SetModelManager(ModelManager* mm) {
        modelManager_ = mm;
    }

    /**
     * @brief 指定したモデルファイル名からバッチ描画領域を初期化する
     * @param[in] objFilename モデルファイル名（例: "cube.obj"）
     */
    void Initialize(const std::string& objFilename);

    /**
     * @brief インスタンシングバッチ描画を実行する
     */
    void Draw() override;

    /**
     * @brief UI向け設定でインスタンシングバッチ描画を実行する
     * @param[in] isUI UIフラグ
     */
    void Draw(bool isUI);

    /**
     * @brief 描画に使用する GPU メッシュリソースを取得する
     * @return 共有 GPU メッシュのポインタ
     */
    const GpuMesh* GetGpuMesh() const; // 共有メッシュ取得

protected:
    /**
     * @brief フラストゥムカリング用のモデル外接球半径を取得する
     * @return モデルの外接球半径
     */
    float GetBoundingSphereRadius() const override;

private:
    /**
     * @brief インスタンシング用の頂点・定数バッファ等のリソースを初期化する
     */
    void InitializeResources();

    /**
     * @brief メッシュのマテリアル定数バッファを生成・割り当てする
     */
    void CreateMaterialResources(const ObjMesh& mesh);
    /**
     * @brief EnsureSharedTexture を実行する。
     */
    void EnsureSharedTexture(const ObjMesh& mesh);

private:
    static ModelManager* modelManager_;

    // 共有モデルデータ(CPU/GPU)
    ResourceHandle modelHandle_{};
    bool isResourcesInitialized_ = false;
};
