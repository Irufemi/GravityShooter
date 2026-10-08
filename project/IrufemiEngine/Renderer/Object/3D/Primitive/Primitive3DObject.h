#pragma once

#include "Renderer/System/Core/IRenderable.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "Core/Type/PrimitiveType.h"
#include "Renderer/System/Data/RenderData.h"

// 前方宣言
class Camera;
class TextureManager;
class DrawManager;
class DebugUI;
struct PrimitiveData;

/**
 * @class Primitive3DObject
 * @brief 汎用的な3Dプリミティブ（立方体、球、平面など）を管理・描画するクラス
 * @details コンポーネント指向に基づき、メッシュ・マテリアル・トランスフォームの各機能を内部に持ちます。
 *          ImGuiエディタからのリアルタイムな形状変更やプロパティ編集に対応します。
 */
class Primitive3DObject : public IRenderable {
public:
    Primitive3DObject() = default;
    ~Primitive3DObject() = default;

    /**
     * @brief 初期化処理
     * @param[in] camera 使用するカメラのポインタ
     * @param[in] type 初期形状タイプ
     * @param[in] texturePath 使用するテクスチャのパス
     */
    void Initialize(Irufemi::PrimitiveType type, const std::string& textureName = "resources/uvChecker.png");

    /**
     * @brief 更新処理
     */
    void Update();

    /**
     * @brief 描画処理
     */
    void SyncBeforeDraw() override;
    /**
     * @brief Draw を実行する。
     */
    void Draw() override;
    /**
     * @brief Draw を実行する。
     */
    void Draw(bool isUI);
    /**
     * @brief DrawOutlineMask を実行する。
     */
    void DrawOutlineMask() override;

    /**
     * @brief 描画処理（カメラを外部から指定する場合）
     * @param[in] camera 描画に使用するカメラ
     */
    void Draw(const Camera& camera);
    /**
     * @brief Draw を実行する。
     */
    void Draw(const Camera& camera, bool isUI);

    /**
     * @brief ImGuiによるデバッグ・編集用UIを表示する
     * @param[in] label UIウィンドウおよび識別用のラベル
     */
    void Debug(const char* label = "Primitive Object");

    // --- 各コンポーネントへのアクセサ ---
    /**
     * @brief Transform を取得する。
     * @return 取得された Transform
     */
    PrimitiveTransform& GetTransform() {
        return transform_;
    }
    /**
     * @brief Transform を取得する。
     * @return 取得された Transform
     */
    const PrimitiveTransform& GetTransform() const {
        return transform_;
    }
    /**
     * @brief Mesh を取得する。
     * @return 取得された Mesh
     */
    MeshDesc& GetMesh() {
        return mesh_;
    }
    /**
     * @brief Mesh を取得する（読み取り専用）。
     * @return 取得された Mesh
     */
    const MeshDesc& GetMesh() const {
        return mesh_;
    }
    /**
     * @brief Material を取得する。
     * @return 取得された Material
     */
    MaterialDesc& GetMaterial() {
        return material_;
    }
    /**
     * @brief Material を取得する（読み取り専用）。
     * @return 取得された Material
     */
    const MaterialDesc& GetMaterial() const {
        return material_;
    }
    /**
     * @brief IsCullingEnabled かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsCullingEnabled() const {
        return isCullingEnabled_;
    }

    // --- 補助メソッド ---
    /**
     * @brief Center を取得する。
     * @return 取得された Center
     */
    Irufemi::Vector3 GetCenter() const {
        return transform_.transform.translate;
    }
    /**
     * @brief Right を取得する。
     * @return 取得された Right
     */
    Irufemi::Vector3 GetRight() const;
    /**
     * @brief Up を取得する。
     * @return 取得された Up
     */
    Irufemi::Vector3 GetUp() const;
    /**
     * @brief Direction を取得する。
     * @return 取得された Direction
     */
    Irufemi::Vector3 GetDirection() const;

