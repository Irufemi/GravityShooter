#pragma once

#include <Windows.h>
#include <string>
#include <cstdint>

enum class DisplayMode { Windowed, Borderless };

// 前方宣言
class InputManager;
class IrufemiEngine;

/**
 * @class WinApp
 * @brief Windowsアプリケーションの基盤（ウィンドウ生成と管理）を担うクラス
 * @details ウィンドウクラスの登録、ウィンドウの生成、メッセージループの制御、COMライブラリの初期化などを行います。
 */
class WinApp final {
public:
    // クラス定数(課題要件の int 定数)
    static constexpr int kClassVersion = 1;

    /**
     * @brief コンストラクタ
     */
    WinApp() = default;

    /**
     * @brief デストラクタ
     */
    ~WinApp();

    /**
     * @brief 初期化
     * @details ウィンドウの生成および COM の初期化を行います。
     * @param[in] hInstance インスタンスハンドル
     * @param[in] width ウィンドウの横幅
     * @param[in] height ウィンドウの縦幅
     * @param[in] title ウィンドウタイトル
     * @return 初期化成功なら true
     */
    bool Initialize(HINSTANCE hInstance, int width = 1280, int height = 720, const std::wstring& title = L"Window");

    /**
     * @brief 終了処理
     * @details ウィンドウの破棄および COM の終了処理を行います。
     */
    void Finalize();

    /**
     * @brief メッセージ処理
     * @details Windowsメッセージを処理します。アプリ終了メッセージを受け取った場合は false を返します。
     * @return アプリケーションを続行する場合は true, 終了する場合は false
     */
    bool ProcessMessages();

    /** @name ゲッター */
    ///@{
    HWND GetHwnd() const {
        return hwnd_;
    }
    /**
     * @brief HInstance を取得する。
     * @return 取得された HInstance
     */
    HINSTANCE GetHInstance() const {
        return hInstance_;
    }
    /**
     * @brief ClientWidth を取得する。
     * @return 取得された ClientWidth
     */
    int GetClientWidth() const {
        return clientWidth_;
    }
    /**
     * @brief ClientHeight を取得する。
     * @return 取得された ClientHeight
     */
    int GetClientHeight() const {
        return clientHeight_;
    }
    /**
     * @brief IsCursorLocked かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsCursorLocked() const {
        return cursorLocked_;
    }
    /**
     * @brief D&Dでドロップされたファイルパスを取得する
     */
    std::string GetDroppedFilePath() const {
        return droppedFilePath_;
    }

    /**
     * @brief ドロップされたファイルパスをクリアする
     */
    void ClearDroppedFilePath() {
        droppedFilePath_.clear();
    }

    /**
     * @brief ドロップされたファイルパスを設定する（内部用）
     */
    void SetDroppedFilePath(const std::string& path) {
        droppedFilePath_ = path;
    }
    ///@}

    /**
     * @brief ディスプレイモードの取得
     */
    DisplayMode GetDisplayMode() const {
        return displayMode_;
    }

    /**
     * @brief ディスプレイモードの変更
     */
    void SetDisplayMode(DisplayMode mode);

    /**
     * @brief カーソル固定状態の設定
     */
    void SetCursorLocked(bool lock);

    /**
     * @brief InputManagerのポインタを設定
     */
    void SetInputManager(InputManager* inputManager);

    /**
     * @brief IrufemiEngineのポインタを設定
     */
    void SetEngine(IrufemiEngine* engine) {
        engine_ = engine;
    }

    /**
     * @brief Windowsキーの無効化（キオスクモード）の有効/無効を設定
     * @param enable trueならWindowsキーをブロック、falseなら通常動作
     */
    void SetWindowsKeyLock(bool enable);

    /**
     * @brief Windowsキーが無効化されているかを取得
     */
    bool IsWindowsKeyLocked() const {
        return windowsKeyLockEnabled_;
    }

    /**
     * @brief 静的ウィンドウプロシージャ
     */
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /**
     * @brief 低レベルキーボードフックプロシージャ
     */
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    /**
     * @brief 例外発生時のダンプ出力用
     */
    static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);

    // コピー/ムーブ禁止
    WinApp(const WinApp&) = delete;
    WinApp& operator=(const WinApp&) = delete;
    WinApp(WinApp&&) = delete;
    WinApp& operator=(WinApp&&) = delete;

private:
    /**
     * @brief 非静的メッセージ処理本体
     */
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;
    std::wstring className_ = L"IrufemiWinClass";
    std::wstring windowTitle_;
    int32_t clientWidth_ = 0;
    int32_t clientHeight_ = 0;
    bool comInitialized_ = false;
    bool didRegisterClass_ = false;
    InputManager* inputManager_ = nullptr; // InputManagerへのポインタ
    IrufemiEngine* engine_ = nullptr;      // IrufemiEngineへのポインタ

    std::string droppedFilePath_ = ""; // 最後にドロップされたファイルのパス
    bool cursorLocked_ = true;         // カーソル固定状態デフォルト真
    DisplayMode displayMode_ = DisplayMode::Windowed;
    RECT windowedRect_{}; // ウィンドウモード時のサイズ保存用

    bool windowsKeyLockEnabled_ = false; // Windowsキー無効化フラグ
    static inline HHOOK keyboardHook_ = nullptr;
    static inline HWND activeHwnd_ = nullptr;
    static inline bool globalKeyLockEnabled_ = false;
};
