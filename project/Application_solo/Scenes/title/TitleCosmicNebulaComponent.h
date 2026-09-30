#pragma once

#include "Framework/Component/Component.h"
#include "Core/Math/Vector4.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

/**
 * @class TitleCosmicNebulaComponent
 * @brief タイトル画面用 神秘的な深宇宙星雲（Cosmic Nebula）背景レンダラーコンポーネント
 * @details 漆黒の深宇宙に広がるエメラルド〜シアン〜パープルの有機的星雲ガスと重力渦、
 *          瞬く星屑を最奥深度(Z=1.0)全画面パスで描画します。
 */
class TitleCosmicNebulaComponent : public Component {
public: // 定数バッファ構造体 (HLSL: register b6 / RootSlot::Special)
    struct alignas(256) CosmicNebulaParams {
        float pulseIntensity = 0.0f;                         //!< 出撃・決定パルス [0.0 - 1.0]
        float time = 0.0f;                                   //!< 経過時間
        float swirlStrength = 0.65f;                         //!< 渦の回転強度
        float density = 1.0f;                                //!< 星雲濃度
        Irufemi::Vector4 centerUV{0.5f, 0.42f, 0.0f, 0.0f};  //!< 重力渦の中心UV (xy: 中心, zw: パララックスオフセット)
        Irufemi::Vector4 mouseUV{0.5f, 0.5f, 0.0f, 0.0f};    //!< マウスカーソル (xy: 正規化UV, z: インタラクション強度, w: 予備)
        float pad[52]{};                                     //!< 256バイトアライメントパディング
    };

public: // メンバ関数
    TitleCosmicNebulaComponent() = default;
    ~TitleCosmicNebulaComponent() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;

    std::string GetComponentName() const override {
        return "TitleCosmicNebulaComponent";
    }

    void OnRegisterProperties() override;

    /**
     * @brief GAME START 決定時の重力光彩パルスを発火する
     * @param[in] power パルス強度
     */
    void TriggerPulse(float power = 1.0f);

private:
    void CreateConstantBuffer();

private: // メンバ変数
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    CosmicNebulaParams* mappedParams_ = nullptr;
    CosmicNebulaParams params_{};

    float totalTime_ = 0.0f;
    float pulseTimer_ = 0.0f;
    bool isPulseActive_ = false;

    // マウスカーソル追従・速度ベクトル場（Velocity-Aligned Wake）用
    Irufemi::Vector2 smoothedMousePos_{640.0f, 360.0f};  //!< スムーズ補間済みマウス位置
    Irufemi::Vector2 smoothedMouseUV_{0.5f, 0.5f};       //!< スムーズ補間済み正規化UV
    Irufemi::Vector2 prevRawMouseUV_{0.5f, 0.5f};        //!< 前フレームのマウスUV
    Irufemi::Vector2 smoothedVelocity_{0.0f, 0.0f};      //!< 平滑化されたマウス移動速度ベクトル
    Irufemi::Vector2 parallaxOffset_{0.0f, 0.0f};        //!< パララックスオフセット
};
