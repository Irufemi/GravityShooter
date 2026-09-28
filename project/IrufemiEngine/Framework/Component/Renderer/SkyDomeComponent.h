#pragma once

#include "Framework/Component/Component.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Vector2.h"
#include "Renderer/Data/Material.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

/**
 * @class SkyDomeComponent
 * @brief 3Dメッシュ不要で全画面プロシージャル天球を描画するコンポーネント
 * @details 画面の視線レイと球面マッピングにより、最奥深度（無限遠）に空テクスチャを展開します。
 */
class SkyDomeComponent : public Component {
public:
    SkyDomeComponent() = default;
    ~SkyDomeComponent() override;

    /**
     * @brief 初期化処理を行います
     */
    void Initialize() override;

    /**
     * @brief 生成時の自己完結初期化（定数バッファ生成・テクスチャ読込）を行います
     */
    void OnAwake() override;

    /**
     * @brief 毎フレームの更新処理（マテリアル定数バッファの同期）を行います
     */
    void Update() override;

    /**
     * @brief 描画コマンドの送信（DrawManager::SubmitSkydome）を行います
     */
    void Draw() override;

    /**
     * @brief コンポーネント名を取得する
     * @return コンポーネント名文字列
     */
    std::string GetComponentName() const override {
        return "SkyDomeComponent";
    }

    /**
     * @brief シリアライズ処理
     */
    nlohmann::json Serialize() override;

    /**
     * @brief デシリアライズ処理
     */
    void Deserialize(const nlohmann::json& j) override;

    // --- ゲッター・セッター ---
    const std::string& GetTexturePath() const {
        return texturePath_;
    }
    void SetTexturePath(const std::string& path);

    const Irufemi::Vector4& GetColor() const {
        return color_;
    }
    void SetColor(const Irufemi::Vector4& color) {
        color_ = color;
    }

    float GetIntensity() const {
        return intensity_;
    }
    void SetIntensity(float intensity) {
        intensity_ = intensity;
    }

    const Irufemi::Vector2& GetUvOffset() const {
        return uvOffset_;
    }
    void SetUvOffset(const Irufemi::Vector2& offset) {
        uvOffset_ = offset;
    }

    const Irufemi::Vector2& GetUvTiling() const {
        return uvTiling_;
    }
    void SetUvTiling(const Irufemi::Vector2& tiling) {
        uvTiling_ = tiling;
    }

protected:
    /**
     * @brief インスペクター編集用プロパティを登録する
     */
    void OnRegisterProperties() override;

private:
    void UpdateMaterialBuffer();
    void ReloadTexture();

private:
    std::string texturePath_ = "resources/texture/sky/skydome.png"; ///< 空テクスチャパス
    Irufemi::Vector4 color_ = {1.0f, 1.0f, 1.0f, 1.0f};             ///< 空のカラー乗数
    float intensity_ = 1.0f;                                         ///< 輝度乗数
    Irufemi::Vector2 uvOffset_ = {0.0f, 0.0f};                       ///< UVオフセット
    Irufemi::Vector2 uvTiling_ = {1.0f, 1.0f};                       ///< UVタイリング

    uint32_t textureIndex_ = 0;                                      ///< [Bindless] テクスチャインデックス
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_ = nullptr; ///< マテリアル定数バッファ
    Material* mappedMaterialData_ = nullptr;                         ///< マップポインタ
};
