#include "Core/Profiler/TelemetryReceiver.h"
#include "Framework/Utility/ConsoleCommandSystem.h"
#include "Core/Utility/Log.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

struct TelemetryReceiver::NetworkData {
    SOCKET udpSocket_ = INVALID_SOCKET;
    sockaddr_in listenAddr_{};
};

TelemetryReceiver::TelemetryReceiver() : networkData_(std::make_unique<NetworkData>()) {}

TelemetryReceiver::~TelemetryReceiver() {
    Finalize();
}

void TelemetryReceiver::Initialize(uint16_t listenPort) {
    if (isRunning_) {
        return;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        isWsaInitialized_ = true;

        networkData_->udpSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (networkData_->udpSocket_ != INVALID_SOCKET) {
            // アドレス再利用可能設定
            int reuse = 1;
            setsockopt(networkData_->udpSocket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
                       sizeof(reuse));

            // バインド設定 (127.0.0.1:8889)
            networkData_->listenAddr_.sin_family = AF_INET;
            networkData_->listenAddr_.sin_port = htons(listenPort);
            networkData_->listenAddr_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

            if (bind(networkData_->udpSocket_, reinterpret_cast<sockaddr*>(&networkData_->listenAddr_),
                     sizeof(networkData_->listenAddr_)) == SOCKET_ERROR) {
                closesocket(networkData_->udpSocket_);
                networkData_->udpSocket_ = INVALID_SOCKET;
                Log::OutPutLog(std::cerr, "[TelemetryReceiver] Failed to bind to port " + std::to_string(listenPort) + "\n");
                return;
            }
        }
    }

    if (networkData_->udpSocket_ != INVALID_SOCKET) {
        isRunning_ = true;
        workerThread_ = std::thread(&TelemetryReceiver::ThreadLoop, this);
        Log::OutPutLog(std::cout, "[TelemetryReceiver] Listening on 127.0.0.1:" + std::to_string(listenPort) + "\n");
    }
}

void TelemetryReceiver::Finalize() {
    if (isRunning_) {
        isRunning_ = false;

        if (networkData_ && networkData_->udpSocket_ != INVALID_SOCKET) {
            closesocket(networkData_->udpSocket_);
            networkData_->udpSocket_ = INVALID_SOCKET;
        }

        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }

    if (isWsaInitialized_) {
        WSACleanup();
        isWsaInitialized_ = false;
    }
}

void TelemetryReceiver::ThreadLoop() {
    constexpr size_t kBufferSize = 4096;
    std::vector<char> buffer(kBufferSize);

    while (isRunning_) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(networkData_->udpSocket_, &readSet);

        // 100ms タイムアウトで select を監視（スレッド停止時の即座な復帰用）
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int selectRet = select(0, &readSet, nullptr, nullptr, &tv);
        if (selectRet > 0 && FD_ISSET(networkData_->udpSocket_, &readSet)) {
            sockaddr_in senderAddr{};
            int senderAddrLen = sizeof(senderAddr);
            int bytesReceived = recvfrom(networkData_->udpSocket_, buffer.data(), static_cast<int>(kBufferSize - 1), 0,
                                         reinterpret_cast<sockaddr*>(&senderAddr), &senderAddrLen);

            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0';
                std::string receivedStr(buffer.data(), bytesReceived);

                // 末尾の改行コードをトリミング
                while (!receivedStr.empty() && (receivedStr.back() == '\r' || receivedStr.back() == '\n')) {
                    receivedStr.pop_back();
                }

                if (!receivedStr.empty()) {
                    std::lock_guard<std::mutex> lock(commandQueueMutex_);
                    pendingCommands_.push(receivedStr);
                }
            }
        }
    }
}

void TelemetryReceiver::ProcessPendingCommands() {
    std::vector<std::string> commandsToRun;
    {
        std::lock_guard<std::mutex> lock(commandQueueMutex_);
        while (!pendingCommands_.empty()) {
            commandsToRun.push_back(std::move(pendingCommands_.front()));
            pendingCommands_.pop();
        }
    }

    for (const auto& cmd : commandsToRun) {
        Log::OutPutLog(std::cout, "[TelemetryReceiver] Executing remote command: '" + cmd + "'\n");
        Irufemi::ConsoleCommandSystem::ExecuteCommand(cmd);
    }
}
