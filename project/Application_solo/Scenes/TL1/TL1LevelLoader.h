#pragma once

#include <string>
#include <vector>
#include "Core/Utility/JsonUtility.h"
#include "Core/Math/Vector3.h"

class BaseScene;

// 自キャラの生成データ
struct PlayerSpawnData {
    // 平行移動
    Irufemi::Vector3 translation;
    // 回転角
    Irufemi::Vector3 rotation;
};

// 敵キャラの生成データ
struct EnemySpawnData {
    // ファイル名
    std::string fileName;
    // 平行移動
    Irufemi::Vector3 translation;
    // 回転角
    Irufemi::Vector3 rotation;
};

// レベルデータ
struct LevelData {
    // 自キャラ配列
    std::vector<PlayerSpawnData> players;
    // 敵キャラ配列
    std::vector<EnemySpawnData> enemies;
};

/**
 * @class TL1LevelLoader
 * @brief Blenderから出力された独自JSON形式のレベルデータを読み込み、
 *        IrufemiEngine標準のJSON形式にコンバートしてシーンに構築するクラス
 */
class TL1LevelLoader {
public:
    /**
     * @brief レベルデータを読み込み、シーンに配置する
     * @param filepath JSONファイルのパス (例: "resources/Levels/TL1.json")
     * @param scene 配置先のシーン
     * @return 抽出されたレベルデータ（自キャラの座標など）
     */
    static LevelData Load(const std::string& filepath, BaseScene* scene);

private:
    /**
     * @brief Blender出力ノードをエンジン標準ノードに変換する
     * @param blenderNode Blender側で出力されたJSONオブジェクト
     * @return 変換後のエンジン標準JSONオブジェクト
     */
    static nlohmann::json ConvertBlenderJsonToEngineJson(const nlohmann::json& blenderNode);
};
