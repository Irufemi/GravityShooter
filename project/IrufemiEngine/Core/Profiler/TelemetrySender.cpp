#include "Core/Profiler/TelemetrySender.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

using namespace Irufemi;

struct TelemetrySender::NetworkData {
    SOCKET udpSocket_ = INVALID_SOCKET;
    sockaddr_in targetAddr_{};
};

TelemetrySender::TelemetrySender() : networkData_(std::make_unique<NetworkData>()) {}

TelemetrySender::~TelemetrySender() {
    Finalize();
}

void TelemetrySender::Initialize(const std::string& targetIp, uint16_t targetPort) {
    if (isRunning_) {
        return;
    }

    // Winsock初期化
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        isWsaInitialized_ = true;

        // UDPソケット作成
        networkData_->udpSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (networkData_->udpSocket_ != INVALID_SOCKET) {
            // ノンブロッキングモードに設定（万が一のための安全策）
            u_long mode = 1;
            ioctlsocket(networkData_->udpSocket_, FIONBIO, &mode);

            // 送信先アドレスの設定
            networkData_->targetAddr_.sin_family = AF_INET;
            networkData_->targetAddr_.sin_port = htons(targetPort);
            inet_pton(AF_INET, targetIp.c_str(), &networkData_->targetAddr_.sin_addr);
        }
    }

    // スレッド起動
    isRunning_ = true;
    workerThread_ = std::thread(&TelemetrySender::ThreadLoop, this);
}

void TelemetrySender::Finalize() {
    if (isRunning_) {
        isRunning_ = false;
        triggerSend_ = true;
        cv_.notify_all();

        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }

    if (networkData_->udpSocket_ != INVALID_SOCKET) {
        closesocket(networkData_->udpSocket_);
        networkData_->udpSocket_ = INVALID_SOCKET;
    }

    if (isWsaInitialized_) {
        WSACleanup();
        isWsaInitialized_ = false;
    }
}

void TelemetrySender::SetMetric(const std::string& key, const nlohmann::json& value) {
    if (!isRunning_) {
        return;
    }
    std::lock_guard<std::mutex> lock(dataMutex_);
    currentMetrics_[key] = value;
}

void TelemetrySender::LogEvent(const std::string& message) {
    if (!isRunning_) {
        return;
    }
    std::lock_guard<std::mutex> lock(dataMutex_);
    pendingEvents_.push_back(message);
}

void TelemetrySender::OnFrameEnd() {
    if (!isRunning_) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        triggerSend_ = true;
    }
    cv_.notify_one();
}

void TelemetrySender::ThreadLoop() {
    while (isRunning_) {
        nlohmann::json metricsToSend;
        std::vector<std::string> eventsToSend;

        {
            std::unique_lock<std::mutex> lock(dataMutex_);
            // triggerSend_ が true になるか、終了(isRunning_ == false)になるまで待機
            cv_.wait(lock, [this]() { return triggerSend_.load() || !isRunning_.load(); });

            if (!isRunning_) {
                break;
            }

            // 送信用のデータをコピー
            metricsToSend = currentMetrics_;
            eventsToSend = std::move(pendingEvents_);

            // イベントログキューをクリア
            pendingEvents_.clear();
            triggerSend_ = false;
        }

        // UDPソケットが無効ならスキップ
        if (networkData_->udpSocket_ == INVALID_SOCKET) {
            continue;
        }

        // イベントをメトリクスに追加して送信
        if (!eventsToSend.empty()) {
            metricsToSend["Events"] = eventsToSend;
        }

        // JSONを文字列化
        std::string payload = metricsToSend.dump();

        // データを送信
        sendto(networkData_->udpSocket_, payload.c_str(), static_cast<int>(payload.size()), 0,
               (SOCKADDR*)&networkData_->targetAddr_, sizeof(networkData_->targetAddr_));
    }
}
