#pragma once

namespace Irufemi {

/**
 * @class UIAnimator
 * @brief UI演出（明滅、高速点滅、浮遊）のための数学的なアニメーション計算を行うユーティリティクラス
 */
class UIAnimator {
public:
    /**
     * @brief タイマーを進行させる
     * @param deltaTime 経過時間（秒） 例: 1.0f / 60.0f
     */
    void Update(float deltaTime);

    /**
     * @brief タイマーをリセットする
     */
    void Reset();

    /**
     * @brief サイン波を用いたゆっくりとした明滅用のアルファ値を取得する（ダークソウル風など）
     * @param base 最小のアルファ値（例: 0.6f）
     * @param amplitude 変動幅（例: 0.4f なら 0.6f 〜 1.0f の間を推移）
     * @param speed 波の速度（例: 3.0f）
     * @return 計算されたアルファ値
     */
    float GetPulseAlpha(float base = 0.6f, float amplitude = 0.4f, float speed = 3.0f) const;

    /**
     * @brief 高速点滅時の描画可視状態を取得する（フラッシュ表現）
     * @param speed 点滅の速度（例: 40.0f）
     * @return true なら描画、false なら非表示
     */
    bool GetFlashVisibility(float speed = 40.0f) const;

    /**
     * @brief サイン波を用いた浮遊アニメーション用のオフセット値を取得する
     * @param amplitude 浮遊の振幅（例: 0.2f）
     * @param speed 浮遊の速度（例: 2.0f）
     * @param phaseOffset 位相のズレ（複数のオブジェクトをずらして動かす場合など）
     * @return 計算されたオフセット値
     */
    float GetFloatOffset(float amplitude = 0.2f, float speed = 2.0f, float phaseOffset = 0.0f) const;

    /**
     * @brief 現在のアニメーション時間を取得する
     */
    float GetTime() const {
        return time_;
    }

private:
    float time_ = 0.0f;
};

} // namespace Irufemi
