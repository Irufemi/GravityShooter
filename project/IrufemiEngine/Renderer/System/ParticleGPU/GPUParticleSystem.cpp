#include "Core/Utility/ErrorUtility.h"
#include "Core/Utility/Log.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraphBuilder.h"
#include "Renderer/System/ParticleGPU/GPUParticleSystem.h"
#include <iostream>
#include <atomic>
#include <Windows.h>
#include <cstdio>
#include "Renderer/Camera/CameraManager.h"
#include "Core/Shape/Sphere.h"
#include "Renderer/Data/VertexData.h"
#include "Physics/Collision/Collision.h"
#include "Core/Math/Geometry/Frustum.h"
#include "Core/Math/Math.h"
#include "Renderer/Camera/Camera.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DirectXUtils.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/UI/DebugUI.h"
#include "Renderer/DrawManager.h"
#include "Renderer/Object/PrimitiveManager.h"
#include "Renderer/Object/Line/LineClass.h"
#include "Renderer/Object/Batch/DebugPrimitiveRenderer.h"
#include "Resource/Texture/TextureManager.h"
#include <algorithm>
#include <cassert>
#include <vector>

// 静的メンバ変数の実体定義
DirectXCommon* GPUParticleSystem::dxCommon_ = nullptr;
DrawManager* GPUParticleSystem::drawManager_ = nullptr;
TextureManager* GPUParticleSystem::textureManager_ = nullptr;
IrufemiEngine* GPUParticleSystem::engine_ = nullptr;

// コンストラクタ
GPUParticleSystem::GPUParticleSystem() {
    emittersData_.emplace_back();
}

GPUParticleSystem::~GPUParticleSystem() {
    if (dxCommon_) {
        uint64_t fv = dxCommon_->GetCurrentFrameFenceValue();
        if (auto* srvPool = dxCommon_->GetSrvPool()) {
            for (int i = 0; i < 3; ++i) {
                srvPool->FreeAfterFence(emittersSrvIndex_[i], fv);
                srvPool->FreeAfterFence(fieldsSrvIndex_[i], fv);
            }
            srvPool->FreeAfterFence(perFrameSrvIndex_, fv);
            srvPool->FreeAfterFence(particleUavIndex_, fv);
            srvPool->FreeAfterFence(particleSrvIndex_, fv);
            srvPool->FreeAfterFence(freeListIndexUavIndex_, fv);
            srvPool->FreeAfterFence(freeListUavIndex_, fv);
            srvPool->FreeAfterFence(sortIndex_, fv);
            srvPool->FreeAfterFence(sortSrvIndex_, fv);
        }
        for (int i = 0; i < 3; ++i) {
            dxCommon_->ReleaseAfterFence(emittersResource_[i]);
            dxCommon_->ReleaseAfterFence(fieldsResource_[i]);
        }
        dxCommon_->ReleaseAfterFence(particleResource_);
        dxCommon_->ReleaseAfterFence(freeListIndexResource_);
        dxCommon_->ReleaseAfterFence(freeListResource_);
        dxCommon_->ReleaseAfterFence(sortResource_);

        if (textureManager_ && textureHandle_.IsValid()) {
            textureManager_->ReleaseTexture(textureHandle_);
        }
    }
}

// 初期化
void GPUParticleSystem::Initialize(const std::string& textureName) {

    IRUFEMI_ASSERT(dxCommon_);
    IRUFEMI_ASSERT(drawManager_);
    IRUFEMI_ASSERT(textureManager_);
    IRUFEMI_ASSERT(engine_);

    CreateBuffersAndViews();

    // 各GPUParticleSystemインスタンスごとに異なるシードを持たせて、乱数系列が完全に被るのを防ぐ
    static std::atomic<uint32_t> s_uniqueSeed{0};
    emittersData_[0].randomSeed = ++s_uniqueSeed;

    // 形状の初期設定 (デフォルトは Quad/Irufemi::Plane)
    SetPrimitive(Irufemi::PrimitiveType::Plane);

    if (textureManager_) {
        auto textureNames = textureManager_->GetTextureNamesForDebug();
        auto it = std::find(textureNames.begin(), textureNames.end(), textureName);
        if (it != textureNames.end()) {
            selectedTextureIndex_ = static_cast<int>(std::distance(textureNames.begin(), it));
        }
        if (textureHandle_.IsValid()) {
            textureManager_->ReleaseTexture(textureHandle_);
        }
        textureHandle_ = textureManager_->LoadTexture(textureName);
    }

    // デフォルトでスフィアエミッターを設定
    SetSphereEmitter(Irufemi::Vector3(0, 0, 0), 2.0f, 30.0f);

    // Milestone 1: 初期調整 (レガシー演出の復元)
    auto& em = emittersData_[0];
    em.minLife = 0.4f;
    em.maxLife = 0.8f;
    em.startScaleMinX = 0.2f;
    em.startScaleMinY = 0.2f;
    em.startScaleMinZ = 0.2f;
    em.startScaleMaxX = 0.5f;
    em.startScaleMaxY = 0.5f;
    em.startScaleMaxZ = 0.5f;
    em.endScaleMinX = 0.01f;
    em.endScaleMinY = 0.01f;
    em.endScaleMinZ = 0.01f;
    em.endScaleMaxX = 0.1f;
    em.endScaleMaxY = 0.1f;
    em.endScaleMaxZ = 0.1f;
    em.startColorMinR = 1.0f;
    em.startColorMinG = 1.0f;
    em.startColorMinB = 0.3f;
    em.startColorMinA = 1.0f;
    em.startColorMaxR = 1.0f;
    em.startColorMaxG = 1.0f;
    em.startColorMaxB = 0.4f;
    em.startColorMaxA = 1.0f;
    em.endColorMinR = 1.0f;
    em.endColorMinG = 0.1f;
    em.endColorMinB = 0.0f;
    em.endColorMinA = 0.0f;
    em.endColorMaxR = 1.0f;
    em.endColorMaxG = 0.5f;
    em.endColorMaxB = 0.1f;
    em.endColorMaxA = 0.0f;
    em.colorMode = 0;
    em.gravity = 0.0f;
    em.damping = 0.0f;
    em.jitter = 0.01f;    // 座標のゆらぎ
    em.billboardMode = 1; // デフォルトはカメラビルボード
    em.burstCount = 0;

    // Milestone 3: 初期設定
    em.atlasRows = 1;
    em.atlasCols = 1;
    em.groundHeight = -100.0f;
    em.bounce = 0.5f;
    em.attractorStrength = 0.0f;
    em.attractorPosX = 0.0f;
    em.attractorPosY = 0.0f;
    em.attractorPosZ = 0.0f;

    SetEmit(true);

    perFrameData_->deltaTime = engine_->GetDeltaTime();

#if defined(USE_IMGUI)
    debugLineRegion_ = std::make_unique<Line3DBatch>();
    debugLineRegion_->Initialize();
#endif

    // ゲーム開始時（ローディング中）にCSを使ったバッファ初期化を済ませる
    if (dxCommon_) {
        // 同期待ちをさせるため、初回フレームでの遅延をなくす
        dxCommon_->ExecuteUploadCommands([this](ID3D12GraphicsCommandList* cmdList) {
            ID3D12DescriptorHeap* descriptorHeaps[] = {dxCommon_->GetSrvDescriptorHeap()};
            cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

            cmdList->SetComputeRootSignature(dxCommon_->GetComputeRootSignature());

            // 1. 初期化シェーダーの実行 (VRAM上のデータ構造を初期化)
            cmdList->SetPipelineState(dxCommon_->GetPSOManager()->GetComputePSO("GpuParticleInitialize"));
            cmdList->SetComputeRootDescriptorTable(3, particleUavHandleGPU_);
            cmdList->SetComputeRootDescriptorTable(6, freeListIndexUavHandleGPU_);
            cmdList->SetComputeRootDescriptorTable(7, freeListUavHandleGPU_);

            cmdList->Dispatch((kMaxParticles + 1023) / 1024, 1, 1);

            /**
             * @brief 初期化終了後の UAV バリア
             * @details [設計ルール] グローバルバリア (pResource=nullptr)
             * はGPU並列効率を低下させるため禁止。
             * 読み書きを行う3つのリソースを明示してバリアを張る。
             */
            DirectXUtils::UAVBarriers(cmdList,
                                      {particleResource_.Get(), freeListIndexResource_.Get(), freeListResource_.Get()});

            // 2. Emit / Update シェーダーを空バインドして JIT 誘発
            // 実行はしない（Descriptor等も最低限のまま）
            cmdList->SetPipelineState(dxCommon_->GetPSOManager()->GetComputePSO("GpuParticleEmit"));
            cmdList->SetPipelineState(dxCommon_->GetPSOManager()->GetComputePSO("GpuParticleUpdate"));
        });
        isInitializedCS_ = true;
    } else {
        isInitializedCS_ = false;
    }
}

