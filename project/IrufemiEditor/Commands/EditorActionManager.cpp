#include "Commands/EditorActionManager.h"

#ifdef EditorMode
#include "Core/EditorManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Scene/SceneSerializer.h"

#include "Commands/EditorCommands.h"
#include <filesystem>
#include <algorithm>
#include <limits>

EditorActionManager::EditorActionManager(EditorManager* editor) : editorManager_(editor) {}

void EditorActionManager::PushAndExecute(std::unique_ptr<ICommand> command) {
    if (!command) {
        return;
    }

    // 操作の実行
    command->Do();

    // Undoスタックへ追加
    undoStack_.push_back(std::move(command));

    // 新しい操作をしたのでRedoスタックはクリア
    redoStack_.clear();

    // 履歴制限
    if (undoStack_.size() > maxHistory_) {
        undoStack_.pop_front();
    }
}

void EditorActionManager::Undo() {
    if (undoStack_.empty()) {
        return;
    }

    // Undoスタックから取り出してUndo実行
    auto command = std::move(undoStack_.back());
    undoStack_.pop_back();

    command->Undo();

    // Redoスタックへ移動
    redoStack_.push_back(std::move(command));
}

void EditorActionManager::Redo() {
    if (redoStack_.empty()) {
        return;
    }

    // Redoスタックから取り出して再度実行
    auto command = std::move(redoStack_.back());
    redoStack_.pop_back();

    command->Do();

    // Undoスタックへ戻す
    undoStack_.push_back(std::move(command));
}

void EditorActionManager::ClearHistory() {
    undoStack_.clear();
    redoStack_.clear();
}

void EditorActionManager::CreateObjectFromAsset(const std::string& assetPath, const Irufemi::Vector3& position) {
    if (!editorManager_) {
        return;
    }
    auto* engine = editorManager_->GetEngine();
    if (!engine || !engine->GetSceneManager()) {
        return;
    }

    auto* baseScene = dynamic_cast<BaseScene*>(engine->GetSceneManager()->GetCurrentScene());
    if (!baseScene) {
        return;
    }

    std::filesystem::path droppedPath(reinterpret_cast<const char8_t*>(assetPath.c_str()));
    std::string ext = droppedPath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    std::shared_ptr<GameObject> newObj = nullptr;
    std::string stemString = reinterpret_cast<const char*>(droppedPath.stem().u8string().c_str());

    if (ext == ".png" || ext == ".jpg" || ext == ".dds" || ext == ".bmp") {
        newObj = std::make_shared<GameObject>("Sprite_" + stemString);
        auto transform = newObj->GetTransform();
        transform->SetPosition(position);
        auto spriteRenderer = newObj->AddComponent<SpriteRendererComponent>();
        spriteRenderer->SetTexture(assetPath);
        newObj->Initialize();
    } else if (ext == ".obj" || ext == ".gltf" || ext == ".fbx" || ext == ".glb") {
        newObj = std::make_shared<GameObject>("Model_" + stemString);
        auto transform = newObj->GetTransform();
        transform->SetPosition(position);
        auto meshRenderer = newObj->AddComponent<MeshRendererComponent>();

        // 同名ファイルに対応するため、ファイル名だけでなく相対パスを渡す
        std::string modelName = assetPath;
        std::replace(modelName.begin(), modelName.end(), '\\', '/');
        std::string lowerPath = modelName;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
        if (lowerPath.find("resources/model/") == 0) {
            modelName = modelName.substr(16);
        }
        meshRenderer->LoadModel(modelName);
        newObj->Initialize();
    } else if (ext == ".json" || ext == ".prefab") {
        newObj = SceneSerializer::LoadPrefab(assetPath);
        if (newObj) {
            if (auto transform = newObj->GetComponent<TransformComponent>()) {
                transform->SetPosition(position);
            }
        }
    }

    if (newObj) {
        newObj->SetIsSerializable(true); // エディタから生成されたものは保存対象

        // PrefabMode 中なら、生成したオブジェクトを Prefab の階層下（選択中オブジェクトまたはPrefabルート）に自動配置
        std::shared_ptr<GameObject> parent = nullptr;
        if (editorManager_->IsPrefabMode()) {
            if (auto selected = editorManager_->GetSelectedObject()) {
                parent = selected;
            } else {
                parent = editorManager_->GetEditingPrefabRoot();
            }
        }

        PushAndExecute(std::make_unique<CreateObjectCommand>(baseScene, newObj, parent));
        editorManager_->SetSelectedObject(newObj);
    }
}

