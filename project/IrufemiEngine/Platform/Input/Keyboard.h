#pragma once
#include <Windows.h>
#include <array>
#include <cstdint>

/**
 * @class Keyboard
 * @brief キーボード入力を管理するクラス
 * @details 各キーの押下状態（Down, Up, Pressed, Released）を取得します。
 *          DIK（DirectInput Key）から VK（Virtual Key）への変換機能も内蔵しています。
 */
class Keyboard {
public:
    static constexpr size_t kKeyCount = 256;

    Keyboard() = default;
    ~Keyboard() = default;

    /** @name 初期化・更新 */
    ///@{
    void Initialize();
    /**
     * @brief キーボードの入力状態（Windows API GetKeyboardState）を毎フレーム更新する
     */
    void Update();
    /**
     * @brief 全キーの入力状態をクリア（非アクティブ時等）する
     */
    void Clear();
    /**
     * @brief 指定した仮想キーの入力状態を消費（クリア）する
     * @param key 仮想キーコード (VK_xxx)
     * @details 同一フレーム内の後続システムへキー入力を伝播させない（Handled状態にする）ために使用します。
     */
    void ConsumeKey(uint8_t key);
    ///@}

    /** @name キー状態の取得 */
    ///@{
    /** @brief 指定した仮想キー（VK）が押されているか判定 */
    bool IsKeyDown(uint8_t key) const;
    /** @brief 指定した仮想キー（VK）が離されているか判定 */
    bool IsKeyUp(uint8_t key) const;
    /** @brief 指定した仮想キー（VK）が押された瞬間か判定（立ち上がり検出） */
    bool IsKeyPressed(uint8_t key) const;
    /** @brief 指定した仮想キー（VK）が離された瞬間か判定（立ち下がり検出） */
    bool IsKeyReleased(uint8_t key) const;
    ///@}

    /** @name DIK互換API */
    ///@{
    /** @brief 指定したDirectInputキー（DIK）が押されているか判定 */
    bool IsKeyDownDIK(uint8_t dik) const;
    /** @brief 指定したDirectInputキー（DIK）が離されているか判定 */
    bool IsKeyUpDIK(uint8_t dik) const;
    /** @brief 指定したDirectInputキー（DIK）が押された瞬間か判定（立ち上がり検出） */
    bool IsKeyPressedDIK(uint8_t dik) const;
    /** @brief 指定したDirectInputキー（DIK）が離された瞬間か判定（立ち下がり検出） */
    bool IsKeyReleasedDIK(uint8_t dik) const;
    ///@}

private:
    std::array<BYTE, kKeyCount> currentKeys_{};
    std::array<BYTE, kKeyCount> previousKeys_{};

    /**
     * @brief DIKからVKへの変換
     */
    static uint8_t DIKToVK(uint8_t dik);
};
