#include "Renderer/Object/Particle/ParticleObject.h"
#include "Renderer/System/ParticleGPU/GPUParticleManager.h"
#include "Resource/Model/ModelManager.h"
#include "Renderer/System/Core/BaseModel.h"
#include "Framework/Component/Component.h"
#include <fstream>
#include <iostream>
#include "Core/Utility/Log.h"

TextureManager* ParticleObject::textureManager_ = nullptr;

ParticleObject::ParticleObject() {}

ParticleObject::~ParticleObject() {
    if (emitterHandle_.IsValid() && gpuParticleManager_) {
        gpuParticleManager_->UnregisterEmitter(emitterHandle_);
    }
}

void ParticleObject::Initialize() {
    if (emitOnAwake_) {
        Play();
    }
    if (burstCountOnAwake_ > 0) {
        EmitBurst(burstCountOnAwake_);
    }

    // 実行時のラグ（Hitch）を防ぐため、初期化時に強制的にGPUバッファをプレウォーム（事前確保）する
    PrewarmSystem();
}

void ParticleObject::PrewarmSystem() {
    // パラメータは送信せず、単にマネージャーにテクスチャ等の登録（GPUバッファの確保）だけを依頼する
    if (!emitterHandle_.IsValid() && gpuParticleManager_) {
        emitterHandle_ = gpuParticleManager_->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_,
                                                              enableLighting_, depthWrite_);
    }
}

void ParticleObject::Play() {
    isPlaying_ = true;
    MarkDirty();
}

void ParticleObject::Restart() {
    Play();
    if (burstCountOnAwake_ > 0) {
        EmitBurst(burstCountOnAwake_);
    }
    // 1フレームのズレを防ぐため、GPUマネージャーへ最新情報を即座に送信する
    UpdateSystem();
    isDirty_ = false;
}

void ParticleObject::Stop() {
    isPlaying_ = false;
    MarkDirty();
}

void ParticleObject::EmitBurst(int count) {
    burstCountPending_ += count;
    Play();
}

void ParticleObject::Update() {
    if (isDirty_ || burstCountPending_ > 0) {
        UpdateSystem();
        isDirty_ = false;
    }
}

void ParticleObject::SetTexturePath(const std::string& path) {
    if (texturePath_ != path) {
        texturePath_ = path;
        if (emitterHandle_.IsValid() && gpuParticleManager_) {
            gpuParticleManager_->UnregisterEmitter(emitterHandle_);
            emitterHandle_ = gpuParticleManager_->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_,
                                                                  enableLighting_, depthWrite_);
        }
        MarkDirty();
    }
}

void ParticleObject::SetBlendMode(Irufemi::BlendMode mode) {
    if (blendMode_ != mode) {
        blendMode_ = mode;
        if (emitterHandle_.IsValid() && gpuParticleManager_) {
            gpuParticleManager_->UnregisterEmitter(emitterHandle_);
            emitterHandle_ = gpuParticleManager_->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_,
                                                                  enableLighting_, depthWrite_);
        }
        MarkDirty();
    }
}

void ParticleObject::SetDepthWrite(PSOManager::DepthWrite depthWrite) {
    if (depthWrite_ != depthWrite) {
        depthWrite_ = depthWrite;
        if (emitterHandle_.IsValid() && gpuParticleManager_) {
            gpuParticleManager_->UnregisterEmitter(emitterHandle_);
            emitterHandle_ = gpuParticleManager_->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_,
                                                                  enableLighting_, depthWrite_);
        }
        MarkDirty();
    }
}

void ParticleObject::SetEnableLighting(bool val) {
    if (enableLighting_ != val) {
        enableLighting_ = val;
        if (emitterHandle_.IsValid() && gpuParticleManager_) {
            gpuParticleManager_->UnregisterEmitter(emitterHandle_);
            emitterHandle_ = gpuParticleManager_->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_,
                                                                  enableLighting_, depthWrite_);
        }
        MarkDirty();
    }
}

void ParticleObject::SetUnscaledTime(bool val) {
    if (isUnscaledTime_ != val) {
        isUnscaledTime_ = val;
        if (emitterHandle_.IsValid() && gpuParticleManager_) {
            gpuParticleManager_->UnregisterEmitter(emitterHandle_);
            emitterHandle_ = gpuParticleManager_->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_,
                                                                  enableLighting_, depthWrite_);
        }
        MarkDirty();
    }
}

