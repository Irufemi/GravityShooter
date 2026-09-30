/**
 * @file DebugScene.cpp
 * @brief エンジンの各機能のテストおよび実装例を示すためのデバッグ用シーンクラスの実装
 */

#include "Framework/Scene/DebugScene.h" // Unified debug UI enabled

#include "Framework/Scene/SceneManager.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"
#include "Core/Math/Math.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"
#include "Framework/Component/Renderer/SkeletonDebugRendererComponent.h"
#include "Core/System/IrufemiEngine.h"

// デストラクタ
DebugScene::~DebugScene() {
    if (lightningParamsResource_ && lightningParamsData_) {
        lightningParamsResource_->Unmap(0, nullptr);
        lightningParamsData_ = nullptr;
    }
}

// 初期化
void DebugScene::Initialize(IrufemiEngine* engine) {
    BaseScene::Initialize(engine);

    Camera* activeCamera = engine_->GetCameraManager()->GetActiveCamera();
    activeCamera->SetTranslate(Irufemi::Vector3{0.0f, 0.0f, -10.0f});
    activeCamera->UpdateMatrix();

    auto areaLight = std::make_unique<AreaLight>();
    areaLight->color = {1.0f, 0.5f, 0.5f, 1.0f};
    areaLight->position = {0.0f, 2.0f, 2.0f};
    areaLight->intensity = 1.0f;
    areaLight->direction = {0.0f, -1.0f, 0.0f};
    areaLight->range = 10.0f;
    areaLight->size = {2.0f, 2.0f};
    areaLight->isActive = 1;
    areaLights_.push_back(std::move(areaLight));

    isActiveObj_ = false;
    isActiveSprite_ = false;
    isActiveStanfordBunny_ = false;
    isActiveUtashTeapot_ = false;
    isActiveMultiMesh_ = false;
    isActiveMultiMaterial_ = false;
    isActiveSuzanne_ = false;
    isActiveFence_ = false;
    isActiveTerrain_ = false;
    isActiveVoxelParticle_ = false;
    isActiveGPUParticle_ = false;
    isActiveAnimatedCube_ = false;
    isActiveWalk_ = false;
    isActiveSneakWalk_ = false;
    isActiveBlendTest_ = false;
    isActiveSkybox_ = false;
    isActivePrimitiveObj_ = false;
    isActivePrimitive2DObj_ = false;
    isActiveLightningCrawl_ = false;
    isActiveImGuiDemo_ = false;

    if (isActiveSprite_) {
        sprite_ = std::make_unique<Sprite>();
        sprite_->Initialize();
    }
    if (isActiveObj_) {
        obj_ = std::make_unique<StaticModelObject>();
        obj_->Initialize("sample/plane.gltf");
    }
    if (isActiveStanfordBunny_) {
        stanfordBunny_ = std::make_unique<StaticModelObject>();
        stanfordBunny_->Initialize("sample/bunny.obj");
    }
    if (isActiveUtashTeapot_) {
        utashTeapot_ = std::make_unique<StaticModelObject>();
        utashTeapot_->Initialize("sample/teapot.obj");
    }
    if (isActiveMultiMesh_) {
        multiMesh_ = std::make_unique<StaticModelObject>();
        multiMesh_->Initialize("sample/multiMesh.obj");
    }
    if (isActiveMultiMaterial_) {
        multiMaterial_ = std::make_unique<StaticModelObject>();
        multiMaterial_->Initialize("sample/multiMaterial.obj");
    }
    if (isActiveSuzanne_) {
        suzanne_ = std::make_unique<StaticModelObject>();
        suzanne_->Initialize("sample/suzanne.obj");
    }
    if (isActiveFence_) {
        fence_ = std::make_unique<StaticModelObject>();
        fence_->Initialize("sample/fence.obj");
    }
    if (isActiveTerrain_) {
        terrain_ = std::make_unique<StaticModelObject>();
        terrain_->Initialize("sample/terrain.obj");
    }
    if (isActiveGPUParticle_) {
        particleObj_ = std::make_unique<ParticleObject>();
        particleObj_->SetTexturePath("resources/circle.png");
        particleObj_->SetEmitType(0);
        particleObj_->SetVelocity(5.0f);
        particleObj_->SetGravity(0.0f);
        particleObj_->SetDamping(0.05f);
        particleObj_->SetLifeTimeMin(0.3f);
        particleObj_->SetLifeTimeMax(0.6f);
        particleObj_->SetRadius(5.0f);
        particleObj_->SetColor({1.0f, 0.5f, 0.0f, 1.0f});
        particleObj_->Initialize();
    }
    if (isActiveVoxelParticle_) {
        voxelEmitterHandle_ = engine_->GetVoxelParticleManager()->RegisterEmitter("sample/terrain.obj", {64, 64, 64});
    }
    if (isActiveAnimatedCube_) {
        animatedCube_ = std::make_shared<GameObject>("AnimatedCube");
        animatedCube_->SetScene(this);
        animatedCube_->AddComponent<SkinnedMeshRendererComponent>()->LoadModel("sample/AnimatedCube.gltf");
        animatedCube_->AddComponent<AnimatorComponent>()->Play("sample/AnimatedCube.gltf", true);
        animatedCube_->AddComponent<SkeletonDebugRendererComponent>();
    }
    if (isActiveWalk_) {
        walk_ = std::make_shared<GameObject>("Walk");
        walk_->SetScene(this);
        walk_->AddComponent<SkinnedMeshRendererComponent>()->LoadModel("sample/walk.gltf");
        walk_->AddComponent<AnimatorComponent>()->Play("sample/walk.gltf", true);
        walk_->AddComponent<SkeletonDebugRendererComponent>();
    }
    if (isActiveSneakWalk_) {
        sneakWalk_ = std::make_shared<GameObject>("SneakWalk");
        sneakWalk_->SetScene(this);
        sneakWalk_->AddComponent<SkinnedMeshRendererComponent>()->LoadModel("sample/sneakWalk.gltf");
        sneakWalk_->AddComponent<AnimatorComponent>()->Play("sample/sneakWalk.gltf", true);
        sneakWalk_->AddComponent<SkeletonDebugRendererComponent>();
    }
    if (isActiveSkybox_) {
        skybox_ = std::make_unique<Skybox>();
        skybox_->Initialize("resources/qwantani_night_puresky_1k_cubemap.dds");
    }
    if (isActivePrimitiveObj_) {
        primitiveObj_ = std::make_unique<Primitive3DObject>();
        primitiveObj_->Initialize(Irufemi::PrimitiveType::Cube);
        primitiveObj_->SetPosition({0.0f, 0.0f, 0.0f}); // 他のオブジェクトと被らないように少しずらす
    }

    // 電撃エフェクトの初期化
    lightningCylinder_ = std::make_unique<Primitive3DObject>();
    lightningCylinder_->Initialize(Irufemi::PrimitiveType::Cylinder);
    lightningCylinder_->SetScale({0.2f, 10.0f, 0.2f}); // ビームっぽく細長く
    lightningCylinder_->SetPosition({-2.0f, 0.0f, 0.0f});

    lightningParamsResource_ = engine_->GetDirectXCommon()->CreateBufferResource(sizeof(LightningParams));
    lightningParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightningParamsData_));
    if (lightningParamsData_) {
        *lightningParamsData_ = LightningParams();
        lightningParamsData_->noiseThreshold = 0.2f; // 出現しやすくする
        lightningParamsData_->intensity = 5.0f;      // 輝きを強める
    }
    lightningCylinder_->SetCullingEnabled(false); // シリンダー内面も可視化するためカリングを無効化
}

