#include "Framework/Prefab/PrefabManager.h"
#include "Framework/GameObject/GameObject.h"
#include "Core/Utility/JsonUtility.h"

std::shared_ptr<const nlohmann::json> PrefabManager::GetPrefabJson(const std::string& filepath) {
    auto existing = cache_.WithRead([&](const PrefabCacheData& data) -> std::shared_ptr<nlohmann::json> {
        auto it = data.jsonCache.find(filepath);
        return (it != data.jsonCache.end()) ? it->second : nullptr;
    });
    if (existing) {
        return existing;
    }

    nlohmann::json root;
    if (!Irufemi::JsonUtility::LoadFromFile(filepath, root)) {
        static const auto kEmptyJson = std::make_shared<nlohmann::json>(nlohmann::json::object());
        return kEmptyJson;
    }

    auto jsonPtr = std::make_shared<nlohmann::json>(std::move(root));
    return cache_.WithWrite([&](PrefabCacheData& data) -> std::shared_ptr<const nlohmann::json> {
        auto [insertedIt, success] = data.jsonCache.emplace(filepath, jsonPtr);
        return insertedIt->second;
    });
}

std::shared_ptr<GameObject> PrefabManager::GetTemplate(const std::string& filepath) {
    auto cached = cache_.WithRead([&](const PrefabCacheData& data) -> std::shared_ptr<GameObject> {
        auto it = data.templateCache.find(filepath);
        return (it != data.templateCache.end()) ? it->second : nullptr;
    });
    if (cached) {
        return cached;
    }

    auto jsonPtr = GetPrefabJson(filepath);
    if (!jsonPtr || jsonPtr->empty()) {
        return nullptr;
    }

    auto templateObj = std::make_shared<GameObject>();
    templateObj->Deserialize(*jsonPtr);

    return cache_.WithWrite([&](PrefabCacheData& data) -> std::shared_ptr<GameObject> {
        auto it = data.templateCache.find(filepath);
        if (it != data.templateCache.end()) {
            return it->second;
        }
        data.templateCache[filepath] = templateObj;
        return templateObj;
    });
}

std::shared_ptr<GameObject> PrefabManager::Instantiate(const std::string& filepath) {
    auto templateObj = GetTemplate(filepath);
    if (!templateObj) {
        return nullptr;
    }

    // テンプレートからディープコピー (高速クローン)
    auto obj = templateObj->Clone();
    return obj;
}

PrefabManager::~PrefabManager() {
    ClearCache();
}

void PrefabManager::ClearCache() {
    cache_.WithWrite([](PrefabCacheData& data) {
        for (auto& [path, templateObj] : data.templateCache) {
            if (templateObj && !templateObj->IsDestroyed()) {
                templateObj->Destroy();
            }
        }
        data.jsonCache.clear();
        data.templateCache.clear();
    });
}