void ParticleObject::SetEmitterModelPath(const std::string& path) {
    if (emitterModelPath_ != path) {
        emitterModelPath_ = path;
        emitterModelHandle_ = ResourceHandle(); // Reset the cached handle
        MarkDirty();
    }
}

void ParticleObject::UpdateSystem() {
    if (!emitterHandle_.IsValid() && gpuParticleManager_) {
        emitterHandle_ = gpuParticleManager_->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_,
                                                              enableLighting_, depthWrite_);
    }

    GPUParticleEmitter data;
    data.emit = isPlaying_ ? 1 : 0;

    data.type = emitType_;
    data.translateX = position_.x;
    data.translateY = position_.y;
    data.translateZ = position_.z;
    data.emissionRate = emissionRate_;
    data.minLife = lifeTimeMin_;
    data.maxLife = lifeTimeMax_;
    data.velocity = velocity_;
    data.radius = radius_;
    data.spread = spread_;
    data.gravity = gravity_;
    data.damping = damping_;

    data.startColorMinR = color_.x;
    data.startColorMinG = color_.y;
    data.startColorMinB = color_.z;
    data.startColorMinA = color_.w;
    data.startColorMaxR = color_.x;
    data.startColorMaxG = color_.y;
    data.startColorMaxB = color_.z;
    data.startColorMaxA = color_.w;

    data.midColorMinR = midColor_.x;
    data.midColorMinG = midColor_.y;
    data.midColorMinB = midColor_.z;
    data.midColorMinA = midColor_.w;
    data.midColorMaxR = midColor_.x;
    data.midColorMaxG = midColor_.y;
    data.midColorMaxB = midColor_.z;
    data.midColorMaxA = midColor_.w;

    data.endColorMinR = color_.x;
    data.endColorMinG = color_.y;
    data.endColorMinB = color_.z;
    data.endColorMinA = 0.0f;
    data.endColorMaxR = color_.x;
    data.endColorMaxG = color_.y;
    data.endColorMaxB = color_.z;
    data.endColorMaxA = 0.0f;

    data.startScaleMinX = startScale_.x;
    data.startScaleMinY = startScale_.y;
    data.startScaleMinZ = startScale_.z;
    data.startScaleMaxX = startScale_.x;
    data.startScaleMaxY = startScale_.y;
    data.startScaleMaxZ = startScale_.z;

    data.midScaleMinX = midScale_.x;
    data.midScaleMinY = midScale_.y;
    data.midScaleMinZ = midScale_.z;
    data.midScaleMaxX = midScale_.x;
    data.midScaleMaxY = midScale_.y;
    data.midScaleMaxZ = midScale_.z;

    data.endScaleMinX = endScale_.x;
    data.endScaleMinY = endScale_.y;
    data.endScaleMinZ = endScale_.z;
    data.endScaleMaxX = endScale_.x;
    data.endScaleMaxY = endScale_.y;
    data.endScaleMaxZ = endScale_.z;

    data.midPoint = midPoint_;

    data.directionX = direction_.x;
    data.directionY = direction_.y;
    data.directionZ = direction_.z;

    data.areaSizeX = areaSize_.x;
    data.areaSizeY = areaSize_.y;
    data.areaSizeZ = areaSize_.z;

    data.bounce = bounce_;
    data.groundHeight = groundHeight_;

    data.attractorStrength = attractorStrength_;
    data.attractorPosX = attractorPos_.x;
    data.attractorPosY = attractorPos_.y;
    data.attractorPosZ = attractorPos_.z;

    data.atlasRows = atlasRows_;
    data.atlasCols = atlasCols_;
    data.billboardMode = billboardMode_;
    data.jitter = jitter_;

    data.enableTrail = enableTrail_ ? 1 : 0;
    data.trailFrequency = trailFrequency_;
    data.showDebugArea = showDebugArea_ ? 1 : 0;
    data.enableDeathEmit = enableDeathEmit_ ? 1 : 0;
    data.enableRandomRotation = enableRandomRotation_ ? 1 : 0;

    if (emitType_ == 6) {
        if (!emitterModelHandle_.IsValid() && !emitterModelPath_.empty()) {
            if (modelManager_) {
                emitterModelHandle_ = modelManager_->LoadModel(emitterModelPath_);
            }
        }
        if (emitterModelHandle_.IsValid() && modelManager_) {
            auto cachedModel_ = modelManager_->Resolve(emitterModelHandle_);
            if (cachedModel_) {
                auto cpuModel = cachedModel_->cpuModel;
                if (cpuModel && !cpuModel->meshes.empty() && !cachedModel_->gpuMeshes.empty()) {
                    auto& mesh = cpuModel->meshes[0];
                    auto gpuMesh = cachedModel_->gpuMeshes[0];
                    if (gpuParticleManager_ && gpuMesh->vertexResource) {
                        gpuParticleManager_->SetMeshEmitterBuffer(emitterHandle_,
                                                                  gpuMesh->vertexResource->GetGPUVirtualAddress());
                    }
                    data.padFreqTime = static_cast<float>(mesh.vertices.size());
                }
            }
        }
    }

    if (burstCountPending_ > 0) {
        data.burstCount = burstCountPending_;
        burstCountPending_ = 0;
    }
    if (gpuParticleManager_) {
        gpuParticleManager_->UpdateEmitterData(emitterHandle_, data);
    }
}