// 更新
void DebugScene::Update() {
    // =====
    // ↓ゲームの更新
    // =====
#ifdef USE_IMGUI
    ImGui::Begin("Activation");
    ImGui::Checkbox("Sprite", &isActiveSprite_);
    ImGui::Checkbox("Primitive2D Test", &isActivePrimitive2DObj_);
    ImGui::Checkbox("Primitive Test", &isActivePrimitiveObj_);

    ImGui::Checkbox("Obj", &isActiveObj_);
    ImGui::Checkbox("Utash Teapot", &isActiveUtashTeapot_);
    ImGui::Checkbox("Stanford Bunny", &isActiveStanfordBunny_);
    ImGui::Checkbox("MultiMesh", &isActiveMultiMesh_);
    ImGui::Checkbox("MultiMaterial", &isActiveMultiMaterial_);
    ImGui::Checkbox("Suzanne", &isActiveSuzanne_);
    ImGui::Checkbox("Fence", &isActiveFence_);
    ImGui::Checkbox("Terrain", &isActiveTerrain_);

    ImGui::Checkbox("GPU Particle (Code)", &isActiveGPUParticle_);
    ImGui::Checkbox("VoxelParticle", &isActiveVoxelParticle_);
    ImGui::Checkbox("AnimatedCube", &isActiveAnimatedCube_);
    ImGui::Checkbox("Walk", &isActiveWalk_);
    ImGui::Checkbox("SneakWalk", &isActiveSneakWalk_);
    ImGui::Checkbox("Blend Test", &isActiveBlendTest_);
    ImGui::Checkbox("Skybox", &isActiveSkybox_);

    ImGui::Checkbox("Lightning Crawl", &isActiveLightningCrawl_);
    ImGui::Checkbox("ImGui Demo", &isActiveImGuiDemo_);
    ImGui::End();

    if (isActiveImGuiDemo_) {
        ImGui::ShowDemoWindow();
    }
#endif

    // 3D
    if (isActiveObj_) {
        if (!obj_) {
            obj_ = std::make_unique<StaticModelObject>();
            obj_->Initialize("sample/plane.gltf");
        }
        obj_->Update();
    }
    if (isActiveUtashTeapot_) {
        if (!utashTeapot_) {
            utashTeapot_ = std::make_unique<StaticModelObject>();
            utashTeapot_->Initialize("sample/teapot.obj");
        }
        utashTeapot_->Debug("Utash Teapot");
        utashTeapot_->Update();
    }
    if (isActiveStanfordBunny_) {
        if (!stanfordBunny_) {
            stanfordBunny_ = std::make_unique<StaticModelObject>();
            stanfordBunny_->Initialize("sample/bunny.obj");
        }
        stanfordBunny_->Debug("Stanford Bunny");
        stanfordBunny_->Update();
    }
    if (isActiveMultiMesh_) {
        if (!multiMesh_) {
            multiMesh_ = std::make_unique<StaticModelObject>();
            multiMesh_->Initialize("sample/multiMesh.obj");
        }
        multiMesh_->Debug("MultiMesh");
        multiMesh_->Update();
    }
    if (isActiveMultiMaterial_) {
        if (!multiMaterial_) {
            multiMaterial_ = std::make_unique<StaticModelObject>();
            multiMaterial_->Initialize("sample/multiMaterial.obj");
        }
        multiMaterial_->Debug("MultiMaterial");
        multiMaterial_->Update();
    }
    if (isActiveSuzanne_) {
        if (!suzanne_) {
            suzanne_ = std::make_unique<StaticModelObject>();
            suzanne_->Initialize("sample/suzanne.obj");
        }
        suzanne_->Update();
    }
    if (isActiveFence_) {
        if (!fence_) {
            fence_ = std::make_unique<StaticModelObject>();
            fence_->Initialize("sample/fence.obj");
        }
        fence_->Debug("Fence");
        fence_->Update();
    }
    if (isActiveTerrain_) {
        if (!terrain_) {
            terrain_ = std::make_unique<StaticModelObject>();
            terrain_->Initialize("sample/terrain.obj");
        }
        terrain_->Debug("Terrain");
        terrain_->Update();
    }

    if (isActiveGPUParticle_) {
        if (!particleObj_) {
            particleObj_ = std::make_unique<ParticleObject>();
            particleObj_->SetTexturePath("resources/circle.png");
            particleObj_->SetEmitType(0);
            particleObj_->SetVelocity(5.0f);
            particleObj_->SetGravity(0.0f);
            particleObj_->SetDamping(0.05f);
            particleObj_->SetLifeTimeMin(0.3f);
            particleObj_->SetLifeTimeMax(0.6f);
            particleObj_->SetRadius(5.0f);
            particleObj_->SetColor({1.0f, 0.5f, 0.0f, 1.0f});
            particleObj_->SetMidColor({1.0f, 0.0f, 0.0f, 1.0f});
            particleObj_->Initialize();
        }
#ifdef USE_IMGUI
        ImGui::Begin("Hardcoded Particle Test");
        particleObj_->DebugUI("Hardcoded Particle Test");
        ImGui::End();
#endif
        particleObj_->Update();
    } else {
        if (particleObj_) {
            particleObj_.reset();
        }
    }

    if (isActiveVoxelParticle_) {
        if (!voxelEmitterHandle_.IsValid()) {
            voxelEmitterHandle_ =
                engine_->GetVoxelParticleManager()->RegisterEmitter("sample/terrain.obj", {64, 64, 64});
            voxelEmitParams_.emit = 0;
            voxelEmitParams_.emitPosition = {0.0f, 0.0f, 0.0f};
            voxelEmitParams_.lifeTime = 2.0f;
            voxelEmitParams_.gravity = 2.0f;
            voxelEmitParams_.dispersion = 8.0f;
            voxelEmitParams_.convergence = 0.1f;
            voxelEmitParams_.baseVelocity = {0.0f, 5.0f, 0.0f};
        }
        engine_->GetVoxelParticleManager()->UpdateEmitterData(voxelEmitterHandle_, voxelEmitParams_);
    } else {
        if (voxelEmitterHandle_.IsValid()) {
            engine_->GetVoxelParticleManager()->UnregisterEmitter(voxelEmitterHandle_);
            voxelEmitterHandle_ = {};
        }
    }

    if (isActiveAnimatedCube_) {
        if (!animatedCube_) {
            animatedCube_ = std::make_shared<GameObject>("AnimatedCube");
            animatedCube_->SetScene(this);
            animatedCube_->AddComponent<SkinnedMeshRendererComponent>()->LoadModel("sample/AnimatedCube.gltf");
            animatedCube_->AddComponent<AnimatorComponent>()->Play("sample/AnimatedCube.gltf", true);
            animatedCube_->AddComponent<SkeletonDebugRendererComponent>();
        }
        animatedCube_->Update();
    }
    if (isActiveWalk_) {
        if (!walk_) {
            walk_ = std::make_shared<GameObject>("Walk");
            walk_->SetScene(this);
            walk_->AddComponent<SkinnedMeshRendererComponent>()->LoadModel("sample/walk.gltf");
            walk_->AddComponent<AnimatorComponent>()->Play("sample/walk.gltf", true);
            walk_->AddComponent<SkeletonDebugRendererComponent>();
        }
        walk_->Update();
    }
    if (isActiveSneakWalk_) {
        if (!sneakWalk_) {
            sneakWalk_ = std::make_shared<GameObject>("SneakWalk");
            sneakWalk_->SetScene(this);
            sneakWalk_->AddComponent<SkinnedMeshRendererComponent>()->LoadModel("sample/sneakWalk.gltf");
            sneakWalk_->AddComponent<AnimatorComponent>()->Play("sample/sneakWalk.gltf", true);
            sneakWalk_->AddComponent<SkeletonDebugRendererComponent>();
        }
        sneakWalk_->Update();
    }
    if (isActiveBlendTest_) {
        if (!blendTest_) {
            blendTest_ = std::make_shared<GameObject>("BlendTest");
            blendTest_->SetScene(this);
            blendTest_->AddComponent<SkinnedMeshRendererComponent>()->LoadModel("sample/walk.gltf");
            blendTest_->AddComponent<AnimatorComponent>()->Play("sample/walk.gltf", true);
            blendTest_->AddComponent<SkeletonDebugRendererComponent>();
        }
        blendTest_->Update();
    }
    if (isActiveSkybox_) {
        if (!skybox_) {
            skybox_ = std::make_unique<Skybox>();
            skybox_->Initialize("resources/qwantani_night_puresky_1k_cubemap.dds");
        }
        skybox_->Update();
    }
    if (isActivePrimitiveObj_) {
        if (!primitiveObj_) {
            primitiveObj_ = std::make_unique<Primitive3DObject>();
            primitiveObj_->Initialize(Irufemi::PrimitiveType::Cube);
        }
        primitiveObj_->Update();
    }

    if (isActiveLightningCrawl_) {
        lightningCylinder_->Update();
    }

    // 2D
    if (isActiveSprite_) {
        if (!sprite_) {
            sprite_ = std::make_unique<Sprite>();
            sprite_->Initialize();
        }
        sprite_->Debug("Sprite");
        sprite_->Update();
    }

    if (isActivePrimitive2DObj_) {
        if (!primitive2DObj_) {
            primitive2DObj_ = std::make_unique<Primitive2DObject>();
            primitive2DObj_->Initialize(Irufemi::Primitive2DType::Circle, "resources/uvChecker.png");
            primitive2DObj_->SetSize({100.0f, 100.0f});
            primitive2DObj_->SetPosition({640.0f, 360.0f, 0.0f});
            primitive2DObj_->SetPivot({0.5f, 0.5f});
        }
        primitive2DObj_->Update();
    } else {
        if (primitive2DObj_) {
            primitive2DObj_.reset();
        }
    }

    if (engine_->GetInputManager()->IsKeyPressed('P') ||
        engine_->GetInputManager()->IsButtonPressed(XINPUT_GAMEPAD_A)) {
        engine_->GetSceneManager()->Request("InGame");
    }

    // =====
    // ↑ゲームの更新
    // =====

    BaseScene::Update();

    // 環境マップをDrawManagerに設定
    if (isActiveSkybox_) {
        D3D12_GPU_DESCRIPTOR_HANDLE envMapHandle =
            engine_->GetTextureManager()->ResolveCubeMap(skybox_->GetTextureHandle());
        engine_->GetDrawManager()->SetEnvironmentMap(envMapHandle);
    }
}

