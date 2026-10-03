#include <string>
#include <memory>
#include <vector>
#include <algorithm>

#include "Platform/WindowsAPI/WinApp.h"
#include "Platform/Input/InputManager.h"
#include "Platform/Input/Mouse.h"
#include "Framework/UI/DebugUI.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/Utility/FileSystem.h"
#include "Core/Utility/StringUtility.h"

#include <Windows.h>
#include <DbgHelp.h>
#include <strsafe.h>
#include <shellapi.h>

#define ENABLE_ESCAPE_EXIT 0 // 1: 有効, 0: 無効

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Dbghelp.lib")

WinApp::~WinApp() {
    Finalize();
}

bool WinApp::Initialize(HINSTANCE hInstance, int width, int height, const std::wstring& title) {

    hInstance_ = hInstance;
    clientWidth_ = width;
    clientHeight_ = height;
    windowTitle_ = title;

    // システムタイマーの分解能を上げる
    timeBeginPeriod(1);

    // ─────────────────────────────────────────────────────
    // 学校資料準拠：WNDCLASS + RegisterClass + CreateWindow
    // ─────────────────────────────────────────────────────

    /*ウィンドウを作ろう*/

    /// ウィンドウクラスを登録する

    WNDCLASSW wc{};
    // ウィンドウプロシージャ
    wc.lpfnWndProc = &WinApp::WndProc;
    // ウィンドウクラス名(なんでもいい)
    wc.lpszClassName = className_.c_str();
    // インスタンスハンドル
    wc.hInstance = hInstance;
    // カーソル
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    // ウィンドウクラスを登録する
    ATOM atom = RegisterClassW(&wc);
    didRegisterClass_ = (atom != 0); // 既に登録済みなら 0(解除しない)

    /// ウィンドウサイズを決める

    // ウィンドウサイズを表す構造体にクライアント領域を入れる
    RECT wrc = {0, 0, clientWidth_, clientHeight_};

    // クライアント領域をもとに実際のサイズにwrcを変更してもらう
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    /// ウィンドウを生成して表示

    // ウィンドウの生成
    hwnd_ = CreateWindowW(wc.lpszClassName,     // 利用するクラス名
                          windowTitle_.c_str(), // タイトルバーの文字(何でも良い)
                          WS_OVERLAPPEDWINDOW,  // よく見るウィンドウスタイル
                          CW_USEDEFAULT,        // 表示X座標(windowsに任せる)
                          CW_USEDEFAULT,        // 表示Y座標(windowsに任せる)
                          wrc.right - wrc.left, // ウィンドウ横幅
                          wrc.bottom - wrc.top, // ウィンドウ縦幅
                          nullptr,              // 親ウィンドウハンドル
                          nullptr,              // メニューハンドル
                          wc.hInstance,         // インスタンスハンドル
                          this                  // オプション
    );
    if (!hwnd_) {
        return false;
    }

    /*ウィンドウを作ろう*/

    /// ウィンドウを生成して表示

    // ウィンドウを表示する
    ShowWindow(hwnd_, SW_SHOW);
    SetWindowTextW(hwnd_, windowTitle_.c_str());

    // 実クライアントサイズ
    RECT cr{};
    GetClientRect(hwnd_, &cr);
    clientWidth_ = cr.right - cr.left;
    clientHeight_ = cr.bottom - cr.top;

    // Raw Inputデバイス（マウス）の登録
    RAWINPUTDEVICE rid[1];
    rid[0].usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
    rid[0].usUsage = 0x02;     // HID_USAGE_GENERIC_MOUSE
    rid[0].dwFlags = 0;        // ウィンドウがアクティブな時のみ受け取る
    rid[0].hwndTarget = hwnd_;
    RegisterRawInputDevices(rid, 1, sizeof(rid[0]));

    // ドラッグ＆ドロップを受け付ける
    DragAcceptFiles(hwnd_, TRUE);

    return true;
}

void WinApp::Finalize() {
    SetWindowsKeyLock(false);
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (didRegisterClass_ && !className_.empty() && hInstance_) { // 自分が登録した場合のみ解除
        UnregisterClassW(className_.c_str(), hInstance_);
        didRegisterClass_ = false;
    }
}

void WinApp::SetCursorLocked(bool lock) {
    cursorLocked_ = lock;
    if (inputManager_) {
        if (auto* mouse = inputManager_->GetMouse()) {
            // ゲーム画面が最前面の時だけロック状態を反映する
            if (GetForegroundWindow() == hwnd_) {
                mouse->SetLocked(cursorLocked_);
            }
        }
    }
}