void ParticleObject::Serialize(nlohmann::json& j) const {
    j["texturePath"] = texturePath_;
    j["blendMode"] = static_cast<int>(blendMode_);
    j["depthWrite"] = static_cast<int>(depthWrite_);
    j["enableLighting"] = enableLighting_;
    j["isUnscaledTime"] = isUnscaledTime_;
    j["emitOnAwake"] = emitOnAwake_;

    if (emitType_ != 0) {
        j["emitType"] = emitType_;
    }
    if (emissionRate_ != 50.0f) {
        j["emissionRate"] = emissionRate_;
    }
    if (lifeTimeMin_ != 0.5f) {
        j["lifeTimeMin"] = lifeTimeMin_;
    }
    if (lifeTimeMax_ != 1.0f) {
        j["lifeTimeMax"] = lifeTimeMax_;
    }
    if (velocity_ != 1.0f) {
        j["velocity"] = velocity_;
    }
    if (radius_ != 0.0f) {
        j["radius"] = radius_;
    }
    if (spread_ != 0.1f) {
        j["spread"] = spread_;
    }

    if (atlasRows_ != 1) {
        j["atlasRows"] = atlasRows_;
    }
    if (atlasCols_ != 1) {
        j["atlasCols"] = atlasCols_;
    }

    if (gravity_ != 0.0f) {
        j["gravity"] = gravity_;
    }
    if (damping_ != 0.0f) {
        j["damping"] = damping_;
    }
    if (bounce_ != 0.0f) {
        j["bounce"] = bounce_;
    }
    if (groundHeight_ != -100.0f) {
        j["groundHeight"] = groundHeight_;
    }
    if (attractorStrength_ != 0.0f) {
        j["attractorStrength"] = attractorStrength_;
    }
    if (attractorPos_.x != 0.0f || attractorPos_.y != 0.0f || attractorPos_.z != 0.0f) {
        j["attractorPos"] = {attractorPos_.x, attractorPos_.y, attractorPos_.z};
    }
    if (jitter_ != 0.0f) {
        j["jitter"] = jitter_;
    }

    if (enableTrail_) {
        j["enableTrail"] = enableTrail_;
    }
    if (trailFrequency_ != 0.05f) {
        j["trailFrequency"] = trailFrequency_;
    }
    if (!showDebugArea_) {
        j["showDebugArea"] = showDebugArea_;
    }
    if (enableDeathEmit_) {
        j["enableDeathEmit"] = enableDeathEmit_;
    }
    if (enableRandomRotation_) {
        j["enableRandomRotation"] = enableRandomRotation_;
    }

    if (billboardMode_ != 1) {
        j["billboardMode"] = billboardMode_;
    }
    if (burstCountOnAwake_ != 0) {
        j["burstCountOnAwake"] = burstCountOnAwake_;
    }
    if (color_.x != 1.0f || color_.y != 1.0f || color_.z != 1.0f || color_.w != 1.0f) {
        j["color"] = {color_.x, color_.y, color_.z, color_.w};
    }
    if (midColor_.x != 1.0f || midColor_.y != 1.0f || midColor_.z != 1.0f || midColor_.w != 1.0f) {
        j["midColor"] = {midColor_.x, midColor_.y, midColor_.z, midColor_.w};
    }
    if (startScale_.x != 1.0f || startScale_.y != 1.0f || startScale_.z != 1.0f) {
        j["startScale"] = {startScale_.x, startScale_.y, startScale_.z};
    }
    if (midScale_.x != 1.0f || midScale_.y != 1.0f || midScale_.z != 1.0f) {
        j["midScale"] = {midScale_.x, midScale_.y, midScale_.z};
    }
    if (endScale_.x != 0.0f || endScale_.y != 0.0f || endScale_.z != 0.0f) {
        j["endScale"] = {endScale_.x, endScale_.y, endScale_.z};
    }
    if (midPoint_ != 0.5f) {
        j["midPoint"] = midPoint_;
    }

    if (direction_.x != 0.0f || direction_.y != 0.0f || direction_.z != 0.0f) {
        j["direction"] = {direction_.x, direction_.y, direction_.z};
    }
    if (!emitterModelPath_.empty()) {
        j["emitterModelPath"] = emitterModelPath_;
    }
    if (areaSize_.x != 10.0f || areaSize_.y != 10.0f || areaSize_.z != 10.0f) {
        j["areaSize"] = {areaSize_.x, areaSize_.y, areaSize_.z};
    }
}

