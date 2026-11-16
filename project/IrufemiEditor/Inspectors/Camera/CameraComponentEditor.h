#pragma once

#ifdef EditorMode
#include "Core/IComponentEditor.h"
#include <memory>

namespace Irufemi {
class Line3DBatch;
}

/**
 * @class CameraComponentEditor
 * @brief CameraComponent用のカスタムインスペクター拡張
 * @details FOVの度数法での編集や、シーンビュー上での視錐台（Frustum）デバッグ描画を行います。
 */
class CameraComponentEditor : public IComponentEditor {
public:
    CameraComponentEditor();
    ~CameraComponentEditor() override;

    void Draw(Component* component, EditorActionManager* actionManager) override;

private:
    std::unique_ptr<Irufemi::Line3DBatch> debugLineBatch_;
};

#endif // EditorMode
