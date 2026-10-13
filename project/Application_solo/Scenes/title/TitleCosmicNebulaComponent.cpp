#include "Scenes/title/TitleCosmicNebulaComponent.h"

#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/RootSignatureConfig.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "Renderer/DrawManager.h"
#include "Renderer/Fluid/FluidSimulation2D.h"
#include "Platform/Input/InputManager.h"
#include <algorithm>
#include <cassert>

TitleCosmicNebulaComponent::~TitleCosmicNebulaComponent() {
    if (constantBuffer_ && mappedParams_) {
        constantBuffer_->Unmap(0, nullptr);
        mappedParams_ = nullptr;
    }
}

void TitleCosmicNebulaComponent::Initialize() {
    CreateConstantBuffer();

    // タイトル星雲用に流体減衰率を最適化＆前シーンの残存運動量を完全ゼロリセット
    if (auto engine = GetEngine()) {
        if (auto fluid = engine->GetFluidSimulation()) {
            fluid->Reset();
            fluid->SetDensityDissipation(0.92f);   // 濃度減衰率
            fluid->SetVelocityDissipation(0.925f); // 速度減衰率（約0.5秒でスッと自然に消える減衰）
        }
    }
}

void TitleCosmicNebulaComponent::OnRegisterProperties() {
    // インスペクタ用
}

void TitleCosmicNebulaComponent::CreateConstantBuffer() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    auto dx = engine->GetDirectXCommon();
    if (!dx) {
        return;
    }

    constantBuffer_ = dx->CreateBufferResource(sizeof(CosmicNebulaParams));
    if (constantBuffer_) {
        HRESULT hr = constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedParams_));
        assert(SUCCEEDED(hr) && "Failed to map CosmicNebula constant buffer.");
        if (SUCCEEDED(hr) && mappedParams_) {
            *mappedParams_ = params_;
        }
    }
}