void ParticleObject::Deserialize(const nlohmann::json& j) {
    if (j.contains("texturePath")) {
        SetTexturePath(j["texturePath"]);
    }
    if (j.contains("blendMode")) {
        SetBlendMode(static_cast<Irufemi::BlendMode>(j["blendMode"].get<int>()));
    }
    if (j.contains("depthWrite")) {
        SetDepthWrite(static_cast<PSOManager::DepthWrite>(j["depthWrite"].get<int>()));
    }
    if (j.contains("enableLighting")) {
        SetEnableLighting(j["enableLighting"].get<bool>());
    }
    if (j.contains("isUnscaledTime")) {
        SetUnscaledTime(j["isUnscaledTime"].get<bool>());
    }
    if (j.contains("emitOnAwake")) {
        emitOnAwake_ = j["emitOnAwake"].get<bool>();
    }

    if (j.contains("emitType")) {
        emitType_ = j["emitType"].get<int>();
    }
    if (j.contains("emissionRate")) {
        emissionRate_ = j["emissionRate"].get<float>();
    }
    if (j.contains("lifeTimeMin")) {
        lifeTimeMin_ = j["lifeTimeMin"].get<float>();
    }
    if (j.contains("lifeTimeMax")) {
        lifeTimeMax_ = j["lifeTimeMax"].get<float>();
    }
    if (j.contains("velocity")) {
        velocity_ = j["velocity"].get<float>();
    }
    if (j.contains("radius")) {
        radius_ = j["radius"].get<float>();
    }
    if (j.contains("spread")) {
        spread_ = j["spread"].get<float>();
    }

    if (j.contains("atlasRows")) {
        atlasRows_ = j["atlasRows"].get<int>();
    }
    if (j.contains("atlasCols")) {
        atlasCols_ = j["atlasCols"].get<int>();
    }

    if (j.contains("gravity")) {
        gravity_ = j["gravity"].get<float>();
    }
    if (j.contains("damping")) {
        damping_ = j["damping"].get<float>();
    }
    if (j.contains("bounce")) {
        bounce_ = j["bounce"].get<float>();
    }
    if (j.contains("groundHeight")) {
        groundHeight_ = j["groundHeight"].get<float>();
    }
    if (j.contains("attractorStrength")) {
        attractorStrength_ = j["attractorStrength"].get<float>();
    }
    if (j.contains("attractorPos") && j["attractorPos"].size() == 3) {
        attractorPos_ = {j["attractorPos"][0], j["attractorPos"][1], j["attractorPos"][2]};
    }
    if (j.contains("jitter")) {
        jitter_ = j["jitter"].get<float>();
    }

    if (j.contains("enableTrail")) {
        enableTrail_ = j["enableTrail"].get<bool>();
    }
    if (j.contains("trailFrequency")) {
        trailFrequency_ = j["trailFrequency"].get<float>();
    }
    if (j.contains("showDebugArea")) {
        showDebugArea_ = j["showDebugArea"].get<bool>();
    }
    if (j.contains("enableDeathEmit")) {
        enableDeathEmit_ = j["enableDeathEmit"].get<bool>();
    }
    if (j.contains("enableRandomRotation")) {
        enableRandomRotation_ = j["enableRandomRotation"].get<bool>();
    }

    if (j.contains("billboardMode")) {
        billboardMode_ = j["billboardMode"].get<int>();
    }
    if (j.contains("burstCountOnAwake")) {
        burstCountOnAwake_ = j["burstCountOnAwake"].get<int>();
    }
    if (j.contains("color") && j["color"].size() == 4) {
        color_ = {j["color"][0], j["color"][1], j["color"][2], j["color"][3]};
    }
    if (j.contains("midColor") && j["midColor"].size() == 4) {
        midColor_ = {j["midColor"][0], j["midColor"][1], j["midColor"][2], j["midColor"][3]};
    }
    if (j.contains("startScale") && j["startScale"].size() == 3) {
        startScale_ = {j["startScale"][0], j["startScale"][1], j["startScale"][2]};
    }
    if (j.contains("midScale") && j["midScale"].size() == 3) {
        midScale_ = {j["midScale"][0], j["midScale"][1], j["midScale"][2]};
    }
    if (j.contains("endScale") && j["endScale"].size() == 3) {
        endScale_ = {j["endScale"][0], j["endScale"][1], j["endScale"][2]};
    }
    if (j.contains("midPoint")) {
        midPoint_ = j["midPoint"].get<float>();
    }

    if (j.contains("direction")) {
        auto dir = j["direction"];
        direction_ = {dir[0], dir[1], dir[2]};
    }
    if (j.contains("emitterModelPath")) {
        emitterModelPath_ = j["emitterModelPath"].get<std::string>();
    }
    if (j.contains("areaSize") && j["areaSize"].size() == 3) {
        areaSize_ = {j["areaSize"][0], j["areaSize"][1], j["areaSize"][2]};
    }

    MarkDirty();
}