void DebugScene::Draw() {
    BaseScene::Draw();

    // 3D
    engine_->SetBlend(Irufemi::BlendMode::kBlendModeNormal);
    engine_->SetDepthWrite(PSOManager::DepthWrite::Enable);
    engine_->SetCull(PSOManager::CullMode::Back);
    engine_->ApplyPSO("Object3D");

    if (isActiveObj_) {
        obj_->Draw();
    }
    if (isActiveUtashTeapot_) {
        utashTeapot_->Draw();
    }
    if (isActiveStanfordBunny_) {
        stanfordBunny_->Draw();
    }
    if (isActiveMultiMesh_) {
        multiMesh_->Draw();
    }
    if (isActiveMultiMaterial_) {
        multiMaterial_->Draw();
    }
    if (isActiveSuzanne_) {
        suzanne_->Draw();
    }
    if (isActiveFence_) {
        fence_->Draw();
    }
    if (isActiveTerrain_) {
        terrain_->Draw();
    }
    if (isActiveAnimatedCube_) {
        animatedCube_->Draw();
    }
    if (isActiveWalk_) {
        walk_->Draw();
    }
    if (isActiveSneakWalk_) {
        sneakWalk_->Draw();
    }
    if (isActiveBlendTest_) {
        blendTest_->Draw();
    }
    if (isActivePrimitiveObj_) {
        primitiveObj_->Draw();
    }
    if (isActiveSkybox_) {
        skybox_->Draw();
    }

    if (isActiveLightningCrawl_) {
        lightningCylinder_->SyncBeforeDraw();

        engine_->GetDrawManager()->SubmitPostRender([this]() {
            engine_->SetBlend(Irufemi::BlendMode::kBlendModeAdd);
            engine_->SetDepthWrite(PSOManager::DepthWrite::Disable);
            engine_->SetCull(PSOManager::CullMode::None);

            engine_->ApplyPSO("LightningCrawl");
            engine_->BindLightningParams(lightningParamsResource_->GetGPUVirtualAddress());

            RenderPackets::Standard3DPacket packet{};
            packet.resource = lightningCylinder_->GetMesh().resource.get();
            packet.blendMode = Irufemi::BlendMode::kBlendModeAdd;
            packet.depthWrite = PSOManager::DepthWrite::Disable;
            packet.cullMode = PSOManager::CullMode::None;
            engine_->GetDrawManager()->DrawStandard3D(packet);

            // 状態を戻す
            engine_->SetBlend(Irufemi::BlendMode::kBlendModeNormal);
            engine_->SetDepthWrite(PSOManager::DepthWrite::Enable);
            engine_->SetCull(PSOManager::CullMode::Back);
        });
    }

    engine_->SetBlend(Irufemi::BlendMode::kBlendModeAdd);
    engine_->SetDepthWrite(PSOManager::DepthWrite::Disable);

    // Effect と Particle は Additive / DepthDisable で描画
    engine_->SetCull(PSOManager::CullMode::None);
    engine_->SetBlend(Irufemi::BlendMode::kBlendModeAdd);
    engine_->SetDepthWrite(PSOManager::DepthWrite::Disable);
    engine_->ApplyPSO("Particle");

    // Managerが描画するのでここでは何もしない

    // 2D
    engine_->SetBlend(Irufemi::BlendMode::kBlendModeNormal);
    engine_->SetDepthWrite(PSOManager::DepthWrite::Disable);
    engine_->ApplyPSO("Sprite");

    if (isActiveSprite_) {
        sprite_->Draw();
    }
    if (isActivePrimitive2DObj_) {
        primitive2DObj_->Draw();
    }
}

