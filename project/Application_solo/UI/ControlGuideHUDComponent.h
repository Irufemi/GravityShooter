#pragma once
#include "Framework/Component/Component.h"
#include <memory>
#include <string>

namespace Irufemi { class SpriteRendererComponent; }
namespace Irufemi { class TextRendererComponent; }
namespace Irufemi { class GameObject; }
namespace Irufemi { class BaseScene; }

/**
 * @class ControlGuideHUDComponent
 * @brief 画面左下にキーボード/マウスおよびゲームパッドの操作ガイドを日本語で常時表示するViewコンポーネント
 */
class ControlGuideHUDComponent : public Irufemi::Component {
public:
    ControlGuideHUDComponent();
    ~ControlGuideHUDComponent() override;

    void Initialize() override;
    void Start() override;
    void Update() override;

    void SetVisible(bool visible);

    /// @brief シーン上に存在する ControlGuideHUDComponent を取得（未存在時は自動プロビジョニング）
    static ControlGuideHUDComponent* GetOrCreate(Irufemi::BaseScene* scene);

private:
    void EnsureUIElements();

    std::shared_ptr<Irufemi::GameObject> containerObj_;
    std::shared_ptr<Irufemi::GameObject> plateObj_;
    std::shared_ptr<Irufemi::GameObject> topBorderObj_;
    std::shared_ptr<Irufemi::GameObject> leftBarObj_;
    std::shared_ptr<Irufemi::GameObject> headerTextObj_;
    std::shared_ptr<Irufemi::GameObject> row1TextObj_;
    std::shared_ptr<Irufemi::GameObject> row2TextObj_;

    Irufemi::SpriteRendererComponent* plateSprite_ = nullptr;
    Irufemi::SpriteRendererComponent* topBorderSprite_ = nullptr;
    Irufemi::SpriteRendererComponent* leftBarSprite_ = nullptr;
    Irufemi::TextRendererComponent* headerText_ = nullptr;
    Irufemi::TextRendererComponent* row1Text_ = nullptr;
    Irufemi::TextRendererComponent* row2Text_ = nullptr;

    bool isElementsCreated_ = false;
    bool isVisible_ = true;
    std::string fontId_ = "toro_glitch";
};
