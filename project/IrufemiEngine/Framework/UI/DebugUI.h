#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <array>
#include <cstddef>
#include <memory>
#include <vector>
#include "Core/Type/BlendMode.h"
#include "Renderer/Pipeline/PSOManager.h"

// 前方宣言
class TextureManager;
class SceneManager;
class IrufemiEngine;
class DirectXCommon;
class ThreadPool;
class Object3DResource;
class Object2DResource;
struct Material;
namespace Irufemi {
struct ObjMaterial;
struct Transform;
struct Matrix4x4;
struct Sphere;
struct Animation;
} // namespace Irufemi

struct DirectionalLight;
struct PointLight;
struct SpotLight;
struct AreaLight;
struct LightningParams;

#ifdef USE_IMGUI

#include "imgui/imgui.h"
#include "imgui/ImGuizmo.h"

#endif // USE_IMGUI

/**
 * @class DebugUI
 * @brief ImGuiを使用したデバッグインターフェースを管理するクラス
 * @details 画面上に各種パラメータ（ライト、トランスフォーム、マテリアル等）を調整・確認するためのUIを表示します。
 *          パフォーマンス計測（FPS/フレーム時間）機能も備えています。
 */
class DebugUI {
private: // メンバ変数
    // ポインタ参照(非所有)

    DirectXCommon* dxCommon_ = nullptr;

    TextureManager* textureManager_ = nullptr;

    uint32_t srvIndex_ = 0xFFFFFFFF;

public: // メンバ関数
    /** @name 初期化・終了処理 */
    ///@{
    /**
     * @brief 初期化
     */
    void Initialize(HWND hwnd, DirectXCommon* dxCommon);

    /**
     * @brief TextureManagerをセットする
     */
    void SetTextureManager(TextureManager* textureManager) {
        this->textureManager_ = textureManager;
    }

    /**
     * @brief 終了処理
     */
    void Shutdown();
    ///@}

#ifdef USE_IMGUI
    /**
     * @brief WndProcHandler を実行する。
     */
    static LRESULT WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif // USE_IMGUI

    /** @name フレーム制御 */
    ///@{
    /**
     * @brief フレーム開始処理
     */
    void FrameStart();

    /**
     * @brief 描画コマンドの積み込み
     */
    void QueueDrawCommands();

    /**
     * @brief ポストプロセス以降の描画コマンド積み込み
     */
    void QueuePostDrawCommands();
    ///@}

    /** @name ライト・座標系のデバッグ */
    ///@{
    /**
     * @brief ライト全体の編集UIを表示する
     */
    static void DebugLights(DirectionalLight* directionalLight, std::vector<std::unique_ptr<PointLight>>& pointLights,
                            std::vector<std::unique_ptr<SpotLight>>& spotLights,
                            std::vector<std::unique_ptr<AreaLight>>& areaLights);

    /**
     * @brief 3Dトランスフォームの編集
     */
    static void DebugTransform(Irufemi::Transform& transform);

    /**
     * @brief 2Dトランスフォーム（スプライト用）の編集
     */
    static void DebugTransform2D(Irufemi::Transform& transform);

    /**
     * @brief トランスフォーム情報のテキスト表示
     */
    static void TextTransform(Irufemi::Transform& transform, const char* name = "");
    ///@}

    /** @name マテリアル・テクスチャのデバッグ */
    ///@{
    static void DebugMaterialBy3D(Material* material);

    /**
     * @brief 2Dスプライト用マテリアルのプロパティ（カラー、UVタイリング等）を編集するImGui UIを表示する
     * @param[in,out] material 編集対象のマテリアル
     */
    static void DebugMaterialBy2D(Material* material);

    /**
     * @brief OBJモデル用マテリアルのプロパティ（環境光・拡散反射光・鏡面反射光等）を編集するImGui UIを表示する
     * @param[in,out] material 編集対象のOBJマテリアル
     * @param[in] unique_id ImGuiのウィジェット識別子用プレフィックス
     */
    static void DebugObjMaterial(Irufemi::ObjMaterial* material, const char* unique_id = "");