    // --- ヘルパーSetter ---
    /**
     * @brief Transform を設定する。
     * @param[in] t 設定する Transform の値
     */
    void SetTransform(const Irufemi::Transform& t) {
        transform_.transform = t;
        transform_.isDirty = true;
    }
    /**
     * @brief Position を設定する。
     * @param[in] pos 設定する Position の値
     */
    void SetPosition(const Irufemi::Vector3& pos) {
        transform_.transform.translate = pos;
        transform_.isDirty = true;
    }
    /**
     * @brief Rotate を設定する。
     * @param[in] rot 設定する Rotate の値
     */
    void SetRotate(const Irufemi::Vector3& rot) {
        transform_.transform.rotate = rot;
        transform_.isDirty = true;
    }
    /**
     * @brief Scale を設定する。
     * @param[in] scale 設定する Scale の値
     */
    void SetScale(const Irufemi::Vector3& scale) {
        transform_.transform.scale = scale;
        transform_.isDirty = true;
    }
    /**
     * @brief Color を設定する。
     * @param[in] color 設定する Color の値
     */
    void SetColor(const Irufemi::Vector4& color) {
        material_.color = color;
    }
    /**
     * @brief Texture を設定する。
     * @param[in] path 設定する Texture の値
     */
    void SetTexture(const std::string& path) {
        material_.texturePath = path;
    }
    /**
     * @brief Shape を設定する。
     * @param[in] type 設定する Shape の値
     */
    void SetShape(Irufemi::PrimitiveType type) {
        mesh_.ChangeMesh(type);
        transform_.isDirty = true;
    }
    /**
     * @brief CustomPSO を設定する（生ポインタ指定・下位互換用）。
     * @param[in] pso 設定する CustomPSO の値
     */
    void SetCustomPSO(ID3D12PipelineState* pso) {
        if (mesh_.resource) {
            mesh_.resource->SetCustomPSO(pso);
        }
    }
    /**
     * @brief CustomPSO を設定する（名前指定・推奨）。
     * @param[in] psoName 設定する PSO 名
     * @param[in] blend ブレンドモード
     * @param[in] depth 深度書き込み設定
     * @param[in] cull カリングモード
     */
    void SetCustomPSO(const std::string& psoName, Irufemi::BlendMode blend = Irufemi::BlendMode::kBlendModeNormal,
                      PSOManager::DepthWrite depth = PSOManager::DepthWrite::Enable,
                      PSOManager::CullMode cull = PSOManager::CullMode::Back) {
        if (mesh_.resource) {
            mesh_.resource->SetCustomPSO(psoName, blend, depth, cull);
        }
    }
    /**
     * @brief CustomCBVAddress を設定する。
     * @param[in] addr 設定する CustomCBVAddress の値
     */
    void SetCustomCBVAddress(D3D12_GPU_VIRTUAL_ADDRESS addr) {
        if (mesh_.resource) {
            mesh_.resource->SetCustomCBVAddress(addr);
        }
    }

    /**
     * @brief カスタムの PrimitiveData を用いて現在のリソースを破棄し再初期化する
     * @param[in] data 再生成に使用する頂点・インデックスデータ
     */
    void ReinitializeMesh(const PrimitiveData& data);

