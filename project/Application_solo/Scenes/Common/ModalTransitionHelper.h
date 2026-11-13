#pragma once
#include <algorithm>
#include <cmath>
#include <functional>

namespace ModalTransition {

/**
 * @enum State
 * @brief モーダルシーンの開閉アニメーション状態
 */
enum class State {
    Opening, //!< オープン中 (Fade & Scale In)
    Open,    //!< 通常表示中
    Closing  //!< クローズ中 (Fade & Scale Out)
};

/**
 * @struct Controller
 * @brief モーダルUIの開閉トランジション制御ヘルパー
 */
struct Controller {
    State state = State::Opening;
    float timer = 0.0f;
    float openDuration = 0.16f;
    float closeDuration = 0.12f;

    void StartOpen() {
        state = State::Opening;
        timer = 0.0f;
    }

    void StartClose() {
        state = State::Closing;
        timer = 0.0f;
    }

    bool IsOpen() const {
        return state == State::Open;
    }

    bool IsOpening() const {
        return state == State::Opening;
    }

    bool IsClosing() const {
        return state == State::Closing;
    }

    bool IsAnimating() const {
        return state == State::Opening || state == State::Closing;
    }

    /**
     * @brief トランジションを更新する
     * @tparam ApplyFunc void(float progress) 互換の呼び出し可能オブジェクト
     * @tparam CloseFunc void() 互換の呼び出し可能オブジェクト
     * @param dt デルタタイム
     * @param applyTransition イージング進行度 (0.0f〜1.0f) をUIに反映する関数
     * @param onClosed クローズ完了時に呼び出される関数
     * @return アニメーション中なら true (後続の入力更新をガードする用途)
     */
    template <typename ApplyFunc, typename CloseFunc>
    bool Update(float dt, ApplyFunc&& applyTransition, CloseFunc&& onClosed) {
        if (state == State::Opening) {
            timer += dt;
            float progress = std::clamp(timer / openDuration, 0.0f, 1.0f);
            // EaseOutCubic
            float ease = 1.0f - std::pow(1.0f - progress, 3.0f);
            applyTransition(ease);

            if (progress >= 1.0f) {
                state = State::Open;
                applyTransition(1.0f);
            }
            return true;
        }

        if (state == State::Closing) {
            timer += dt;
            float progress = std::clamp(timer / closeDuration, 0.0f, 1.0f);
            // EaseInQuad
            float ease = 1.0f - (progress * progress);
            applyTransition(ease);

            if (progress >= 1.0f) {
                onClosed();
            }
            return true;
        }

        return false;
    }
};

} // namespace ModalTransition

