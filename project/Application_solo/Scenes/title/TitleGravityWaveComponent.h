#pragma once

#include "Framework/Component/Component.h"
#include "Core/Math/Vector2.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <string>

/**
 * @class TitleGravityWaveComponent
 * @brief タイトル画面用 プロシージャル重力波＆高電圧プラズマ放電レンダラーコンポーネント
 * @details マウスやゲームパッドの操作速度に応じて時空の歪み・ちぎれ放電アークをリアルタイムに計算し、
 *          RootSlot::Special (register b6) 定数バッファを更新して全画面描画パスを実行します。
 */
class TitleGravityWaveComponent : public Component {
public: // 定数バッファ構造体 (HLSL: register b6 / RootSlot::Special)
    struct alignas(256) GravityWaveParams {
        Irufemi::Vector2 mousePos{0.5f, 0.5f};      //!< マウス/カーソルUV座標 [0.0 - 1.0] (8 bytes)
        Irufemi::Vector2 mouseVelocity{0.0f, 0.0f}; //!< カーソル移動速度ベクトル (8 bytes) -> offset 16
        float speedThreshold = 0.75f;               //!< 破断（ちぎれ）発生速度しきい値 (4 bytes)
        float clickImpulse = 0.0f;                  //!< クリック衝撃波タイマー [0.0 - 1.0] (4 bytes)
        float gridScale = 28.0f;                    //!< グリッド密度 (4 bytes)
        float warpStrength = 0.10f;                 //!< 空間の歪み強度 (4 bytes) -> offset 32
        float tearIntensity = 0.0f;                 //!< 破断持続減衰タイマー [0.0 - 1.0] (4 bytes)
        float pad[1] = {0.0f};                      //!< パディング (4 bytes) -> offset 40
        float pad2[2] = {0.0f, 0.0f};               //!< 16バイトアライメントパディング (8 bytes) -> 計48 bytes
    };

public: // メンバ関数
    TitleGravityWaveComponent() = default;
    ~TitleGravityWaveComponent() override;

    void Initialize() override;
    void Update() override;
    void Draw() override;

    std::string GetComponentName() const override {
        return "TitleGravityWaveComponent";
    }

    void OnRegisterProperties() override;

    /**
     * @brief 外部から重力波パルス（衝撃波インパルス）を発火する
     * @param[in] power 衝撃波の初期強度 [0.0 - 1.0]
     */
    void TriggerImpulse(float power = 1.0f);

    /**
     * @brief カーソルUV座標を取得する
     */
    const Irufemi::Vector2& GetCursorUV() const {
        return currentUV_;
    }

private:
    void UpdateInput(float deltaTime);
    void CreateConstantBuffer();

private: // メンバ変数
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    GravityWaveParams* mappedParams_ = nullptr;
    GravityWaveParams params_{};

    Irufemi::Vector2 currentUV_{0.5f, 0.5f};
    Irufemi::Vector2 prevUV_{0.5f, 0.5f};
    Irufemi::Vector2 velocity_{0.0f, 0.0f};

    float impulseTimer_ = 0.0f;
    bool isImpulseActive_ = false;

    float tearDecayTimer_ = 0.0f;

    const float kGamepadSensitivity_ = 1.0f;
};
