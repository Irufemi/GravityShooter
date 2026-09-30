#include "Scenes/title/TitleGravityWaveComponent.h"

#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/RootSignatureConfig.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "Renderer/DrawManager.h"
#include "Platform/Input/InputManager.h"
#include "Platform/Input/GamePad.h"
#include <algorithm>
#include <cmath>

TitleGravityWaveComponent::~TitleGravityWaveComponent() {
    if (constantBuffer_ && mappedParams_) {
        constantBuffer_->Unmap(0, nullptr);
        mappedParams_ = nullptr;
    }
}

void TitleGravityWaveComponent::Initialize() {
    CreateConstantBuffer();
}

void TitleGravityWaveComponent::OnRegisterProperties() {
    // インスペクタ調整用（必要に応じて公開）
}

void TitleGravityWaveComponent::CreateConstantBuffer() {
    auto engine = GetEngine();
    if (!engine) return;

    auto dx = engine->GetDirectXCommon();
    if (!dx) return;

    // 256バイトアライメントされた定数バッファを生成
    constantBuffer_ = dx->CreateBufferResource(sizeof(GravityWaveParams));
    if (constantBuffer_) {
        constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedParams_));
        if (mappedParams_) {
            *mappedParams_ = params_;
        }
    }
}

void TitleGravityWaveComponent::Update() {
    auto engine = GetEngine();
    if (!engine) return;

    float deltaTime = engine->GetDeltaTime();
    if (deltaTime <= 0.0f) {
        deltaTime = 1.0f / 60.0f;
    }

    // 1. 入力からカーソル座標と速度を更新
    UpdateInput(deltaTime);

    // 2. 衝撃波インパルス（波紋進行）タイマー
    if (isImpulseActive_) {
        impulseTimer_ += deltaTime * 1.5f; // 約0.66秒で広がりきる
        if (impulseTimer_ >= 1.0f) {
            impulseTimer_ = 0.0f;
            isImpulseActive_ = false;
        }
    }

    // 3. 放電アーク持続減衰タイマー
    float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y);
    if (speed > params_.speedThreshold) {
        tearDecayTimer_ = 1.0f; // しきい値を超えたら瞬時にフル点火
    } else if (tearDecayTimer_ > 0.0f) {
        tearDecayTimer_ = (std::max)(0.0f, tearDecayTimer_ - deltaTime * 2.8f);
    }

    // 4. 定数バッファパラメータの同期
    params_.mousePos = currentUV_;
    params_.mouseVelocity = velocity_;
    params_.speedThreshold = 0.75f;
    params_.clickImpulse = isImpulseActive_ ? impulseTimer_ : 0.0f;
    params_.gridScale = 28.0f;
    params_.warpStrength = 0.10f;
    params_.tearIntensity = tearDecayTimer_;

    if (mappedParams_) {
        *mappedParams_ = params_;
    }
}

void TitleGravityWaveComponent::UpdateInput(float deltaTime) {
    auto engine = GetEngine();
    if (!engine) return;

    auto inputManager = engine->GetInputManager();
    if (!inputManager) return;

    bool hasMouseMoved = false;

    // マウス入力のトラッキング
    if (auto mouse = inputManager->GetMouse()) {
        const auto& mousePos = mouse->GetPosition();
        float screenW = static_cast<float>(engine->GetGameResolutionWidth());
        float screenH = static_cast<float>(engine->GetGameResolutionHeight());
        if (screenW > 0.0f && screenH > 0.0f) {
            Irufemi::Vector2 newUV = {
                std::clamp(mousePos.x / screenW, 0.0f, 1.0f),
                std::clamp(mousePos.y / screenH, 0.0f, 1.0f)
            };

            float moveDist = std::abs(newUV.x - prevUV_.x) + std::abs(newUV.y - prevUV_.y);
            if (moveDist > 0.0001f) {
                currentUV_ = newUV;
                hasMouseMoved = true;
            }
        }
    }

    // ゲームパッド入力（右スティックまたは左スティック）による仮想カーソル移動
    if (!hasMouseMoved) {
        if (auto gamepad = inputManager->GetGamePad()) {
            float stickX = gamepad->GetRightStickX();
            float stickY = gamepad->GetRightStickY();

            // 右スティックが無操作なら左スティックも監視
            if (std::abs(stickX) < 0.15f && std::abs(stickY) < 0.15f) {
                stickX = gamepad->GetLeftStickX();
                stickY = gamepad->GetLeftStickY();
            }

            if (std::abs(stickX) >= 0.15f || std::abs(stickY) >= 0.15f) {
                currentUV_.x = std::clamp(currentUV_.x + stickX * kGamepadSensitivity_ * deltaTime, 0.0f, 1.0f);
                // Y軸は上がプラスなのでUV空間（下がプラス）に合わせて反転
                currentUV_.y = std::clamp(currentUV_.y - stickY * kGamepadSensitivity_ * deltaTime, 0.0f, 1.0f);
            }
        }
    }

    // 速度ベクトルの計算と指数平滑化 (EMA)
    Irufemi::Vector2 rawVelocity = {
        (currentUV_.x - prevUV_.x) / deltaTime,
        (currentUV_.y - prevUV_.y) / deltaTime
    };

    const float kAlpha = 0.35f;
    velocity_.x = velocity_.x * (1.0f - kAlpha) + rawVelocity.x * kAlpha;
    velocity_.y = velocity_.y * (1.0f - kAlpha) + rawVelocity.y * kAlpha;

    prevUV_ = currentUV_;
}

void TitleGravityWaveComponent::TriggerImpulse(float power) {
    (void)power;
    isImpulseActive_ = true;
    impulseTimer_ = 0.001f;
}

void TitleGravityWaveComponent::Draw() {
    auto engine = GetEngine();
    if (!engine) return;

    auto drawManager = engine->GetDrawManager();
    if (!drawManager || !constantBuffer_) return;

    // 全画面重力波＆プラズマ放電パスをポストレンダーキューに提出
    drawManager->SubmitPostRender([this, engine]() {
        auto dx = engine->GetDirectXCommon();
        if (!dx) return;

        auto cmdList = dx->GetCommandList();
        auto psoManager = engine->GetPSOManager();
        if (!cmdList || !psoManager) return;

        // GravitationalWave PSO (Fullscreen.VS.hlsl + GravitationalWave.PS.hlsl)
        auto pso = psoManager->GetPSO("GravitationalWave", Irufemi::BlendMode::kBlendModeNormal,
                                      PSOManager::DepthWrite::Off, PSOManager::CullMode::None);
        if (pso) {
            cmdList->SetPipelineState(pso);
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // register(b6) / RootSlot::Special に定数バッファをバインド
            cmdList->SetGraphicsRootConstantBufferView(static_cast<UINT>(RootSlot::Special),
                                                      constantBuffer_->GetGPUVirtualAddress());

            // SV_VertexID による全画面1枚三角形描画 (3頂点)
            cmdList->DrawInstanced(3, 1, 0, 0);
        }
    });
}
