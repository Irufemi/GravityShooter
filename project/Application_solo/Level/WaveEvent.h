#pragma once
#include <string>
#include <memory>
#include "Core/Utility/JsonUtility.h"
#include "Core/Math/Vector3.h"

class WaveManagerComponent;

struct WaveEventData {
    float triggerDistance;
    std::string eventType;
    nlohmann::json parameters;

    // 距離でソートするためのオペレータ
    bool operator>(const WaveEventData& other) const {
        return triggerDistance > other.triggerDistance;
    }
};

class IWaveEventHandler {
public:
    virtual ~IWaveEventHandler() = default;

    /**
     * @brief イベントを実行する
     * @param data JSONから読み込まれたイベントパラメータ
     * @param railPos イベント発火時点でのレール上の絶対座標
     * @param railForward イベント発火時点でのレール上の接線（進行方向）ベクトル
     * @param railRight イベント発火時点でのレール上の右方向ベクトル
     */
    virtual void Execute(WaveManagerComponent* manager, const WaveEventData& data, const Irufemi::Vector3& railPos,
                         const Irufemi::Vector3& railForward, const Irufemi::Vector3& railRight) = 0;

#if defined(_DEBUG) || defined(EditorMode) || defined(DEVELOPMENT)
    /**
     * @brief エディタ用のプレビュー（Gizmo等）を描画する
     */
    virtual void DrawEditorPreview(WaveManagerComponent* manager, const WaveEventData& data,
                                   const Irufemi::Vector3& railPos, const Irufemi::Vector3& railForward,
                                   const Irufemi::Vector3& railRight) {}
#endif
};
