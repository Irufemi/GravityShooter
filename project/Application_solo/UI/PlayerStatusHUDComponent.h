#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector4.h"
#include <memory>
#include <vector>
#include <string>

namespace Irufemi {
class SpriteRendererComponent;
}
namespace Irufemi {
class TextRendererComponent;
}
class PlayerHealthComponent;
class GravityPlayerComponent;
namespace Irufemi {
class GameObject;
}
namespace Irufemi {
class BaseScene;
}

/**
 * @class PlayerStatusHUDComponent
 * @brief プレイヤーの耐久度（HPバー、Ghost後追い遅延バー）およびデブリ保持スロット（5連Pip）を描画するHUDコンポーネント
 */
class PlayerStatusHUDComponent : public Irufemi::Component {
public:
    PlayerStatusHUDComponent();
    ~PlayerStatusHUDComponent() override;

    void Initialize() override;
    void Start() override;
    void Update() override;

    /**
     * @brief HUD全体の表示・非表示を切り替える（死亡演出時など）
     * @param[in] visible 表示フラグ
     */
    void SetVisible(bool visible);

    /**
     * @brief シーン上に存在する PlayerStatusHUDComponent を取得（未存在時は自動生成）
     * @param[in] scene 対象シーン
     * @return コンポーネントへのポインタ
     */
    static PlayerStatusHUDComponent* GetOrCreate(Irufemi::BaseScene* scene);

private:
    void EnsureUIElements();
    void BindPlayerComponents();
    void SetupDebrisPips(int maxCount);
    void UpdateHealthAnimation(float dt);
    void UpdateDebrisPips(float dt);
    void UpdateCriticalPulse(float dt);

private:
    // UI階層オブジェクト
    std::shared_ptr<Irufemi::GameObject> rootContainerObj_;

    // HP表示
    std::shared_ptr<Irufemi::GameObject> hpHeaderObj_;
    std::shared_ptr<Irufemi::GameObject> hpBarBgObj_;
    std::shared_ptr<Irufemi::GameObject> hpGhostBarObj_;
    std::shared_ptr<Irufemi::GameObject> hpFrontBarObj_;
    std::shared_ptr<Irufemi::GameObject> hpTextObj_;

    // デブリPipスロット（動的スロット）
    std::shared_ptr<Irufemi::GameObject> debrisHeaderObj_;
    std::shared_ptr<Irufemi::GameObject> debrisCountObj_;
    int maxPipCount_ = 10;
    float pipWidth_ = 9.5f;
    float pipHeight_ = 8.0f;
    std::vector<std::shared_ptr<Irufemi::GameObject>> debrisPipBgObjs_;
    std::vector<std::shared_ptr<Irufemi::GameObject>> debrisPipFillObjs_;
    std::vector<Irufemi::SpriteRendererComponent*> debrisPipFillSprites_;
    std::vector<float> pipPopScales_;

    // キャッシュされたレンダラー
    Irufemi::SpriteRendererComponent* hpBarBgSprite_ = nullptr;
    Irufemi::SpriteRendererComponent* hpGhostBarSprite_ = nullptr;
    Irufemi::SpriteRendererComponent* hpFrontBarSprite_ = nullptr;
    Irufemi::TextRendererComponent* hpHeaderText_ = nullptr;
    Irufemi::TextRendererComponent* hpText_ = nullptr;
    Irufemi::TextRendererComponent* debrisHeaderText_ = nullptr;
    Irufemi::TextRendererComponent* debrisCountText_ = nullptr;

    // プレイヤーコンポーネント参照
    std::weak_ptr<Irufemi::GameObject> playerObj_;
    PlayerHealthComponent* cachedHealthComp_ = nullptr;
    GravityPlayerComponent* cachedGravityComp_ = nullptr;

    // HPアニメーションステート
    float currentHpRatio_ = 1.0f;
    float targetHpRatio_ = 1.0f;
    float ghostHpRatio_ = 1.0f;
    float ghostDelayTimer_ = 0.0f;
    float hitFlashTimer_ = 0.0f;
    static constexpr float kGhostDelayDuration = 0.35f;

    // デブリステート
    int lastDebrisCount_ = 0;

    // 低HPパルス
    float criticalPulseTimer_ = 0.0f;

    bool isElementsCreated_ = false;
    bool isVisible_ = true;
};
