#pragma once
#include "Renderer/System/Core/IRenderable.h"
#include "Renderer/System/Core/MultiBufferSyncState.h"

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Renderer/Data/TransformationMatrix.h"
#include "Resource/Model/Data/ObjModel.h"
#include "Renderer/System/Core/Object3DResource.h"
#include "Renderer/Data/Material.h"
#include <d3d12.h>
#include <string>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>

// 前方宣言
class Camera;
class IrufemiEngine;
struct ManagedModel;
struct ObjMaterial;

/**
 * @class BaseModel
 * @brief 3Dモデル（静的メッシュおよびアニメーションメッシュ）の共通基底クラス
 * @details Transform管理、マテリアルオーバーライド、フラストゥムカリング、
 * および基本リソース管理を提供します。
 */
class BaseModel : public IRenderable, public MultiBufferSyncState {
public: // メンバ関数
    virtual ~BaseModel();

    // 描画用の変換行列リソースのGPUアドレスを取得
    /**
     * @brief TransformationGpuAddress を取得する。
     * @return 取得された TransformationGpuAddress
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetTransformationGpuAddress() const;

protected: // 内部ヘルパ
    /**
     * @brief CPU側のマテリアルデータをGPUリソースへ転送する（内部用）
     */
    void UpdateMaterials();

public: // ゲッター・セッター
    /**
     * @brief モデルが持つメッシュ数を取得
     */
    size_t GetMeshCount() const;

    // 指定したインデックスのメッシュのマテリアルを取得(読み取り専用)
    /**
     * @brief Material を取得する。
     * @return 取得された Material
     */
    const ObjMaterial* GetMaterial(size_t meshIndex) const;
    // 指定したインデックスのメッシュのマテリアルを取得(書き込み可能)
    /**
     * @brief Material を取得する。
     * @return 取得された Material
     */
    ObjMaterial* GetMaterial(size_t meshIndex);

    /**
     * @brief Color を設定する。
     * @param[in] color 設定する Color の値
     */
    void SetColor(const Irufemi::Vector4& color) {
        color_ = color;
        MarkAsDirty();
    }
    /**
     * @brief Color を取得する。
     * @return 取得された Color
     */
    const Irufemi::Vector4& GetColor() const {
        return color_;
    }

    /**
     * @brief MaterialOverrides を設定する。
     * @param[in] overrides 設定する
     * MaterialOverrides（メッシュインデックスごとのマテリアルオーバーライド）のマップポインタ
     */
    void SetMaterialOverrides(const std::unordered_map<size_t, ObjMaterial>* overrides) {
        materialOverrides_ = overrides;
        MarkAsDirty();
    }
    const std::unordered_map<size_t, ObjMaterial>* GetMaterialOverrides() const {
        return materialOverrides_;
    }

    /**
     * @brief Alpha を設定する。
     * @param[in] alpha 設定する Alpha の値
     */
    void SetAlpha(float alpha) {
        color_.w = alpha;
        MarkAsDirty();
    }

    /**
     * @brief Position を設定する。
     * @param[in] position 設定する Position の値
     */
    void SetPosition(const Irufemi::Vector3& position) {
        transform_.translate = position;
        MarkAsDirty();
    }
    /**
     * @brief Position を取得する。
     * @return 取得された Position
     */
    const Irufemi::Vector3& GetPosition() const {
        return transform_.translate;
    }

    /**
     * @brief CpuModel を取得する。
     * @return 取得された CpuModel
     */
    std::shared_ptr<ObjModel> GetCpuModel() const;

    /**
     * @brief Translate を設定する。
     * @param[in] translate 設定する Translate の値
     */
    void SetTranslate(const Irufemi::Vector3& translate) {
        transform_.translate = translate;
        MarkAsDirty();
    }

    /**
     * @brief Rotate を設定する。
     * @param[in] rotate 設定する Rotate の値
     */
    void SetRotate(const Irufemi::Vector3& rotate) {
        transform_.rotate = rotate;
        MarkAsDirty();
    }
    /**
     * @brief Rotate を取得する。
     * @return 取得された Rotate
     */
    const Irufemi::Vector3& GetRotate() const {
        return transform_.rotate;
    }

