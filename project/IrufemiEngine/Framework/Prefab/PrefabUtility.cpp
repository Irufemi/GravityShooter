#include "Framework/Prefab/PrefabUtility.h"
#include "Core/Utility/JsonUtility.h"
#include <nlohmann/json.hpp>

PrefabMetrics PrefabUtility::ExtractMetrics(const std::string& prefabPath) {
    PrefabMetrics metrics;
    if (prefabPath.empty()) {
        return metrics;
    }

    nlohmann::json j;
    if (!Irufemi::JsonUtility::LoadFromFile(prefabPath, j)) {
        return metrics;
    }

    for (const auto& compJson : j["components"]) {
        if (!compJson.contains("type") || !compJson.contains("data")) {
            continue;
        }

        std::string type = compJson["type"].get<std::string>();
        const auto& data = compJson["data"];

        if (type == "TransformComponent") {
            if (data.contains("scale") && data["scale"].is_array() && data["scale"].size() >= 3) {
                metrics.baseScale.x = data["scale"][0].get<float>();
                metrics.baseScale.y = data["scale"][1].get<float>();
                metrics.baseScale.z = data["scale"][2].get<float>();
            } else if (data.contains("Scale") && data["Scale"].is_array() && data["Scale"].size() >= 3) {
                metrics.baseScale.x = data["Scale"][0].get<float>();
                metrics.baseScale.y = data["Scale"][1].get<float>();
                metrics.baseScale.z = data["Scale"][2].get<float>();
            } else if (data.contains("Scale X") && data.contains("Scale Y") && data.contains("Scale Z")) {
                metrics.baseScale.x = data["Scale X"].get<float>();
                metrics.baseScale.y = data["Scale Y"].get<float>();
                metrics.baseScale.z = data["Scale Z"].get<float>();
            }
        } else if (type == "SphereColliderComponent") {
            metrics.hasSphereCollider = true;
            if (data.contains("localRadius")) {
                metrics.colliderRadius = data["localRadius"].get<float>();
            } else if (data.contains("radius")) {
                metrics.colliderRadius = data["radius"].get<float>();
            } else if (data.contains("Local Radius")) {
                metrics.colliderRadius = data["Local Radius"].get<float>();
            }
        } else if (type == "OBBColliderComponent") {
            metrics.hasOBBCollider = true;
            if (data.contains("localSize") && data["localSize"].is_array() && data["localSize"].size() >= 3) {
                metrics.obbSize.x = data["localSize"][0].get<float>();
                metrics.obbSize.y = data["localSize"][1].get<float>();
                metrics.obbSize.z = data["localSize"][2].get<float>();
            } else if (data.contains("size") && data["size"].is_array() && data["size"].size() >= 3) {
                metrics.obbSize.x = data["size"][0].get<float>();
                metrics.obbSize.y = data["size"][1].get<float>();
                metrics.obbSize.z = data["size"][2].get<float>();
            }
        } else if (type == "StaticModelGroupComponent" || type == "MeshRendererComponent") {
            if (data.contains("modelName")) {
                metrics.modelPath = data["modelName"].get<std::string>();
            } else if (data.contains("Model Name")) {
                metrics.modelPath = data["Model Name"].get<std::string>();
            }
        }
    }

    return metrics;
}
