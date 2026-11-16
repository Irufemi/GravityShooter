#include "Inspectors/ComponentEditorRegistry.h"

#ifdef EditorMode
#include <imgui/imgui.h>

// Core
#include "Core/IComponentEditor.h"
#include "UI/ComponentUIHelpers.h"

// Component Editors
#include "Inspectors/Transform/TransformComponentEditor.h"
#include "Inspectors/Rendering/MeshRendererComponentEditor.h"
#include "Inspectors/Rendering/SkinnedMeshRendererComponentEditor.h"
#include "Inspectors/Rendering/ModelBatchRendererComponentEditor.h"
#include "Inspectors/Rendering/PrimitiveRendererComponentEditor.h"
#include "Inspectors/Rendering/Primitive2DRendererComponentEditor.h"
#include "Inspectors/Rendering/SpriteRendererComponentEditor.h"
#include "Inspectors/Rendering/TextRendererComponentEditor.h"
#include "Inspectors/Physics/ColliderComponentEditors.h"
#include "Inspectors/Effects/ParticleEmitterComponentEditor.h"
#include "Inspectors/Effects/VoxelParticleComponentEditor.h"
#include "Inspectors/Effects/EffectMaskComponentEditor.h"
#include "Inspectors/Effects/GlobalPostProcessComponentEditor.h"
#include "Inspectors/Camera/CameraComponentEditor.h"

// Engine Components
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Framework/Component/Renderer/ModelBatchRendererComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/Component/Renderer/Primitive2DRendererComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/Component/Collider/AABBColliderComponent.h"
#include "Framework/Component/Collider/OBBColliderComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/Component/Effect/ParticleEmitterComponent.h"
#include "Framework/Component/Effect/VoxelParticleComponent.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Framework/Component/Effect/GlobalPostProcessComponent.h"
#include "Framework/Component/Camera/CameraComponent.h"

using namespace Irufemi;

// =======================================================================
// ComponentEditorRegistry
// =======================================================================

ComponentEditorRegistry::ComponentEditorRegistry() {}
ComponentEditorRegistry::~ComponentEditorRegistry() {}

void ComponentEditorRegistry::RegisterAllEditors() {
    RegisterEditor<TransformComponent, TransformComponentEditor>();
    RegisterEditor<MeshRendererComponent, MeshRendererComponentEditor>();
    RegisterEditor<SkinnedMeshRendererComponent, SkinnedMeshRendererComponentEditor>();
    RegisterEditor<ModelBatchRendererComponent, ModelBatchRendererComponentEditor>();
    RegisterEditor<PrimitiveRendererComponent, PrimitiveRendererComponentEditor>();
    RegisterEditor<Primitive2DRendererComponent, Primitive2DRendererComponentEditor>();
    RegisterEditor<SpriteRendererComponent, SpriteRendererComponentEditor>();
    RegisterEditor<TextRendererComponent, TextRendererComponentEditor>();
    RegisterEditor<AABBColliderComponent, AABBColliderComponentEditor>();
    RegisterEditor<OBBColliderComponent, OBBColliderComponentEditor>();
    RegisterEditor<SphereColliderComponent, SphereColliderComponentEditor>();
    RegisterEditor<ParticleEmitterComponent, ParticleEmitterComponentEditor>();
    RegisterEditor<VoxelParticleComponent, VoxelParticleComponentEditor>();
    RegisterEditor<EffectMaskComponent, EffectMaskComponentEditor>();
    RegisterEditor<GlobalPostProcessComponent, GlobalPostProcessComponentEditor>();
    RegisterEditor<CameraComponent, CameraComponentEditor>();
}

void ComponentEditorRegistry::DrawComponent(Component* component, EditorActionManager* actionManager) {
    if (!component) {
        return;
    }
    ImGui::PushID(component);
    auto it = editors_.find(typeid(*component));
    if (it != editors_.end()) {
        it->second->Draw(component, actionManager);
    } else {
        ComponentUIHelpers::DrawFallbackPropertiesGUI(component, actionManager);
    }
    ImGui::PopID();
}

#endif // EditorMode
