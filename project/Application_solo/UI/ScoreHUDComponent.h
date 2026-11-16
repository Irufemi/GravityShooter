#pragma once
#include "Framework/Component/Component.h"
#include <memory>
#include <string>

namespace Irufemi {
class TextRendererComponent;
}
class ScoreManagerComponent;
namespace Irufemi {
class GameObject;
}
namespace Irufemi {
class BaseScene;
}

/**
 * @class ScoreHUDComponent
 * @brief スコア（右上固定）およびマルチキル撃破ポップアップ（照準追従・瞬間表示）を描画するViewコンポーネント
 */
class ScoreHUDComponent : public Irufemi::Component {
public:
    ScoreHUDComponent();
    ~ScoreHUDComponent() override;

    void Initialize() override;
    void Start() override;
    void Update() override;

    /// @brief シーン上に存在する ScoreHUDComponent を取得（未存在時は自動プロビジョニング）
    static ScoreHUDComponent* GetOrCreate(Irufemi::BaseScene* scene);

private:
    void EnsureUIElements();
    void UpdateDisplay(float dt);

    // 右上固定：トータルスコア ＆ 撃破総数
    std::shared_ptr<Irufemi::GameObject> scoreHeaderObj_;
    std::shared_ptr<Irufemi::GameObject> scoreObj_;
    std::shared_ptr<Irufemi::GameObject> killsObj_;
    Irufemi::TextRendererComponent* scoreHeaderText_ = nullptr;
    Irufemi::TextRendererComponent* scoreText_ = nullptr;
    Irufemi::TextRendererComponent* killsText_ = nullptr;

    // 照準周辺：戦闘ポップアップ（普段は完全非表示）
    std::shared_ptr<Irufemi::GameObject> popObj_;
    std::shared_ptr<Irufemi::GameObject> popScoreObj_;
    Irufemi::TextRendererComponent* popText_ = nullptr;
    Irufemi::TextRendererComponent* popScoreText_ = nullptr;

    // 画面中央：特大バナー通知（SHIELD BREAK! 等）
    std::shared_ptr<Irufemi::GameObject> centerBannerObj_;
    std::shared_ptr<Irufemi::GameObject> centerSubBannerObj_;
    Irufemi::TextRendererComponent* centerBannerText_ = nullptr;
    Irufemi::TextRendererComponent* centerSubBannerText_ = nullptr;

    float displayedScore_ = 0.0f;
    float popAnimT_ = 0.0f; // 0.0f 〜 1.0f (浮き上がり＆フェード用)
    bool isElementsCreated_ = false;
};
