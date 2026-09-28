#include "Framework/Component/ComponentFactory.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/SkyDomeComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/Component/Renderer/Primitive2DRendererComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/Component/Renderer/ModelBatchRendererComponent.h"
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Framework/Component/Renderer/SkeletonDebugRendererComponent.h"
#include "Framework/Component/Logic/AnimatorComponent.h"
#include "Framework/Component/Logic/BoneAttachmentComponent.h"
#include "Framework/Component/Logic/SpawnPointComponent.h"
#include "Framework/Component/Effect/VoxelParticleComponent.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Framework/Component/Effect/ScreenEffectComponent.h"
#include "Framework/Component/Effect/GlobalPostProcessComponent.h"
#include "Framework/Component/Effect/FogComponent.h"
#include "Framework/Component/Collider/AABBColliderComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/Component/Collider/OBBColliderComponent.h"
#include "Framework/Component/Audio/AudioSourceComponent.h"
#include "Framework/Component/Effect/ParticleEmitterComponent.h"
#include "Framework/Component/Effect/ParticleFieldComponent.h"
#include "Renderer/Object/Line/LineClass.h"
#include "Framework/Component/UI/ButtonComponent.h"
#include "Framework/Component/UI/SliderComponent.h"
#include "Framework/Component/UI/CanvasComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"
#include "Framework/Component/Camera/CameraShakeComponent.h"
#include "Framework/Component/Camera/TargetFollowComponent.h"
#include "Framework/Component/Utility/LifetimeComponent.h"
#include "Framework/Component/Utility/SplineComponent.h"
#include "Framework/Component/Utility/SplineNodeComponent.h"
#include "Framework/Component/VirtualEntity/VirtualEntityManagerComponent.h"
#include "Core/System/ComponentPool.h"
#include "Core/Utility/Log.h"
#include <iostream>

std::unordered_map<std::string, ComponentFactory::ComponentRegistration>& ComponentFactory::GetMap() {
    static std::unordered_map<std::string, ComponentRegistration> factoryMap;
    return factoryMap;
}

void ComponentFactory::Register(const std::string& typeName, const char* category, CreatorFunc func) {
    GetMap()[typeName] = {category, func};
}

std::shared_ptr<Component> ComponentFactory::Create(const std::string& typeName) {
    auto& map = GetMap();
    auto it = map.find(typeName);
    if (it != map.end()) {
        return it->second.creator();
    }
    Log::OutPutLog(std::cerr, "[ComponentFactory] Error: Component type '" + typeName + "' is not registered.\n");
    return nullptr;
}

const std::unordered_map<std::string, ComponentFactory::ComponentRegistration>& ComponentFactory::GetFactoryMap() {
    return GetMap();
}

void ComponentFactory::RegisterAllCoreComponents() {
    Register("TransformComponent", "Core", []() {
        if constexpr (IsPooledComponent<TransformComponent>::value) {
            return std::static_pointer_cast<Component>(ComponentPool<TransformComponent>::GetInstance().Create());
        } else {
            return std::static_pointer_cast<Component>(std::make_shared<TransformComponent>());
        }
    });
    Register("MeshRendererComponent", "Renderer", []() { return std::make_shared<MeshRendererComponent>(); });
    Register("SkyDomeComponent", "Renderer", []() { return std::make_shared<SkyDomeComponent>(); });
    Register("PrimitiveRendererComponent", "Renderer", []() { return std::make_shared<PrimitiveRendererComponent>(); });
    Register("Primitive2DRendererComponent", "Renderer",
             []() { return std::make_shared<Primitive2DRendererComponent>(); });
    Register("ModelBatchRendererComponent", "Renderer",
             []() { return std::make_shared<ModelBatchRendererComponent>(); });
    Register("SpriteRendererComponent", "Renderer", []() { return std::make_shared<SpriteRendererComponent>(); });
    Register("TextRendererComponent", "Renderer", []() { return std::make_shared<TextRendererComponent>(); });
    Register("SkinnedMeshRendererComponent", "Renderer",
             []() { return std::make_shared<SkinnedMeshRendererComponent>(); });
    Register("SkeletonDebugRendererComponent", "Debug",
             []() { return std::make_shared<SkeletonDebugRendererComponent>(); });
    Register("AnimatorComponent", "Logic", []() { return std::make_shared<AnimatorComponent>(); });
    Register("BoneAttachmentComponent", "Logic", []() { return std::make_shared<BoneAttachmentComponent>(); });
    Register("SpawnPointComponent", "Logic", []() { return std::make_shared<SpawnPointComponent>(); });
    Register("VoxelParticleComponent", "Effect", []() { return std::make_shared<VoxelParticleComponent>(); });
    Register("EffectMaskComponent", "Effect", []() { return std::make_shared<EffectMaskComponent>(); });
    Register("ScreenEffectComponent", "Effect", []() { return std::make_shared<ScreenEffectComponent>(); });
    Register("GlobalPostProcessComponent", "Effect", []() { return std::make_shared<GlobalPostProcessComponent>(); });
    Register("FogComponent", "Effect", []() { return std::make_shared<FogComponent>(); });
    Register("AABBColliderComponent", "Collider", []() { return std::make_shared<AABBColliderComponent>(); });
    Register("SphereColliderComponent", "Collider", []() { return std::make_shared<SphereColliderComponent>(); });
    Register("OBBColliderComponent", "Collider", []() { return std::make_shared<OBBColliderComponent>(); });
    Register("AudioSourceComponent", "Audio", []() { return std::make_shared<AudioSourceComponent>(); });
    Register("ParticleEmitterComponent", "Effect", []() { return std::make_shared<ParticleEmitterComponent>(); });
    Register("ParticleFieldComponent", "Effect", []() { return std::make_shared<ParticleFieldComponent>(); });
    Register("ButtonComponent", "UI", []() { return std::make_shared<ButtonComponent>(); });
    Register("SliderComponent", "UI", []() { return std::make_shared<SliderComponent>(); });
    Register("CanvasComponent", "UI", []() { return std::make_shared<CanvasComponent>(); });
    Register("CameraComponent", "Camera", []() { return std::make_shared<CameraComponent>(); });
    Register("CameraShakeComponent", "Camera", []() { return std::make_shared<CameraShakeComponent>(); });
    Register("TargetFollowComponent", "Camera", []() { return std::make_shared<TargetFollowComponent>(); });
    Register("LifetimeComponent", "Utility", []() { return std::make_shared<LifetimeComponent>(); });
    Register("SplineComponent", "Utility", []() { return std::make_shared<SplineComponent>(); });
    Register("SplineNodeComponent", "Utility", []() { return std::make_shared<SplineNodeComponent>(); });
    Register("VirtualEntityManagerComponent", "Utility",
             []() { return std::make_shared<VirtualEntityManagerComponent>(); });
}