void TitleCosmicNebulaComponent::Update() {
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    float deltaTime = engine->GetDeltaTime();
    if (deltaTime <= 0.0f) {
        deltaTime = 1.0f / 60.0f;
    }

    totalTime_ += deltaTime;

    // 出撃パルスタイマー進行（約1.1秒間持続し、自機加速と同期して美しく広がる）
    if (isPulseActive_) {
        pulseTimer_ += deltaTime * 0.9f;
        if (pulseTimer_ >= 1.0f) {
            pulseTimer_ = 0.0f;
            isPulseActive_ = false;
        }
    }

    // マウスカーソル追従 & 移動アクティビティ計算（ゲーム画面論理解像度 1280 x 720 基準）
    Irufemi::Vector2 targetMousePos = smoothedMousePos_;
    bool isLeftClicked = false;
    if (auto inputManager = engine->GetInputManager()) {
        // マウス、ゲームパッド、およびデモ自動巡回に対応する統合仮想カーソル座標を取得
        targetMousePos = inputManager->GetVirtualCursorPosition();

        if (inputManager->IsCursorActionPressed()) {
            isLeftClicked = true;
        }
        if (auto mouse = inputManager->GetMouse()) {
            if (mouse->IsButtonPressed(Mouse::Button::Left)) {
                isLeftClicked = true;
            }
        }
    }

    // ゲーム画面の解像度（SceneViewPanel の仮想マウス座標系と一致）
    constexpr float kGameWidth = 1280.0f;
    constexpr float kGameHeight = 720.0f;

    // 滑らかな追従補間 (Smooth Damp)
    float lerpFactor = std::clamp(deltaTime * 14.0f, 0.0f, 1.0f);
    smoothedMousePos_.x += (targetMousePos.x - smoothedMousePos_.x) * lerpFactor;
    smoothedMousePos_.y += (targetMousePos.y - smoothedMousePos_.y) * lerpFactor;

    // ゲーム画面に対する正確な正規化 UV (0.0 - 1.0)
    smoothedMouseUV_.x = std::clamp(smoothedMousePos_.x / kGameWidth, 0.0f, 1.0f);
    smoothedMouseUV_.y = std::clamp(smoothedMousePos_.y / kGameHeight, 0.0f, 1.0f);

    // --- 速度ベクトルの算出と平滑化（Velocity-Aligned Wake Field） ---
    float safeDeltaTime = (deltaTime > 0.0001f) ? deltaTime : (1.0f / 60.0f);
    Irufemi::Vector2 rawVelocity{(smoothedMouseUV_.x - prevRawMouseUV_.x) / safeDeltaTime,
                                 (smoothedMouseUV_.y - prevRawMouseUV_.y) / safeDeltaTime};
    prevRawMouseUV_ = smoothedMouseUV_;

    // 速度ベクトルの平滑化追従（過度な急激変化を緩和し、滑らかな流体の引き波を実現）
    float velLerp = std::clamp(deltaTime * 10.0f, 0.0f, 1.0f);
    smoothedVelocity_.x += (rawVelocity.x - smoothedVelocity_.x) * velLerp;
    smoothedVelocity_.y += (rawVelocity.y - smoothedVelocity_.y) * velLerp;

    // --- GPGPU 流体シミュレーションへの物理注入 ---
    if (auto fluid = engine->GetFluidSimulation()) {
        // 水面的な弾性復元（手で切った乱れが約0.4〜0.5秒でスッと元に戻る減衰率）
        fluid->SetVelocityDissipation(0.925f);

        float speed = std::hypot(smoothedVelocity_.x, smoothedVelocity_.y);
        if (speed > 0.010f) {
            // 指先・手先サイズに絞った外力ベクトル（速度場）の注入
            fluid->AddForce(smoothedMouseUV_, smoothedVelocity_ * 6.0f, 0.028f);
        }

        // 出撃シーケンス突入時の重力特異点渦の注入（※通常クリックでは発火しない）
        // ベルカーブ (sin) で滑らかに立ち上がり、パルス終了時に自然にゼロへ収束
        if (isPulseActive_) {
            float pulseWave = std::sin(std::clamp(pulseTimer_, 0.0f, 1.0f) * 3.14159265f);
            fluid->AddGravityVortex(Irufemi::Vector2{0.5f, 0.42f}, 22.0f * pulseWave, 26.0f * pulseWave, 0.42f);
        }
    }

    // 背景全体は中央にドシッと固定
    float vortexCenterX = 0.5f;
    float vortexCenterY = 0.42f;

    // --- 曲線トレイル履歴の更新（手で水面を切ったような曲線の引き波） ---
    // 既存ノードの生存時間を減衰（約0.6秒で滑らかにフェードアウト）
    for (auto& node : trailHistory_) {
        node.life = (std::max)(0.0f, node.life - deltaTime * 1.6f);
    }

    // 移動距離と速度の判定（マウスが動いたら新しい軌跡ノードを登録）
    float distFromLast = std::hypot(smoothedMouseUV_.x - lastSpawnUV_.x, smoothedMouseUV_.y - lastSpawnUV_.y);
    float currentSpeed = std::hypot(smoothedVelocity_.x, smoothedVelocity_.y);

    if (distFromLast > 0.012f && currentSpeed > 0.05f) {
        // 後ろへシフトして新規点を先頭に挿入
        for (size_t i = kMaxTrailPoints - 1; i > 0; --i) {
            trailHistory_[i] = trailHistory_[i - 1];
        }
        trailHistory_[0].uv = smoothedMouseUV_;
        trailHistory_[0].life = std::clamp(currentSpeed * 0.9f, 0.4f, 1.0f);
        lastSpawnUV_ = smoothedMouseUV_;
    } else {
        // 微小移動・静止時は先頭ノードの座標を現在位置に同期
        trailHistory_[0].uv = smoothedMouseUV_;
        if (currentSpeed < 0.02f) {
            trailHistory_[0].life = (std::max)(0.0f, trailHistory_[0].life - deltaTime * 3.0f);
        }
    }

    // 定数バッファのトレイル配列に反映
    for (size_t i = 0; i < kMaxTrailPoints; ++i) {
        params_.trailPoints[i] = {trailHistory_[i].uv.x, trailHistory_[i].uv.y, trailHistory_[i].life, 0.0f};
    }

    // パラメータ更新
    params_.pulseIntensity = isPulseActive_ ? pulseTimer_ : 0.0f;
    params_.time = totalTime_;
    params_.swirlStrength = 0.65f * swirlMultiplier_;
    params_.density = 1.0f;
    // centerUV: xy = 特異点中心, z = インプロージョン崩壊進行度 [0.0 - 1.0], w = 予備
    params_.centerUV = {vortexCenterX, vortexCenterY, collapseProgress_, 0.0f};
    // mouseUV: xy = カーソル正規化UV, zw = 平滑化移動速度ベクトル (Velocity)
    params_.mouseUV = {smoothedMouseUV_.x, smoothedMouseUV_.y, smoothedVelocity_.x, smoothedVelocity_.y};

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
    if (!engine) {
        return;
    }

    auto drawManager = engine->GetDrawManager();
    if (!drawManager || !constantBuffer_) {
        return;
    }

    // 最奥深度 (Z=1.0) による星雲背景パスを BeforeOpaque（3D不透明パス直前・Skybox直後）に提出
    drawManager->SubmitCustomPass(Irufemi::RenderStage::BeforeOpaque, [this, engine]() {
        auto dx = engine->GetDirectXCommon();
        if (!dx) {
            return;
        }

        auto cmdList = dx->GetCommandList();
        auto psoManager = engine->GetPSOManager();
        if (!cmdList || !psoManager) {
            return;
        }

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

            // GPGPU 流体シミュレーションテクスチャをバインド
            // register(t5) / RootSlot::ShadowMap: 流速場 (Velocity: RG16F)
            // register(t6) / RootSlot::DepthMap: 濃度場 (Density: RGBA16F)
            if (auto fluid = engine->GetFluidSimulation()) {
                cmdList->SetGraphicsRootDescriptorTable(static_cast<UINT>(RootSlot::ShadowMap),
                                                        fluid->GetVelocitySRV());
                cmdList->SetGraphicsRootDescriptorTable(static_cast<UINT>(RootSlot::DepthMap),
                                                        fluid->GetDensitySRV());
            }

            // SV_VertexID による全画面最奥三角形描画 (3頂点)
            cmdList->DrawInstanced(3, 1, 0, 0);
        }
    });
}
