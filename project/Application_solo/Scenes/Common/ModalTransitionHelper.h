#pragma once
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

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

/**
 * @struct UIElementState
 * @brief モーダルUI要素のアニメーション基準状態
 */
struct UIElementState {
    std::shared_ptr<Irufemi::GameObject> obj;
    Irufemi::Vector3 basePos{};
    Irufemi::Vector3 baseScale{};
    Irufemi::Vector4 baseColor{};
    bool isSprite = false;
    bool isBackdrop = false;
};

/**
 * @class UIAnimator
 * @brief モーダルシーンのUI要素キャッシュおよび開閉トランジション演出（スケール＆アルファ補間）の統括クラス
 */
class UIAnimator {
public:
    void Clear() {
        cachedElements_.clear();
        isCached_ = false;
    }

    bool IsCached() const {
        return isCached_;
    }

    /**
     * @brief シーン内のゲームオブジェクト群からUI要素を抽出・キャッシュする
     * @param objs 対象のGameObjectリスト
     * @param backdropName 暗幕（スケール不変・アルファのみフェードする背景）のオブジェクト名
     * @param excludeObjs キャッシュ対象外とするオブジェクト（カーソル等）
     */
    void CacheElements(const std::vector<std::shared_ptr<Irufemi::GameObject>>& objs,
                       const std::string& backdropName = "DarkBackdrop",
                       const std::vector<std::shared_ptr<Irufemi::GameObject>>& excludeObjs = {}) {
        cachedElements_.clear();
        for (const auto& obj : objs) {
            if (!obj) {
                continue;
            }
            if (std::find(excludeObjs.begin(), excludeObjs.end(), obj) != excludeObjs.end()) {
                continue;
            }
            auto transform = obj->GetComponent<Irufemi::TransformComponent>();
            if (!transform) {
                continue;
            }

            UIElementState state{};
            state.obj = obj;
            state.basePos = transform->GetPosition();
            state.baseScale = transform->GetScale();
            state.isBackdrop = (obj->GetName() == backdropName);

            if (auto sprite = obj->GetComponent<Irufemi::SpriteRendererComponent>()) {
                state.baseColor = sprite->GetColor();
                state.isSprite = true;
                cachedElements_.push_back(state);
            } else if (auto text = obj->GetComponent<Irufemi::TextRendererComponent>()) {
                state.baseColor = text->GetColor();
                state.isSprite = false;
                cachedElements_.push_back(state);
            }
        }
        isCached_ = true;
    }

    /**
     * @brief イージング進行度 (0.0f〜1.0f) をUI要素群へ反映する
     * @param progress 進行度 (0.0: 完全透明・縮小, 1.0: 通常表示)
     * @param center 拡大・縮小の中心点（デフォルト: 640x360 画面中央）
     */
    void ApplyTransition(float progress, const Irufemi::Vector3& center = {640.0f, 360.0f, 0.0f}) {
        for (auto& elem : cachedElements_) {
            if (!elem.obj) {
                continue;
            }
            auto transform = elem.obj->GetComponent<Irufemi::TransformComponent>();
            if (!transform) {
                continue;
            }

            if (elem.isBackdrop) {
                if (auto sprite = elem.obj->GetComponent<Irufemi::SpriteRendererComponent>()) {
                    sprite->SetColor(
                        {elem.baseColor.x, elem.baseColor.y, elem.baseColor.z, elem.baseColor.w * progress});
                }
            } else {
                float scaleMul = std::lerp(0.95f, 1.0f, progress);
                Irufemi::Vector3 offset = elem.basePos - center;
                transform->SetPosition(center + offset * scaleMul);
                transform->SetScale({elem.baseScale.x * scaleMul, elem.baseScale.y * scaleMul, elem.baseScale.z});

                if (elem.isSprite) {
                    if (auto sprite = elem.obj->GetComponent<Irufemi::SpriteRendererComponent>()) {
                        sprite->SetColor(
                            {elem.baseColor.x, elem.baseColor.y, elem.baseColor.z, elem.baseColor.w * progress});
                    }
                } else {
                    if (auto text = elem.obj->GetComponent<Irufemi::TextRendererComponent>()) {
                        text->SetColor(
                            {elem.baseColor.x, elem.baseColor.y, elem.baseColor.z, elem.baseColor.w * progress});
                    }
                }
            }
        }
    }

private:
    std::vector<UIElementState> cachedElements_;
    bool isCached_ = false;
};

} // namespace ModalTransition