bool ParticleObject::LoadFromJson(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        /**
         * @brief エディタのコンソールパネルにも出力するため、Log::OutPutLog を使用
         */
        Log::OutPutLog(std::cerr, "Failed to open particle json: " + filepath);
        return false;
    }

    nlohmann::json j;
    try {
        file >> j;
        Deserialize(j);
        return true;
    } catch (const nlohmann::json::exception& e) {
        /**
         * @brief エディタのコンソールパネルにも出力するため、Log::OutPutLog を使用
         */
        Log::OutPutLog(std::cerr, "JSON parse error in " + filepath + ": " + std::string(e.what()));
        return false;
    }
}

void ParticleObject::CopyFrom(const ParticleObject& other) {
    position_ = other.position_;
    rotation_ = other.rotation_;

    texturePath_ = other.texturePath_;
    blendMode_ = other.blendMode_;
    depthWrite_ = other.depthWrite_;
    enableLighting_ = other.enableLighting_;
    isUnscaledTime_ = other.isUnscaledTime_;
    emitOnAwake_ = other.emitOnAwake_;
    burstCountOnAwake_ = other.burstCountOnAwake_;

    emitterModelPath_ = other.emitterModelPath_;
    emitterModelHandle_ = ResourceHandle();

    emitType_ = other.emitType_;
    emissionRate_ = other.emissionRate_;
    lifeTimeMin_ = other.lifeTimeMin_;
    lifeTimeMax_ = other.lifeTimeMax_;
    velocity_ = other.velocity_;
    radius_ = other.radius_;
    spread_ = other.spread_;

    atlasRows_ = other.atlasRows_;
    atlasCols_ = other.atlasCols_;

    gravity_ = other.gravity_;
    damping_ = other.damping_;
    bounce_ = other.bounce_;
    groundHeight_ = other.groundHeight_;
    attractorStrength_ = other.attractorStrength_;
    attractorPos_ = other.attractorPos_;
    jitter_ = other.jitter_;
    enableTrail_ = other.enableTrail_;
    trailFrequency_ = other.trailFrequency_;
    enableDeathEmit_ = other.enableDeathEmit_;
    enableRandomRotation_ = other.enableRandomRotation_;
    showDebugArea_ = other.showDebugArea_;

    billboardMode_ = other.billboardMode_;
    color_ = other.color_;
    midColor_ = other.midColor_;
    startScale_ = other.startScale_;
    midScale_ = other.midScale_;
    endScale_ = other.endScale_;
    midPoint_ = other.midPoint_;

    direction_ = other.direction_;
    areaSize_ = other.areaSize_;

    if (emitterHandle_.IsValid() && gpuParticleManager_) {
        gpuParticleManager_->UnregisterEmitter(emitterHandle_);
        emitterHandle_ = GPUParticleManager::EmitterHandle();
    }

    isPlaying_ = other.isPlaying_;
    burstCountPending_ = other.burstCountPending_;
    isDirty_ = true;
}

