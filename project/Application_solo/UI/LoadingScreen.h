#pragma once
#include <array>
#include <memory>
#include <vector>

#include "Core/System/ILoadingScreen.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
class IrufemiEngine;
namespace Irufemi {
class Sprite;
class Primitive2DObject;
class Camera;
class Text;
class Object2DResource;
} // namespace Irufemi

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
    void OnDrawLoadingScreen(IrufemiEngine* engine, float deltaTime) override;

    void SetFadeAlpha(float alpha) override {
        masterAlpha_ = alpha;
    }

private:
    float masterAlpha_ = 1.0f; ///< フェードイン/アウトの全体不透明度
    std::shared_ptr<Irufemi::Camera> camera_;
    std::unique_ptr<Irufemi::Sprite> bgSprite_;
    std::unique_ptr<Irufemi::Text> nowLoadingText_;
    std::array<std::unique_ptr<Irufemi::Primitive2DObject>, 3> dots_;

    // アトラクトデモ中のシステムオーバーレイHUD
    std::unique_ptr<Irufemi::Text> demoText_;
    float demoBlinkTimer_ = 0.0f;

    float animationTimer_ = 0.0f;

    // --- 重力水面膜（Gravitational Veil Surface）---
    std::unique_ptr<Irufemi::Object2DResource> veilResource_; ///< 全画面プロシージャル重力流体膜
};
