#include "Core/Utility/Log.h"
#include "Core/Utility/StringUtility.h"
#include <Windows.h>
#include <iostream>

/*ログを出そう*/

#include <format>

// ファイルやディレクトリに関する操作を行うライブラリ
#include <filesystem>

// ファイルに書いたり読んだりするライブラリ
#include <fstream>

// 時間を扱うライブラリ
#include <chrono>

#include "Core/Utility/FileSystem.h"

void Log::Initialize() {

    /*ログを出そう*/

    // ログのディレクトリを用意
    std::filesystem::create_directories(FileSystem::GetLogPath());

    // 現在時刻を取得
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    // ログファイルの名前にコンマ何秒はいらないので、削って秒にする
    std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> nowSeconds =
        std::chrono::time_point_cast<std::chrono::seconds>(now);
    // 日本時間(PCの設定時間)に変換
    std::chrono::zoned_time localTime{std::chrono::current_zone(), nowSeconds};
    // formatを使って毎月日_時分秒の文字列に変換
    std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
    // 時刻を使ってファイル名を決定
    std::string logFilePath = FileSystem::GetLogPath() + "/" + dateString + ".log";
    // ファイルを使って書き込み準備
    logStream_.open(logFilePath, std::ios::out | std::ios::trunc);

    // 出力ウィンドウへの文字出力
    OutPutLog(std::cout, "Hello,DirectX!\n");
}

/*ログを出そう*/

// 出力ウィンドウに文字を出す
void Log::OutPutLog(std::ostream& os, const std::string& message) {
    os << message << "\n";
    // UTF-8文字列を安全にワイド文字列(UTF-16)へ変換して出力(文字化け防止)
    OutputDebugStringW(ConvertString(message).c_str());

    bool isError = (&os == &std::cerr);

    std::lock_guard<std::mutex> lock(logMutex_);
    if (logStream_.is_open()) {
        logStream_ << message << "\n";
    }
    logHistory_.push_back({message, isError});
    if (logHistory_.size() > kMaxLogLines) {
        logHistory_.pop_front(); // std::deque による O(1) 先頭削除
    }
}