void WinApp::SetInputManager(InputManager* inputManager) {
    inputManager_ = inputManager;
    // InputManagerがセットされた時点で、現在のロック状態を適用する
    if (inputManager_) {
        if (auto* mouse = inputManager_->GetMouse()) {
            // ウィンドウが既にアクティブならロックを適用
            if (GetFocus() == hwnd_) {
                mouse->SetLocked(cursorLocked_);
            }
        }
    }
}

bool WinApp::ProcessMessages() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

LRESULT CALLBACK WinApp::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* pThis = reinterpret_cast<WinApp*>(cs->lpCreateParams);
        if (pThis) {
            pThis->hwnd_ = hWnd; // ここで先に設定
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        }
        return TRUE; // NCCREATE 成功
    }

    // thisポインタを取得
    auto* pThis = reinterpret_cast<WinApp*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

#ifdef USE_IMGUI
    // カーソルがロックされていない場合のみImGuiにメッセージを渡す
    if (pThis && !pThis->IsCursorLocked()) {
        if (DebugUI::WndProcHandler(hWnd, msg, wParam, lParam)) {
            // ImGuiがメッセージを処理した場合、以降の処理は行わない
            return TRUE;
        }
    }
#endif // USE_IMGUI

    if (msg == WM_SETCURSOR) {
        if (pThis && pThis->IsCursorLocked()) {
            SetCursor(nullptr);
            return TRUE;
        }
    }

    if (pThis) {
        return pThis->HandleMessage(hWnd, msg, wParam, lParam);
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT WinApp::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_INPUT: {
        UINT dwSize = 0;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
        if (dwSize > 0) {
            std::vector<BYTE> lpb(dwSize);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, lpb.data(), &dwSize,
                                sizeof(RAWINPUTHEADER)) == dwSize) {
                RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(lpb.data());
                if (raw->header.dwType == RIM_TYPEMOUSE) {
                    if (inputManager_) {
                        if (auto* mouse = inputManager_->GetMouse()) {
                            // 通常の相対移動のみ（絶対座標移動等は除外）
                            if ((raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
                                float dx = static_cast<float>(raw->data.mouse.lLastX);
                                float dy = static_cast<float>(raw->data.mouse.lLastY);
                                mouse->AddRawDelta(dx, dy);
                            }
                        }
                    }
                }
            }
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    case WM_KEYDOWN:
#if ENABLE_ESCAPE_EXIT
        if (wParam == VK_ESCAPE) {
            PostMessage(hWnd, WM_CLOSE, 0, 0);
        }
#endif
        return 0;
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        // Altキー押下時のWindowsデフォルト動作（警告音など）を無効化
        return 0;
    case WM_SYSCOMMAND:
        // Altキー押下によるメニューバーへのフォーカス（画面フリーズの原因）を無効化
        if ((wParam & 0xfff0) == SC_KEYMENU) {
            return 0;
        }
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    case WM_MOUSEWHEEL:
        if (inputManager_) {
            if (auto* mouse = inputManager_->GetMouse()) {
                float wheelDelta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
                mouse->SetWheelDelta(wheelDelta);
            }
        }
        return 0;
    case WM_SETFOCUS: // ウィンドウがアクティブになった
        activeHwnd_ = hwnd_;
        if (inputManager_) {
            if (auto* mouse = inputManager_->GetMouse()) {
                mouse->SetLocked(cursorLocked_);
            }
        }
        return 0;

    case WM_KILLFOCUS: // ウィンドウが非アクティブになった
        if (activeHwnd_ == hwnd_) {
            activeHwnd_ = nullptr;
        }
        if (inputManager_) {
            if (auto* mouse = inputManager_->GetMouse()) {
                mouse->SetLocked(false);
            }
        }
        return 0;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED && engine_) {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (width > 0 && height > 0) {
                clientWidth_ = width;
                clientHeight_ = height;
                engine_->OnResize(width, height);
            }
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    case WM_DROPFILES: {
        HDROP hDrop = reinterpret_cast<HDROP>(wParam);
        UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        if (fileCount > 0) {
            // 最初のファイルだけ取得
            wchar_t filePathW[MAX_PATH];
            DragQueryFileW(hDrop, 0, filePathW, MAX_PATH);

            // ワイド文字からマルチバイト文字(UTF-8想定、またはShiftJIS)に変換
            char filePathA[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, filePathW, -1, filePathA, MAX_PATH, nullptr, nullptr);

            droppedFilePath_ = filePathA;
        }
        DragFinish(hDrop);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

LONG WINAPI WinApp::ExportDump(EXCEPTION_POINTERS* exception) {

    // 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpディレクトリ以下に出力
    SYSTEMTIME time;
    GetLocalTime(&time);
    wchar_t filePath[MAX_PATH] = {0};
    std::wstring dumpDir = ConvertString(FileSystem::GetDumpPath());
    CreateDirectoryW(dumpDir.c_str(), nullptr);
    StringCchPrintfW(filePath, MAX_PATH, L"%ls/%04d-%02d%02d-%02d%02d.dmp", dumpDir.c_str(), time.wYear, time.wMonth,
                     time.wDay, time.wHour, time.wMinute);
    HANDLE dumpFileHandle =
        CreateFileW(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    // processId(このexeのId)とクラッシュ(例外)の発生したthreadIdを取得
    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId();
    // 設定情報を入力
    MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{0};
    minidumpInformation.ThreadId = threadId;
    minidumpInformation.ExceptionPointers = exception;
    minidumpInformation.ClientPointers = TRUE;
    // Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
    MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr,
                      nullptr);
    // ほかに関連づけられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する
    return EXCEPTION_EXECUTE_HANDLER;
}

void WinApp::SetDisplayMode(DisplayMode mode) {
    if (displayMode_ == mode) {
        return;
    }

    if (mode == DisplayMode::Borderless) {
        // Save current windowed rect
        GetWindowRect(hwnd_, &windowedRect_);

        // Change style to WS_POPUP
        SetWindowLongW(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);

        // Get monitor info
        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfoW(monitor, &mi);

        // Resize and position window to fill the monitor
        SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER);
    } else if (mode == DisplayMode::Windowed) {
        // Change style back to WS_OVERLAPPEDWINDOW
        SetWindowLongW(hwnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);

        // 規定のゲーム解像度（16:9）をクライアント領域として正確に復元
        int targetClientW = 1280;
        int targetClientH = 720;
        if (engine_) {
            targetClientW = engine_->GetGameResolutionWidth();
            targetClientH = engine_->GetGameResolutionHeight();
        }

        RECT wrc = {0, 0, targetClientW, targetClientH};
        AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);
        int winW = wrc.right - wrc.left;
        int winH = wrc.bottom - wrc.top;

        // 保存された位置があればその位置を基準に、なければ画面中央に配置
        int winX = (windowedRect_.right - windowedRect_.left > 0) ? windowedRect_.left : 100;
        int winY = (windowedRect_.right - windowedRect_.left > 0) ? windowedRect_.top : 100;

        // モニター作業領域（タスクバー除く）を考慮して画面内に収める
        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfoW(monitor, &mi);
        if (winX + winW > mi.rcWork.right || winX < mi.rcWork.left) {
            winX = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - winW) / 2;
        }
        if (winY + winH > mi.rcWork.bottom || winY < mi.rcWork.top) {
            winY = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - winH) / 2;
        }

        SetWindowPos(hwnd_, HWND_NOTOPMOST, winX, winY, winW, winH,
                     SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    }

    displayMode_ = mode;
}

