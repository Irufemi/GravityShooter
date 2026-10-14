#include "Core/Utility/ErrorUtility.h"
#define NOMINMAX
#include "Framework/UI/DebugUI.h"
#include <Windows.h>

// #define USE_EDITOR

/*開発のUIを出そう*/

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "EngineResources/FontAwesome/IconsFontAwesome6.h"
#include "Core/Utility/FileSystem.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif // USE_IMGUI
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <filesystem>
#include <numeric>
#include "Resource/Texture/TextureManager.h"
#include "Framework/Scene/SceneManager.h"
#include "Core/Shape/Sphere.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Transform.h"
#include "Renderer/Data/DirectionalLight.h"
#include "Renderer/Data/PointLight.h"
#include "Renderer/Data/SpotLight.h"
#include "Renderer/Data/AreaLight.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "RHI/DirectX12/DescriptorPool.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/Data/Material.h"
#include "Resource/Model/Data/ObjModel.h"
#include "Resource/Model/Data/Animation.h"
#include "Renderer/System/Core/LineResource.h"
#include "Renderer/System/Core/Object3DResource.h"
#include "Renderer/System/Core/Object2DResource.h"

#include "Core/Math/Math.h"
#include "Renderer/Data/LightningParams.h"
#include "Framework/Utility/CVar.h"
#include "Framework/Scene/IScene.h"
#include "Renderer/ScreenCaptureManager.h"
#include "Renderer/System/ParticleGPU/GPUParticleManager.h"
#include "Renderer/DrawManager.h"
#include "Renderer/Pipeline/RenderGraph/RenderGraph.h"
#include "Core/System/ThreadPool.h"
#include "Renderer/ScreenCaptureManager.h"
#include <chrono>
#include <iomanip>

namespace {
#ifdef USE_IMGUI
/**
 * @struct LightEditorContext
 * @brief エディタでのライト新規作成用一時パラメータ（ViewとStateの分離）
 */
struct LightEditorContext {
    PointLight pointLight{};
    SpotLight spotLight{};
    AreaLight areaLight{};

    LightEditorContext() {
        pointLight.color = {1.0f, 1.0f, 1.0f, 1.0f};
        pointLight.position = {0.0f, 1.0f, 0.0f};
        pointLight.intensity = 1.0f;
        pointLight.radius = 10.0f;
        pointLight.decay = 1.0f;
        pointLight.isActive = 1;

        spotLight.color = {1.0f, 1.0f, 1.0f, 1.0f};
        spotLight.position = {0.0f, 1.0f, 0.0f};
        spotLight.intensity = 1.0f;
        spotLight.direction = {0.0f, -1.0f, 0.0f};
        spotLight.distance = 10.0f;
        spotLight.decay = 1.0f;
        spotLight.cosAngle = 0.86602540378f; // cos(pi / 6)
        spotLight.falloff = 1.0f;
        spotLight.isActive = 1;

        areaLight.color = {1.0f, 1.0f, 1.0f, 1.0f};
        areaLight.position = {0.0f, 1.0f, 0.0f};
        areaLight.intensity = 1.0f;
        areaLight.direction = {0.0f, -1.0f, 0.0f};
        areaLight.range = 10.0f;
        areaLight.size = {1.0f, 1.0f};
        areaLight.isActive = 1;
    }
};

LightEditorContext lightEditorContext_;
#endif // USE_IMGUI
} // namespace

void DebugUI::Initialize([[maybe_unused]] HWND hwnd, [[maybe_unused]] DirectXCommon* dxCommon) {
#ifdef USE_IMGUI
    /*開発UIをだそう*/
    const char* iniFileName = "imgui.ini";

#ifdef EditorMode
    iniFileName = "imgui_editor.ini";
#endif

    // exeのパス(UTF-8)を取得して、必ずexeと同じディレクトリにiniを保存・読み込みする
    std::string exePath = FileSystem::GetExePath();
    std::string exeDir = exePath.substr(0, exePath.find_last_of("/\\"));
    static std::string absoluteIniPath = exeDir + "/" + iniFileName;

    std::filesystem::path iniFsPath = std::filesystem::path(reinterpret_cast<const char8_t*>(absoluteIniPath.c_str()));

    // 初回起動時（iniが無い場合）に、リポジトリにコミットされているプリセットをコピーする
    if (!std::filesystem::exists(iniFsPath)) {
        // 1. カレントディレクトリ基準での検索（VSからの実行時用）
        std::string presetPath1 = FileSystem::GetEngineRoot() + "/EngineResources/default_imgui.ini";
        std::filesystem::path presetFsPath =
            std::filesystem::path(reinterpret_cast<const char8_t*>(presetPath1.c_str()));

        // 2. exeディレクトリ基準での検索（Editorビルドを直接実行した場合用）
        if (!std::filesystem::exists(presetFsPath)) {
            std::string presetPath2 = exeDir + "/../../IrufemiEngine/EngineResources/default_imgui.ini";
            presetFsPath = std::filesystem::path(reinterpret_cast<const char8_t*>(presetPath2.c_str()));
        }

        if (std::filesystem::exists(presetFsPath)) {
            std::error_code ec;
            std::filesystem::copy_file(presetFsPath, iniFsPath, ec);
        }
    }

    dxCommon_ = dxCommon;

    /*開発UIを出そう*/
    // ImGuiの初期化。詳細はさして重要ではないので解説は省略する。
    // こういうもんである
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();

    // 1. ベースフォント（英数字用）として FiraMono を読み込む
    std::string firaMonoPath = FileSystem::GetEngineRoot() + "/EngineResources/Fira_Mono/FiraMono-Regular.ttf";
    ImFont* baseFont = io.Fonts->AddFontFromFileTTF(firaMonoPath.c_str(), 16.0f);

    // フォントファイルが見つからなかった場合（exe単体起動時など）、デフォルトフォントを追加してクラッシュを防ぐ
    if (baseFont == nullptr) {
        io.Fonts->AddFontDefault();
    }

    // 2. 日本語フォントを MergeMode (結合モード) で読み込み、FiraMono にない文字を補完する
    ImFontConfig config;
    config.MergeMode = true;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msgothic.ttc", 16.0f, &config,
                                 io.Fonts->GetGlyphRangesJapanese());

    // 3. FontAwesome を MergeMode で読み込む
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = 16.0f; // アイコンの等幅調整
    static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
    if (baseFont !=
        nullptr) { // FontAwesome はパスが相対なので、もしFiraMonoが見つからない環境なら読み込みをスキップしてもよい
        std::string faPath = FileSystem::GetEngineRoot() + "/EngineResources/FontAwesome/fa-solid-900.ttf";
        io.Fonts->AddFontFromFileTTF(faPath.c_str(), 16.0f, &icons_config, icons_ranges);
    }

