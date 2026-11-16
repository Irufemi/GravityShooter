#pragma once
#include <memory>
#include <vector>

#include "Core/System/ILoadingScreen.h"
class IrufemiEngine;
namespace Irufemi {
class Sprite;
class Primitive2DObject;
class Camera;
}

/**
 * @class LoadingScreen
 * @brief すべてのScene共通で使用されるローディング画面クラス
 * @details SceneManagerによって保持され、モデルやテクスチャのロード中（非同期読み込み待ち）の間だけ更新・描画されます。
 */
class LoadingScreen : public Irufemi::ILoadingScreen {
public:
    LoadingScreen();
    ~LoadingScreen();

    void Initialize(IrufemiEngine* engine);
    void Finalize();
    void Update(float deltaTime);
    void Draw(IrufemiEngine* engine);

    // ILoadingScreen の実装
    void OnDrawLoadingScreen(IrufemiEngine* engine, float deltaTime) override {
        Update(deltaTime);
        Draw(engine);
    }

private:
    std::unique_ptr<Irufemi::Camera> camera_;
    std::unique_ptr<Irufemi::Sprite> bgSprite_;
    std::unique_ptr<Irufemi::Sprite> nowLoadingText_;
    std::vector<std::unique_ptr<Irufemi::Primitive2DObject>> dots_;

    float animationTimer_ = 0.0f;
    int dotCount_ = 0;
};
