#include "Core/System/DirectoryWatcher.h"
#include "Core/Utility/ErrorUtility.h"
#include <windows.h>

using namespace Irufemi;

DirectoryWatcher::DirectoryWatcher(const std::filesystem::path& targetDirectory, std::function<void()> onChangeCallback)
    : targetDirectory_(targetDirectory), onChangeCallback_(onChangeCallback), isRunning_(true) {

    // ディレクトリハンドルの取得 (FILE_FLAG_OVERLAPPED を指定して非同期待機可能にする)
    directoryHandle_ = CreateFileW(targetDirectory_.c_str(), FILE_LIST_DIRECTORY,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                                   FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

    if (directoryHandle_ == INVALID_HANDLE_VALUE) {
        IRUFEMI_WARNING(false, "DirectoryWatcher: Failed to open directory handle.");
        directoryHandle_ = nullptr;
        isRunning_ = false;
        return;
    }

    // 終了通知用イベントの作成
    stopEvent_ = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!stopEvent_) {
        IRUFEMI_WARNING(false, "DirectoryWatcher: Failed to create stop event.");
        CloseHandle(directoryHandle_);
        directoryHandle_ = nullptr;
        isRunning_ = false;
        return;
    }

    workerThread_ = std::thread(&DirectoryWatcher::WatchLoop, this);
}

DirectoryWatcher::~DirectoryWatcher() {
    isRunning_ = false;
    if (stopEvent_) {
        SetEvent(stopEvent_);
    }
    if (directoryHandle_) {
        CancelIoEx(directoryHandle_, NULL);
    }
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    if (stopEvent_) {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
    if (directoryHandle_) {
        CloseHandle(directoryHandle_);
        directoryHandle_ = nullptr;
    }
}

void DirectoryWatcher::WatchLoop() {
    alignas(DWORD) char buffer[4096];
    DWORD bytesReturned = 0;
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!overlapped.hEvent) {
        IRUFEMI_WARNING(false, "DirectoryWatcher: Failed to create overlapped event.");
        isRunning_ = false;
        return;
    }

    HANDLE handles[2] = {stopEvent_, overlapped.hEvent};

    while (isRunning_) {
        ResetEvent(overlapped.hEvent);
        BOOL success = ReadDirectoryChangesW(directoryHandle_, buffer, sizeof(buffer),
                                             TRUE, // サブディレクトリも監視
                                             FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                                 FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
                                                 FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
                                             &bytesReturned, &overlapped, NULL);

        if (!success && GetLastError() != ERROR_IO_PENDING) {
            break;
        }

        // 終了イベントまたはI/O完了イベントのいずれかを待機
        DWORD waitResult = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            // stopEvent_ がシグナル化されたため待機をキャンセルして即座に終了
            CancelIoEx(directoryHandle_, &overlapped);
            DWORD transferred = 0;
            GetOverlappedResult(directoryHandle_, &overlapped, &transferred, TRUE);
            break;
        } else if (waitResult == WAIT_OBJECT_0 + 1) {
            // ファイル変更完了
            DWORD transferred = 0;
            if (GetOverlappedResult(directoryHandle_, &overlapped, &transferred, FALSE)) {
                // --- デバウンス処理 (Debounce): 終了シグナルで即時中断可能な待機 ---
                if (WaitForSingleObject(stopEvent_, 200) == WAIT_OBJECT_0) {
                    break;
                }

                if (isRunning_ && onChangeCallback_) {
                    onChangeCallback_();
                }
            }
        } else {
            CancelIoEx(directoryHandle_, &overlapped);
            DWORD transferred = 0;
            GetOverlappedResult(directoryHandle_, &overlapped, &transferred, TRUE);
            break;
        }
    }

    CloseHandle(overlapped.hEvent);
}