#ifdef EditorMode
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Dockingを有効にする
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // マルチビューポートを有効にする
#endif                                                  // EditorMode

    // 構築した絶対パス(UTF-8)をIniFilenameに設定
    io.IniFilename = absoluteIniPath.c_str();

    ImGui_ImplWin32_Init(hwnd);

    DescriptorPool* srvPool = dxCommon->GetSrvPool();
    ID3D12DescriptorHeap* srvHeap = srvPool->GetHeap();

    // ImGui用にディスクリプタを1つ確保
    srvIndex_ = srvPool->Allocate();
    IRUFEMI_ASSERT(srvIndex_ != DescriptorPool::kInvalid);

    ImGui_ImplDX12_Init(dxCommon->GetDevice(),
                        kMaxFramesInFlight, // エンジンの最大フレーム実行数に合わせる
                        dxCommon->GetSwapChainDesc().Format, // スワップチェーン作成用にUNORMフォーマットを使用
                        srvHeap, srvPool->GetCPUHandle(srvIndex_), srvPool->GetGPUHandle(srvIndex_));

    // フォントアトラスをビルドし、テクスチャをGPUにアップロードする
    io.Fonts->Build();
    ImGui_ImplDX12_CreateDeviceObjects();
    ImGui_ImplDX12_UpdateTexture(io.Fonts->TexData);

#endif // USE_IMGUI
}

void DebugUI::FrameStart() {

#ifdef USE_IMGUI

    /*開発のUIを出そう*/

    /// ImGuiを使う
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
#endif // USE_IMGUI
}

void DebugUI::Shutdown() {
#ifdef USE_IMGUI

    /*開発のUIを出そう*/

    /// ImGuiの終了処理

    // ImGuiの終了処理。詳細はさして重要ではないので解説は省略する。
    // こういうもんである。初期化と逆順に行う。
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (dxCommon_ && dxCommon_->GetSrvPool()) {
        dxCommon_->GetSrvPool()->FreeAfterFence(srvIndex_, dxCommon_->GetCurrentFrameFenceValue());
        srvIndex_ = DescriptorPool::kInvalid;
    }

#endif // USE_IMGUI
}
#ifdef USE_IMGUI

LRESULT DebugUI::WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return TRUE;
    }

    return FALSE;
}
#endif // USE_IMGUI

void DebugUI::QueueDrawCommands() {
#ifdef USE_IMGUI

    /*開発のUIを出そう*/

    /// ImGuiを使う

    // ImGuiの内部コマンドを生成する
    ImGui::Render();
#endif // USE_IMGUI
}

void DebugUI::QueuePostDrawCommands() {
#ifdef USE_IMGUI

    /*開発のUIを出そう*/
    /*開発のUIを出そう*/

    /// ImGuiを描画する

    // レンダーターゲットの設定 (Main Window) - ImGui用にUNORM版RTV(index 2, 3)を使用する
    uint32_t imGuiRtvIndex = dxCommon_->GetCurrentBackBufferIndex() + 2;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->GetRTVCPUDescriptorHandle(imGuiRtvIndex);
    dxCommon_->GetCommandList()->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    /// ImGuiを描画する

    // 実際のcommandListのImGuiの描画コマンドを積む
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon_->GetCommandList());

    // マルチビューポートの更新処理
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault(nullptr, (void*)dxCommon_->GetCommandList());
    }

#endif // USE_IMGUI
}

