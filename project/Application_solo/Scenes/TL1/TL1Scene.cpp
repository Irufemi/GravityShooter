#include "Scenes/TL1/TL1Scene.h"
#include "Scenes/TL1/MagicBrushClient.h"
#include "Engine/Irufemi.h"
#include "RHI/DirectX12/ShaderCompiler.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/ShaderManager.h"
#include "RHI/DirectX12/RootSignatureConfig.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Resource/Texture/TextureManager.h"
#include "Platform/WindowsAPI/WinApp.h"
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <filesystem>
#include "Core/Utility/StringUtility.h"
#include "Scenes/TL1/TL1LevelLoader.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/Utility/Log.h"
#include <iostream>
#include <unordered_map>

#ifdef USE_IMGUI

#include "imgui.h"
#endif

TL1Scene::~TL1Scene() = default;

/**
 * @brief 初期化
 */
void TL1Scene::Initialize(IrufemiEngine* engine) {
    BaseScene::Initialize(engine);
    magicBrushClient_ = std::make_unique<MagicBrushClient>();

    std::string errorLog;
    ShaderCompileOptions options;
    options.entryPoint = L"main";
    vsBlob_ = engine_->GetDirectXCommon()->GetShaderManager()->GetOrCompile(L"Fullscreen.VS.hlsl", options, L"vs_6_0",
                                                                            &errorLog);

    // Pythonサーバーの自動起動
    magicBrushClient_->StartPythonServer();

    // Blenderレベルデータの読み込み
    LevelData levelData = TL1LevelLoader::Load("resources/Levels/TL1.json", this);

    // プレイヤー配置データからテスト用ダミープレイヤーを配置
    if (!levelData.players.empty()) {
        auto& playerData = levelData.players[0];
        auto dummyPlayer = std::make_shared<GameObject>();
        dummyPlayer->SetName("Player");
        dummyPlayer->SetScene(this);

        if (auto transform = dummyPlayer->GetTransform()) {
            transform->SetPosition(playerData.translation);
            transform->SetRotation(playerData.rotation);
        }

        this->AddGameObject(dummyPlayer);

        Log::OutPutLog(std::cout, "[TL1Scene] Dummy Player spawned at (" + std::to_string(playerData.translation.x) +
                                      ", " + std::to_string(playerData.translation.y) + ", " +
                                      std::to_string(playerData.translation.z) + ")\n");
    }

    // 敵配置データから敵を配置
    for (const auto& enemyData : levelData.enemies) {
        auto dummyEnemy = std::make_shared<GameObject>();
        dummyEnemy->SetName("Enemy");
        dummyEnemy->SetScene(this);

        if (auto transform = dummyEnemy->GetTransform()) {
            transform->SetPosition(enemyData.translation);
            transform->SetRotation(enemyData.rotation);
        }

        this->AddGameObject(dummyEnemy);

        Log::OutPutLog(std::cout, "[TL1Scene] Dummy Enemy spawned at (" + std::to_string(enemyData.translation.x) +
                                      ", " + std::to_string(enemyData.translation.y) + ", " +
                                      std::to_string(enemyData.translation.z) + ")\n");
    }
}

/**
 * @brief 更新
 */
void TL1Scene::Update() {
    BaseScene::Update();

    // 成功したシェーダーをPSOに登録する
    if (magicBrushClient_ && !isShaderRegistered_ && vsBlob_) {
        if (magicBrushClient_->GetState() == MagicBrushClient::State::Success) {
            auto psBlob = magicBrushClient_->GetResultBlob();
            if (psBlob) {
                engine_->GetPSOManager()->RegisterShader(shaderName_, {{vsBlob_, psBlob, nullptr}});
                isShaderRegistered_ = true;
            }
        }
    }
}

/**
 * @brief 描画
 */
