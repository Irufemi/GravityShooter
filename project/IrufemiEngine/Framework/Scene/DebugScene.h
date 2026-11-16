#pragma once

/**
 * @file DebugScene.h
 * @brief エンジンの各機能のテストおよび実装例を示すためのデバッグ用シーンクラス
 */

#include "Framework/Scene/BaseScene.h"

#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Framework/Component/Logic/AnimatorComponent.h"
#include "Renderer/Object/3D/Primitive/Primitive3DObject.h"
#include "Renderer/Object/2D/Primitive/Primitive2DObject.h"
#include "Engine/Irufemi.h"
#include "Renderer/Data/LightningParams.h"

#include <memory>
#include <vector>
#include "Renderer/Object/Particle/ParticleObject.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"

// 前方宣言
class IrufemiEngine;

namespace Irufemi {

using ::IrufemiEngine;

/**
 * @class DebugScene
 * @brief 各種オブジェクト（2D、3D、パーティクルなど）の描画テストや、機能の実装例を確認するためのシーン
 */
class DebugScene : public BaseScene {
public: // メンバ関数(ゲーム)
    /**
     * @brief デストラクタ
     */
    ~DebugScene() override;

    /**
     * @brief 初期化処理
     * @param engine エンジンのポインタ
     */
    void Initialize(IrufemiEngine* engine) override;

    /**
     * @brief 更新処理
     */
    void Update() override;

    /**
     * @brief 描画処理
     */
    void Draw() override;

    /**
     * @brief デバッグタブの描画処理（タブ埋め込み）
     */
    void DrawDebugTabItem() override;

    /**
     * @brief シーン独自の独立デバッグウィンドウ描画処理
     */
    void DrawStandaloneDebugWindows() override;

private: // メンバ関数(内部ヘルパ)
private: // メンバ変数(ゲーム)
    std::unique_ptr<Irufemi::Sprite> sprite_ = nullptr;
    bool isActiveSprite_ = false;

    // --- プリミティブ形状のテスト用（汎用クラスに統合済み） ---
    std::unique_ptr<Irufemi::Primitive3DObject> primitiveObj_ = nullptr;
    bool isActivePrimitiveObj_ = false;

    std::unique_ptr<Irufemi::Primitive2DObject> primitive2DObj_ = nullptr;
    bool isActivePrimitive2DObj_ = false;

    std::unique_ptr<Irufemi::StaticModelObject> obj_ = nullptr;
    bool isActiveObj_ = false;

    std::unique_ptr<Irufemi::StaticModelObject> utashTeapot_ = nullptr;
    bool isActiveUtashTeapot_ = false;

    std::unique_ptr<Irufemi::StaticModelObject> stanfordBunny_ = nullptr;
    bool isActiveStanfordBunny_ = false;

    std::unique_ptr<Irufemi::StaticModelObject> multiMesh_ = nullptr;
    bool isActiveMultiMesh_ = false;

    std::unique_ptr<Irufemi::StaticModelObject> multiMaterial_ = nullptr;
    bool isActiveMultiMaterial_ = false;

    std::unique_ptr<Irufemi::StaticModelObject> suzanne_ = nullptr;
    bool isActiveSuzanne_ = false;

    std::unique_ptr<Irufemi::StaticModelObject> fence_ = nullptr;
    bool isActiveFence_ = false;

    std::unique_ptr<Irufemi::StaticModelObject> terrain_ = nullptr;
    bool isActiveTerrain_ = false;

    Irufemi::VoxelParticleManager::EmitterHandle voxelEmitterHandle_{};
    Irufemi::VoxelEmitter voxelEmitParams_{};
    bool isActiveVoxelParticle_ = false;

    // ハードコーディングされたGPUパーティクルのテスト用
    std::unique_ptr<Irufemi::ParticleObject> particleObj_ = nullptr;
    bool isActiveGPUParticle_ = false;

    std::shared_ptr<GameObject> animatedCube_ = nullptr;
    bool isActiveAnimatedCube_ = false;

    std::shared_ptr<GameObject> walk_ = nullptr;
    bool isActiveWalk_ = false;

    std::shared_ptr<GameObject> sneakWalk_ = nullptr;
    bool isActiveSneakWalk_ = false;

    std::shared_ptr<GameObject> blendTest_ = nullptr;
    bool isActiveBlendTest_ = false;

    std::unique_ptr<Irufemi::Skybox> skybox_ = nullptr;

    // --- ライト ---
    bool isActiveSkybox_ = false;

    // --- ImGuiデモ ---
    bool isActiveImGuiDemo_ = false;

    // --- 電撃エフェクトデモ ---
    bool isActiveLightningCrawl_ = false;
    std::unique_ptr<Irufemi::Primitive3DObject> lightningCylinder_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> lightningParamsResource_ = nullptr;
    Irufemi::LightningParams* lightningParamsData_ = nullptr;

private: // メンバ変数(システム)
#ifdef USE_IMGUI
    ImGuizmo::OPERATION gizmoOperation_ = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE gizmoMode_ = ImGuizmo::LOCAL;
#endif
};

} // namespace Irufemi