    /**
     * @brief パーティクル用マテリアルのブレンドモード・カラー設定を編集するImGui UIを表示する
     * @param[in,out] material 編集対象のマテリアル
     */
    static void DebugMaterialByParticle(Material* material);

    /**
     * @brief 3Dオブジェクト用テクスチャの選択・差し替えUIを表示する
     * @param[in,out] resource 編集対象の3Dリソース
     * @param[in,out] selectedTextureIndex 選択中のテクスチャインデックス
     */
    void DebugTexture(Object3DResource* resource, int& selectedTextureIndex);
    /**
     * @brief 2Dスプライト用テクスチャの選択・差し替えUIを表示する
     * @param[in,out] resource 編集対象の2Dリソース
     * @param[in,out] selectedTextureIndex 選択中のテクスチャインデックス
     */
    void DebugTexture(Object2DResource* resource, int& selectedTextureIndex);

    /**
     * @brief 平行光源（DirectionalLight）の向き・色・強度を編集するImGui UIを表示する
     * @param[in,out] directionalLightData 編集対象のディレクショナルライトデータ
     */
    static void DebugDirectionalLight(DirectionalLight* directionalLightData);

    /**
     * @brief UV座標変換（Transform形式：スケール・回転・移動）を編集するImGui UIを表示する
     * @param[in,out] uvTransform 編集対象のUVトランスフォーム
     */
    static void DebugUvTransform(Irufemi::Transform& uvTransform);

    /**
     * @brief UV座標変換（Matrix4x4形式）を分解・編集するImGui UIを表示する
     * @param[in,out] uvTransform 編集対象のUV変換行列
     */
    static void DebugUvTransform(Irufemi::Matrix4x4& uvTransform);

    /**
     * @brief マテリアルの個別オーバーライド設定を編集するUI
     */
    static void DebugMaterialOverrides(float* envCoef, int32_t* lightingMode, int32_t* useClamp,
                                       int32_t* enableLighting, const char* unique_id = "");

    /**
     * @brief 電撃エフェクト調整UI
     */
    static void DebugLightning(LightningParams* params);

    /**
     * @brief アニメーション制御UI
     */
    static void DebugAnimationControl(const Irufemi::Animation& animation, float& currentTime,
                                      const char* unique_id = "");
    ///@}

    /** @name エンジン情報のデバッグ */
    ///@{
    /**
     * @brief 形状情報の表示（球体等）
     */
    static void DebugSphereInfo(Irufemi::Sphere& sphere);

    /**
     * @brief シーン切り替えタブの表示
     */
    void SceneSelectorTab(SceneManager* sm);

    /**
     * @brief ポストプロセス調整タブの表示
     */
    void PostProcessTab(IrufemiEngine* engine);

    /**
     * @brief ScreenCaptureManager 用のデバッグタブ
     */
    void ScreenCaptureTab(class ScreenCaptureManager* captureManager);

    /**
     * @brief 統合デバッグウィンドウの開始
     * @param[in,out] pOpen ウィンドウの開閉フラグ（nullptrの場合は閉じるボタンなし）
     */
    bool BeginEngineDebugWindow(bool* pOpen = nullptr);

    /**
     * @brief 統合デバッグウィンドウの終了
     */
    void EndEngineDebugWindow();

    /**
     * @brief エンジン共通の各種デバッグタブ（PostProcess, Scene, Display, Capture, GPUParticle等）を描画する
     * @details タブバー上でマウスホイールを回転することで、前後のタブ（ページ）への切り替えに対応します。
     * @param[in] engine エンジンのポインタ
     */
    void DrawCommonEngineTabs(IrufemiEngine* engine);

    /**
     * @brief PSO設定（描画ステート）の編集UI
     */
    static void DebugPsoSettings(Irufemi::BlendMode* blendMode, PSOManager::DepthWrite* depthWrite,
                                 PSOManager::CullMode* cullMode, const char* unique_id = "##PsoSettings");
    ///@}
};
