#include "Core/Profiler/TelemetryReceiver.h"
#include "Framework/Utility/ConsoleCommandSystem.h"
#include "Core/Utility/Log.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

struct TelemetryReceiver::NetworkData {
    SOCKET listenSocket_ = INVALID_SOCKET;
    sockaddr_in listenAddr_{};
    std::unordered_map<uint64_t, SOCKET> clients_;
    std::unordered_map<uint64_t, std::string> clientBuffers_;
    uint64_t nextClientId_ = 1;
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

        networkData_->listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (networkData_->listenSocket_ != INVALID_SOCKET) {
            // アドレス再利用可能設定
            int reuse = 1;
            setsockopt(networkData_->listenSocket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
                       sizeof(reuse));

            // バインド設定 (127.0.0.1:8889)
            networkData_->listenAddr_.sin_family = AF_INET;
            networkData_->listenAddr_.sin_port = htons(listenPort);
            networkData_->listenAddr_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

            if (bind(networkData_->listenSocket_, reinterpret_cast<sockaddr*>(&networkData_->listenAddr_),
                     sizeof(networkData_->listenAddr_)) == SOCKET_ERROR) {
                closesocket(networkData_->listenSocket_);
                networkData_->listenSocket_ = INVALID_SOCKET;
                Log::OutPutLog(std::cerr, "[TelemetryReceiver] Failed to bind TCP port " + std::to_string(listenPort) + "\n");
                return;
            }

            if (listen(networkData_->listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
                closesocket(networkData_->listenSocket_);
                networkData_->listenSocket_ = INVALID_SOCKET;
                Log::OutPutLog(std::cerr, "[TelemetryReceiver] Failed to listen on TCP port " + std::to_string(listenPort) + "\n");
                return;
            }
        }
    }

    if (networkData_->listenSocket_ != INVALID_SOCKET) {
        isRunning_ = true;
        workerThread_ = std::thread(&TelemetryReceiver::ThreadLoop, this);
        Log::OutPutLog(std::cout, "[TelemetryReceiver] TCP Server listening on 127.0.0.1:" + std::to_string(listenPort) + "\n");
    }
}

void TelemetryReceiver::Finalize() {
    if (isRunning_) {
        isRunning_ = false;

        if (networkData_) {
            if (networkData_->listenSocket_ != INVALID_SOCKET) {
                closesocket(networkData_->listenSocket_);
                networkData_->listenSocket_ = INVALID_SOCKET;
            }
            for (auto& [id, sock] : networkData_->clients_) {
                if (sock != INVALID_SOCKET) {
                    closesocket(sock);
                }
            }
            networkData_->clients_.clear();
            networkData_->clientBuffers_.clear();
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
    std::vector<char> recvBuffer(kBufferSize);

    while (isRunning_) {
        // 1. 送信待ちレスポンスをクライアントへ配信
        std::vector<ResponseMessage> responsesToSend;
        {
            std::lock_guard<std::mutex> lock(responseQueueMutex_);
            responsesToSend = std::move(pendingResponses_);
            pendingResponses_.clear();
        }

        for (const auto& resp : responsesToSend) {
            auto it = networkData_->clients_.find(resp.clientId);
            if (it != networkData_->clients_.end() && it->second != INVALID_SOCKET) {
                send(it->second, resp.message.c_str(), static_cast<int>(resp.message.length()), 0);
            }
        }

        // 2. select で listenSocket および接続中クライアントの受信監視
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(networkData_->listenSocket_, &readSet);

        SOCKET maxSock = networkData_->listenSocket_;
        for (const auto& [id, sock] : networkData_->clients_) {
            FD_SET(sock, &readSet);
            if (sock > maxSock) {
                maxSock = sock;
            }
        }

        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 50000; // 50ms タイムアウト

        int selectRet = select(0, &readSet, nullptr, nullptr, &tv);
        if (selectRet <= 0) {
            continue;
        }

        // 新規接続の受付
        if (FD_ISSET(networkData_->listenSocket_, &readSet)) {
            sockaddr_in clientAddr{};
            int clientAddrLen = sizeof(clientAddr);
            SOCKET newClient = accept(networkData_->listenSocket_, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
            if (newClient != INVALID_SOCKET) {
                uint64_t clientId = networkData_->nextClientId_++;
                networkData_->clients_[clientId] = newClient;
                networkData_->clientBuffers_[clientId] = "";
                Log::OutPutLog(std::cout, "[TelemetryReceiver] TCP Client connected (ID: " + std::to_string(clientId) + ")\n");
            }
        }

        // 接続中クライアントからのデータ受信
        std::vector<uint64_t> disconnectedClients;
        for (auto& [id, sock] : networkData_->clients_) {
            if (FD_ISSET(sock, &readSet)) {
                int bytesReceived = recv(sock, recvBuffer.data(), static_cast<int>(kBufferSize - 1), 0);
                if (bytesReceived > 0) {
                    recvBuffer[bytesReceived] = '\0';
                    std::string& clientBuf = networkData_->clientBuffers_[id];
                    clientBuf.append(recvBuffer.data(), bytesReceived);

                    // 改行区切りでコマンドを取り出す
                    size_t newlinePos = std::string::npos;
                    while ((newlinePos = clientBuf.find('\n')) != std::string::npos) {
                        std::string cmd = clientBuf.substr(0, newlinePos);
                        clientBuf.erase(0, newlinePos + 1);

                        // \r をトリミング
                        if (!cmd.empty() && cmd.back() == '\r') {
                            cmd.pop_back();
                        }

                        if (!cmd.empty()) {
                            std::lock_guard<std::mutex> lock(commandQueueMutex_);
                            pendingCommands_.push({cmd, id});
                        }
                    }
                } else {
                    // 切断またはエラー
                    disconnectedClients.push_back(id);
                }
            }
        }

        // 切断されたクライアントのクリーンアップ
        for (uint64_t id : disconnectedClients) {
            auto it = networkData_->clients_.find(id);
            if (it != networkData_->clients_.end()) {
                closesocket(it->second);
                networkData_->clients_.erase(it);
                networkData_->clientBuffers_.erase(id);
                Log::OutPutLog(std::cout, "[TelemetryReceiver] TCP Client disconnected (ID: " + std::to_string(id) + ")\n");
            }
        }
    }
}

void TelemetryReceiver::ProcessPendingCommands() {
    std::vector<PendingCommand> commandsToRun;
    {
        std::lock_guard<std::mutex> lock(commandQueueMutex_);
        while (!pendingCommands_.empty()) {
            commandsToRun.push_back(std::move(pendingCommands_.front()));
            pendingCommands_.pop();
        }
    }

    if (commandsToRun.empty()) {
        return;
    }

    std::vector<ResponseMessage> responses;
    for (const auto& item : commandsToRun) {
        Log::OutPutLog(std::cout, "[TelemetryReceiver] Executing TCP remote command: '" + item.command + "'\n");
        std::string resultMsg;
        Irufemi::ConsoleCommandSystem::ExecuteCommand(item.command, &resultMsg);

        if (!resultMsg.empty()) {
            responses.push_back({item.clientId, resultMsg + "\n"});
        }
    }

    if (!responses.empty()) {
        std::lock_guard<std::mutex> lock(responseQueueMutex_);
        pendingResponses_.insert(pendingResponses_.end(), responses.begin(), responses.end());
    }
}