void EditorActionManager::CreatePrimitiveObject(const std::string& typeName) {
    if (!editorManager_) {
        return;
    }
    auto* engine = editorManager_->GetEngine();
    if (!engine || !engine->GetSceneManager()) {
        return;
    }

    auto* baseScene = dynamic_cast<BaseScene*>(engine->GetSceneManager()->GetCurrentScene());
    if (!baseScene) {
        return;
    }

    std::shared_ptr<GameObject> obj = nullptr;

    if (typeName == "Empty") {
        obj = std::make_shared<GameObject>("Empty Object");
        obj->GetTransform();
        obj->Initialize();
    } else if (typeName == "Cube" || typeName == "Sphere" || typeName == "Cylinder" || typeName == "Plane") {
        obj = std::make_shared<GameObject>(typeName);
        obj->GetTransform();
        auto renderer = obj->AddComponent<PrimitiveRendererComponent>();
        if (typeName == "Cube") {
            renderer->SetShape(Irufemi::PrimitiveType::Cube);
        } else if (typeName == "Sphere") {
            renderer->SetShape(Irufemi::PrimitiveType::Sphere);
        } else if (typeName == "Cylinder") {
            renderer->SetShape(Irufemi::PrimitiveType::Cylinder);
        } else if (typeName == "Plane") {
            renderer->SetShape(Irufemi::PrimitiveType::Plane);
        }
        obj->Initialize();
    } else if (typeName == "Model") {
        obj = std::make_shared<GameObject>("Model");
        obj->GetTransform();
        obj->AddComponent<MeshRendererComponent>();
        obj->Initialize();
    } else if (typeName == "Sprite") {
        obj = std::make_shared<GameObject>("Sprite");
        obj->GetTransform();
        auto spriteRenderer = obj->AddComponent<SpriteRendererComponent>();
        obj->GetComponent<TransformComponent>()->SetPosition({640.0f, 360.0f, 0.0f});
        obj->Initialize();
    }

    if (obj) {
        obj->SetIsSerializable(true); // エディタから生成されたものは保存対象

        // PrefabMode 中なら、生成したオブジェクトを Prefab の階層下（選択中オブジェクトまたはPrefabルート）に自動配置
        std::shared_ptr<GameObject> parent = nullptr;
        if (editorManager_->IsPrefabMode()) {
            if (auto selected = editorManager_->GetSelectedObject()) {
                parent = selected;
            } else {
                parent = editorManager_->GetEditingPrefabRoot();
            }
        }

        PushAndExecute(std::make_unique<CreateObjectCommand>(baseScene, obj, parent));
        editorManager_->SetSelectedObject(obj);
    }
}

void EditorActionManager::DuplicateObject(std::shared_ptr<GameObject> target) {
    if (!target || !editorManager_) {
        return;
    }

    // PrefabMode 中のルートオブジェクトは複製不可（単一ルート維持のため）
    if (editorManager_->IsPrefabMode() && target == editorManager_->GetEditingPrefabRoot()) {
        Log::OutPutLog(std::cout, "Cannot duplicate the root GameObject in Prefab Mode.");
        return;
    }
    auto* engine = editorManager_->GetEngine();
    if (!engine || !engine->GetSceneManager()) {
        return;
    }

    auto* baseScene = dynamic_cast<BaseScene*>(engine->GetSceneManager()->GetCurrentScene());
    if (!baseScene) {
        return;
    }

    auto clone = target->Clone();

    // クローンの場合は親をセットしてから生成コマンドを発行する
    auto parent = target->GetParent();
    size_t index = (std::numeric_limits<size_t>::max)();
    if (parent) {
        index = parent->GetChildIndex(target) + 1; // オリジナルの次に入れる
    } else {
        index = baseScene->GetGameObjectIndex(target) + 1;
    }

    PushAndExecute(std::make_unique<CreateObjectCommand>(baseScene, clone, parent, index));
    editorManager_->SetSelectedObject(clone);
}

void EditorActionManager::DeleteObject(std::shared_ptr<GameObject> target) {
    if (!target || !editorManager_) {
        return;
    }

    // PrefabMode 中の Prefab ルートオブジェクトは削除不可（ステージ破綻防止）
    if (editorManager_->IsPrefabMode() && target == editorManager_->GetEditingPrefabRoot()) {
        Log::OutPutLog(std::cout, "Cannot delete the root GameObject in Prefab Mode.");
        return;
    }
    auto* engine = editorManager_->GetEngine();
    if (!engine || !engine->GetSceneManager()) {
        return;
    }

    auto* baseScene = dynamic_cast<BaseScene*>(engine->GetSceneManager()->GetCurrentScene());
    if (!baseScene) {
        return;
    }

    // 削除コマンドの発行
    PushAndExecute(std::make_unique<DeleteObjectCommand>(baseScene, target));

    // 削除されたものが選択されていたら解除
    if (editorManager_->GetSelectedObject() == target) {
        editorManager_->ClearSelectedObject();
    }
}

#endif // EditorMode