void DebugScene::DrawDebugTabItem() {
#ifdef USE_IMGUI
    BaseScene::DrawDebugTabItem();
#endif
}

void DebugScene::DrawStandaloneDebugWindows() {
#ifdef USE_IMGUI
    if (ImGui::Begin("DebugScene Global Settings")) {
        static bool s_showAllBones = false;
        static bool s_showAllAxes = false;
        bool changed = false;
        if (ImGui::Checkbox("Show All Debug Bones", &s_showAllBones)) {
            changed = true;
        }
        if (ImGui::Checkbox("Show All Debug Axes", &s_showAllAxes)) {
            changed = true;
        }

        if (changed) {
            auto setDebugProps = [](GameObject* go) {
                if (!go) {
                    return;
                }
                if (auto comp = go->GetComponent<SkeletonDebugRendererComponent>()) {
                    comp->SetShowBones(s_showAllBones);
                    comp->SetShowAxes(s_showAllAxes);
                }
            };
            setDebugProps(animatedCube_.get());
            setDebugProps(walk_.get());
            setDebugProps(sneakWalk_.get());
            setDebugProps(blendTest_.get());
        }
    }
    ImGui::End();

    if (isActiveSkybox_ && skybox_) {
        skybox_->Debug();
    }

    if (isActivePrimitiveObj_ && primitiveObj_) {
        primitiveObj_->Debug("Primitive Object (New)");
    }

    if (isActiveVoxelParticle_ && voxelEmitterHandle_.IsValid()) {
        if (ImGui::Begin("Voxel Particle Test")) {
            ImGui::Separator();
            ImGui::Checkbox("Active Voxel Particle", &isActiveVoxelParticle_);

            const char* particleTypes[] = {"Default",     "Building",           "AshDisintegration",
                                           "FineScatter", "DebrisLargeGravity", "DebrisExplosive"};
            int currentType = static_cast<int>(voxelEmitParams_.particleType);
            if (ImGui::Combo("Particle Type", &currentType, particleTypes, IM_ARRAYSIZE(particleTypes))) {
                voxelEmitParams_.particleType = static_cast<uint32_t>(currentType);
            }
            ImGui::DragFloat("LifeTime", &voxelEmitParams_.lifeTime, 0.1f, 0.1f, 10.0f);
            ImGui::DragFloat("Gravity", &voxelEmitParams_.gravity, 0.1f, -20.0f, 100.0f);
            ImGui::DragFloat("Dispersion", &voxelEmitParams_.dispersion, 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat("Convergence", &voxelEmitParams_.convergence, 0.01f, 0.0f, 1.0f);

            bool isEmitting = voxelEmitParams_.emit > 0;
            if (ImGui::Checkbox("Continuous Emit", &isEmitting)) {
                voxelEmitParams_.emit = isEmitting ? 1 : 0;
            }

            if (ImGui::Button("Play Explosion")) {
                engine_->GetVoxelParticleManager()->PlayExplosion("sample/terrain.obj", {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
                                                                  {1, 1, 1}, voxelEmitParams_, {64, 64, 64});
            }
        }
        ImGui::End();
    }

    if (isActivePrimitive2DObj_ && primitive2DObj_) {
        primitive2DObj_->Debug("Primitive2D Test");
    }

    if (isActiveBlendTest_ && blendTest_) {
        if (ImGui::Begin("Blend Test Control")) {
            if (ImGui::Button("Crossfade to Walk (1.0s)")) {
                blendTest_->GetComponent<AnimatorComponent>()->Play("sample/walk.gltf", true, 1.0f);
            }
            if (ImGui::Button("Crossfade to SneakWalk (1.0s)")) {
                blendTest_->GetComponent<AnimatorComponent>()->Play("sample/sneakWalk.gltf", true, 1.0f);
            }
        }
        ImGui::End();
    }

    if (isActiveLightningCrawl_ && lightningCylinder_) {
        lightningCylinder_->Debug("Lightning Cylinder");

        if (ImGui::Begin("Cylinder: Lightning Cylinder")) {
            ImGui::Separator();
            ImGui::Checkbox("Active Lightning Crawl", &isActiveLightningCrawl_);
            DebugUI::DebugLightning(lightningParamsData_);
        }
        ImGui::End();
    }
#endif
}