    /**
     * @brief RotateX を設定する。
     * @param[in] rotate 設定する RotateX の値
     */
    void SetRotateX(const float& rotate) {
        transform_.rotate.x = rotate;
        MarkAsDirty();
    }
    /**
     * @brief RotateY を設定する。
     * @param[in] rotate 設定する RotateY の値
     */
    void SetRotateY(const float& rotate) {
        transform_.rotate.y = rotate;
        MarkAsDirty();
    }
    /**
     * @brief RotateZ を設定する。
     * @param[in] rotate 設定する RotateZ の値
     */
    void SetRotateZ(const float& rotate) {
        transform_.rotate.z = rotate;
        MarkAsDirty();
    }

    /**
     * @brief Scale を設定する。
     * @param[in] scale 設定する Scale の値
     */
    void SetScale(const Irufemi::Vector3& scale) {
        transform_.scale = scale;
        MarkAsDirty();
    }
    /**
     * @brief Scale を取得する。
     * @return 取得された Scale
     */
    const Irufemi::Vector3& GetScale() const {
        return transform_.scale;
    }

    /**
     * @brief Transform を設定する。
     * @param[in] transform 設定する Transform の値
     */
    void SetTransform(const Irufemi::Transform& transform) {
        transform_ = transform;
        MarkAsDirty();
    }
    /**
     * @brief Transform を取得する。
     * @return 取得された Transform
     */
    const Irufemi::Transform& GetTransform() const {
        return transform_;
    }

    /**
     * @brief EnvironmentCoefficient を設定する。
     * @param[in] coefficient 設定する EnvironmentCoefficient の値
     */
    void SetEnvironmentCoefficient(float coefficient) {
        environmentCoefficient_ = coefficient;
        isDirty_ = true;
        MarkAsDirty();
    }
    /**
     * @brief EnvironmentCoefficient を取得する。
     * @return 取得された EnvironmentCoefficient
     */
    float GetEnvironmentCoefficient() const {
        return environmentCoefficient_;
    }

    /**
     * @brief LightingModeOverride を設定する。
     * @param[in] mode 設定する LightingModeOverride の値
     */
    void SetLightingModeOverride(int32_t mode) {
        lightingModeOverride_ = mode;
        isDirty_ = true;
        MarkAsDirty();
    }
    /**
     * @brief UseClampSamplerOverride を設定する。
     * @param[in] useClamp 設定する UseClampSamplerOverride の値
     */
    void SetUseClampSamplerOverride(int32_t useClamp) {
        useClampSamplerOverride_ = useClamp;
        isDirty_ = true;
        MarkAsDirty();
    }
    /**
     * @brief EnableLightingOverride を設定する。
     * @param[in] enable 設定する EnableLightingOverride の値
     */
    void SetEnableLightingOverride(int32_t enable) {
        enableLightingOverride_ = enable;
        isDirty_ = true;
        MarkAsDirty();
    }
    /**
     * @brief EnableEffectMask を設定する。
     * @param[in] enable 設定する EnableEffectMask の値
     */
    void SetEnableEffectMask(bool enable) {
        enableEffectMask_ = enable;
        isDirty_ = true;
        MarkAsDirty();
    }
    /**
     * @brief CustomEffectType を設定する。
     * @param[in] type 設定する CustomEffectType の値
     */
    void SetCustomEffectType(int32_t type) {
        customEffectType_ = type;
        isDirty_ = true;
        MarkAsDirty();
    }
    /**
     * @brief CustomEffectParam を設定する。
     * @param[in] param 設定する CustomEffectParam の値
     */
    void SetCustomEffectParam(float param) {
        customEffectParam_ = param;
        isDirty_ = true;
        MarkAsDirty();
    }

    /**
     * @brief EnableLightingToAllMeshes を設定する。
     * @param[in] enable 設定する EnableLightingToAllMeshes の値
     */
    void SetEnableLightingToAllMeshes(bool enable) {
        enableLightingOverride_ = enable ? 1 : 0;
        MarkAsDirty();
    }

    /**
     * @brief 全メッシュリソースに環境マップ（CubeMap）を設定する
     * @param[in] handle 環境マップテクスチャハンドル
     */
    void SetEnvironmentMap(ResourceHandle handle) {
        for (auto& res : meshResources_) {
            if (res) {
                res->SetEnvironmentMapHandle(handle);
            }
        }
    }

    /**
     * @brief MarkAsDirty を実行する。
     */
    void MarkAsDirty() override {
        MultiBufferSyncState::MarkAsDirty();
        for (auto& res : meshResources_) {
            if (res) {
                res->MarkAsDirty();
            }
        }
    }

    /**
     * @brief IrufemiEngine を設定する。
     * @param[in] engine 設定する IrufemiEngine の値
     */
    static void SetIrufemiEngine(IrufemiEngine* engine) {
        engine_ = engine;
    }