void DebugUI::DebugLights([[maybe_unused]] DirectionalLight* directionalLight,
                          [[maybe_unused]] std::vector<std::unique_ptr<PointLight>>& pointLights,
                          [[maybe_unused]] std::vector<std::unique_ptr<SpotLight>>& spotLights,
                          [[maybe_unused]] std::vector<std::unique_ptr<AreaLight>>& areaLights) {
#ifdef USE_IMGUI
    // Lights 統合タブ
    if (ImGui::BeginTabItem("Lights")) {
        if (ImGui::BeginTabBar("LightTabs")) {

            // Light Editor タブ
            if (ImGui::BeginTabItem("Editor")) {
                ImGui::SeparatorText("PointLight Template");
                ImGui::ColorEdit4("PL Color", &lightEditorContext_.pointLight.color.x);
                ImGui::DragFloat3("PL Position", &lightEditorContext_.pointLight.position.x, 0.01f);
                ImGui::DragFloat("PL Intensity", &lightEditorContext_.pointLight.intensity, 0.01f, 0.0f);
                ImGui::DragFloat("PL Radius", &lightEditorContext_.pointLight.radius, 0.01f, 0.0f);
                ImGui::DragFloat("PL Decay", &lightEditorContext_.pointLight.decay, 0.01f, 0.0f);
                if (ImGui::Button("Add PointLight to Scene")) {
                    auto newLight = std::make_unique<PointLight>(lightEditorContext_.pointLight);
                    pointLights.push_back(std::move(newLight));
                }

                ImGui::Separator();

                ImGui::SeparatorText("SpotLight Template");
                ImGui::ColorEdit4("SL Color", &lightEditorContext_.spotLight.color.x);
                ImGui::DragFloat3("SL Position", &lightEditorContext_.spotLight.position.x, 0.01f);
                ImGui::DragFloat("SL Intensity", &lightEditorContext_.spotLight.intensity, 0.01f, 0.0f);
                ImGui::DragFloat3("SL Direction", &lightEditorContext_.spotLight.direction.x, 0.01f);
                lightEditorContext_.spotLight.direction =
                    Irufemi::Math::Normalize(lightEditorContext_.spotLight.direction);
                ImGui::DragFloat("SL Distance", &lightEditorContext_.spotLight.distance, 0.01f, 0.0f);
                ImGui::DragFloat("SL Decay", &lightEditorContext_.spotLight.decay, 0.01f, 0.0f);
                ImGui::DragFloat("SL CosAngle", &lightEditorContext_.spotLight.cosAngle, 0.01f, 0.0f, 1.0f);
                if (ImGui::Button("Add SpotLight to Scene")) {
                    auto newLight = std::make_unique<SpotLight>(lightEditorContext_.spotLight);
                    spotLights.push_back(std::move(newLight));
                }

                ImGui::Separator();

                ImGui::SeparatorText("AreaLight Template");
                ImGui::ColorEdit4("AL Color", &lightEditorContext_.areaLight.color.x);
                ImGui::DragFloat3("AL Position", &lightEditorContext_.areaLight.position.x, 0.01f);
                ImGui::DragFloat("AL Intensity", &lightEditorContext_.areaLight.intensity, 0.01f, 0.0f);
                ImGui::DragFloat3("AL Direction", &lightEditorContext_.areaLight.direction.x, 0.01f);
                lightEditorContext_.areaLight.direction =
                    Irufemi::Math::Normalize(lightEditorContext_.areaLight.direction);
                ImGui::DragFloat("AL Range", &lightEditorContext_.areaLight.range, 0.01f, 0.0f);
                ImGui::DragFloat2("AL Size", &lightEditorContext_.areaLight.size.x, 0.01f, 0.0f);
                if (ImGui::Button("Add AreaLight to Scene")) {
                    auto newLight = std::make_unique<AreaLight>(lightEditorContext_.areaLight);
                    areaLights.push_back(std::move(newLight));
                }

                ImGui::EndTabItem();
            }

            // DirectionalLight タブ
            if (directionalLight && ImGui::BeginTabItem("Directional")) {
                ImGui::ColorEdit4("Color", &directionalLight->color.x);
                ImGui::DragFloat3("Direction", &directionalLight->direction.x, 0.01f);
                directionalLight->direction = Irufemi::Math::Normalize(directionalLight->direction);
                ImGui::DragFloat("Intensity", &directionalLight->intensity, 0.01f, 0.0f);
                ImGui::EndTabItem();
            }

            // PointLights タブ
            if (ImGui::BeginTabItem("Point")) {
                int pointLightToRemove = -1;
                for (size_t i = 0; i < pointLights.size(); ++i) {
                    auto& light = pointLights[i];
                    std::string label = "PointLight " + std::to_string(i);
                    if (ImGui::CollapsingHeader(label.c_str())) {
                        ImGui::PushID(static_cast<int>(i));
                        if (ImGui::Button("[-] Remove")) {
                            pointLightToRemove = static_cast<int>(i);
                        }
                        ImGui::SameLine();
                        ImGui::Checkbox("IsActive", reinterpret_cast<bool*>(&light->isActive));
                        ImGui::ColorEdit4("Color", &light->color.x);
                        ImGui::DragFloat3("Position", &light->position.x, 0.01f);
                        ImGui::DragFloat("Intensity", &light->intensity, 0.01f, 0.0f);
                        ImGui::DragFloat("Radius", &light->radius, 0.01f, 0.0f);
                        ImGui::DragFloat("Decay", &light->decay, 0.01f, 0.0f);
                        ImGui::PopID();
                    }
                }
                if (pointLightToRemove != -1) {
                    pointLights.erase(pointLights.begin() + pointLightToRemove);
                }
                ImGui::EndTabItem();
            }

            // SpotLights タブ
            if (ImGui::BeginTabItem("Spot")) {
                int spotLightToRemove = -1;
                for (size_t i = 0; i < spotLights.size(); ++i) {
                    auto& light = spotLights[i];
                    std::string label = "SpotLight " + std::to_string(i);
                    if (ImGui::CollapsingHeader(label.c_str())) {
                        ImGui::PushID(static_cast<int>(i + pointLights.size()));
                        if (ImGui::Button("[-] Remove")) {
                            spotLightToRemove = static_cast<int>(i);
                        }
                        ImGui::SameLine();
                        ImGui::Checkbox("IsActive", reinterpret_cast<bool*>(&light->isActive));
                        ImGui::ColorEdit4("Color", &light->color.x);
                        ImGui::DragFloat3("Position", &light->position.x, 0.01f);
                        ImGui::DragFloat("Intensity", &light->intensity, 0.01f, 0.0f);
                        ImGui::DragFloat3("Direction", &light->direction.x, 0.01f);
                        light->direction = Irufemi::Math::Normalize(light->direction);
                        ImGui::DragFloat("Distance", &light->distance, 0.01f, 0.0f);
                        ImGui::DragFloat("Decay", &light->decay, 0.01f, 0.0f);
                        ImGui::DragFloat("CosAngle", &light->cosAngle, 0.01f, 0.0f, 1.0f);
                        ImGui::PopID();
                    }
                }
                if (spotLightToRemove != -1) {
                    spotLights.erase(spotLights.begin() + spotLightToRemove);
                }
                ImGui::EndTabItem();
            }

            // AreaLights タブ
            if (ImGui::BeginTabItem("Area")) {
                int areaLightToRemove = -1;
                for (size_t i = 0; i < areaLights.size(); ++i) {
                    auto& light = areaLights[i];
                    std::string label = "AreaLight " + std::to_string(i);
                    if (ImGui::CollapsingHeader(label.c_str())) {
                        ImGui::PushID(static_cast<int>(i + pointLights.size() + spotLights.size()));
                        if (ImGui::Button("[-] Remove")) {
                            areaLightToRemove = static_cast<int>(i);
                        }
                        ImGui::SameLine();
                        ImGui::Checkbox("IsActive", reinterpret_cast<bool*>(&light->isActive));
                        ImGui::ColorEdit4("Color", &light->color.x);
                        ImGui::DragFloat3("Position", &light->position.x, 0.01f);
                        ImGui::DragFloat("Intensity", &light->intensity, 0.01f, 0.0f);
                        ImGui::DragFloat3("Direction", &light->direction.x, 0.01f);
                        light->direction = Irufemi::Math::Normalize(light->direction);
                        ImGui::DragFloat("Range", &light->range, 0.01f, 0.0f);
                        ImGui::DragFloat2("Size", &light->size.x, 0.01f, 0.0f);
                        ImGui::PopID();
                    }
                }
                if (areaLightToRemove != -1) {
                    areaLights.erase(areaLights.begin() + areaLightToRemove);
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
        ImGui::EndTabItem();
    }

#endif
}

// transform
void DebugUI::DebugTransform([[maybe_unused]] Irufemi::Transform& transform) {
#ifdef USE_IMGUI

    if (ImGui::CollapsingHeader("transform")) {
        ImGui::DragFloat3("scale", &transform.scale.x, 0.05f);
        ImGui::DragFloat3("rotate", &transform.rotate.x, 0.05f);
        ImGui::DragFloat3("translate", &transform.translate.x, 0.05f);
        static bool rotateX = false;
        ImGui::Checkbox("RotateX", &rotateX);
        if (rotateX) {
            transform.rotate.x += static_cast<float>(0.05f / std::numbers::pi);
        }
        static bool rotateY = false;
        ImGui::Checkbox("RotateY", &rotateY);
        if (rotateY) {
            transform.rotate.y += static_cast<float>(0.05f / std::numbers::pi);
        }
        static bool rotateZ = false;
        ImGui::Checkbox("RotateZ", &rotateZ);
        if (rotateZ) {
            transform.rotate.z += static_cast<float>(0.05f / std::numbers::pi);
        }
    }
#endif // USE_IMGUI
}

// transform
void DebugUI::DebugTransform2D([[maybe_unused]] Irufemi::Transform& transform) {
#ifdef USE_IMGUI

    if (ImGui::CollapsingHeader("transform")) {
        ImGui::DragFloat2("scale", &transform.scale.x, 0.05f);
        ImGui::DragFloat("rotate", &transform.rotate.z, 0.05f);
        ImGui::DragFloat2("translate", &transform.translate.x, 0.05f);
        static bool rotate = false;
        ImGui::Checkbox("Rotate", &rotate);
        if (rotate) {
            transform.rotate.z += static_cast<float>(0.05f / std::numbers::pi);
        }
    }
#endif // USE_IMGUI
}

void DebugUI::TextTransform([[maybe_unused]] Irufemi::Transform& transform, [[maybe_unused]] const char* name) {
#ifdef USE_IMGUI

    std::string header = std::string("transform") + name;
    if (ImGui::CollapsingHeader(header.c_str())) {
        ImGui::Text("scale: (%.2f, %.2f, %.2f)", transform.scale.x, transform.scale.y, transform.scale.z);
        ImGui::Text("rotate: (%.2f, %.2f, %.2f)", transform.rotate.x, transform.rotate.y, transform.rotate.z);
        ImGui::Text("translate: (%.2f, %.2f, %.2f)", transform.translate.x, transform.translate.y,
                    transform.translate.z);
    }
#endif // USE_IMGUI
}

// ObjMaterial
void DebugUI::DebugObjMaterial([[maybe_unused]] ObjMaterial* material, [[maybe_unused]] const char* unique_id) {
#ifdef USE_IMGUI
    if (!material) {
        return;
    }

    std::string id_str = unique_id;

    ImGui::ColorEdit4(("Color" + id_str).c_str(), &material->color.x);
    ImGui::Checkbox(("Enable Lighting" + id_str).c_str(), &material->enableLighting);
    ImGui::DragFloat(("Metallic" + id_str).c_str(), &material->metallic, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat(("Roughness" + id_str).c_str(), &material->roughness, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat(("Environment Coefficient" + id_str).c_str(), &material->environmentCoefficient, 0.01f, 0.0f,
                     1.0f);

    // UV Irufemi::Transform
    if (ImGui::TreeNode(("UV Transform" + id_str).c_str())) {
        DebugUvTransform(material->uvTransform);
        ImGui::TreePop();
    }
#endif // USE_IMGUI
}

void DebugUI::DebugMaterialOverrides(float* envCoef, int32_t* lightingMode, int32_t* useClamp, int32_t* enableLighting,
                                     const char* unique_id) {
#ifdef USE_IMGUI
    std::string id = unique_id;
    if (ImGui::TreeNode(("Material Overrides" + id).c_str())) {
        ImGui::DragFloat(("Env Coefficient" + id).c_str(), envCoef, 0.01f, 0.00f, 10.0f);

        const char* lightingItems[] = {"Model Default", "None", "Lambert", "Half-Lambert", "PBR"};
        int currentLighting = *lightingMode + 1; // -1 -> 0
        if (ImGui::Combo(("Lighting Mode" + id).c_str(), &currentLighting, lightingItems,
                         IM_ARRAYSIZE(lightingItems))) {
            *lightingMode = currentLighting - 1;
        }

        const char* clampItems[] = {"Model Default", "WRAP", "CLAMP"};
        int currentClamp = *useClamp + 1; // -1 -> 0
        if (ImGui::Combo(("Sampler Mode" + id).c_str(), &currentClamp, clampItems, IM_ARRAYSIZE(clampItems))) {
            *useClamp = currentClamp - 1;
        }

        const char* enableItems[] = {"Model Default", "OFF", "ON"};
        int currentEnable = *enableLighting + 1; // -1 -> 0
        if (ImGui::Combo(("Enable Lighting" + id).c_str(), &currentEnable, enableItems, IM_ARRAYSIZE(enableItems))) {
            *enableLighting = currentEnable - 1;
        }

        ImGui::TreePop();
    }
#endif
}

void DebugUI::DebugAnimationControl([[maybe_unused]] const Animation& animation, [[maybe_unused]] float& currentTime,
                                    [[maybe_unused]] const char* unique_id) {
#ifdef USE_IMGUI
    std::string id = unique_id;
    if (ImGui::TreeNode(("Animation Control" + id).c_str())) {
        ImGui::SliderFloat(("Time" + id).c_str(), &currentTime, 0.0f, animation.duration);
        ImGui::TreePop();
    }
#endif
}

// Material
void DebugUI::DebugMaterialBy3D([[maybe_unused]] Material* materialData) {
#ifdef USE_IMGUI

    if (ImGui::CollapsingHeader("material")) {
        ImGui::ColorEdit4("spriteColor", &materialData->color.x);
        bool enableLighting = materialData->enableLighting;
        if (ImGui::Checkbox("enableLighting", &enableLighting)) {
            materialData->enableLighting = enableLighting;
        }
        // lightingMode選択
        const char* items[] = {"NonLighting", "Lambert", "HalfLambert", "PBR"};
        int currentMode = materialData->lightingMode;
        if (ImGui::Combo("LightingMode", &currentMode, items, IM_ARRAYSIZE(items))) {
            materialData->lightingMode = currentMode;
        }
        ImGui::DragFloat("Metallic", &materialData->metallic, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Roughness", &materialData->roughness, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Environment Coefficient", &materialData->environmentCoefficient, 0.01f, 0.0f, 1.0f);

        ImGui::DragFloat("Alpha Reference", &materialData->alphaReference, 0.01f, 0.0f, 1.0f);
        const char* clampItems[] = {"WRAP (Default)", "CLAMP (Linear)", "CLAMP (Point)"};
        int currentClamp = materialData->useClampSampler;
        if (ImGui::Combo("Sampler Mode", &currentClamp, clampItems, IM_ARRAYSIZE(clampItems))) {
            materialData->useClampSampler = currentClamp;
        }
    }
#endif // USE_IMGUI
}

// Material
void DebugUI::DebugMaterialBy2D([[maybe_unused]] Material* materialData) {
#ifdef USE_IMGUI

    if (ImGui::CollapsingHeader("material")) {
        ImGui::ColorEdit4("spriteColor", &materialData->color.x);
    }
#endif // USE_IMGUI
}

// Particle 専用マテリアルのデバッグ表示
void DebugUI::DebugMaterialByParticle([[maybe_unused]] Material* materialData) {
#ifdef USE_IMGUI

    if (!materialData) {
        return;
    }

    if (ImGui::CollapsingHeader("particle material")) {
        // 基本プロパティ
        ImGui::ColorEdit4("color", &materialData->color.x);

        // サンプラ切替フラグ(0 = WRAP(s0), 1 = CLAMP(s1))
        bool useClamp = materialData->useClampSampler != 0;
        if (ImGui::Checkbox("Use Clamp Sampler (V)", &useClamp)) {
            materialData->useClampSampler = useClamp ? 1 : 0;
        }

        // --- UV Irufemi::Transform 編集(より実用的に) ---
        // materialData->uvTransform は 4x4 行列。
        // 編集用に translate/scale/rotate(Z) を抽出し、編集後に再構成する。
        // 抽出は「一般的な affine(回転 + scale + translate)を想定した簡易逆変換」です。
        // U/V は X/Y 成分に対応している前提。
        float tx = materialData->uvTransform.m[3][0];
        float ty = materialData->uvTransform.m[3][1];

        // 簡易スケール抽出：対角成分を利用(斜交/shear を無視する簡易推定)
        float sx = materialData->uvTransform.m[0][0];
        float sy = materialData->uvTransform.m[1][1];

        // 簡易回転(ラジアン)： atan2( m10, m00 ) を使用(回転+scale の混在を近似)
        float rot = std::atan2(materialData->uvTransform.m[1][0], materialData->uvTransform.m[0][0]);

        bool changed = false;
        if (ImGui::TreeNode("UV Transform (affine)")) {
            if (ImGui::DragFloat2("UV Translate", &tx, 0.01f, -100.0f, 100.0f)) {
                changed = true;
            }
            if (ImGui::DragFloat2("UV Scale", &sx, 0.01f, -100.0f, 100.0f)) {
                changed = true;
            }
            if (ImGui::SliderAngle("UV Rotate (deg)", &rot)) {
                changed = true;
            }
            ImGui::TextWrapped(
                "注: 複雑な歪み(shear 等)がある場合は完璧に逆変換できません。一般的な UV 編集用途に最適化しています。");
            ImGui::TreePop();
        }

        if (changed) {
            // Irufemi::Transform 構造を使って行列を再構成(function/Math.h の MakeAffineMatrix を利用)
            Irufemi::Transform uvT;
            uvT.translate = {tx, ty, 0.0f};
            uvT.scale = {sx, sy, 1.0f};
            uvT.rotate = {0.0f, 0.0f, rot}; // rad

            materialData->uvTransform = Irufemi::Math::MakeAffineMatrix(uvT.scale, uvT.rotate, uvT.translate);
        }
    }
#endif // USE_IMGUI
}

// 画像
void DebugUI::DebugTexture([[maybe_unused]] Object3DResource* resource, [[maybe_unused]] int& selectedTextureIndex) {
#ifdef USE_IMGUI
    if (textureManager_ && resource) {
        auto textureNames = textureManager_->GetTextureNamesForDebug();

        if (!textureNames.empty()) {
            const char* preview = textureNames[selectedTextureIndex].c_str();
            if (ImGui::BeginCombo("Texture", preview)) {
                for (int i = 0; i < static_cast<int>(textureNames.size()); ++i) {
                    bool isSelected = (i == selectedTextureIndex);
                    if (ImGui::Selectable(textureNames[i].c_str(), isSelected)) {
                        selectedTextureIndex = i;
                        if (resource->GetTextureHandle().IsValid()) {
                            textureManager_->ReleaseTexture(resource->GetTextureHandle());
                        }
                        resource->SetTextureHandle(textureManager_->LoadTexture(textureNames[i]));
                    }
                }
                ImGui::EndCombo();
            }
        }
    }
#endif
}

void DebugUI::DebugTexture([[maybe_unused]] Object2DResource* resource, [[maybe_unused]] int& selectedTextureIndex) {
#ifdef USE_IMGUI
    if (textureManager_ && resource) {
        auto textureNames = textureManager_->GetTextureNamesForDebug();

        if (!textureNames.empty()) {
            const char* preview = textureNames[selectedTextureIndex].c_str();
            if (ImGui::BeginCombo("Texture", preview)) {
                for (int i = 0; i < static_cast<int>(textureNames.size()); ++i) {
                    bool isSelected = (i == selectedTextureIndex);
                    if (ImGui::Selectable(textureNames[i].c_str(), isSelected)) {
                        selectedTextureIndex = i;
                        if (resource->GetTextureHandle().IsValid()) {
                            textureManager_->ReleaseTexture(resource->GetTextureHandle());
                        }
                        resource->SetTextureHandle(textureManager_->LoadTexture(textureNames[i]));
                    }
                }
                ImGui::EndCombo();
            }
        }
    }
#endif
}

// DirectionalLight
void DebugUI::DebugDirectionalLight([[maybe_unused]] DirectionalLight* directionalLightData) {
#ifdef USE_IMGUI

    if (ImGui::CollapsingHeader("directionalLight")) {
        ImGui::ColorEdit4("lightColor", &directionalLightData->color.x);
        ImGui::DragFloat3("lightDirection", &directionalLightData->direction.x, 0.01f);
        ImGui::DragFloat("intensity", &directionalLightData->intensity, 0.01f, 0.0f);
    }
#endif // USE_IMGUI
}

// UvTransform
void DebugUI::DebugUvTransform([[maybe_unused]] Irufemi::Transform& uvTransform) {
#ifdef USE_IMGUI

    if (ImGui::CollapsingHeader("uvTransform")) {
        ImGui::DragFloat3("UVTranslate", &uvTransform.translate.x, 0.01f, -10.0f, 10.0f);
        ImGui::DragFloat3("UVScale", &uvTransform.scale.x, 0.01f, -10.0f, 10.0f);
        ImGui::SliderAngle("UVRotate", &uvTransform.rotate.z);
    }
#endif // USE_IMGUI
}

// UvTransform
void DebugUI::DebugUvTransform([[maybe_unused]] Irufemi::Matrix4x4& uvTransform) {
#ifdef USE_IMGUI
    if (ImGui::CollapsingHeader("uvTransform")) {
        // 編集用に translate/scale/rotate(Z) を抽出
        float tx = uvTransform.m[3][0];
        float ty = uvTransform.m[3][1];
        float sx = std::sqrt(uvTransform.m[0][0] * uvTransform.m[0][0] + uvTransform.m[0][1] * uvTransform.m[0][1]);
        float sy = std::sqrt(uvTransform.m[1][0] * uvTransform.m[1][0] + uvTransform.m[1][1] * uvTransform.m[1][1]);
        float rot = std::atan2(uvTransform.m[1][0], uvTransform.m[0][0]);

        bool changed = false;
        if (ImGui::DragFloat2("UVTranslate", &tx, 0.01f)) {
            changed = true;
        }
        if (ImGui::DragFloat2("UVScale", &sx, 0.01f)) {
            sy = sx; // XとYを同じ値に保つ
            changed = true;
        }
        if (ImGui::SliderAngle("UVRotate", &rot)) {
            changed = true;
        }

        if (changed) {
            // Irufemi::Transform 構造を使って行列を再構成
            Irufemi::Transform uvT;
            uvT.translate = {tx, ty, 0.0f};
            uvT.scale = {sx, sy, 1.0f};
            uvT.rotate = {0.0f, 0.0f, rot}; // rad
            uvTransform = Irufemi::Math::MakeAffineMatrix(uvT.scale, uvT.rotate, uvT.translate);
        }
    }
#endif // USE_IMGUI
}
// Irufemi::Sphere
void DebugUI::DebugSphereInfo([[maybe_unused]] Irufemi::Sphere& sphere) {
#ifdef USE_IMGUI

    if (ImGui::CollapsingHeader("info")) {
        ImGui::DragFloat3("Center", &sphere.center.x, 0.01f, -10.0f, 10.0f);
        ImGui::DragFloat("radius", &sphere.radius, 0.01f, -10.0f, 10.0f);
    }
#endif // USE_IMGUI
}

void DebugUI::SceneSelectorTab([[maybe_unused]] SceneManager* sm) {
#ifdef USE_IMGUI
    if (!sm) {
        return;
    }

    if (ImGui::BeginTabItem("Scene Selector")) {
        if (ImGui::BeginChild("SceneSelectorScroll", ImVec2(0, 0), false)) {
            const auto names = sm->GetRegisteredKeys();
            if (names.empty()) {
                ImGui::EndChild();
                ImGui::EndTabItem();
                return;
            }

            // 現在シーンのインデックス
            int currentIdx = 0;
            for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                if (names[i] == sm->GetCurrent()) {
                    currentIdx = i;
                    break;
                }
            }

            if (ImGui::BeginCombo("Scene", names[currentIdx].c_str())) {
                for (int i = 0; i < static_cast<int>(names.size()); ++i) {
                    bool selected = (i == currentIdx);
                    if (ImGui::Selectable(names[i].c_str(), selected)) {
                        sm->Request(names[i]); // 次フレーム頭で切替
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::EndChild();
        }
        ImGui::EndTabItem();
    }
#endif // USE_IMGUI
}

void DebugUI::DebugPsoSettings([[maybe_unused]] Irufemi::BlendMode* blendMode,
                               [[maybe_unused]] PSOManager::DepthWrite* depthWrite,
                               [[maybe_unused]] PSOManager::CullMode* cullMode,
                               [[maybe_unused]] const char* unique_id) {
#ifdef USE_IMGUI
    if (!blendMode || !depthWrite || !cullMode) {
        return;
    }

    // Blend Mode
    int blendIdx = static_cast<int>(*blendMode);
    const char* blendNames[] = {"None", "Normal", "Add", "Subtract", "Multiply", "Screen"};
    std::string blendLabel = "Blend Mode";
    blendLabel += unique_id;
    if (ImGui::Combo(blendLabel.c_str(), &blendIdx, blendNames, IM_ARRAYSIZE(blendNames))) {
        *blendMode = static_cast<Irufemi::BlendMode>(blendIdx);
    }

    // Depth Write
    int depthIdx = (*depthWrite == PSOManager::DepthWrite::Enable) ? 0 : 1;
    const char* depthNames[] = {"Enable", "Disable"};
    std::string depthLabel = "Depth Write";
    depthLabel += unique_id;
    if (ImGui::Combo(depthLabel.c_str(), &depthIdx, depthNames, IM_ARRAYSIZE(depthNames))) {
        *depthWrite = (depthIdx == 0) ? PSOManager::DepthWrite::Enable : PSOManager::DepthWrite::Disable;
    }

    // Cull Mode
    int cullIdx = static_cast<int>(*cullMode);
    const char* cullNames[] = {"Back", "Front", "None"};
    std::string cullLabel = "Cull Mode";
    cullLabel += unique_id;
    if (ImGui::Combo(cullLabel.c_str(), &cullIdx, cullNames, IM_ARRAYSIZE(cullNames))) {
        *cullMode = static_cast<PSOManager::CullMode>(cullIdx);
    }
#endif // USE_IMGUI
}

void DebugUI::PostProcessTab([[maybe_unused]] IrufemiEngine* engine) {
#ifdef USE_IMGUI
    if (!engine) {
        return;
    }

    if (ImGui::BeginTabItem("Post Processing")) {
        if (ImGui::BeginChild("PostProcessScroll", ImVec2(0, 0), false)) {
            auto* ppManager = engine->GetPostProcessManager();
            if (!ppManager) {
                ImGui::EndChild();
                ImGui::EndTabItem();
                return;
            }

            const char* modeNames[] = {"None",
                                       "Grayscale",
                                       "Sepia",
                                       "Vignette",
                                       "Smoothing",
                                       "GaussianFilter",
                                       "DepthBasedOutline",
                                       "RadialBlur",
                                       "Dissolve",
                                       "Noise",
                                       "HSV",
                                       "ToneMapping",
                                       "Fade",
                                       "Slide",
                                       "Bloom",
                                       "Glitch",
                                       "DualKawaseBlur",
                                       "LuminanceBasedOutline",
                                       "Pixelation",
                                       "Pointillism",
                                       "Posterization",
                                       "NightVision",
                                       "Kaleidoscope",
                                       "ChromaticAberration",
                                       "DisplacementMap",
                                       "DirectionalBlur",
                                       "Halftone",
                                       "DepthOfField",
                                       "LightShafts"};
            auto activeModes = ppManager->GetActiveModes();

            if (ImGui::Button("Clear All Effects")) {
                ppManager->ClearActiveModes();
                activeModes.clear();
            }

            ImGui::Separator();
            ImGui::Text("Available Effects:");

            // エフェクト選択
            ImGui::PushID("AvailableEffects");
            for (int i = 1; i < (int)IM_ARRAYSIZE(modeNames); ++i) { // None 以外を表示
                PostProcessMode m = static_cast<PostProcessMode>(i);
                bool isEnabled = std::find(activeModes.begin(), activeModes.end(), m) != activeModes.end();

                if (ImGui::Checkbox(modeNames[i], &isEnabled)) {
                    if (isEnabled) {
                        ppManager->AddActiveMode(m);
                    } else {
                        activeModes.erase(std::remove(activeModes.begin(), activeModes.end(), m), activeModes.end());
                        ppManager->SetActiveModes(activeModes);
                    }
                }
            }
            ImGui::PopID();

            ImGui::Separator();
            ImGui::Text("Active Stack (Draw Order):");
            if (activeModes.empty()) {
                ImGui::TextDisabled("(No effects active - Clean Copy)");
            } else {
                for (size_t i = 0; i < activeModes.size(); ++i) {
                    ImGui::BulletText("%d: %s", static_cast<int>(i + 1), modeNames[static_cast<int>(activeModes[i])]);
                }
            }

            ImGui::Separator();
            ImGui::Text("Parameters:");

            // 有効な全てのエフェクトのパラメータを表示
            ImGui::PushID("Parameters");
            for (auto mode : activeModes) {
                if (ImGui::TreeNode(modeNames[static_cast<int>(mode)])) {
                    if (mode == PostProcessMode::ChromaticAberration) {
                        auto& params = ppManager->GetChromaticAberrationParams();
                        ImGui::DragFloat("Intensity", &params.intensity, 0.001f, 0.0f, 1.0f);
                    } else if (mode == PostProcessMode::DisplacementMap) {
                        auto& params = ppManager->GetDisplacementMapParams();
                        ImGui::DragFloat("Intensity", &params.intensity, 0.001f, 0.0f, 1.0f);
                        ImGui::DragFloat("Time Scale", &params.timeScale, 0.01f, 0.0f, 10.0f);
                    } else if (mode == PostProcessMode::DirectionalBlur) {
                        auto& params = ppManager->GetDirectionalBlurParams();
                        ImGui::DragFloat2("Direction", &params.direction.x, 0.01f, -1.0f, 1.0f);
                        ImGui::DragFloat("Strength", &params.strength, 0.001f, 0.0f, 1.0f);
                        ImGui::SliderInt("Samples", &params.samples, 2, 32);
                    } else if (mode == PostProcessMode::Halftone) {
                        auto& params = ppManager->GetHalftoneParams();
                        ImGui::DragFloat("Scale", &params.scale, 1.0f, 10.0f, 500.0f);
                        // ImGui::SliderAngle handles radians automatically
                        float angleDeg = params.angle * (180.0f / 3.14159265f);
                        if (ImGui::SliderFloat("Angle", &angleDeg, -180.0f, 180.0f)) {
                            params.angle = angleDeg * (3.14159265f / 180.0f);
                        }
                        ImGui::DragFloat("Blend", &params.blend, 0.01f, 0.0f, 1.0f);
                    } else if (mode == PostProcessMode::DepthOfField) {
                        auto& params = ppManager->GetDepthOfFieldParams();
                        ImGui::DragFloat("Focus Distance", &params.focusDistance, 0.1f, 0.0f, 1000.0f);
                        ImGui::DragFloat("Focus Range", &params.focusRange, 0.1f, 0.1f, 500.0f);
                        ImGui::DragFloat("Blur Size", &params.blurSize, 0.1f, 0.0f, 50.0f);
                        ImGui::SliderInt("Samples", &params.samples, 4, 64);
                    } else if (mode == PostProcessMode::LightShafts) {
                        auto& params = ppManager->GetLightShaftsParams();
                        ImGui::DragFloat2("Light Screen Pos", &params.lightScreenPos.x, 0.01f, -1.0f, 2.0f);
                        ImGui::DragFloat("Density", &params.density, 0.01f, 0.0f, 5.0f);
                        ImGui::DragFloat("Decay", &params.decay, 0.001f, 0.8f, 1.0f);
                        ImGui::DragFloat("Weight", &params.weight, 0.01f, 0.0f, 2.0f);
                        ImGui::DragFloat("Exposure", &params.exposure, 0.01f, 0.0f, 5.0f);
                        ImGui::SliderInt("Samples", &params.samples, 8, 128);
                    } else if (mode == PostProcessMode::Kaleidoscope) {
                        auto& params = ppManager->GetKaleidoscopeParams();
                        ImGui::DragFloat("Segments", &params.segments, 0.1f, 1.0f, 32.0f);
                    } else if (mode == PostProcessMode::NightVision) {
                        auto& params = ppManager->GetNightVisionParams();
                        ImGui::DragFloat("Intensity", &params.intensity, 0.01f, 0.0f, 2.0f);
                    } else if (mode == PostProcessMode::Vignette) {
                        auto& params = ppManager->GetVignetteParams();
                        ImGui::DragFloat("Vignette Radius", &params.radius, 0.01f, 0.0f, 2.0f);
                        ImGui::DragFloat("Vignette Softness", &params.softness, 0.01f, 0.0f, 2.0f);
                    } else if (mode == PostProcessMode::Smoothing) {
                        auto& params = ppManager->GetSmoothingParams();
                        if (ImGui::SliderInt("Kernel Size", reinterpret_cast<int*>(&params.kernelSize), 1, 31)) {
                            if (params.kernelSize < 1) {
                                params.kernelSize = 1;
                            }
                            if (params.kernelSize > 1 && params.kernelSize % 2 == 0) {
                                params.kernelSize += 1;
                            }
                        }
                    } else if (mode == PostProcessMode::GaussianFilter) {
                        auto& params = ppManager->GetGaussianParams();
                        ImGui::DragFloat("Sigma", &params.sigma, 0.01f, 0.01f, 10.0f);
                        if (ImGui::SliderInt("Kernel Size", reinterpret_cast<int*>(&params.kernelSize), 1, 31)) {
                            if (params.kernelSize < 1) {
                                params.kernelSize = 1;
                            }
                            if (params.kernelSize > 1 && params.kernelSize % 2 == 0) {
                                params.kernelSize += 1;
                            }
                        }
                    } else if (mode == PostProcessMode::DepthBasedOutline) {
                        auto& params = ppManager->GetOutlineParams();
                        ImGui::DragFloat("Outline Intensity", &params.intensity, 0.1f, 0.0f, 20.0f);
                    } else if (mode == PostProcessMode::RadialBlur) {
                        auto& params = ppManager->GetRadialBlurParams();
                        ImGui::DragFloat2("Center", &params.center.x, 0.01f, 0.0f, 1.0f);
                        ImGui::DragFloat("Blur Width", &params.blurWidth, 0.001f, 0.0f, 0.1f);
                        ImGui::SliderInt("Samples", reinterpret_cast<int*>(&params.numSamples), 1, 100);
                    } else if (mode == PostProcessMode::Dissolve) {
                        auto& params = ppManager->GetDissolveParams();
                        ImGui::SliderFloat("Threshold", &params.threshold, 0.0f, 1.0f);
                        ImGui::SliderFloat("Edge Range", &params.edgeRange, 0.0f, 0.2f);
                        ImGui::ColorEdit4("Edge Color", &params.edgeColor.x);
                        ImGui::ColorEdit4("Background Color", &params.backgroundColor.x);
                        const char* noiseTypes[] = {"Noise 0", "Noise 1"};
                        ImGui::Combo("Noise Type", reinterpret_cast<int*>(&params.noiseType), noiseTypes,
                                     IM_ARRAYSIZE(noiseTypes));
                    } else if (mode == PostProcessMode::Noise) {
                        auto& params = ppManager->GetNoiseParams();
                        ImGui::SliderFloat("Noise Intensity", &params.intensity, 0.0f, 1.0f);
                    } else if (mode == PostProcessMode::HSV) {
                        auto& params = ppManager->GetHSVParams();
                        ImGui::DragFloat("HueOffset", &params.hue, 0.001f, -1.0f, 1.0f);
                        ImGui::DragFloat("SaturationOffset", &params.saturation, 0.001f, -1.0f, 1.0f);
                        ImGui::DragFloat("ValueOffset", &params.value, 0.001f, -1.0f, 1.0f);
                    } else if (mode == PostProcessMode::ToneMapping) {
                        auto& params = ppManager->GetToneMappingParams();
                        ImGui::DragFloat("Exposure", &params.exposure, 0.01f, 0.0f, 10.0f);
                    } else if (mode == PostProcessMode::Bloom) {
                        auto& params = ppManager->GetBloomParams();
                        ImGui::DragFloat("Threshold", &params.threshold, 0.01f, 0.0f, 5.0f);
                        ImGui::DragFloat("Sigma", &params.sigma, 0.01f, 0.01f, 10.0f);
                        ImGui::DragFloat("Intensity", &params.intensity, 0.01f, 0.0f, 10.0f);
                        if (ImGui::SliderInt("Kernel Size", &params.kernelSize, 1, 51)) {
                            if (params.kernelSize < 1) {
                                params.kernelSize = 1;
                            }
                            if (params.kernelSize > 1 && params.kernelSize % 2 == 0) {
                                params.kernelSize += 1;
                            }
                        }
                    } else if (mode == PostProcessMode::Glitch) {
                        auto& params = ppManager->GetGlitchParams();
                        ImGui::SliderFloat("Glitch Intensity", &params.intensity, 0.0f, 5.0f);
                    } else if (mode == PostProcessMode::DualKawaseBlur) {
                        auto& params = ppManager->GetDualKawaseBlurParams();
                        ImGui::DragFloat("Blur Radius Offset", &params.blurRadius, 0.01f, 0.0f, 5.0f);
                        ImGui::SliderInt("Iteration Count", &params.iterationCount, 1,
                                         PostProcessManager::kMaxKawaseIterations);
                        ImGui::DragFloat("Intensity", &params.intensity, 0.01f, 0.0f, 10.0f);
                    } else if (mode == PostProcessMode::LuminanceBasedOutline) {
                        auto& params = ppManager->GetLuminanceOutlineParams();
                        ImGui::DragFloat("Threshold", &params.threshold, 0.01f, 0.0f, 1.0f);
                        ImGui::ColorEdit4("Outline Color", &params.outlineColor.x);
                    } else if (mode == PostProcessMode::Pixelation) {
                        auto& params = ppManager->GetPixelationParams();
                        ImGui::DragFloat("Pixel Size", &params.pixelSize, 0.1f, 1.0f, 64.0f);
                    } else if (mode == PostProcessMode::Pointillism) {
                        auto& params = ppManager->GetPointillismParams();
                        ImGui::DragFloat("Stroke Size", &params.strokeSize, 0.1f, 1.0f, 50.0f);
                        ImGui::DragFloat("Color Steps", &params.colorSteps, 0.1f, 2.0f, 32.0f);
                    } else if (mode == PostProcessMode::Posterization) {
                        auto& params = ppManager->GetPosterizationParams();
                        ImGui::DragFloat("Color Steps", &params.colorSteps, 0.1f, 2.0f, 32.0f);
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::PopID();
            ImGui::EndChild();
        }
        ImGui::EndTabItem();
    }
#endif // USE_IMGUI
}

bool DebugUI::BeginEngineDebugWindow(bool* pOpen) {
#ifdef USE_IMGUI
    return ImGui::Begin("Engine Debugger", pOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
#else
    (void)pOpen;
    return false;
#endif
}

void DebugUI::EndEngineDebugWindow() {
#ifdef USE_IMGUI
    ImGui::End();
#endif
}

void DebugUI::DebugLightning([[maybe_unused]] LightningParams* params) {
#ifdef USE_IMGUI
    if (!params) {
        return;
    }

    if (ImGui::TreeNode("Lightning Crawl Settings")) {
        ImGui::Separator();
        ImGui::Text("Surface Settings");
        ImGui::ColorEdit4("Surface Color", &params->color.x);
        ImGui::DragFloat("Surface Speed", &params->speed, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Surface Intensity", &params->intensity, 0.1f, 0.0f, 100.0f);
        ImGui::DragFloat("Surface Noise Scale", &params->noiseScale, 0.01f, 0.01f, 20.0f);
        ImGui::DragFloat("Surface Threshold", &params->noiseThreshold, 0.001f, 0.0f, 1.0f);

        ImGui::Separator();
        ImGui::Text("Core Settings");
        ImGui::ColorEdit4("Core Color", &params->coreColor.x);
        ImGui::DragFloat("Core Intensity", &params->coreIntensity, 0.1f, 0.0f, 100.0f);
        ImGui::DragFloat("Core Threshold", &params->coreThreshold, 0.001f, 0.0f, 1.0f);
        ImGui::DragFloat("Core Scale", &params->coreScale, 0.01f, 0.01f, 20.0f);

        ImGui::TreePop();
    }
#endif
}

namespace {
std::wstring GenerateScreenshotPath(const std::wstring& prefix) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    struct tm timeinfo;
    localtime_s(&timeinfo, &time);
    std::wstringstream wss;
    wss << L"resources/screenshots/" << prefix << L"_";
    wss << std::put_time(&timeinfo, L"%Y%m%d_%H%M%S");
    wss << L".png";
    return wss.str();
}
} // namespace

void DebugUI::ScreenCaptureTab(ScreenCaptureManager* captureManager) {
#ifdef USE_IMGUI
    if (!captureManager) {
        return;
    }

    if (ImGui::BeginTabItem("Screen Capture")) {
        if (ImGui::BeginChild("ScreenCaptureScroll", ImVec2(0, 0), false)) {
            if (ImGui::Button("Capture (Scene Only)")) {
                captureManager->RequestCapture(GenerateScreenshotPath(L"scene"), ScreenCaptureType::SceneOnly);
            }
            if (ImGui::Button("Capture (With UI)")) {
                captureManager->RequestCapture(GenerateScreenshotPath(L"ui"), ScreenCaptureType::WithUI);
            }
            if (ImGui::Button("Capture (Alpha)")) {
                captureManager->RequestCaptureWithAlpha(GenerateScreenshotPath(L"alpha"));
            }
            if (ImGui::Button("Capture (Depth)")) {
                captureManager->RequestCaptureDepth(GenerateScreenshotPath(L"depth"));
            }
            ImGui::EndChild();
        }
        ImGui::EndTabItem();
    }
#endif
}

void DebugUI::DrawCommonEngineTabs([[maybe_unused]] IrufemiEngine* engine) {
#ifdef USE_IMGUI
    if (!engine) {
        return;
    }

    if (ImGui::BeginTabBar("EngineCommonTabs", ImGuiTabBarFlags_Reorderable)) {
        // --- 一線級エンジン基準: タブバー見出し領域の上でのみ 1 ノッチずつ確実にタブを切り替える ---
        ImGuiTabBar* tabBar = ImGui::GetCurrentTabBar();
        if (tabBar && ImGui::IsMouseHoveringRect(tabBar->BarRect.Min, tabBar->BarRect.Max)) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                // 現在アクティブなタブのインデックスを探す
                int currentIdx = -1;
                for (int i = 0; i < tabBar->Tabs.Size; ++i) {
                    if (tabBar->Tabs[i].ID == tabBar->SelectedTabId) {
                        currentIdx = i;
                        break;
                    }
                }
                if (currentIdx != -1) {
                    // 下ホイールで次(+1)、上ホイールで前(-1)へ1個だけ移動（端で安全に停止）
                    int nextIdx = currentIdx + (wheel < 0.0f ? 1 : -1);
                    if (nextIdx >= 0 && nextIdx < tabBar->Tabs.Size) {
                        ImGui::TabBarQueueFocus(tabBar, &tabBar->Tabs[nextIdx]);
                    }
                }
            }
        }

        // --- 各タブの描画（クリック操作を妨害しない標準仕様） ---
        // 1. Scene Selector タブ (関数内で BeginTabItem("Scene Selector") を実行)
        if (engine->GetSceneManager()) {
            SceneSelectorTab(engine->GetSceneManager());
        }

        // 2. Post Processing タブ (関数内で BeginTabItem("Post Processing") を実行)
        PostProcessTab(engine);

        // 3. Camera & Lights / Scene Debug タブ
        if (ImGui::BeginTabItem("Scene")) {
            if (ImGui::BeginChild("SceneScroll", ImVec2(0, 0), false)) {
                if (engine->GetSceneManager()) {
                    if (auto* currentScene = engine->GetSceneManager()->GetCurrentScene()) {
                        currentScene->DrawDebugTabItem();
                    } else {
                        ImGui::TextDisabled("No active scene.");
                    }
                }
                ImGui::EndChild();
            }
            ImGui::EndTabItem();
        }

        // 4. Display / System タブ
        if (ImGui::BeginTabItem("Display")) {
            if (ImGui::BeginChild("DisplayScroll", ImVec2(0, 0), false)) {
                int displayModeInt = Irufemi::CVarSystem::GetInt("r.DisplayMode");
                if (ImGui::Combo("Mode", &displayModeInt, "Windowed\0Borderless\0")) {
                    Irufemi::CVarSystem::SetInt("r.DisplayMode", displayModeInt);
                }
                bool vSync = Irufemi::CVarSystem::GetBool("r.VSync");
                if (ImGui::Checkbox("VSync", &vSync)) {
                    Irufemi::CVarSystem::SetBool("r.VSync", vSync);
                }
                if (engine->GetDirectXCommon() && engine->GetDirectXCommon()->IsTearingSupported()) {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Tearing (VRR) is Supported.");
                } else {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Tearing (VRR) is NOT Supported.");
                }
                ImGui::EndChild();
            }
            ImGui::EndTabItem();
        }

        // 5. Screen Capture タブ (関数内で BeginTabItem("Screen Capture") を実行)
        if (engine->GetScreenCaptureManager()) {
            ScreenCaptureTab(engine->GetScreenCaptureManager());
        }

        // 6. GPU Particle タブ (関数内で BeginTabItem("GPU Particle Manager") を実行)
        if (engine->GetGPUParticleManager()) {
            engine->GetGPUParticleManager()->Debug();
        }

        ImGui::EndTabBar();
    }
#endif
}