// 更新
void GPUParticleSystem::Update() {
    if (emittersData_.empty() || !engine_) {
        return;
    }
    Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera();
    if (!activeCam) {
        return;
    }

    bool anyEmitting = false;
    for (const auto& em : emittersData_) {
        if (em.emit) {
            anyEmitting = true;
        }
    }

    // タイムスケールに応じた時間取得
    float dt = isUnscaledTime_ ? engine_->GetRealDeltaTime() : engine_->GetGameDeltaTime();
    float currentTime = isUnscaledTime_ ? engine_->GetTotalTime() : engine_->GetGameTime();

    // 持続時間制御
    if (anyEmitting && duration_ > 0.0f) {
        totalTime_ += dt;
        if (totalTime_ >= duration_) {
            if (isLooping_) {
                totalTime_ = 0.0f;
                // ※ループ時は必要なら一瞬だけ全クリアする等の処理を検討
            } else {
                for (auto& em : emittersData_) {
                    em.emit = 0;
                }
                anyEmitting = false;
            }
        }
    }

    isCulled_ = false;
    if (isCullingEnabled_) {
        bool anyVisible = false;
        for (const auto& em : emittersData_) {
            // 基本形状の半径/半対角長
            float baseRadius = em.radius;
            if (em.type == 4) { // Box
                baseRadius =
                    std::sqrt(em.areaSizeX * em.areaSizeX + em.areaSizeY * em.areaSizeY + em.areaSizeZ * em.areaSizeZ) *
                    0.5f;
            }

            // 粒子の最大到達距離 (初速 + 拡散度) * 寿命
            float maxSpeed = em.velocity * (1.0f + (std::max)(0.0f, em.spread));
            float maxTravelDistance = maxSpeed * em.maxLife;
            float maxGravityDrop = 0.5f * std::abs(em.gravity) * em.maxLife * em.maxLife;
            float maxScale =
                (std::max)({em.startScaleMaxX, em.startScaleMaxY, em.startScaleMaxZ, em.endScaleMaxX, em.endScaleMaxY,
                            em.endScaleMaxZ, em.midScaleMaxX, em.midScaleMaxY, em.midScaleMaxZ});

            Irufemi::Sphere boundingSphere;
            if (em.type == 1) { // Beam: 進行方向に中心をオフセットして無駄な肥大化を防ぐ
                Irufemi::Vector3 rawDir = {em.directionX, em.directionY, em.directionZ};
                float lenSq = rawDir.x * rawDir.x + rawDir.y * rawDir.y + rawDir.z * rawDir.z;
                Irufemi::Vector3 dir =
                    (lenSq > 0.0001f) ? Irufemi::Math::Normalize(rawDir) : Irufemi::Vector3{0.0f, 0.0f, 1.0f};
                boundingSphere.center = {em.translateX + dir.x * (maxTravelDistance * 0.5f),
                                         em.translateY + dir.y * (maxTravelDistance * 0.5f),
                                         em.translateZ + dir.z * (maxTravelDistance * 0.5f)};
                boundingSphere.radius =
                    (baseRadius + (maxTravelDistance * 0.5f) + maxGravityDrop + maxScale + em.jitter) * 1.1f;
            } else {
                boundingSphere.center = {em.translateX, em.translateY, em.translateZ};
                boundingSphere.radius = (baseRadius + maxTravelDistance + maxGravityDrop + maxScale + em.jitter) * 1.1f;
            }

            if (Irufemi::Collision::IsCollision(activeCam->GetFrustum(), boundingSphere)) {
                anyVisible = true;
                break;
            }
        }
        if (!anyVisible) {
            isCulled_ = true;
            // 画面外でも計算（CS）は継続させるため、returnによる打ち切りは行わない
        }
    }

    /*Particleを発生させる*/

    if (anyEmitting) {
        timeSinceStop_ = 0.0f; // 停止タイマーをリセット
    } else {
        timeSinceStop_ += dt; // 停止してからの時間を計測
    }

    perFrameData_->time = currentTime;
    perFrameData_->deltaTime = dt;

    uint32_t totalBurstCount = 0;
    float maxLifeOverall = 0.0f;
    for (auto& em : emittersData_) {
        if (em.emit) {
            if (em.emissionRate > 0.0f) {
                em.emissionResidue += em.emissionRate * dt;
                uint32_t spawnCount = static_cast<uint32_t>(em.emissionResidue);
                em.burstCount += spawnCount;
                em.emissionResidue -= spawnCount;
            }
        } else {
            em.emissionResidue = 0.0f; // Emit停止中なら端数リセット
        }
        totalBurstCount += em.burstCount;
        maxLifeOverall = (std::max)(maxLifeOverall, em.maxLife);
    }

    UpdateDebugLines();
    if (debugLineRegion_) {
        debugLineRegion_->Update();
    }

    // パーティクルが生存している可能性がある、または単発放出（バースト）が要求された場合のみCSの更新フラグを立てる
    if (anyEmitting || timeSinceStop_ <= maxLifeOverall + 0.1f || totalBurstCount > 0 || !isInitializedCS_) {
        needsUpdateCS_ = true;
        // バーストが要求された場合は、休眠から復帰するため停止タイマーをリセット
        if (totalBurstCount > 0) {
            timeSinceStop_ = 0.0f;
        }
    } else {
        needsUpdateCS_ = false; // 完全に休眠
    }

    // エンジンにCompute Shaderの実行を予約する
    if (engine_ && engine_->GetDrawManager() && needsUpdateCS_) {
        engine_->GetDrawManager()->RegisterComputeTask(this);
    }
}

