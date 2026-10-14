#pragma once

#include <cmath>

enum class DamageableType { Unknown = 0, Enemy, Boss, Environment, Player };

/**
 * @class IDamageable
 * @brief ダメージを受けることが可能なゲームオブジェクト用インターフェース
 */
class IDamageable {
public:
    virtual ~IDamageable() = default;

    /**
     * @brief ダメージを与える
     * @param damage 与えるダメージ量
     */
    virtual void TakeDamage(float damage) = 0;

    /**
     * @brief ダメージ対象の種別を取得する
     * @return DamageableType 種別
     */
    virtual DamageableType GetDamageableType() const = 0;

    /**
     * @brief 浮動小数点ダメージを整数ダメージへ安全に変換する（四捨五入＆最低1ダメージ保証）
     * @param damage 浮動小数点のダメージ値
     * @return 整数ダメージ値（0以下の場合は0、正の端数は最低1を保証）
     */
    static int CalculateIntegerDamage(float damage) {
        if (damage <= 0.0f) {
            return 0;
        }
        int intDamage = static_cast<int>(std::round(damage));
        return (intDamage < 1) ? 1 : intDamage;
    }
};
