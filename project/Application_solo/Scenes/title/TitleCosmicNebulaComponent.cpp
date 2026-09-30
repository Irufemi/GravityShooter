#include "Scenes/title/TitleCosmicNebulaComponent.h"

#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/RootSignatureConfig.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "Renderer/DrawManager.h"
#include "Platform/Input/InputManager.h"
#include <algorithm>

TitleCosmicNebulaComponent::~TitleCosmicNebulaComponent() {
    if (constantBuffer_ && mappedParams_) {
        constantBuffer_->Unmap(0, nullptr);
        mappedParams_ = nullptr;
    }
}

void TitleCosmicNebulaComponent::Initialize() {
    CreateConstantBuffer();
}

void TitleCosmicNebulaComponent::OnRegisterProperties() {
    // インスペクタ用
}

void TitleCosmicNebulaComponent::CreateConstantBuffer() {
    auto engine = GetEngine();
    if (!engine) return;

    auto dx = engine->GetDirectXCommon();
    if (!dx) return;

    constantBuffer_ = dx->CreateBufferResource(sizeof(CosmicNebulaParams));
    if (constantBuffer_) {
        constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedParams_));
        if (mappedParams_) {
            *mappedParams_ = params_;
        }
    }
}

void TitleCosmicNebulaComponent::Update() {
    auto engine = GetEngine();
    if (!engine) return;

    float deltaTime = engine->GetDeltaTime();
    if (deltaTime <= 0.0f) {
        deltaTime = 1.0f / 60.0f;
    }

    totalTime_ += deltaTime;

    // 出撃パルスタイマー進行
    if (isPulseActive_) {
        pulseTimer_ += deltaTime * 1.5f;
        if (pulseTimer_ >= 1.0f) {
            pulseTimer_ = 0.0f;
            isPulseActive_ = false;
        }
    }

    // マウスカーソル追従（ゲーム画面論理解像度 1280 x 720 基準）
    Irufemi::Vector2 targetMousePos = smoothedMousePos_;
    if (auto inputManager = engine->GetInputManager()) {
        if (auto mouse = inputManager->GetMouse()) {
            targetMousePos = mouse->GetPosition();
        }
    }

    // ゲーム画面の解像度（SceneViewPanel の仮想マウス座標系と一致）
    constexpr float kGameWidth = 1280.0f;
    constexpr float kGameHeight = 720.0f;

    // 滑らかな追従補間 (Smooth Damp)
    float lerpFactor = std::clamp(deltaTime * 10.0f, 0.0f, 1.0f);
    smoothedMousePos_.x += (targetMousePos.x - smoothedMousePos_.x) * lerpFactor;
    smoothedMousePos_.y += (targetMousePos.y - smoothedMousePos_.y) * lerpFactor;

    // ゲーム画面に対する正確な正規化 UV (0.0 - 1.0)
    smoothedMouseUV_.x = std::clamp(smoothedMousePos_.x / kGameWidth, 0.0f, 1.0f);
    smoothedMouseUV_.y = std::clamp(smoothedMousePos_.y / kGameHeight, 0.0f, 1.0f);

    // 背景全体は中央にドシッと固定（マウス移動で画面全体が揺れるのを完全に防止）
    float vortexCenterX = 0.5f;
    float vortexCenterY = 0.42f;
    float parallaxX = 0.0f;
    float parallaxY = 0.0f;

    // パラメータ更新
    params_.pulseIntensity = isPulseActive_ ? pulseTimer_ : 0.0f;
    params_.time = totalTime_;
    params_.swirlStrength = 0.65f;
    params_.density = 1.0f;
    params_.centerUV = { vortexCenterX, vortexCenterY, parallaxX, parallaxY };
    params_.mouseUV = { smoothedMouseUV_.x, smoothedMouseUV_.y, 1.0f, 0.0f };

    if (mappedParams_) {
        *mappedParams_ = params_;
    }
}

void TitleCosmicNebulaComponent::TriggerPulse(float power) {
    (void)power;
    isPulseActive_ = true;
    pulseTimer_ = 0.001f;
}

void TitleCosmicNebulaComponent::Draw() {
    auto engine = GetEngine();
    if (!engine) return;

    auto drawManager = engine->GetDrawManager();
    if (!drawManager || !constantBuffer_) return;

    // 最奥深度 (Z=1.0) による星雲背景パスをポストレンダーキューに提出
    drawManager->SubmitPostRender([this, engine]() {
        auto dx = engine->GetDirectXCommon();
        if (!dx) return;

        auto cmdList = dx->GetCommandList();
        auto psoManager = engine->GetPSOManager();
        if (!cmdList || !psoManager) return;

        // CosmicNebula PSO (CosmicNebula.VS.hlsl + CosmicNebula.PS.hlsl)
        // DepthWrite::Disable (深度テスト有効・書き込み無効) により、自機・ガレキの奥にのみ星雲が描画される
        auto pso = psoManager->GetPSO("CosmicNebula", Irufemi::BlendMode::kBlendModeNormal,
                                      PSOManager::DepthWrite::Disable, PSOManager::CullMode::None);
        if (pso) {
            cmdList->SetPipelineState(pso);
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // register(b6) / RootSlot::Special に定数バッファをバインド
            cmdList->SetGraphicsRootConstantBufferView(static_cast<UINT>(RootSlot::Special),
                                                      constantBuffer_->GetGPUVirtualAddress());

            // SV_VertexID による全画面最奥三角形描画 (3頂点)
            cmdList->DrawInstanced(3, 1, 0, 0);
        }
    });
}