void GPUParticleSystem::SyncBeforeDraw() {
    uint32_t frameIndex = dxCommon_->GetFrameIndex();

    // PerViewはUpdateが呼ばれなくても毎フレーム必ず最新化する（ポーズ中のカメラ移動・マルチバッファ対策）
    if (engine_) {
        if (Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera()) {
            perViewBuffer_[frameIndex]->viewProjection = activeCam->GetViewProjectionMatrix3D();
            Irufemi::Matrix4x4 backToFrontMatrix_ = Irufemi::Math::MakeRotateYMatrix(0.0f);
            Irufemi::Matrix4x4 billboardMatrix_ =
                Irufemi::Math::Multiply(backToFrontMatrix_, activeCam->GetCameraMatrix());
            billboardMatrix_.m[3][0] = 0.0f;
            billboardMatrix_.m[3][1] = 0.0f;
            billboardMatrix_.m[3][2] = 0.0f;
            perViewBuffer_[frameIndex]->billboardMatrix = billboardMatrix_;
            perViewBuffer_[frameIndex]->worldPosition = activeCam->GetTranslate();
            perViewBuffer_[frameIndex]->cameraNear = activeCam->GetNearZ();
            perViewBuffer_[frameIndex]->cameraFar = activeCam->GetFarZ();
        }
    }

    // 同一フレーム内で複数回呼び出された場合は無駄な転送を防ぐ
    // 特に、DispatchCompute後のburstCount=0の再転送（バグ）を防ぐ効果がある
    if (lastUpdateFrame_ == frameIndex) {
        return;
    }

    if (emittersData_.empty()) {
        emittersData_.emplace_back();
    }

    memcpy(emittersMappedData_[frameIndex], emittersData_.data(), sizeof(GPUParticleEmitter) * emittersData_.size());

    // 今回のburstCountをクリアする前に、DispatchComputeShadersを実行する。

    if (globalFields_ && !globalFields_->empty()) {
        uint32_t count = (std::min)(static_cast<uint32_t>(globalFields_->size()), kMaxFields);
        memcpy(fieldsMappedData_[frameIndex], globalFields_->data(), sizeof(ParticleField) * count);
        if (count < kMaxFields) {
            memset(fieldsMappedData_[frameIndex] + count, 0, sizeof(ParticleField) * (kMaxFields - count));
        }
    } else {
        memset(fieldsMappedData_[frameIndex], 0, sizeof(ParticleField) * kMaxFields);
    }

    // [Bindless] テクスチャインデックスの反映
    if (engine_ && engine_->GetTextureManager()) {
        cpuMaterialData_.textureIndex = engine_->GetTextureManager()->GetSrvIndex(textureHandle_);
    } else {
        cpuMaterialData_.textureIndex = 0;
    }

    perFrameBuffer_.Update(*perFrameData_, frameIndex);
    materialBuffer_.Update(cpuMaterialData_, frameIndex);

    lastUpdateFrame_ = frameIndex;
}

