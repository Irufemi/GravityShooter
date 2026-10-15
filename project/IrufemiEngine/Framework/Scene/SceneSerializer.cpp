#include "Framework/Scene/SceneSerializer.h"
#include "Framework/Scene/IScene.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Prefab/PrefabManager.h"
#include "Core/Utility/Log.h"
#include "Core/Utility/FileSystem.h"
#include "Core/Utility/JsonUtility.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

bool SceneSerializer::Save(IScene* scene, const std::string& sceneName) {
    if (!scene) {
        return false;
    }

    // シーン自身にシリアライズを委譲する
    nlohmann::json root = scene->Serialize();
    std::string pathStr = GetSceneFilePath(scene, sceneName);
    return Irufemi::JsonUtility::SaveToFile(pathStr, root, 2);
}

bool SceneSerializer::Load(IScene* scene, const std::string& sceneName) {
    if (!scene) {
        return false;
    }

    std::string pathStr = GetSceneFilePath(scene, sceneName);
    nlohmann::json root;
    if (!Irufemi::JsonUtility::LoadFromFile(pathStr, root)) {
        return false;
    }

    // デシリアライズをシーンに委譲
    scene->Deserialize(root);
    return true;
}

bool SceneSerializer::Exists(IScene* scene, const std::string& sceneName) {
    return fs::exists(GetSceneFilePath(scene, sceneName));
}

bool SceneSerializer::SavePrefab(const std::shared_ptr<GameObject>& obj, const std::string& filepath) {
    if (!obj) {
        return false;
    }

    // 単一のオブジェクトをシリアライズ
    nlohmann::json root = obj->Serialize();
    return Irufemi::JsonUtility::SaveToFile(filepath, root, 2);
}

nlohmann::json SceneSerializer::GetPrefabJson(const std::string& filepath) {
    if (!prefabManager_) {
        Log::OutPutLog(std::cerr, "[SceneSerializer] PrefabManager is null! Cannot get prefab JSON: " + filepath);
        return nlohmann::json::object();
    }
    return prefabManager_->GetPrefabJson(filepath);
}

std::shared_ptr<GameObject> SceneSerializer::LoadPrefab(const std::string& filepath) {
    if (!prefabManager_) {
        Log::OutPutLog(std::cerr, "[SceneSerializer] PrefabManager is null! Cannot load prefab: " + filepath);
        return nullptr;
    }
    return prefabManager_->Instantiate(filepath);
}

void SceneSerializer::ClearCache() {
    if (prefabManager_) {
        prefabManager_->ClearCache();
    }
}

std::string SceneSerializer::GetSceneFilePath(IScene* scene, const std::string& sceneName) {
    fs::path dir = FileSystem::GetResourcePath("scenes");
    if (scene && scene->GetEngine()) {
        dir = scene->GetEngine()->GetSceneDirectory();
    }
    fs::path filePath = dir / (sceneName + ".json");
    return filePath.generic_string();
}
