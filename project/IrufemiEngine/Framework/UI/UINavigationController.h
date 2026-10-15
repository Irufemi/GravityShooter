#pragma once
#include <functional>
#include <algorithm>
#include <cmath>

class InputManager;

/**
 * @enum NavigationInputMode
 * @brief 現在のUI操作デバイスモード（ハイブリッド入力調停用）
 */
enum class NavigationInputMode {
    GamepadOrKeyboard, //!< ゲームパッド（十字キー/スティック）またはキーボードによるフォーカス操作中
    Mouse //!< マウス（物理移動・クリック）による操作中
};

/**
 * @class UINavigationController
 * @brief AAA品質のUIナビゲーション管理コントローラー（ハイブリッド入力調停・キーリピート・選択管理）
 * @details
 * ゲームパッド・キーボード・マウスが共存する環境において、
 * 「十字キーで移動した直後に静止マウスのホバーで元のボタンに引き戻される現象」を完全に防止します。
 * また、スティックや十字キーの長押しリピート（初回待機＋連続送り）および決定判定を一元提供します。
 */
class UINavigationController {
public:
    UINavigationController() = default;
    explicit UINavigationController(int itemCount, bool isHorizontal = false)
        : itemCount_(itemCount), isHorizontal_(isHorizontal) {}
    ~UINavigationController() = default;

    /**
     * @brief 項目数を設定する
     * @param count 選択項目の総数
     */
    void SetItemCount(int count) {
        itemCount_ = count;
        if (itemCount_ > 0 && selectedIndex_ >= itemCount_) {
            selectedIndex_ = itemCount_ - 1;
        }
    }

    /**
     * @brief 項目数を取得する
     */
    int GetItemCount() const {
        return itemCount_;
    }

    /**
     * @brief 現在選択されている項目のインデックスを取得する
     */
    int GetSelectedIndex() const {
        return selectedIndex_;
    }

    /**
     * @brief 現在選択されている項目のインデックスを設定する
     * @param index 設定するインデックス
     * @param triggerCallback コールバックを発火するかどうか
     */
    void SetSelectedIndex(int index, bool triggerCallback = true) {
        if (itemCount_ <= 0) {
            selectedIndex_ = 0;
            return;
        }
        int clamped = std::clamp(index, 0, itemCount_ - 1);
        if (selectedIndex_ != clamped) {
            selectedIndex_ = clamped;
            if (triggerCallback && onSelectionChanged_) {
                onSelectionChanged_(selectedIndex_);
            }
        }
    }

    /**
     * @brief 選択変更時のコールバック関数を設定する
     */
    void SetOnSelectionChanged(std::function<void(int newIndex)> callback) {
        onSelectionChanged_ = std::move(callback);
    }

    /**
     * @brief 横方向ナビゲーションモードを設定する
     */
    void SetHorizontal(bool isHorizontal) {
        isHorizontal_ = isHorizontal;
    }

    /**
     * @brief ループ（先頭と末尾のラップ）を許可するかを設定する
     */
    void SetWrapEnabled(bool wrap) {
        wrapEnabled_ = wrap;
    }

    /**
     * @brief 毎フレームの更新処理（ナビゲーション入力・デバイス調停・リピートタイマー）
     * @param input InputManager へのポインタ
     * @param deltaTime フレーム経過時間（秒）
     * @return 選択インデックスが変更された場合は true
     */
    bool Update(InputManager* input, float deltaTime);

    /**
     * @brief マウスホバーによるインデックス更新を試みる
     * @details ゲームパッド操作中かつマウスが静止している間は上書きをブロックします。
     * @param hoveredIndex マウスカーソル下の項目インデックス
     * @param input InputManager へのポインタ
     * @return 選択が更新された場合は true
     */
    bool TrySetHoveredIndex(int hoveredIndex, InputManager* input);

    /**
     * @brief 現在マウスホバーによる選択が許可されている状態か判定
     */
    bool IsMouseHoverAllowed() const {
        return activeInputMode_ == NavigationInputMode::Mouse;
    }

    /**
     * @brief 現在のアクティブな入力デバイスモードを取得
     */
    NavigationInputMode GetActiveInputMode() const {
        return activeInputMode_;
    }

    /**
     * @brief 入力モードを明示的にゲームパッド/キーボードに変更する
     */
    void SetGamepadMode() {
        activeInputMode_ = NavigationInputMode::GamepadOrKeyboard;
    }

    /**
     * @brief 入力モードを明示的にマウスに変更する
     */
    void SetMouseMode() {
        activeInputMode_ = NavigationInputMode::Mouse;
    }

    /**
     * @brief 状態をリセットする（メニュー再表示時など）
     */
    void Reset() {
        repeatTimer_ = 0.0f;
        isRepeating_ = false;
        lastStickDir_ = 0;
    }

private:
    int itemCount_ = 0;
    int selectedIndex_ = 0;
    bool isHorizontal_ = false;
    bool wrapEnabled_ = true;

    NavigationInputMode activeInputMode_ = NavigationInputMode::GamepadOrKeyboard;

    // 長押しリピート制御用パラメータ
    float repeatTimer_ = 0.0f;
    bool isRepeating_ = false;
    int lastStickDir_ = 0; // -1, 0, 1

    static constexpr float kInitialRepeatDelay_ = 0.35f;    // 長押し開始までの待機時間（秒）
    static constexpr float kRepeatInterval_ = 0.10f;        // 連続送り間隔（秒）
    static constexpr float kStickThreshold_ = 0.55f;        // スティック倒し判定閾値
    static constexpr float kStickReleaseThreshold_ = 0.25f; // スティック戻り判定閾値

    std::function<void(int newIndex)> onSelectionChanged_;
};