    /**
     * @brief CullingEnabled を設定する。
     * @param[in] enabled 設定する CullingEnabled の値
     */
    void SetCullingEnabled(bool enabled) {
        isCullingEnabled_ = enabled;
    }
    /**
     * @brief IsCullingEnabled かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsCullingEnabled() const {
        return isCullingEnabled_;
    }
    /**
     * @brief CastShadows を設定する。
     * @param[in] cast 設定する CastShadows の値
     */
    void SetCastShadows(bool cast) {
        castShadows_ = cast;
    }
    /**
     * @brief CastShadows を取得する。
     * @return 取得された CastShadows
     */
    bool GetCastShadows() const {
        return castShadows_;
    }

    /**
     * @brief TransformationMatrix を取得する。
     * @return 取得された TransformationMatrix
     */
    const TransformationMatrix& GetTransformationMatrix() const {
        return transformationMatrix_;
    }
    /**
     * @brief TransformationMatrix を設定する。
     * @param[in] transformationMatrix 設定する TransformationMatrix の値
     */
    void SetTransformationMatrix(const TransformationMatrix& transformationMatrix) {
        transformationMatrix_ = transformationMatrix;
    }

    /**
     * @brief CustomPSO を設定する（生ポインタ指定・下位互換用）。
     * @param[in] pso 設定する CustomPSO の値
     */
    void SetCustomPSO(ID3D12PipelineState* pso) {
        for (auto& res : meshResources_) {
            if (res) {
                res->SetCustomPSO(pso);
            }
        }
    }
    /**
     * @brief CustomPSO を設定する（名前指定・推奨）。
     * @param[in] psoName 設定する PSO 名
     * @param[in] blend ブレンドモード
     * @param[in] depth 深度設定
     * @param[in] cull カリングモード
     */
    void SetCustomPSO(const std::string& psoName, Irufemi::BlendMode blend = Irufemi::BlendMode::kBlendModeNormal,
                      PSOManager::DepthWrite depth = PSOManager::DepthWrite::Enable,
                      PSOManager::CullMode cull = PSOManager::CullMode::Back) {
        for (auto& res : meshResources_) {
            if (res) {
                res->SetCustomPSO(psoName, blend, depth, cull);
            }
        }
    }
    /**
     * @brief CustomCBVAddress を設定する。
     * @param[in] addr 設定する CustomCBVAddress の値
     */
    void SetCustomCBVAddress(D3D12_GPU_VIRTUAL_ADDRESS addr) {
        for (auto& res : meshResources_) {
            if (res) {
                res->SetCustomCBVAddress(addr);
            }
        }
    }

protected: // メンバ変数
    // 共有モデルデータ(CPU/GPU)
    ResourceHandle modelHandle_;
    ResourceHandle nextModelHandle_; // 次フレームで切り替えるためのモデル
    bool isModelChanged_ = false;    // モデル切り替えフラグ

    // オブジェクト全体のTransform
    Irufemi::Transform transform_{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    TransformationMatrix transformationMatrix_{};
    Irufemi::Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f}; // インスタンスカラー
    float environmentCoefficient_ = 1.0f;               // インスタンス環境マップ係数
    int32_t lightingModeOverride_ = -1;                 // -1:使用しない, 0以上:上書き
    int32_t useClampSamplerOverride_ = -1;              // -1:使用しない, 0以上:上書き
    int32_t enableLightingOverride_ = -1;               // -1:使用しない, 0以上:上書き

    // マテリアル個別上書き
    const std::unordered_map<size_t, ObjMaterial>* materialOverrides_ = nullptr;

    // リソース (メッシュごとの頂点/インデックスなど)
    bool enableEffectMask_ = false;  // エフェクトから保護するかどうか
    int32_t customEffectType_ = 0;   // カスタムエフェクトのタイプ
    float customEffectParam_ = 0.0f; // カスタムエフェクトのパラメータ

    // --- 描画リソース ---
    std::vector<std::unique_ptr<Object3DResource>> meshResources_;

    // 変換行列用リソース (全メッシュ共有)
    uint32_t transformCbIndex_ = static_cast<uint32_t>(-1);

    static IrufemiEngine* engine_;

    // 行列更新の最適化用
    bool isDirty_ = true;

    bool isCullingEnabled_ = true;
    bool castShadows_ = true;
    Irufemi::Matrix4x4 lastViewMatrix_ = {};
    Irufemi::Matrix4x4 lastProjectionMatrix_ = {};
};
