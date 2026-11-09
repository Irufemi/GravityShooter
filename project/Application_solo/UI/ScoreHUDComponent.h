#pragma once
#include "Framework/Component/Component.h"
#include <memory>
#include <string>

class TextRendererComponent;
class ScoreManagerComponent;
class GameObject;

/**
 * @class ScoreHUDComponent
 * @brief スコア（右上固定）およびマルチキル撃破ポップアップ（照準追従・瞬間表示）を描画するViewコンポーネント
 */
class ScoreHUDComponent : public Component {
public:
    ScoreHUDComponent();
    ~ScoreHUDComponent() override;

    void Initialize() override;
    void Start() override;
    void Update() override;

    /// @brief シーン上に存在する ScoreHUDComponent を取得（未存在時は自動プロビジョニング）
    static ScoreHUDComponent* GetOrCreate(class BaseScene* scene);

private:
    void EnsureUIElements();
    void UpdateDisplay(float dt);

    // 右上固定：トータルスコア ＆ 撃破総数
    std::shared_ptr<GameObject> scoreHeaderObj_;
    std::shared_ptr<GameObject> scoreObj_;
    std::shared_ptr<GameObject> killsObj_;
    TextRendererComponent* scoreHeaderText_ = nullptr;
    TextRendererComponent* scoreText_ = nullptr;
    TextRendererComponent* killsText_ = nullptr;

    // 照準周辺：撃破時ポップアップ（普段は完全非表示）
    std::shared_ptr<GameObject> popObj_;
    std::shared_ptr<GameObject> popScoreObj_;
    TextRendererComponent* popText_ = nullptr;
    TextRendererComponent* popScoreText_ = nullptr;

    float displayedScore_ = 0.0f;
    float popAnimT_ = 0.0f; // 0.0f 〜 1.0f (浮き上がり＆フェード用)
    bool isElementsCreated_ = false;
};
