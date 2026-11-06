#include "Scenes/TL1/TL1LevelLoader.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/Utility/Log.h"
#include "Core/Math/MathFunction.h"
#include "Core/Utility/JsonUtility.h"
#include <iostream>

using json = nlohmann::json;

LevelData TL1LevelLoader::Load(const std::string& filepath, BaseScene* scene) {
    LevelData levelData;

    if (!scene) {
        Log::OutPutLog(std::cerr, "[TL1LevelLoader] Scene is null.\n");
        return levelData;
    }

    json rootJson;
    if (!Irufemi::JsonUtility::LoadFromFile(filepath, rootJson)) {
        Log::OutPutLog(std::cerr, "[TL1LevelLoader] Failed to load JSON file via JsonUtility: " + filepath + "\n");
        return levelData;
    }

    if (!rootJson.contains("objects") || !rootJson["objects"].is_array()) {
        Log::OutPutLog(std::cerr, "[TL1LevelLoader] JSON does not contain 'objects' array.\n");
        return levelData;
    }

    for (const auto& blenderNode : rootJson["objects"]) {
        if (blenderNode.contains("disabled")) {
            bool disabled = blenderNode["disabled"].get<bool>();
            if (disabled) {
                continue;
            }
        }

        // "PlayerSpawn"の判定
        std::string type = blenderNode.value("type", "");
        if (type == "PlayerSpawn") {
            PlayerSpawnData spawnData;
            // 座標と回転の読み取り
            if (blenderNode.contains("transform")) {
                const auto& t = blenderNode["transform"];
                if (t.contains("translation") && t["translation"].size() == 3) {
                    spawnData.translation = {
                        t["translation"][0].get<float>(),
                        t["translation"][2].get<float>(), // Y <- Z
                        t["translation"][1].get<float>()  // Z <- Y
                    };
                }
                if (t.contains("rotation") && t["rotation"].size() == 3) {
                    float degToRad = Irufemi::Math::PI / 180.0f;
                    spawnData.rotation = {
                        t["rotation"][0].get<float>() * degToRad,
                        t["rotation"][2].get<float>() * degToRad, // Y <- Z
                        t["rotation"][1].get<float>() * degToRad  // Z <- Y
                    };
                }
            }
            levelData.players.push_back(spawnData);
            continue; // 背景オブジェクトとしてはシーンに追加しない
        } else if (type == "EnemySpawn") {
            EnemySpawnData spawnData;
            // 座標と回転の読み取り
            if (blenderNode.contains("transform")) {
                const auto& t = blenderNode["transform"];
                if (t.contains("translation") && t["translation"].size() == 3) {
                    spawnData.translation = {
                        t["translation"][0].get<float>(),
                        t["translation"][2].get<float>(), // Y <- Z
                        t["translation"][1].get<float>()  // Z <- Y
                    };
                }
                if (t.contains("rotation") && t["rotation"].size() == 3) {
                    float degToRad = Irufemi::Math::PI / 180.0f;
                    spawnData.rotation = {
                        t["rotation"][0].get<float>() * degToRad,
                        t["rotation"][2].get<float>() * degToRad, // Y <- Z
                        t["rotation"][1].get<float>() * degToRad  // Z <- Y
                    };
                }
            }
            if (blenderNode.contains("file_name")) {
                spawnData.fileName = blenderNode["file_name"].get<std::string>();
            }
            levelData.enemies.push_back(spawnData);
            continue;
        }

        json engineJson = ConvertBlenderJsonToEngineJson(blenderNode);

        auto obj = std::make_shared<GameObject>();
        obj->SetScene(scene);
        try {
            obj->Deserialize(engineJson);
            scene->AddGameObject(obj);
        } catch (const std::exception& e) {
            Log::OutPutLog(std::cerr, "[TL1LevelLoader] Error deserializing object: " + std::string(e.what()) + "\n");
        }
    }

    return levelData;
}

nlohmann::json TL1LevelLoader::ConvertBlenderJsonToEngineJson(const nlohmann::json& blenderNode) {
    json engineJson;

    engineJson["name"] = blenderNode.value("name", "Unnamed");
    engineJson["components"] = json::array();

    // 1. TransformComponent の変換
    if (blenderNode.contains("transform")) {
        const auto& t = blenderNode["transform"];
        json transformData;

        // Blender(X, Y, Z) -> Engine(X, Z, Y) への変換
        if (t.contains("translation") && t["translation"].size() == 3) {
            transformData["position"] = {
                t["translation"][0].get<float>(),
                t["translation"][2].get<float>(), // Y <- Z
                t["translation"][1].get<float>()  // Z <- Y
            };
        }

        if (t.contains("rotation") && t["rotation"].size() == 3) {
            // Blenderのオイラー角(度)からラジアンへ変換し、軸を入れ替える
            float degToRad = Irufemi::Math::PI / 180.0f;
            transformData["rotation"] = {
                t["rotation"][0].get<float>() * degToRad,
                t["rotation"][2].get<float>() * degToRad, // Y <- Z
                t["rotation"][1].get<float>() * degToRad  // Z <- Y
            };
        }

        if (t.contains("scaling") && t["scaling"].size() == 3) {
            transformData["scale"] = {
                t["scaling"][0].get<float>(),
                t["scaling"][2].get<float>(), // Y <- Z
                t["scaling"][1].get<float>()  // Z <- Y
            };
        }

        json transformComp;
        transformComp["type"] = "TransformComponent";
        transformComp["data"] = transformData;
        engineJson["components"].push_back(transformComp);
    }

    // 2. MeshRendererComponent の変換
    if (blenderNode.contains("file_name") && !blenderNode["file_name"].get<std::string>().empty()) {
        json meshComp;
        meshComp["type"] = "MeshRendererComponent";
        meshComp["data"]["modelName"] = blenderNode["file_name"].get<std::string>();
        engineJson["components"].push_back(meshComp);
    }

    // 3. OBBColliderComponent の変換
    if (blenderNode.contains("collider")) {
        const auto& c = blenderNode["collider"];
        if (c.value("type", "") == "BOX" && c.contains("size") && c["size"].size() == 3) {
            json colliderComp;
            colliderComp["type"] = "OBBColliderComponent";
            // Extents (半分) にしつつ、座標軸を変換 (X, Z, Y)
            colliderComp["data"]["localSize"] = {
                c["size"][0].get<float>() * 0.5f,
                c["size"][2].get<float>() * 0.5f, // Y <- Z
                c["size"][1].get<float>() * 0.5f  // Z <- Y
            };

            if (c.contains("center") && c["center"].size() == 3) {
                colliderComp["data"]["localOffset"] = {c["center"][0].get<float>(), c["center"][2].get<float>(),
                                                       c["center"][1].get<float>()};
            }
            engineJson["components"].push_back(colliderComp);
        }
    }

    // 4. 子要素の再帰的変換
    if (blenderNode.contains("children") && blenderNode["children"].is_array()) {
        engineJson["children"] = json::array();
        for (const auto& childNode : blenderNode["children"]) {
            if (childNode.contains("disabled")) {
                bool disabled = childNode["disabled"].get<bool>();
                if (disabled) {
                    continue;
                }
            }
            engineJson["children"].push_back(ConvertBlenderJsonToEngineJson(childNode));
        }
    }

    return engineJson;
}