void ParticleObject::RegisterProperties(Component* comp) {
    if (!comp) {
        return;
    }

    comp->RegisterHeader("Emitter Basics");
    comp->RegisterProperty("Emit On Awake", &emitOnAwake_);
    comp->RegisterProperty("Burst Count On Awake", &burstCountOnAwake_);
    comp->RegisterEnum("Emit Type", &emitType_, {"0: Sphere", "1: Beam", "2: Box", "3: Cylinder"});
    comp->RegisterPropertyRange("Emission Rate", &emissionRate_, 0.0f, 1000.0f);
    comp->RegisterPropertyRange("Life Time Min", &lifeTimeMin_, 0.01f, 10.0f);
    comp->RegisterPropertyRange("Life Time Max", &lifeTimeMax_, 0.01f, 10.0f);
    comp->RegisterPropertyRange("Velocity", &velocity_, 0.0f, 100.0f);
    comp->RegisterPropertyRange("Radius", &radius_, 0.0f, 100.0f);
    comp->RegisterPropertyRange("Spread", &spread_, 0.0f, 1.0f);
    comp->RegisterProperty("Direction", &direction_);
    comp->RegisterProperty("Area Size", &areaSize_);

    comp->RegisterHeader("Physics & Movement");
    comp->RegisterPropertyRange("Gravity", &gravity_, -100.0f, 100.0f).SetTooltip("重力（マイナスで上方向へ移動）");
    comp->RegisterPropertyRange("Damping", &damping_, 0.0f, 1.0f).SetTooltip("空気抵抗（1.0に近いほど減速しやすい）");
    comp->RegisterPropertyRange("Bounce", &bounce_, 0.0f, 1.0f)
        .SetTooltip("地面への反発係数（Ground Heightでバウンドします）");
    comp->RegisterProperty("Ground Height", &groundHeight_);
    comp->RegisterPropertyRange("Jitter", &jitter_, 0.0f, 10.0f)
        .SetTooltip("発生位置のランダムな揺らぎ（ノイズ）の強さ");
    comp->RegisterPropertyRange("Attractor Strength", &attractorStrength_, -100.0f, 100.0f);
    comp->RegisterProperty("Attractor Pos", &attractorPos_);

    comp->RegisterHeader("Visuals");
    comp->RegisterProperty("Texture Path", &texturePath_);
    comp->RegisterEnum("Billboard Mode", &billboardMode_, {"0: None", "1: Billboard", "2: Velocity Billboard"});
    comp->RegisterEnum("Blend Mode", reinterpret_cast<int*>(&blendMode_),
                       {"0: None", "1: Normal (Alpha)", "2: Add", "3: Subtract", "4: Multiply", "5: Screen",
                        "6: Premultiplied"}); // BlendMode enum

    comp->RegisterProperty("Color", &color_);
    comp->RegisterProperty("Mid Color", &midColor_);
    comp->RegisterProperty("Start Scale", &startScale_);
    comp->RegisterProperty("Mid Scale", &midScale_);
    comp->RegisterProperty("End Scale", &endScale_);
    comp->RegisterPropertyRange("Mid Point", &midPoint_, 0.0f, 1.0f)
        .SetTooltip("寿命の中で中間スケール・カラーに到達するタイミング（0.0～1.0）");

    comp->RegisterHeader("Special Features");
    comp->RegisterProperty("Show Debug Area", &showDebugArea_)
        .SetTooltip("エディタ上でパーティクルの発生範囲をワイヤーフレーム表示するか");
    comp->RegisterProperty("Enable Trail", &enableTrail_).SetTooltip("パーティクルの軌跡を描画するか");
    comp->RegisterPropertyRange("Trail Frequency", &trailFrequency_, 0.01f, 1.0f)
        .SetTooltip("軌跡（トレイル）を生成する間隔の頻度");
    comp->RegisterProperty("Enable Death Emit", &enableDeathEmit_).SetTooltip("消滅時に別のパーティクルを発生させるか");
    comp->RegisterProperty("Enable Random Rotation", &enableRandomRotation_)
        .SetTooltip("発生時にランダムな回転角を与えるか");
}
#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include "Resource/Texture/TextureManager.h"

