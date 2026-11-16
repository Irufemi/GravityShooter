#pragma once
#include "Framework/Component/Component.h"
#include <vector>
#include <functional>
#include <cstdint>
#include <mutex>

/**
 * @enum TargetType
 * @brief ターゲット対象のカテゴリ種別フラグ
 */
enum class TargetType : uint32_t {
    None = 0,
    Enemy = 1 << 0,       ///< 敵（雑魚敵、ボス本体）
    BossShield = 1 << 1,  ///< ボスのシールド（がれき）
    Environment = 1 << 2, ///< 破壊可能な環境物
    All = 0xFFFFFFFF
};

/**
 * @class TargetableComponent
 * @brief プレイヤーや各種システムからロックオン・ターゲット指定可能なオブジェクトに付与するコンポーネント
 * @details シーン内のターゲット可能オブジェクトを一括管理し、ロックオン判定時の候補を提供します。
 *          敵や障害物など、ターゲット条件が動的に変化するオブジェクトは SetTargetablePredicate を用いて
 *          ターゲット可否の判定ロジックを登録できます。
 */
class TargetableComponent : public Irufemi::Component {
public:
    using TargetablePredicate = std::function<bool()>;

    TargetableComponent() = default;
    ~TargetableComponent() override;

    void OnEnable() override;
    void OnDisable() override;

    std::string GetComponentName() const override {
        return "TargetableComponent";
    }

    /**
     * @brief 登録されているすべての TargetableComponent のスナップショットを取得する（スレッドセーフ）
     * @return ターゲット候補コンポーネントのリストのコピー
     */
    static std::vector<TargetableComponent*> GetTargets();

    /**
     * @brief シーンアンロード時などに登録されているすべてのターゲットを一括クリアする（スレッドセーフ）
     */
    static void ClearAllTargets();

    /**
     * @brief ターゲット可能かどうかを判定する外部述語関数を設定する
     * @param predicate ターゲット可能ならtrueを返す述語関数
     */
    void SetTargetablePredicate(TargetablePredicate predicate) {
        predicate_ = std::move(predicate);
    }

    /**
     * @brief 現在このオブジェクトがターゲット可能かどうかを判定する
     * @return ターゲット可能ならtrue
     */
    bool IsTargetable() const;

    /**
     * @brief ターゲットの種別を設定する
     */
    void SetTargetType(TargetType type) {
        targetType_ = type;
    }

    /**
     * @brief ターゲットの種別を取得する
     */
    TargetType GetTargetType() const {
        return targetType_;
    }

private:
    static std::vector<TargetableComponent*> targets_;
    static std::mutex targetsMutex_;
    TargetablePredicate predicate_ = nullptr;
    TargetType targetType_ = TargetType::Enemy; ///< デフォルトは敵
};