void TL1Scene::Draw() {
    BaseScene::Draw();

    // プレビュー描画
    if (isShaderRegistered_) {
        engine_->ApplyPSO(shaderName_);
        auto cmd = engine_->GetCommandList();

        // 入力されたテクスチャパス(名)があればそれをテクスチャとしてロードしてセット、無ければダミー(白)をセットする
        uint32_t srvIdx = 0;
        if (!textureImagePath_.empty()) {
            ResourceHandle rHandle = engine_->GetTextureManager()->LoadTexture(textureImagePath_);
            srvIdx = engine_->GetTextureManager()->GetSrvIndex(rHandle);
        }
        if (auto ppMgr = engine_->GetPostProcessManager()) {
            D3D12_GPU_VIRTUAL_ADDRESS bindlessCBV = ppMgr->AllocateBindlessParams(srvIdx);
            cmd->SetGraphicsRootConstantBufferView(static_cast<UINT>(RootSlot::LightCommon), bindlessCBV);
        }

        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmd->DrawInstanced(3, 1, 0, 0);
    }
}

/**
 * @brief シーン固有の独立デバッグウィンドウ描画
 */
void TL1Scene::DrawStandaloneDebugWindows() {
#ifdef USE_IMGUI
    if (ImGui::Begin("AI Magic Brush")) {
        ImGui::Text("AI Shader Generator Interface");
        ImGui::Separator();

        // 文字列入力用のバッファ（静的変数にして入力を保持）
        static char promptBuffer[512] = "";
        static char refImagePathBuffer[MAX_PATH] = "";
        static char shaderNameBuffer[128] = "";
        static char outDirBuffer[MAX_PATH] = "resources/shaders/generated/";

        // 簡易通知メッセージ用の静的変数
        static std::string notificationMsg = "";
        static float notificationTimer = 0.0f;

        // D&Dの受け取り
        if (engine_ && engine_->GetWinApp()) {
            std::string dropped = engine_->GetWinApp()->GetDroppedFilePath();
            if (!dropped.empty()) {
                strncpy_s(refImagePathBuffer, dropped.c_str(), _TRUNCATE);
                engine_->GetWinApp()->ClearDroppedFilePath();
            }
        }

        // サーバー状態と再起動UI
        ImGui::Separator();
        bool isRunning = magicBrushClient_ && magicBrushClient_->IsServerRunning();
        ImGui::Text("Server Status: %s", isRunning ? "Running" : "Stopped");
        if (isRunning) {
            if (ImGui::Button("Restart Server")) {
                magicBrushClient_->RestartPythonServer();
                notificationMsg = "Python Server Restarted.";
                notificationTimer = 3.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop Server")) {
                magicBrushClient_->StopPythonServer();
                notificationMsg = "Python Server Stopped.";
                notificationTimer = 3.0f;
            }
        } else {
            if (ImGui::Button("Start Server")) {
                magicBrushClient_->StartPythonServer();
                notificationMsg = "Python Server Started.";
                notificationTimer = 3.0f;
            }
        }
        ImGui::Separator();

        ImGui::InputText("Prompt", promptBuffer, sizeof(promptBuffer));

        // Reference Image (AI参考画像)
        ImGui::InputText("Reference Image", refImagePathBuffer, sizeof(refImagePathBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            OPENFILENAMEA ofn;
            char szFile[MAX_PATH] = {0};
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = engine_->GetWinApp() ? engine_->GetWinApp()->GetHwnd() : nullptr;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameA(&ofn) == TRUE) {
                strncpy_s(refImagePathBuffer, ofn.lpstrFile, _TRUNCATE);
            }
        }

        // Input Texture (C++バインド用)
        static int selectedTextureIndex = 0;
        std::vector<std::string> textureNames = engine_->GetTextureManager()->GetTextureNames();
        // コンボボックス用に const char* の配列を作る
        std::vector<const char*> comboItems;
        comboItems.push_back("None (White Texture)"); // 先頭に空要素を入れる
        for (const auto& name : textureNames) {
            comboItems.push_back(name.c_str());
        }
        if (ImGui::Combo("Input Texture", &selectedTextureIndex, comboItems.data(),
                         static_cast<int>(comboItems.size()))) {
            if (selectedTextureIndex == 0) {
                textureImagePath_ = "";
            } else {
                textureImagePath_ = textureNames[selectedTextureIndex - 1];
            }
        }

        ImGui::Separator();
        ImGui::Text("Shader Settings");

        // Output Directory (出力先フォルダ)
        ImGui::InputText("Output Directory", outDirBuffer, sizeof(outDirBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Browse Dir...")) {
            IFileOpenDialog* pfd = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
                DWORD dwOptions;
                if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
                    pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);
                }

                HWND hwnd = engine_->GetWinApp() ? engine_->GetWinApp()->GetHwnd() : nullptr;
                if (SUCCEEDED(pfd->Show(hwnd))) {
                    IShellItem* psi = nullptr;
                    if (SUCCEEDED(pfd->GetResult(&psi))) {
                        PWSTR pszFilePath = nullptr;
                        if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                            // WideChar -> UTF-8 マルチバイト変換
                            int size = WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, nullptr, 0, nullptr, nullptr);
                            if (size > 0 && size <= MAX_PATH) {
                                WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, outDirBuffer, size, nullptr, nullptr);

                                // パスの末尾が '/' または '\' でない場合は補完
                                size_t len = strlen(outDirBuffer);
                                if (len > 0 && outDirBuffer[len - 1] != '/' && outDirBuffer[len - 1] != '\\') {
                                    strncat_s(outDirBuffer, "/", _TRUNCATE);
                                }
                            }
                            CoTaskMemFree(pszFilePath);
                        }
                        psi->Release();
                    }
                }
                pfd->Release();
            }
        }

        ImGui::InputText("Shader Name", shaderNameBuffer, sizeof(shaderNameBuffer));
        ImGui::TextDisabled("(Leave empty to auto-generate name)");

        // サーバーが動いていない時はボタンを無効化する
        if (!isRunning) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Generate Shader")) {
            promptText_ = promptBuffer;
            referenceImagePath_ = refImagePathBuffer;

            // シェーダー名の決定
            std::string inputName = shaderNameBuffer;
            if (inputName.empty()) {
                std::time_t t = std::time(nullptr);
                std::tm tm_info;
                localtime_s(&tm_info, &t);
                std::ostringstream oss;
                oss << "GenShader_" << std::put_time(&tm_info, "%m%d_%H%M%S");
                shaderName_ = oss.str();
            } else {
                shaderName_ = inputName;
            }
            outputDirectory_ = outDirBuffer;

            if (magicBrushClient_) {
                isShaderRegistered_ = false; // 再登録できるようにフラグをリセット
                magicBrushClient_->StartGeneration(promptText_, referenceImagePath_, shaderName_, outputDirectory_,
                                                   engine_->GetDirectXCommon()->GetShaderManager());
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Evaluate & Fix")) {
            if (magicBrushClient_ && isShaderRegistered_ &&
                magicBrushClient_->GetState() == MagicBrushClient::State::Success) {
                const auto& history = magicBrushClient_->GetHistory();
                if (!history.empty()) {
                    std::string currentHlsl = history.back().hlslCode;
                    std::string refImg = referenceImagePath_;
                    std::string sName = shaderName_;
                    std::wstring relPath = L"resources/screenshots/temp_evaluate.png";
                    std::error_code ec;
                    std::filesystem::create_directories("resources/screenshots", ec);
                    std::wstring screenshotPath = std::filesystem::absolute(relPath).wstring();

                    auto* captureManager = engine_->GetScreenCaptureManager();
                    if (captureManager) {
                        notificationMsg = "Taking screenshot for AI evaluation...";
                        captureManager->RequestCapture(
                            screenshotPath, ScreenCaptureType::SceneOnly,
                            [this, currentHlsl, refImg, sName, screenshotPath]() {
                                std::string u8path = ConvertString(screenshotPath);
                                magicBrushClient_->StartVisualFix(refImg, u8path, currentHlsl, sName,
                                                                  engine_->GetDirectXCommon()->GetShaderManager());
                                isShaderRegistered_ = false; // 新しいシェーダーができたら再バインドさせるため
                            });
                    }
                }
            } else {
                notificationMsg = "Generate a shader successfully before evaluating.";
            }
        }

        if (!isRunning) {
            ImGui::EndDisabled();
        }

        if (magicBrushClient_) {
            ImGui::Separator();
            struct StateDisplayInfo {
                ImVec4 color;
                const char* text;
            };
            static const std::unordered_map<MagicBrushClient::State, StateDisplayInfo> kStateDisplayTable = {
                {MagicBrushClient::State::Idle, {ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Status: Idle"}},
                {MagicBrushClient::State::Generating,
                 {ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: Generating initial HLSL..."}},
                {MagicBrushClient::State::Compiling, {ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: Compiling..."}},
                {MagicBrushClient::State::Fixing, {ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Status: Fixing Compile Errors..."}},
                {MagicBrushClient::State::WaitingForScreenshot,
                 {ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "Status: Waiting for screenshot..."}},
                {MagicBrushClient::State::VisualEvaluating,
                 {ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Status: Visual Evaluating & Fixing..."}},
                {MagicBrushClient::State::Success, {ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Success!"}},
                {MagicBrushClient::State::Error, {ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Status: Error"}},
            };

            auto state = magicBrushClient_->GetState();
            if (auto it = kStateDisplayTable.find(state); it != kStateDisplayTable.end()) {
                ImGui::TextColored(it->second.color, "%s", it->second.text);
            }
            if (state == MagicBrushClient::State::Error) {
                ImGui::TextWrapped("%s", magicBrushClient_->GetErrorMessage().c_str());
            }

            ImGui::Separator();
            if (ImGui::CollapsingHeader("Server Console", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::BeginChild("LogConsole", ImVec2(0, 150), true, ImGuiWindowFlags_HorizontalScrollbar);
                auto logs = magicBrushClient_->GetServerLogs();
                for (const auto& log : logs) {
                    ImGui::TextUnformatted(log.c_str());
                }
                // 自動スクロール
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
                ImGui::EndChild();
            }

            ImGui::Separator();
            if (ImGui::CollapsingHeader("Generation History")) {
                const auto& history = magicBrushClient_->GetHistory();
                if (history.empty()) {
                    ImGui::TextDisabled("No history yet.");
                } else {
                    static int selectedHistoryIndex = -1;

                    ImGui::BeginChild("HistoryList", ImVec2(0, 100), true);
                    for (int i = static_cast<int>(history.size()) - 1; i >= 0; --i) {
                        const auto& item = history[i];
                        std::string label = "[" + std::to_string(i) + "] " + item.shaderName +
                                            " (Prompt: " + item.prompt.substr(0, 20) + "...)";
                        if (ImGui::Selectable(label.c_str(), selectedHistoryIndex == i)) {
                            selectedHistoryIndex = i;
                        }
                    }
                    ImGui::EndChild();

                    if (selectedHistoryIndex >= 0 && selectedHistoryIndex < history.size()) {
                        if (ImGui::Button("Restore Selected History")) {
                            const auto& item = history[selectedHistoryIndex];
                            strncpy_s(promptBuffer, item.prompt.c_str(), _TRUNCATE);
                            strncpy_s(shaderNameBuffer, item.shaderName.c_str(), _TRUNCATE);
                            shaderName_ = item.shaderName; // 実際の適用名も更新

                            // HistoryのHLSLを直接復元して再適用する
                            isShaderRegistered_ = false; // 再バインドを促す
                            if (magicBrushClient_->RestoreHistory(selectedHistoryIndex,
                                                                  engine_->GetDirectXCommon()->GetShaderManager())) {
                                notificationMsg = "Restored and compiled shader from history.";
                            } else {
                                notificationMsg = "Failed to restore shader from history.";
                            }
                            notificationTimer = 3.0f;
                        }
                    }
                }
            }
        }

        // 通知メッセージの描画とタイマー減算
        if (notificationTimer > 0.0f) {
            notificationTimer -= ImGui::GetIO().DeltaTime;
            // フェードアウト効果 (アルファ値をタイマーに連動させる)
            float alpha = notificationTimer > 1.0f ? 1.0f : notificationTimer;
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, alpha), "[System] %s", notificationMsg.c_str());
        }
    }
    ImGui::End();
#endif
}