void WinApp::SetWindowsKeyLock(bool enable) {
    windowsKeyLockEnabled_ = enable;
    globalKeyLockEnabled_ = enable;
    activeHwnd_ = hwnd_;

    if (enable) {
        if (!keyboardHook_) {
            keyboardHook_ =
                SetWindowsHookExW(WH_KEYBOARD_LL, &WinApp::LowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
        }
    } else {
        if (keyboardHook_) {
            UnhookWindowsHookEx(keyboardHook_);
            keyboardHook_ = nullptr;
        }
    }
}

LRESULT CALLBACK WinApp::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= HC_ACTION && globalKeyLockEnabled_) {
        auto* pKbd = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (pKbd) {
            // Alt + F4 は絶対にブロックせずOSへ通す（強制終了・脱出の保証）
            bool isAltDown = (pKbd->flags & LLKHF_ALTDOWN) != 0;
            if (isAltDown && pKbd->vkCode == VK_F4) {
                return CallNextHookEx(keyboardHook_, nCode, wParam, lParam);
            }

            // 自ウィンドウがアクティブな時のみWindowsキー/アプリキーを破棄
            if (GetForegroundWindow() == activeHwnd_) {
                if (pKbd->vkCode == VK_LWIN || pKbd->vkCode == VK_RWIN || pKbd->vkCode == VK_APPS) {
                    // 1 を返してOSへのキーイベント伝播を破棄
                    return 1;
                }
            }
        }
    }
    return CallNextHookEx(keyboardHook_, nCode, wParam, lParam);
}