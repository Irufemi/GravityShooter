#pragma once

#include "Renderer/System/Core/IRenderable.h"
#include <d3d12.h>
#include <vector>
#include <string>
#include <cstdint>
#include "Renderer/System/Core/Object2DResource.h"
#include "Core/Math/Vector2.h"
#include <wrl.h>
#include <memory>

// 前方宣言
class TextureManager;
class DrawManager;
class DebugUI;
class CameraManager;

/**
 * @class Sprite
 * @brief 2Dスプライトを描画・管理するクラス
 * @details
 * テクスチャの表示、座標変換（位置・回転・拡縮）、アンカーポイントの設定、トリミング（Rect指定）などを行います。
 */
class Sprite : public IRenderable {
public:
    Sprite() = default;
    explicit Sprite(TextureManager* tm, DrawManager* dm = nullptr, CameraManager* cm = nullptr)
        : instTextureManager_(tm), instDrawManager_(dm), instCameraManager_(cm) {}

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
     * @brief インスタンス固有の CameraManager を設定する。
     * @param[in] cm 設定する CameraManager のポインタ
     */
    void SetCameraManagerInstance(CameraManager* cm) {
        instCameraManager_ = cm;
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
     * @brief 使用する CameraManager を取得する（インスタンスメンバ優先）。
     * @return CameraManager のポインタ
     */
    CameraManager* GetCameraManagerInstance() const {
        return instCameraManager_ ? instCameraManager_ : cameraManager_;
    }

private:
    std::unique_ptr<Object2DResource> resource_ = nullptr;

    bool isRotateY_ = true;

    int selectedTextureIndex_ = 0;

    // インスタンス依存ポインタ（優先）
    TextureManager* instTextureManager_ = nullptr;
    DrawManager* instDrawManager_ = nullptr;
    CameraManager* instCameraManager_ = nullptr;

    // 静的フォールバックポインタ
    static CameraManager* cameraManager_;
    static TextureManager* textureManager_;
    static DrawManager* drawManager_;
    static DebugUI* ui_;

    // サイズとアンカー
    Irufemi::Vector2 size_{640.0f, 360.0f}; // 既存の見た目互換のため初期値を640x360に
    Irufemi::Vector2 anchor_{0.0f, 0.0f};   // 左上(0,0) / 中央(0.5,0.5) / 右下(1,1)

    // フリップ状態
    bool isFlipX_ = false;
    bool isFlipY_ = false;

    // 現在のテクスチャのピクセルサイズ(取得できない場合は 0)
    Irufemi::Vector2 textureSize_{0.0f, 0.0f};

    // 切り出し矩形(ピクセル指定)
    bool useTexRect_ = false;
    Irufemi::Vector2 texRectLeftTop_{0.0f, 0.0f}; // px
    Irufemi::Vector2 texRectSize_{0.0f, 0.0f};    // px

    /**
     * @brief 現在のテクスチャ解像度にスプライトサイズを合わせる（内部用）
     */
    void AdjustTextureSize();

    /**
     * @brief アンカー反映で頂点ローカル座標を更新（内部用）
     */
    void ApplyAnchorToVertices();

public: // メンバ関数
    /**
     * @brief デストラクタ
     */
    ~Sprite() = default;

    /**
     * @brief 初期化
     * @param[in] textureName 使用するテクスチャ名（ファイルパス）
     */
    void Initialize(const std::string& textureName = "resources/uvChecker.png");

    /**
     * @brief 更新処理
     * @details 行列計算や定数バッファの更新を行います。
     */
    void Update();

    /**
     * @brief 描画前フレーム同期（DrawManagerへの描画パケット登録）
     */
    void SyncBeforeDraw() override;

    /**
     * @brief スプライトの直接描画処理
     */
    void Draw() override;

    /**
     * @brief デバッグ用UIの表示
     * @param[in] spriteName UIに表示する名前
     */
    void Debug(const char* spriteName = "");

    /** @name ゲッター */
    ///@{
    /**
     * @brief 2D描画用リソースを取得する
     * @return Object2DResource ポインタ
     */
    Object2DResource* GetD3D12Resource() {
        return this->resource_.get();
    }

    /**
     * @brief スプライトの表示サイズを取得する
     * @return 幅と高さ（ピクセル単位）
     */
    const Irufemi::Vector2& GetSize() const {
        return size_;
    }

    /**
     * @brief アンカーポイント（原点位置比率）を取得する
     * @return アンカー座標 (0.0 ~ 1.0)
     */
    const Irufemi::Vector2& GetAnchor() const {
        return anchor_;
    }

    /**
     * @brief 2Dスクリーン座標位置を取得する
     * @return XY座標
     */
    Irufemi::Vector2 GetPosition2D() const;

    /**
     * @brief 回転角を取得する
     * @return オイラー回転角ベクトル
     */
    const Irufemi::Vector3& GetRotation() const {
        return resource_ ? resource_->GetTransform().rotate : Irufemi::Vector3{};
    }

    /**
     * @brief スプライトのカラー値を取得する
     * @return RGBA カラー
     */
    const Irufemi::Vector4& GetColor() const {
        return resource_->GetMaterialData()->color;
    }

    /**
     * @brief 水平方向（X軸）の反転フラグを取得する
     * @return 反転している場合 true
     */
    bool IsFlipX() const {
        return isFlipX_;
    }

    /**
     * @brief 垂直方向（Y軸）の反転フラグを取得する
     * @return 反転している場合 true
     */
    bool IsFlipY() const {
        return isFlipY_;
    }

    /**
     * @brief 適用されているテクスチャのファイル名を取得する
     * @return テクスチャパス/名前
     */
    std::string GetTextureName() const;
    ///@}

    /** @name 設定用API */
    ///@{
    /**
     * @brief スプライトのサイズを設定
     */
    void SetSize(float width, float height);

    /**
     * @brief UIスケールを設定（解像度対応用）
     */
    void SetUIScale(float scale) {
        uiScale_ = scale;
        isDirty_ = true;
    }

    /**
     * @brief アンカーポイント（原点位置）を設定
     * @param[in] ax X座標 (0:左, 0.5:中央, 1:右)
     * @param[in] ay Y座標 (0:上, 0.5:中央, 1:下)
     */
    void SetAnchor(float ax, float ay) {
        anchor_ = {ax, ay};
        isDirty_ = true;
    }

    /**
     * @brief 位置を設定
     */
    void SetPosition(float x, float y, float z = 0.0f) {
        if (resource_) {
            resource_->GetTransform().translate = {x, y, z};
        }
        isDirty_ = true;
    }

    /**
     * @brief 回転を設定（Z軸回転）
     */
    void SetRotation(float rotate) {
        if (resource_) {
            resource_->GetTransform().rotate = Irufemi::Vector3{0.0f, 0.0f, rotate};
        }
        isDirty_ = true;
    }

    /**
     * @brief 色（RGBA）を設定
     */
    void SetColor(const Irufemi::Vector4& color) {
        resource_->GetMaterialData()->color = color;
        isDirty_ = true;
    }

    /**
     * @brief 反転状態を一括設定
     */
    void SetFlip(bool flipX, bool flipY) {
        isFlipX_ = flipX;
        isFlipY_ = flipY;
        isDirty_ = true;
    }
    /**
     * @brief FlipX を設定する。
     * @param[in] flip 設定する FlipX の値
     */
    void SetFlipX(bool flip) {
        isFlipX_ = flip;
        isDirty_ = true;
    }
    /**
     * @brief FlipY を設定する。
     * @param[in] flip 設定する FlipY の値
     */
    void SetFlipY(bool flip) {
        isFlipY_ = flip;
        isDirty_ = true;
    }

    /**
     * @brief テクスチャ内の切り出し範囲をピクセル単位で指定
     * @param[in] x 左上X
     * @param[in] y 左上Y
     * @param[in] w 幅
     * @param[in] h 高さ
     * @param[in] autoResize trueならスプライト自体のサイズも切り出しサイズに合わせる
     * @return 成功なら true
     */
    bool SetTextureRectPixels(int x, int y, int w, int h, bool autoResize = false);

    /**
     * @brief 切り出し指定を解除し、テクスチャ全体を表示するように戻す
     */
    void ClearTextureRect();

    /**
     * @brief テクスチャを動的に変更する
     */
    void SetTexture(const std::string& textureName);
    ///@}

    /** @name 便利エイリアス */
    ///@{
    void SetPositionTopLeft(const float& x, const float& y) {
        SetAnchor(0.0f, 0.0f);
        SetPosition(x, y);
    }
    /**
     * @brief PositionCenter を設定する。
     * @param[in] x 設定する PositionCenter の値
     * @param[in] y 設定する PositionCenter の値
     */
    void SetPositionCenter(const float& x, const float& y) {
        SetAnchor(0.5f, 0.5f);
        SetPosition(x, y);
    }
    ///@}

    /** @name 最前面描画設定 */
    ///@{
    /**
     * @brief ポストプロセスの影響を受けない最前面のUIとして描画するかを設定する
     * @param[in] isTopMost trueなら最前面(バックバッファ直接)に描画する
     */
    void SetTopMost(bool isTopMost) {
        isTopMost_ = isTopMost;
    }
    ///@}

    /** @name 静的メンバ設定（エンジン内部用） */
    ///@{
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
     * @brief CameraManager を設定する。
     * @param[in] camM 設定する CameraManager の値
     */
    static void SetCameraManager(CameraManager* camM) {
        cameraManager_ = camM;
    }
    ///@}

private:
    // 行列更新の最適化用
    bool isDirty_ = true;
    bool isTopMost_ = false; // 最前面UIフラグ
    float uiScale_ = 1.0f;   // UIスケール
    Irufemi::Matrix4x4 lastViewMatrix_ = {};
    Irufemi::Matrix4x4 lastProjectionMatrix_ = {};
};
