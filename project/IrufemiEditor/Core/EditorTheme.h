#pragma once

#ifdef EditorMode

/**
 * @class EditorTheme
 * @brief エディター共通のカラーパレットやボタンスタイルを提供するユーティリティ
 */
class EditorTheme {
public:
    /**
     * @brief UE5/Unityライクなモダン・ダークテーマをImGuiに適用する
     */
    static void ApplyDarkTheme();

    /**
     * @brief 危険な操作（削除など）のボタンスタイルを適用する
     */
    static void PushDangerButtonStyle();

    /**
     * @brief 適用したボタンスタイルを解除する
     */
    static void PopButtonStyle();

    /**
     * @struct ScopedDangerButton
     * @brief 危険ボタンスタイルをRAIIで適用・自動破棄するスコープガード
     */
    struct ScopedDangerButton {
        ScopedDangerButton() {
            PushDangerButtonStyle();
        }
        ~ScopedDangerButton() {
            PopButtonStyle();
        }
        ScopedDangerButton(const ScopedDangerButton&) = delete;
        ScopedDangerButton& operator=(const ScopedDangerButton&) = delete;
    };
};

#endif // EditorMode
