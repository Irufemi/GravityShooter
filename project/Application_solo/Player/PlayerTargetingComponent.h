#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Player/TargetableComponent.h"
#include <vector>
#include <deque>
#include <memory>
#include <future>
#include <unordered_map>
#include <cstdint>

class GameObject;
class LockonMarkerUIComponent;
struct RaycastHit;

/**
 * @class PlayerTargetingComponent
 * @brief プレイヤーのマルチロックオン、手動マーキング、レイキャスト遮蔽判定を行うコンポーネント
 */
class PlayerTargetingComponent : public Component {
public:
    PlayerTargetingComponent() = default;
    ~PlayerTargetingComponent() override;

    void Initialize() override;
    void Start() override;
    void Update() override;
    void OnRegisterProperties() override;

    std::string GetComponentName() const override {
        return "PlayerTargetingComponent";
    }

    /**
     * @brief 右クリック時に呼ばれる。スコアが最も高い敵をキューに追加する
     * @param maxLockOn ロックオンの最大上限数
     */
    void MarkTarget(size_t maxLockOn);

    /**
     * @brief 手動キャンセル(Rキー)などで呼ばれる。キューを全クリア
     */
    void ClearTargets();

    const std::deque<std::shared_ptr<GameObject>>& GetQueuedTargets() const {
        return queuedTargets_;
    }

    /**
     * @brief 現在レティクルが合っているホバー中のターゲットを取得する
     */
    std::shared_ptr<GameObject> GetHoverTarget() const {
        return hoverTarget_;
    }

    /**
     * @brief キューの先頭を取り出して返す（発射用）
     */
    std::shared_ptr<GameObject> PopTarget();

    void SetMaxLockonCount(size_t count) {
        maxLockonCount_ = count;
    }

    /**
     * @brief マウスカーソル位置に基づくワールド空間の照準座標（レイキャスト着地点）を算出する
     * @param maxDistance レイキャストの最大到達距離
     * @return 算出したワールド照準座標
     */
    Irufemi::Vector3 CalculateAimPoint(float maxDistance = 1000.0f) const;

    // --- ターゲット種別の能動的フィルタリングAPI ---
    /**
     * @brief ロックオン対象とするターゲット種別マスクを一括設定する
     */
    void SetTargetTypeMask(uint32_t mask) {
        targetTypeMask_ = mask;
    }

    /**
     * @brief 指定したターゲット種別をロックオン対象に追加する
     */
    void EnableTargetType(TargetType type) {
        targetTypeMask_ |= static_cast<uint32_t>(type);
    }

    /**
     * @brief 指定したターゲット種別をロックオン対象から除外する
     */
    void DisableTargetType(TargetType type) {
        targetTypeMask_ &= ~static_cast<uint32_t>(type);
    }

    /**
     * @brief 現在のターゲット種別マスクを取得する
     */
    uint32_t GetTargetTypeMask() const {
        return targetTypeMask_;
    }

    /**
     * @brief 指定したターゲット種別が現在ロックオン許可されているか判定する
     */
    bool IsTargetTypeAllowed(TargetType type) const {
        return (targetTypeMask_ & static_cast<uint32_t>(type)) != 0;
    }

private:
    size_t maxLockonCount_ = 1;
    std::deque<std::shared_ptr<GameObject>> queuedTargets_;
    std::shared_ptr<GameObject> hoverTarget_ = nullptr;

    // ロックオン許可マスク（デフォルトは敵とボスのシールドのみ許可、環境物は除外）
    uint32_t targetTypeMask_ = static_cast<uint32_t>(TargetType::Enemy) | static_cast<uint32_t>(TargetType::BossShield);

    /** @brief 非同期レイキャストの結果待機用と時間間引き(Amortization)用キャッシュ構造体 */
    struct TargetVisibilityCache {
        std::weak_ptr<GameObject> targetObject; ///< 生存確認用ポインタ（UAF防止）
        bool canSee = false;                    ///< 壁裏チェック完了前はfalse（透過防止）
        bool hasCheckedOnce = false;            ///< 初回判定が実行されたかどうか
        float lastCheckTime = -1.0f;
        std::shared_ptr<std::future<std::pair<bool, RaycastHit>>> pendingTask;
    };
    std::unordered_map<uint64_t, TargetVisibilityCache> visibilityCache_;

    void UpdateHoverTarget();
    void TryFindLockonMarkerUI();

    float lockonRadius2D_ = 200.0f; ///< スクリーン上の許容半径（ピクセル）
    float weight2D_ = 1.0f;         ///< 2D距離のスコア重み
    float weight3D_ = 10.0f;        ///< 3D深度のスコア重み

    std::weak_ptr<LockonMarkerUIComponent> lockonMarkerUI_;
    float uiSearchTimer_ = 0.0f;                     ///< UI未検出時の再試行タイマー
    static constexpr float kUISearchInterval = 0.5f; ///< UI再検索の実行間隔（秒）

    // ロックオン・ホバー時のアウトラインハイライト制御 (Juice)
    void UpdateOutlineHighlights();
    void RestoreAllOutlineHighlights();
    std::unordered_map<uint64_t, Irufemi::Vector4> originalOutlineColors_; ///< ハイライト前の初期色キャッシュ
};
