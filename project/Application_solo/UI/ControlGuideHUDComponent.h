#pragma once
#include "Framework/Component/Component.h"
#include <memory>
#include <string>

class SpriteRendererComponent;
class TextRendererComponent;
class GameObject;

/**
 * @class ControlGuideHUDComponent
 * @brief 画面左下にキーボード/マウスおよびゲームパッドの操作ガイドを日本語で常時表示するViewコンポーネント
 */
class ControlGuideHUDComponent : public Component {
public:
    ControlGuideHUDComponent();
    ~ControlGuideHUDComponent() override;

    void Initialize() override;
    void Start() override;
    void Update() override;

    void SetVisible(bool visible);

    /// @brief シーン上に存在する ControlGuideHUDComponent を取得（未存在時は自動プロビジョニング）
    static ControlGuideHUDComponent* GetOrCreate(class BaseScene* scene);

private:
    void EnsureUIElements();

    std::shared_ptr<GameObject> containerObj_;
    std::shared_ptr<GameObject> plateObj_;
    std::shared_ptr<GameObject> topBorderObj_;
    std::shared_ptr<GameObject> leftBarObj_;
    std::shared_ptr<GameObject> headerTextObj_;
    std::shared_ptr<GameObject> row1TextObj_;
    std::shared_ptr<GameObject> row2TextObj_;

    SpriteRendererComponent* plateSprite_ = nullptr;
    SpriteRendererComponent* topBorderSprite_ = nullptr;
    SpriteRendererComponent* leftBarSprite_ = nullptr;
    TextRendererComponent* headerText_ = nullptr;
    TextRendererComponent* row1Text_ = nullptr;
    TextRendererComponent* row2Text_ = nullptr;

    bool isElementsCreated_ = false;
    bool isVisible_ = true;
    std::string fontId_ = "toro_glitch";
};