    /**
     * @brief CullingEnabled を設定する。
     * @param[in] enabled 設定する CullingEnabled の値
     */
    void SetCullingEnabled(bool enabled) {
        isCullingEnabled_ = enabled;
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

    // --- コールバック ---
    using CustomSyncCallback = std::function<void(uint32_t frameIndex)>;
    /**
     * @brief 描画前のバッファ同期時に呼び出されるコールバックを設定する
     * @param[in] callback 現在のフレームインデックスを受け取る関数
     */
    void SetCustomSyncCallback(CustomSyncCallback callback) {
        customSyncCallback_ = std::move(callback);
    }
    /**
     * @brief 半透明・エフェクトかどうかを設定する（trueにするとZソート付きで奥の不透明モデルの後に描画される）
     */
    void SetIsTransparent(bool isTransparent) {
        isTransparent_ = isTransparent;
    }

    /**
     * @brief インスタンス固有の TextureManager を設定する。
     * @param[in] tm 設定する TextureManager のポインタ
     */
    void SetTextureManagerInstance(TextureManager* tm) {
        instTextureManager_ = tm;
    }

    /**
     * @brief インスタンス固有の DrawManager を設定する。
     * @param[in] dm 設定する DrawManager のポインタ
     */
    void SetDrawManagerInstance(DrawManager* dm) {
        instDrawManager_ = dm;
    }

    /**
     * @brief インスタンス固有の IrufemiEngine を設定する。
     * @param[in] engine 設定する IrufemiEngine のポインタ
     */
    void SetEngineInstance(class IrufemiEngine* engine) {
        instEngine_ = engine;
    }

    /**
     * @brief 使用する TextureManager を取得する（インスタンスメンバ優先）。
     * @return TextureManager のポインタ
     */
    TextureManager* GetTextureManagerInstance() const {
        return instTextureManager_ ? instTextureManager_ : textureManager_;
    }

    /**
     * @brief 使用する DrawManager を取得する（インスタンスメンバ優先）。
     * @return DrawManager のポインタ
     */
    DrawManager* GetDrawManagerInstance() const {
        return instDrawManager_ ? instDrawManager_ : drawManager_;
    }

    /**
     * @brief 使用する IrufemiEngine を取得する（インスタンスメンバ優先）。
     * @return IrufemiEngine のポインタ
     */
    class IrufemiEngine* GetEngineInstance() const {
        return instEngine_ ? instEngine_ : engine_;
    }

    // --- 静的各種マネージャの設定 ---
    /**
     * @brief TextureManager を設定する。
     * @param[in] texM 設定する TextureManager の値
     */
    static void SetTextureManager(TextureManager* texM) {
        textureManager_ = texM;
    }
    /**
     * @brief TextureManager を取得する。
     * @return 取得された TextureManager
     */
    static TextureManager* GetTextureManager() {
        return textureManager_;
    }
    /**
     * @brief DrawManager を設定する。
     * @param[in] drawM 設定する DrawManager の値
     */
    static void SetDrawManager(DrawManager* drawM) {
        drawManager_ = drawM;
    }
    /**
     * @brief DebugUI を設定する。
     * @param[in] ui 設定する DebugUI の値
     */
    static void SetDebugUI(DebugUI* ui) {
        ui_ = ui;
    }
    /**
     * @brief Engine を設定する。
     * @param[in] engine 設定する Engine の値
     */
    static void SetEngine(class IrufemiEngine* engine) {
        engine_ = engine;
    }

private:
    PrimitiveTransform transform_;            //!< トランスフォームコンポーネント
    MeshDesc mesh_;                           // 形状データ
    MaterialDesc material_;                   // マテリアルデータコンポーネント
    bool isCullingEnabled_ = true;            //!< 視錐台カリングの有効フラグ
    bool castShadows_ = true;                 //!< 影を落とすフラグ
    bool isTransparent_ = false;              //!< 半透明・エフェクト（遅延・Zソート描画）フラグ
    CustomSyncCallback customSyncCallback_;   //!< カスタムの同期処理用コールバック
    Irufemi::Matrix4x4 lastCameraViewProj_{}; //!< 前回同期時のカメラ行列キャッシュ

    // インスタンスポインタ（優先）
    TextureManager* instTextureManager_ = nullptr;
    DrawManager* instDrawManager_ = nullptr;
    class IrufemiEngine* instEngine_ = nullptr;

    // 静的ポインタ（既存の設計パターンを継承）
    static TextureManager* textureManager_;
    static DrawManager* drawManager_;
    static DebugUI* ui_;
    static class IrufemiEngine* engine_;
};
