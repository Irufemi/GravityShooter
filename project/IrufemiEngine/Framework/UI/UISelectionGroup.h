#pragma once
#include <vector>
#include <variant>
#include "Framework/UI/UIAnimator.h"
#include "Core/Math/Vector4.h"

namespace Irufemi {
class Sprite;
class StaticModelObject;
class InputManager;
}

/**
 * @class UISelectionGroup
 * @brief 縦並び・横並びのメニュー項目など、複数のSpriteやStaticModelObjectから一つを選択するためのコントローラー
 * @details
 * 方向キーでのインデックス切り替え、選択中の項目の明滅（色変更）、決定入力と決定後のフラッシュ演出を自動で行います。
 */
class UISelectionGroup {
public:
    UISelectionGroup();
    ~UISelectionGroup() = default;

    /**
     * @brief 選択項目となるスプライトをリストの末尾に追加する
     * @param sprite 管理対象のスプライト
     */
    void AddItem(Irufemi::Sprite* sprite);

    /**
     * @brief 選択項目となる3Dモデル(StaticModelObject)をリストの末尾に追加する
     * @param obj 管理対象のStaticModelObject
     */
    void AddItem(Irufemi::StaticModelObject* obj);

    /**
     * @brief 選択中項目の基本色を設定する
     * @param color 色（RGBA）
     */
    void SetActiveBaseColor(const Irufemi::Vector4& color) {
        activeBaseColor_ = color;
    }

    /**
     * @brief 非選択中項目の色を設定する
     * @param color 色（RGBA）
     */
    void SetInactiveColor(const Irufemi::Vector4& color) {
        inactiveColor_ = color;
    }

    /**
     * @brief 状態をリセットする（ポーズ再開時などに呼ぶ）
     */
    void Reset();

    /**
     * @brief 更新処理（キー入力によるカーソル移動、アニメーション更新）
     * @param input InputManagerへのポインタ
     * @param deltaTime デルタタイム（秒、デフォルトは 1.0f / 60.0f）
     */
    void Update(Irufemi::InputManager* input, float deltaTime = 1.0f / 60.0f);

    /**
     * @brief 描画処理（標準の描画を行う場合に使用）
     * @details 特殊な描画（影付きなど）を行いたい場合はこの関数を呼ばず、Update() 後に外部で手動で描画してください。
     */
    void Draw();

    /**
     * @brief 現在選択されている項目のインデックスを取得
     */
    int GetSelectedIndex() const {
        return selectedIndex_;
    }

    /**
     * @brief 決定キー（Space または Enter）が押されたかを判定する
     */
    bool IsDecided() const {
        return isDecided_;
    }

    /**
     * @brief 決定後、遷移遅延（フラッシュ演出など）が終わって次のシーンへ遷移すべきかを返す
     */
    bool ShouldTransition() const;

    /**
     * @brief UIを横並び（左右キー）で操作するかどうかを設定する
     */
    void SetHorizontalMode(bool horizontal) {
        isHorizontal_ = horizontal;
    }

private:
    std::vector<std::variant<Irufemi::Sprite*, Irufemi::StaticModelObject*>> items_;
    int selectedIndex_ = 0;

    UIAnimator animator_;

    Irufemi::Vector4 activeBaseColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
    Irufemi::Vector4 inactiveColor_ = {0.3f, 0.3f, 0.3f, 0.9f};

    bool isDecided_ = false;
    bool isHorizontal_ = false; // 横並び操作モード
    bool isVisible_ = true;     // 決定後のフラッシュ用
    float transitionDelayTimer_ = 0.0f;

    static constexpr float kTransitionDelayLimit = 0.8f;
};