void ParticleObject::DebugUI(const char* name) {
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen)) {
        bool changed = false;

        if (ImGui::TreeNodeEx("General", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Texture Manager Combo
            auto* textureManager = GetTextureManager();
            if (textureManager && !textureManager->GetTextureNamesForDebug().empty()) {
                auto textureNames = textureManager->GetTextureNamesForDebug();
                std::vector<const char*> namesCStr;
                int currentIndex = -1;
                for (int i = 0; i < textureNames.size(); ++i) {
                    namesCStr.push_back(textureNames[i].c_str());
                    if (texturePath_ == textureNames[i]) {
                        currentIndex = i;
                    }
                }
                if (currentIndex == -1) {
                    currentIndex = 0; // fallback
                }

                if (ImGui::Combo("Texture", &currentIndex, namesCStr.data(), (int)namesCStr.size())) {
                    if (texturePath_ != textureNames[currentIndex]) {
                        SetTexturePath(textureNames[currentIndex]);
                        changed = true;
                    }
                }
            }

            const char* blendNames[] = {"None", "Normal", "Add", "Subtract", "Multiply", "Screen", "Premultiplied"};
            int currentBlend = static_cast<int>(blendMode_);
            if (ImGui::Combo("Blend Mode", &currentBlend, blendNames, 7)) {
                SetBlendMode(static_cast<Irufemi::BlendMode>(currentBlend));
                changed = true;
            }

            bool lighting = enableLighting_;
            int currentDepthWrite = static_cast<int>(depthWrite_);
            const char* depthWriteNames[] = {"Enable", "Disable", "Off"};
            if (ImGui::Combo("Depth Write", &currentDepthWrite, depthWriteNames, IM_ARRAYSIZE(depthWriteNames))) {
                SetDepthWrite(static_cast<PSOManager::DepthWrite>(currentDepthWrite));
                changed = true;
            }

            if (ImGui::Checkbox("Enable Lighting", &enableLighting_)) {
                SetEnableLighting(lighting);
            }

            if (ImGui::Checkbox("Unscaled Time", &isUnscaledTime_)) {
                if (emitterHandle_.IsValid() && gpuParticleManager_) {
                    gpuParticleManager_->UnregisterEmitter(emitterHandle_);
                    emitterHandle_ = gpuParticleManager_->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_,
                                                                          enableLighting_, depthWrite_);
                }
                changed = true;
            }

            if (ImGui::Button("再生成")) {
                if (gpuParticleManager_ && emitterHandle_.IsValid()) {
                    gpuParticleManager_->UnregisterEmitter(emitterHandle_);
                    emitterHandle_ = gpuParticleManager_->RegisterEmitter(texturePath_, blendMode_, isUnscaledTime_,
                                                                          enableLighting_, depthWrite_);
                }
            }

            changed |= ImGui::Checkbox("Emit On Awake", &emitOnAwake_);

            ImGui::Separator();
            changed |= ImGui::DragInt("Atlas Rows", &atlasRows_, 1, 1, 16);
            changed |= ImGui::DragInt("Atlas Cols", &atlasCols_, 1, 1, 16);

            ImGui::Separator();
            if (ImGui::Button("Test Burst (50 particles)")) {
                EmitBurst(50);
            }
            ImGui::SameLine();
            if (ImGui::Button("Play")) {
                Play();
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop")) {
                Stop();
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* typeNames[] = {"Sphere", "Beam", "Ring", "Cylinder", "Box", "Hemisphere", "Mesh"};
            if (ImGui::Combo("Emit Type", &emitType_, typeNames, 7)) {
                changed = true;
            }
            if (emitType_ == 6) {
                // Mesh emitter settings
                char pathBuf[256];
                strncpy_s(pathBuf, emitterModelPath_.c_str(), sizeof(pathBuf));
                if (ImGui::InputText("Model Path", pathBuf, sizeof(pathBuf))) {
                    SetEmitterModelPath(pathBuf);
                }
            }
            const char* billboardNames[] = {"None", "Billboard", "Y-Axis"};
            changed |= ImGui::Combo("Billboard Mode", &billboardMode_, billboardNames, 3);

            ImGui::Separator();
            if (emitType_ == 2) { // Box
                changed |= ImGui::DragFloat3("Area Size", &areaSize_.x, 0.1f, 0.0f, 100.0f);
            } else {
                changed |= ImGui::DragFloat("Radius / Size", &radius_, 0.1f, 0.0f, 100.0f);
            }

            changed |= ImGui::DragFloat("Spread", &spread_, 0.01f, 0.0f, 1.0f);
            changed |= ImGui::DragFloat3("Direction", &direction_.x, 0.05f);

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Emission Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::DragFloat("Emission Rate (/sec)", &emissionRate_, 1.0f, 0.0f, 10000.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Physics & Kinetics", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::DragFloat("Velocity", &velocity_, 0.1f, 0.0f, 100.0f);
            changed |= ImGui::DragFloat("Gravity", &gravity_, 0.1f, -50.0f, 50.0f);
            changed |= ImGui::DragFloat("Damping", &damping_, 0.005f, 0.0f, 1.0f);
            changed |= ImGui::DragFloat("Bounce", &bounce_, 0.01f, 0.0f, 1.0f);
            changed |= ImGui::DragFloat("Ground Height", &groundHeight_, 0.1f, -100.0f, 100.0f);

            ImGui::Separator();
            changed |= ImGui::DragFloat("Attractor Strength", &attractorStrength_, 0.1f, -50.0f, 50.0f);
            changed |= ImGui::DragFloat3("Attractor Pos", &attractorPos_.x, 0.1f);

            ImGui::Separator();
            changed |= ImGui::DragFloat("Jitter", &jitter_, 0.01f, 0.0f, 10.0f);

            ImGui::Separator();
            changed |= ImGui::Checkbox("Enable Trail (Sparks)", &enableTrail_);
            if (enableTrail_) {
                changed |= ImGui::DragFloat("Trail Frequency", &trailFrequency_, 0.01f, 0.01f, 1.0f);
            }
            changed |= ImGui::Checkbox("Enable Death Emit", &enableDeathEmit_);
            changed |= ImGui::Checkbox("Enable Random Rotation", &enableRandomRotation_);

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Lifetime & Visuals", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::DragFloat("Life Min", &lifeTimeMin_, 0.05f, 0.01f, 10.0f);
            changed |= ImGui::DragFloat("Life Max", &lifeTimeMax_, 0.05f, 0.01f, 10.0f);

            ImGui::Separator();
            changed |= ImGui::ColorEdit4("Start Color", &color_.x);
            changed |= ImGui::ColorEdit4("Mid Color", &midColor_.x);

            ImGui::Separator();
            changed |= ImGui::DragFloat3("Start Scale", &startScale_.x, 0.05f);
            changed |= ImGui::DragFloat3("Mid Scale", &midScale_.x, 0.05f);
            changed |= ImGui::DragFloat3("End Scale", &endScale_.x, 0.05f);

            ImGui::Separator();
            changed |= ImGui::SliderFloat("Mid Point (0~1)", &midPoint_, 0.0f, 1.0f);

            ImGui::TreePop();
        }

        if (changed) {
            MarkDirty();
        }
    }
#endif
}