void GPUParticleSystem::Setup(RenderGraphBuilder& builder) {
    if (particleResource_) {
        builder.RequireState(particleResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
}

void GPUParticleSystem::DispatchCompute() {
    // カリングされていても、計算（シミュレーション）は継続する
    if (!needsUpdateCS_) {
        return;
    }

    uint32_t frameIndex = dxCommon_->GetFrameIndex();

    SyncBeforeDraw();

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    DispatchComputeShaders(commandList);

    // ComputeShaderのDispatch完了後にburstCountをリセットする
    for (auto& em : emittersData_) {
        em.burstCount = 0;
    }

    needsUpdateCS_ = false;
}

// 描画
void GPUParticleSystem::Draw() {

    if (isCulled_) {
        return;
    }

    uint32_t frameIndex = dxCommon_->GetFrameIndex();

    // 描画パスによってカメラが変わる可能性があるため、毎回の描画で更新
    if (engine_) {
        if (Camera* activeCam = engine_->GetCameraManager()->GetActiveCamera()) {
            perViewBuffer_[frameIndex]->viewProjection = activeCam->GetViewProjectionMatrix3D();
            Irufemi::Matrix4x4 backToFrontMatrix_ = Irufemi::Math::MakeRotateYMatrix(0.0f);
            Irufemi::Matrix4x4 billboardMatrix_ =
                Irufemi::Math::Multiply(backToFrontMatrix_, activeCam->GetCameraMatrix());
            billboardMatrix_.m[3][0] = 0.0f;
            billboardMatrix_.m[3][1] = 0.0f;
            billboardMatrix_.m[3][2] = 0.0f;
            perViewBuffer_[frameIndex]->billboardMatrix = billboardMatrix_;
            perViewBuffer_[frameIndex]->worldPosition = activeCam->GetTranslate();
            perViewBuffer_[frameIndex]->cameraNear = activeCam->GetNearZ();
            perViewBuffer_[frameIndex]->cameraFar = activeCam->GetFarZ();
        }
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // 現在のステートを退避
    Irufemi::BlendMode oldBlend = engine_->currentBlend_;
    PSOManager::DepthWrite oldDepth = engine_->currentDepth_;
    PSOManager::CullMode oldCull = engine_->currentCull_;

    // パーティクル用のステートを設定
    engine_->SetBlend(selectedBlend_);
    engine_->SetDepthWrite(selectedDepth_);
    engine_->SetCull(selectedCull_);

    RenderPackets::GPUParticlePacket packet{};
    packet.vbv = vertexBufferView_;
    packet.ibv = indexBufferView_;
    packet.indexCount = indexCount_;
    packet.materialAddress = materialBuffer_.GetGPUVirtualAddress(frameIndex);
    packet.perViewAddress = perViewBuffer_.GetGPUVirtualAddress(frameIndex);
    packet.particleSrvHandle = particleSrvHandleGPU_;
    packet.sortListSrvHandle = sortSrvHandleGPU_;
    packet.instanceCount = kMaxParticles;
    packet.particleResource = particleResource_.Get();
    packet.blendMode = selectedBlend_;
    packet.depthWrite = selectedDepth_;
    packet.cullMode = selectedCull_;

    if (!customPSOName_.empty()) {
        packet.customPSO =
            dxCommon_->GetPSOManager()->GetPSO(customPSOName_, selectedBlend_, selectedDepth_, selectedCull_);
    }

    drawManager_->SubmitGPUParticle(packet);

#if USE_IMGUI
    if (debugLineRegion_) {
        debugLineRegion_->Draw();
    }
#endif

    // 退避したステートを元に戻す
    engine_->SetBlend(oldBlend);
    engine_->SetDepthWrite(oldDepth);
    engine_->SetCull(oldCull);
}

// デバッグ
void GPUParticleSystem::UpdateDebugLines() {
#if defined(USE_IMGUI)
    if (debugLineRegion_) {
        debugLineRegion_->ClearInstances();
    }

    if (engine_ && engine_->GetDebugPrimitiveRenderer()) {
        auto* debugRenderer = engine_->GetDebugPrimitiveRenderer();
        if (!debugRenderer->IsEnabled() || !debugRenderer->IsCategoryEnabled(DebugCategory::Particle)) {
            return;
        }
    }

    if (showEmitterArea_) {
        for (const auto& em : emittersData_) {
            if (em.showDebugArea == 0) {
                continue;
            }
            if (em.emit == 0 && em.burstCount == 0) {
                continue;
            }
            Irufemi::Vector4 color = {0.0f, 1.0f, 0.0f, 1.0f};
            Irufemi::Vector3 translate = {em.translateX, em.translateY, em.translateZ};
            Irufemi::Vector3 direction = {em.directionX, em.directionY, em.directionZ};
            Irufemi::Vector3 areaSize = {em.areaSizeX, em.areaSizeY, em.areaSizeZ};

            if (em.type == 0) {
                DrawSphereWireframe(translate, em.radius, color);
            } else if (em.type == 1) {
                DrawCylinderWireframe(translate, direction, em.radius, 50.0f, color);
            } else if (em.type == 2) {
                DrawCircle(translate, em.radius, {0, 1, 0}, color);
                DrawCircle(translate, em.radius - em.spread, {0, 1, 0}, color);
            } else if (em.type == 3) {
                DrawCylinderWireframe(translate, direction, em.radius, em.velocity, color);
            } else if (em.type == 4) {
                Irufemi::Vector3 minP = translate - areaSize * 0.5f;
                Irufemi::Vector3 maxP = translate + areaSize * 0.5f;
                DrawAABB(minP, maxP, color);
            }
        }
    }
#endif
}

void GPUParticleSystem::Debug() {
#if defined(USE_IMGUI)
    ImGui::Checkbox("Show Emitter Area", &showEmitterArea_);

    ImGui::Text("System Settings (Global)");
    ImGui::Separator();

    // Blend Mode
    const char* blendNames[] = {"None", "Normal", "Add", "Subtract", "Multiply", "Screen", "Premultiplied"};
    int currentBlend = (int)selectedBlend_;
    if (ImGui::Combo("Blend Mode", &currentBlend, blendNames, 7)) {
        SetBlendMode((Irufemi::BlendMode)currentBlend);
    }

    // Primitive
    const char* primitiveNames[] = {"Triangle", "Plane", "Cube", "Cylinder", "Sphere", "Tetra", "Circle", "Ring"};
    int currentPrim = (int)primitiveType_;
    if (ImGui::Combo("Particle Mesh", &currentPrim, primitiveNames, 8)) {
        SetPrimitive((Irufemi::PrimitiveType)currentPrim);
    }

    // Texture
    if (textureManager_ && !textureManager_->GetTextureNamesForDebug().empty()) {
        auto textureNames = textureManager_->GetTextureNamesForDebug();
        std::vector<const char*> namesCStr;
        for (const auto& name : textureNames) {
            namesCStr.push_back(name.c_str());
        }
        if (ImGui::Combo("Texture", &selectedTextureIndex_, namesCStr.data(), (int)namesCStr.size())) {
            SetTexture(textureNames[selectedTextureIndex_]);
        }
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "Emitter properties (Gravity, Velocity, etc.)");
    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "are managed via ParticleEmitterComponent.");

#endif
}

void GPUParticleSystem::Clear() {
    isInitializedCS_ = false;
    totalTime_ = 0.0f;
}

void GPUParticleSystem::Emit(uint32_t count, uint32_t emitterIndex) {
    if (emitterIndex < emittersData_.size()) {
        emittersData_[emitterIndex].burstCount += count;
    }
}

void GPUParticleSystem::SetSphereEmitter(const Irufemi::Vector3& pos, float radius, float emissionRate,
                                         uint32_t emitterIndex) {
    if (emitterIndex >= emittersData_.size()) {
        return;
    }
    auto* emitter_ = &emittersData_[emitterIndex];
    emitter_->type = 0;
    emitter_->translateX = pos.x;
    emitter_->translateY = pos.y;
    emitter_->translateZ = pos.z;
    emitter_->radius = radius;
    emitter_->emissionRate = emissionRate;
}

void GPUParticleSystem::SetHemisphereEmitter(const Irufemi::Vector3& pos, float radius, float emissionRate,
                                             uint32_t emitterIndex) {
    if (emitterIndex >= emittersData_.size()) {
        return;
    }
    auto* emitter_ = &emittersData_[emitterIndex];
    emitter_->type = 5;
    emitter_->translateX = pos.x;
    emitter_->translateY = pos.y;
    emitter_->translateZ = pos.z;
    emitter_->radius = radius;
    emitter_->emissionRate = emissionRate;
}

void GPUParticleSystem::SetBeamEmitter(const Irufemi::Vector3& pos, const Irufemi::Vector3& direction, float radius,
                                       float velocity, float spread, float emissionRate, uint32_t emitterIndex) {
    if (emitterIndex >= emittersData_.size()) {
        return;
    }
    auto* emitter_ = &emittersData_[emitterIndex];
    emitter_->type = 1;
    emitter_->translateX = pos.x;
    emitter_->translateY = pos.y;
    emitter_->translateZ = pos.z;
    emitter_->directionX = direction.x;
    emitter_->directionY = direction.y;
    emitter_->directionZ = direction.z;
    emitter_->radius = radius;
    emitter_->velocity = velocity;
    emitter_->spread = spread;
    emitter_->emissionRate = emissionRate;
}

void GPUParticleSystem::SetEmit(bool emit, uint32_t emitterIndex) {
    if (emitterIndex < emittersData_.size()) {
        emittersData_[emitterIndex].emit = emit ? 1 : 0;
    }
}

void GPUParticleSystem::SetParticleScale(const Irufemi::Vector3& startMin, const Irufemi::Vector3& startMax,
                                         const Irufemi::Vector3& endMin, const Irufemi::Vector3& endMax,
                                         uint32_t emitterIndex) {
    if (emitterIndex < emittersData_.size()) {
        auto* emitter_ = &emittersData_[emitterIndex];
        emitter_->startScaleMinX = startMin.x;
        emitter_->startScaleMinY = startMin.y;
        emitter_->startScaleMinZ = startMin.z;
        emitter_->startScaleMaxX = startMax.x;
        emitter_->startScaleMaxY = startMax.y;
        emitter_->startScaleMaxZ = startMax.z;
        emitter_->endScaleMinX = endMin.x;
        emitter_->endScaleMinY = endMin.y;
        emitter_->endScaleMinZ = endMin.z;
        emitter_->endScaleMaxX = endMax.x;
        emitter_->endScaleMaxY = endMax.y;
        emitter_->endScaleMaxZ = endMax.z;
    }
}

void GPUParticleSystem::SetMidScale(const Irufemi::Vector3& midMin, const Irufemi::Vector3& midMax, float midPoint,
                                    uint32_t emitterIndex) {
    if (emitterIndex < emittersData_.size()) {
        auto* emitter_ = &emittersData_[emitterIndex];
        emitter_->midScaleMinX = midMin.x;
        emitter_->midScaleMinY = midMin.y;
        emitter_->midScaleMinZ = midMin.z;
        emitter_->midScaleMaxX = midMax.x;
        emitter_->midScaleMaxY = midMax.y;
        emitter_->midScaleMaxZ = midMax.z;
        emitter_->midPoint = midPoint;
    }
}

void GPUParticleSystem::SetParticleColor(const Irufemi::Vector4& startMin, const Irufemi::Vector4& startMax,
                                         const Irufemi::Vector4& endMin, const Irufemi::Vector4& endMax,
                                         uint32_t emitterIndex) {
    if (emitterIndex < emittersData_.size()) {
        auto* emitter_ = &emittersData_[emitterIndex];
        emitter_->startColorMinR = startMin.x;
        emitter_->startColorMinG = startMin.y;
        emitter_->startColorMinB = startMin.z;
        emitter_->startColorMinA = startMin.w;
        emitter_->startColorMaxR = startMax.x;
        emitter_->startColorMaxG = startMax.y;
        emitter_->startColorMaxB = startMax.z;
        emitter_->startColorMaxA = startMax.w;
        emitter_->endColorMinR = endMin.x;
        emitter_->endColorMinG = endMin.y;
        emitter_->endColorMinB = endMin.z;
        emitter_->endColorMinA = endMin.w;
        emitter_->endColorMaxR = endMax.x;
        emitter_->endColorMaxG = endMax.y;
        emitter_->endColorMaxB = endMax.z;
        emitter_->endColorMaxA = endMax.w;
    }
}

void GPUParticleSystem::SetMidColor(const Irufemi::Vector4& midMin, const Irufemi::Vector4& midMax, float midPoint,
                                    uint32_t emitterIndex) {
    if (emitterIndex < emittersData_.size()) {
        auto* emitter_ = &emittersData_[emitterIndex];
        emitter_->midColorMinR = midMin.x;
        emitter_->midColorMinG = midMin.y;
        emitter_->midColorMinB = midMin.z;
        emitter_->midColorMinA = midMin.w;
        emitter_->midColorMaxR = midMax.x;
        emitter_->midColorMaxG = midMax.y;
        emitter_->midColorMaxB = midMax.z;
        emitter_->midColorMaxA = midMax.w;
        emitter_->midPoint = midPoint;
    }
}

void GPUParticleSystem::SetParticleLife(float minLife, float maxLife, uint32_t emitterIndex) {
    if (emitterIndex < emittersData_.size()) {
        auto* emitter_ = &emittersData_[emitterIndex];
        emitter_->minLife = minLife;
        emitter_->maxLife = maxLife;
    }
}

void GPUParticleSystem::SetPrimitive(Irufemi::PrimitiveType type) {
    primitiveType_ = type;
    const auto& res = engine_->GetPrimitiveManager()->GetStandardResource(type);
    vertexBufferView_ = res.vertexBufferView;
    indexBufferView_ = res.indexBufferView;
    indexCount_ = res.indexCount;
}

void GPUParticleSystem::SetBillboard(bool isBillboard, uint32_t emitterIndex) {
    if (emitterIndex < emittersData_.size()) {
        emittersData_[emitterIndex].billboardMode = isBillboard ? 1 : 0;
    }
}

void GPUParticleSystem::SetVelocityAligned(bool isAligned, uint32_t emitterIndex) {
    if (emitterIndex < emittersData_.size()) {
        emittersData_[emitterIndex].billboardMode = isAligned ? 2 : 1;
    }
}

void GPUParticleSystem::SetTexture(const std::string& textureFilePath) {
    if (!textureManager_) {
        return;
    }

    if (textureHandle_.IsValid()) {
        textureManager_->ReleaseTexture(textureHandle_);
    }
    textureHandle_ = textureManager_->LoadTexture(textureFilePath);

    // UIコンボボックス用のインデックス同期
    auto textureNames = textureManager_->GetTextureNamesForDebug();
    auto it = std::find(textureNames.begin(), textureNames.end(), textureFilePath);
    if (it != textureNames.end()) {
        selectedTextureIndex_ = static_cast<int>(std::distance(textureNames.begin(), it));
    }
}

void GPUParticleSystem::SetRingEmitter(const Irufemi::Vector3& pos, float radius, float thickness, float emissionRate,
                                       uint32_t emitterIndex) {
    if (emitterIndex >= emittersData_.size()) {
        return;
    }
    auto* emitter_ = &emittersData_[emitterIndex];
    emitter_->type = 2;
    emitter_->translateX = pos.x;
    emitter_->translateY = pos.y;
    emitter_->translateZ = pos.z;
    emitter_->radius = radius;
    emitter_->spread = thickness; // spreadをthicknessとして流用
    emitter_->emissionRate = emissionRate;
}

void GPUParticleSystem::SetCylinderEmitter(const Irufemi::Vector3& pos, const Irufemi::Vector3& direction, float radius,
                                           float height, float emissionRate, uint32_t emitterIndex) {
    if (emitterIndex >= emittersData_.size()) {
        return;
    }
    auto* emitter_ = &emittersData_[emitterIndex];
    emitter_->type = 3;
    emitter_->translateX = pos.x;
    emitter_->translateY = pos.y;
    emitter_->translateZ = pos.z;
    emitter_->directionX = direction.x;
    emitter_->directionY = direction.y;
    emitter_->directionZ = direction.z;
    emitter_->radius = radius;
    emitter_->velocity = height; // velocityをheightとして流用
    emitter_->emissionRate = emissionRate;
}

void GPUParticleSystem::SetBoxEmitter(const Irufemi::Vector3& pos, const Irufemi::Vector3& size, float emissionRate,
                                      uint32_t emitterIndex) {
    if (emitterIndex >= emittersData_.size()) {
        return;
    }
    auto* emitter_ = &emittersData_[emitterIndex];
    emitter_->type = 4;
    emitter_->translateX = pos.x;
    emitter_->translateY = pos.y;
    emitter_->translateZ = pos.z;
    emitter_->areaSizeX = size.x;
    emitter_->areaSizeY = size.y;
    emitter_->areaSizeZ = size.z;
    emitter_->emissionRate = emissionRate;
}

void GPUParticleSystem::SetMeshEmitter(const Irufemi::Vector3& pos, D3D12_GPU_VIRTUAL_ADDRESS vbAddress,
                                       uint32_t vertexCount, float emissionRate, uint32_t emitterIndex) {
    if (emitterIndex >= emittersData_.size()) {
        return;
    }
    auto* emitter_ = &emittersData_[emitterIndex];
    emitter_->type = 6;
    emitter_->translateX = pos.x;
    emitter_->translateY = pos.y;
    emitter_->translateZ = pos.z;
    emitter_->emissionRate = emissionRate;

    // Use padFreqTime for vertex count to pass to shader
    emitter_->padFreqTime = static_cast<float>(vertexCount);

    if (meshVertexBuffers_.size() <= emitterIndex) {
        meshVertexBuffers_.resize(emitterIndex + 1, 0);
    }
    meshVertexBuffers_[emitterIndex] = vbAddress;
}

void GPUParticleSystem::SetTextureAtlas(uint32_t rows, uint32_t cols, uint32_t emitterIndex) {
    if (emitterIndex < emittersData_.size()) {
        auto* emitter_ = &emittersData_[emitterIndex];
        emitter_->atlasRows = rows;
        emitter_->atlasCols = cols;
    }
}

void GPUParticleSystem::SetGroundCollision(float height, float bounce, uint32_t emitterIndex) {
    if (emitterIndex < emittersData_.size()) {
        auto* emitter_ = &emittersData_[emitterIndex];
        emitter_->groundHeight = height;
        emitter_->bounce = bounce;
    }
}

void GPUParticleSystem::SetAttractor(const Irufemi::Vector3& pos, float strength, uint32_t emitterIndex) {
    if (emitterIndex < emittersData_.size()) {
        auto* emitter_ = &emittersData_[emitterIndex];
        emitter_->attractorPosX = pos.x;
        emitter_->attractorPosY = pos.y;
        emitter_->attractorPosZ = pos.z;
        emitter_->attractorStrength = strength;
    }
}

void GPUParticleSystem::DrawAABB(const Irufemi::Vector3& min, const Irufemi::Vector3& max,
                                 const Irufemi::Vector4& color) {
    if (!debugLineRegion_) {
        return;
    }

    Irufemi::Vector3 v[8] = {{min.x, min.y, min.z}, {max.x, min.y, min.z}, {min.x, max.y, min.z},
                             {max.x, max.y, min.z}, {min.x, min.y, max.z}, {max.x, min.y, max.z},
                             {min.x, max.y, max.z}, {max.x, max.y, max.z}};

    debugLineRegion_->AddInstance(v[0], v[1], color);
    debugLineRegion_->AddInstance(v[1], v[3], color);
    debugLineRegion_->AddInstance(v[3], v[2], color);
    debugLineRegion_->AddInstance(v[2], v[0], color);
    debugLineRegion_->AddInstance(v[4], v[5], color);
    debugLineRegion_->AddInstance(v[5], v[7], color);
    debugLineRegion_->AddInstance(v[7], v[6], color);
    debugLineRegion_->AddInstance(v[6], v[4], color);
    debugLineRegion_->AddInstance(v[0], v[4], color);
    debugLineRegion_->AddInstance(v[1], v[5], color);
    debugLineRegion_->AddInstance(v[2], v[6], color);
    debugLineRegion_->AddInstance(v[3], v[7], color);
}

void GPUParticleSystem::DrawCircle(const Irufemi::Vector3& center, float radius, const Irufemi::Vector3& axis,
                                   const Irufemi::Vector4& color) {
    if (!debugLineRegion_) {
        return;
    }
    Irufemi::Vector3 up = Irufemi::Math::Normalize(axis);
    Irufemi::Vector3 right = Irufemi::Math::Normalize(std::abs(up.y) > 0.9f ? Irufemi::Math::Cross({1, 0, 0}, up)
                                                                            : Irufemi::Math::Cross({0, 1, 0}, up));
    Irufemi::Vector3 forward = Irufemi::Math::Cross(right, up);

    const int segments = 32;
    Irufemi::Vector3 prevPos = center + right * radius;
    for (int i = 1; i <= segments; ++i) {
        float angle = (float)i / segments * 3.141592f * 2.0f;
        Irufemi::Vector3 pos = center + (right * std::cos(angle) + forward * std::sin(angle)) * radius;
        debugLineRegion_->AddInstance(prevPos, pos, color);
        prevPos = pos;
    }
}

void GPUParticleSystem::DrawSphereWireframe(const Irufemi::Vector3& center, float radius,
                                            const Irufemi::Vector4& color) {
    DrawCircle(center, radius, {1, 0, 0}, color);
    DrawCircle(center, radius, {0, 1, 0}, color);
    DrawCircle(center, radius, {0, 0, 1}, color);
}

void GPUParticleSystem::DrawCylinderWireframe(const Irufemi::Vector3& center, const Irufemi::Vector3& direction,
                                              float radius, float height, const Irufemi::Vector4& color) {
    if (!debugLineRegion_) {
        return;
    }
    Irufemi::Vector3 dir = Irufemi::Math::Normalize(direction);
    Irufemi::Vector3 top = center + dir * height;

    DrawCircle(center, radius, dir, color);
    DrawCircle(top, radius, dir, color);

    Irufemi::Vector3 right = Irufemi::Math::Normalize(std::abs(dir.y) > 0.9f ? Irufemi::Math::Cross({1, 0, 0}, dir)
                                                                             : Irufemi::Math::Cross({0, 1, 0}, dir));
    Irufemi::Vector3 forward = Irufemi::Math::Cross(right, dir);

    for (int i = 0; i < 4; ++i) {
        float angle = (float)i / 4.0f * 3.141592f * 2.0f;
        Irufemi::Vector3 offset = (right * std::cos(angle) + forward * std::sin(angle)) * radius;
        debugLineRegion_->AddInstance(center + offset, top + offset, color);
    }
}

void GPUParticleSystem::DispatchComputeShaders(ID3D12GraphicsCommandList* commandList) {
    if (!engine_) {
        return;
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = {dxCommon_->GetSrvDescriptorHeap()};
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    commandList->SetComputeRootSignature(dxCommon_->GetComputeRootSignature());

    uint32_t frameIndex = dxCommon_->GetFrameIndex();

    if (!isInitializedCS_) {
        commandList->SetPipelineState(dxCommon_->GetPSOManager()->GetComputePSO("GpuParticleInitialize"));
        commandList->SetComputeRootDescriptorTable(3, particleUavHandleGPU_);
        commandList->SetComputeRootDescriptorTable(6, freeListIndexUavHandleGPU_);
        commandList->SetComputeRootDescriptorTable(7, freeListUavHandleGPU_);

        commandList->Dispatch((kMaxParticles + 1023) / 1024, 1, 1);

        DirectXUtils::UAVBarriers(commandList,
                                  {particleResource_.Get(), freeListIndexResource_.Get(), freeListResource_.Get()});
        isInitializedCS_ = true;
    }

    // Emit
    commandList->SetPipelineState(dxCommon_->GetPSOManager()->GetComputePSO("GpuParticleEmit"));
    commandList->SetComputeRootDescriptorTable(3, particleUavHandleGPU_);
    commandList->SetComputeRootDescriptorTable(6, freeListIndexUavHandleGPU_);
    commandList->SetComputeRootDescriptorTable(7, freeListUavHandleGPU_);
    commandList->SetComputeRootDescriptorTable(0, emittersSrvHandleGPU_[frameIndex]);                   // t0
    commandList->SetComputeRootDescriptorTable(1, fieldsSrvHandleGPU_[frameIndex]);                     // t1: Fields
    commandList->SetComputeRootConstantBufferView(5, perFrameBuffer_.GetGPUVirtualAddress(frameIndex)); // b1

    for (size_t i = 0; i < emittersData_.size(); ++i) {
        uint32_t emitCount = emittersData_[i].burstCount;
        if (emitCount > 0) {
            commandList->SetComputeRoot32BitConstant(9, (uint32_t)i,
                                                     0); // b2: gEmitterIndex

            // Bind mesh vertex buffer if mesh emitter
            if (emittersData_[i].type == 6 && i < meshVertexBuffers_.size() && meshVertexBuffers_[i] != 0) {
                commandList->SetComputeRootShaderResourceView(10, meshVertexBuffers_[i]);
            }

            commandList->Dispatch((emitCount + 1023) / 1024, 1, 1);
        }
    }

    // Emitフェーズの書き込み完了を保証するためバリアを張る
    DirectXUtils::UAVBarriers(commandList,
                              {particleResource_.Get(), freeListIndexResource_.Get(), freeListResource_.Get()});

    // Update
    commandList->SetPipelineState(dxCommon_->GetPSOManager()->GetComputePSO("GpuParticleUpdate"));
    commandList->SetComputeRootDescriptorTable(3, particleUavHandleGPU_);
    commandList->SetComputeRootDescriptorTable(6, freeListIndexUavHandleGPU_);
    commandList->SetComputeRootDescriptorTable(7, freeListUavHandleGPU_);
    commandList->SetComputeRootDescriptorTable(0, emittersSrvHandleGPU_[frameIndex]);                   // t0
    commandList->SetComputeRootDescriptorTable(1, fieldsSrvHandleGPU_[frameIndex]);                     // t1: Fields
    commandList->SetComputeRootConstantBufferView(5, perFrameBuffer_.GetGPUVirtualAddress(frameIndex)); // b1
    commandList->Dispatch((kMaxParticles + 1023) / 1024, 1, 1);

    // Updateフェーズ完了後のバリア
    DirectXUtils::UAVBarriers(commandList,
                              {particleResource_.Get(), freeListIndexResource_.Get(), freeListResource_.Get()});

    // --- Bitonic Sort Phase ---
    // 前フレームの最後にNON_PIXEL_SHADER_RESOURCEにしたため、ここでUAVに再度遷移させる
    if (isSortResourceInitialized_) {
        DirectXUtils::TransitionBarrier(commandList, sortResource_.Get(),
                                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    } else {
        // 初回はCOMMONからUAVへ遷移させる
        DirectXUtils::TransitionBarrier(commandList, sortResource_.Get(), D3D12_RESOURCE_STATE_COMMON,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        isSortResourceInitialized_ = true;
    }

    // 1. Init Sort List
    commandList->SetPipelineState(dxCommon_->GetPSOManager()->GetComputePSO("GpuParticleInitSort"));
    // Descriptor table mapping for InitParticleSort.CS.hlsl:
    // t0: Particle (u0 in RootSig index 0, wait, it's bound as SRV to slot 0)
    commandList->SetComputeRootDescriptorTable(0, particleSrvHandleGPU_);
    // u0: SortList (Slot 8 in our RootSig setup)
    commandList->SetComputeRootDescriptorTable(8, sortUavHandleGPU_);
    // b0: Irufemi::PerView (Slot 4 in RootSig)
    commandList->SetComputeRootConstantBufferView(4, perViewBuffer_.GetGPUVirtualAddress(frameIndex));

    commandList->Dispatch((kMaxParticles + 1023) / 1024, 1, 1);

    // Wait for Init
    DirectXUtils::UAVBarriers(commandList, {sortResource_.Get()});

    // 2. Execute Bitonic Sort (AAA Approach: ソート不要なブレンドモードはスキップ)
    if (selectedBlend_ == Irufemi::BlendMode::kBlendModeNormal) {
        commandList->SetPipelineState(dxCommon_->GetPSOManager()->GetComputePSO("GpuParticleBitonicSort"));
        commandList->SetComputeRootDescriptorTable(8, sortUavHandleGPU_); // u0

        for (uint32_t k = 2; k <= kMaxParticles; k <<= 1) {
            for (uint32_t j = k >> 1; j > 0; j >>= 1) {
                commandList->SetComputeRoot32BitConstant(9, k, 0); // b2, uint k
                commandList->SetComputeRoot32BitConstant(9, j, 1); // b2, uint j

                commandList->Dispatch(kMaxParticles / 1024, 1, 1);

                DirectXUtils::UAVBarriers(commandList, {sortResource_.Get()});
            }
        }
    }

    // Ensure sorting is completely done before graphics queue uses it
    DirectXUtils::TransitionBarrier(commandList, sortResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    needsUpdateCS_ = false;
    isCsDispatchedThisFrame_ = true;
}

void GPUParticleSystem::CreateBuffersAndViews() {
    auto* srvPool = dxCommon_->GetSrvPool();

    /*Emitter と Irufemi::PerFrame の定数バッファ初期化*/
    for (uint32_t i = 0; i < 3; ++i) { // 3 = kMaxFramesInFlight
        emittersResource_[i] = dxCommon_->CreateBufferResource(sizeof(GPUParticleEmitter) * kMaxEmitters);
        emittersResource_[i]->Map(0, nullptr, reinterpret_cast<void**>(&emittersMappedData_[i]));

        emittersSrvIndex_[i] = srvPool->Allocate();
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = kMaxEmitters;
        srvDesc.Buffer.StructureByteStride = sizeof(GPUParticleEmitter);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        engine_->GetDevice()->CreateShaderResourceView(emittersResource_[i].Get(), &srvDesc,
                                                       srvPool->GetCPUHandle(emittersSrvIndex_[i]));
        emittersSrvHandleGPU_[i] = srvPool->GetGPUHandle(emittersSrvIndex_[i]);

        // Field Resource Initialization
        fieldsResource_[i] = dxCommon_->CreateBufferResource(sizeof(ParticleField) * kMaxFields);
        fieldsResource_[i]->Map(0, nullptr, reinterpret_cast<void**>(&fieldsMappedData_[i]));

        fieldsSrvIndex_[i] = srvPool->Allocate();
        D3D12_SHADER_RESOURCE_VIEW_DESC fieldSrvDesc{};
        fieldSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        fieldSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        fieldSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        fieldSrvDesc.Buffer.FirstElement = 0;
        fieldSrvDesc.Buffer.NumElements = kMaxFields;
        fieldSrvDesc.Buffer.StructureByteStride = sizeof(ParticleField);
        fieldSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        engine_->GetDevice()->CreateShaderResourceView(fieldsResource_[i].Get(), &fieldSrvDesc,
                                                       srvPool->GetCPUHandle(fieldsSrvIndex_[i]));
        fieldsSrvHandleGPU_[i] = srvPool->GetGPUHandle(fieldsSrvIndex_[i]);
    }

    perFrameBuffer_.Initialize(dxCommon_);

    // SRV (perFrame の SRV は未使用のため削除)

    /*GPUParticle*/

    // 1. Particleの情報を格納するためのResourceをD3D12_HEAP_TYPE_DEFAULTで作る
    particleResource_ = dxCommon_->CreateUAVBufferResource(sizeof(ParticleCS) * kMaxParticles);

    // 2. 1に対してUAV等のViewを作る
    // UAV
    particleUavIndex_ = srvPool->Allocate();
    particleUavHandleCPU_ = srvPool->GetCPUHandle(particleUavIndex_);
    particleUavHandleGPU_ = srvPool->GetGPUHandle(particleUavIndex_);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = kMaxParticles;
    uavDesc.Buffer.StructureByteStride = sizeof(ParticleCS);
    dxCommon_->GetDevice()->CreateUnorderedAccessView(particleResource_.Get(), nullptr, &uavDesc,
                                                      particleUavHandleCPU_);

    // SRV
    particleSrvIndex_ = srvPool->Allocate();
    particleSrvHandleCPU_ = srvPool->GetCPUHandle(particleSrvIndex_);
    particleSrvHandleGPU_ = srvPool->GetGPUHandle(particleSrvIndex_);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = kMaxParticles;
    srvDesc.Buffer.StructureByteStride = sizeof(ParticleCS);
    dxCommon_->GetDevice()->CreateShaderResourceView(particleResource_.Get(), &srvDesc, particleSrvHandleCPU_);

    // freeListIndexリソース
    freeListIndexResource_ = dxCommon_->CreateUAVBufferResource(sizeof(int32_t));
    // UAV
    freeListIndexUavIndex_ = srvPool->Allocate();
    freeListIndexUavHandleCPU_ = srvPool->GetCPUHandle(freeListIndexUavIndex_);
    freeListIndexUavHandleGPU_ = srvPool->GetGPUHandle(freeListIndexUavIndex_);
    D3D12_UNORDERED_ACCESS_VIEW_DESC freeListIndexUavDesc{};
    freeListIndexUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    freeListIndexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    freeListIndexUavDesc.Buffer.FirstElement = 0;
    freeListIndexUavDesc.Buffer.NumElements = 1;
    freeListIndexUavDesc.Buffer.StructureByteStride = sizeof(int32_t);
    dxCommon_->GetDevice()->CreateUnorderedAccessView(freeListIndexResource_.Get(), nullptr, &freeListIndexUavDesc,
                                                      freeListIndexUavHandleCPU_);

    // freeListリソース
    freeListResource_ = dxCommon_->CreateUAVBufferResource(sizeof(int32_t) * kMaxParticles);
    // UAV
    freeListUavIndex_ = srvPool->Allocate();
    freeListUavHandleCPU_ = srvPool->GetCPUHandle(freeListUavIndex_);
    freeListUavHandleGPU_ = srvPool->GetGPUHandle(freeListUavIndex_);
    D3D12_UNORDERED_ACCESS_VIEW_DESC freeListUavDesc{};
    freeListUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    freeListUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    freeListUavDesc.Buffer.FirstElement = 0;
    freeListUavDesc.Buffer.NumElements = kMaxParticles;
    freeListUavDesc.Buffer.StructureByteStride = sizeof(int32_t);
    dxCommon_->GetDevice()->CreateUnorderedAccessView(freeListResource_.Get(), nullptr, &freeListUavDesc,
                                                      freeListUavHandleCPU_);

    // sortResource
    sortResource_ = dxCommon_->CreateUAVBufferResource((sizeof(float) + sizeof(uint32_t)) * kMaxParticles);
    sortIndex_ = srvPool->Allocate();
    sortUavHandleCPU_ = srvPool->GetCPUHandle(sortIndex_);
    sortUavHandleGPU_ = srvPool->GetGPUHandle(sortIndex_);
    // UAV
    D3D12_UNORDERED_ACCESS_VIEW_DESC sortUavDesc{};
    sortUavDesc.Format = DXGI_FORMAT_UNKNOWN;
    sortUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    sortUavDesc.Buffer.FirstElement = 0;
    sortUavDesc.Buffer.NumElements = kMaxParticles;
    sortUavDesc.Buffer.StructureByteStride = sizeof(float) + sizeof(uint32_t);
    dxCommon_->GetDevice()->CreateUnorderedAccessView(sortResource_.Get(), nullptr, &sortUavDesc, sortUavHandleCPU_);

    // SRV
    sortSrvIndex_ = srvPool->Allocate();
    sortSrvHandleCPU_ = srvPool->GetCPUHandle(sortSrvIndex_);
    sortSrvHandleGPU_ = srvPool->GetGPUHandle(sortSrvIndex_);
    D3D12_SHADER_RESOURCE_VIEW_DESC sortSrvDesc{};
    sortSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    sortSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sortSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    sortSrvDesc.Buffer.FirstElement = 0;
    sortSrvDesc.Buffer.NumElements = kMaxParticles;
    sortSrvDesc.Buffer.StructureByteStride = sizeof(float) + sizeof(uint32_t);
    dxCommon_->GetDevice()->CreateShaderResourceView(sortResource_.Get(), &sortSrvDesc, sortSrvHandleCPU_);

    // PerView用リソース
    perViewBuffer_.Initialize(dxCommon_);

    // Material用リソース
    materialBuffer_.Initialize(dxCommon_);
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
        materialBuffer_[i]->color = {1.0f, 1.0f, 1.0f, 1.0f};
        materialBuffer_[i]->uvTransform = Irufemi::Math::MakeIdentity4x4();
        materialBuffer_[i]->useClampSampler = 0;
    }
}
