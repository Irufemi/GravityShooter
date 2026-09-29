#include "Panels/ProjectBrowserPanel.h"
#include "Resource/Model/ModelManager.h"
#include "Resource/Texture/TextureManager.h"

#ifdef EditorMode
#include "imgui/imgui.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/EditorManager.h"
#include "EngineResources/FontAwesome/IconsFontAwesome6.h"
#include "UI/EditorDragDrop.h"
#include "Core/System/DirectoryWatcher.h"
#include <algorithm>

ProjectBrowserPanel::ProjectBrowserPanel() {
    projectBrowserFilter_ = std::make_unique<ImGuiTextFilter>();
}

ProjectBrowserPanel::~ProjectBrowserPanel() = default;

void ProjectBrowserPanel::Initialize(EditorManager* editorManager) {
    editorManager_ = editorManager;

    // アプリケーションの実行ディレクトリ（project直下など）をルートとして初期化
    projectRootPath_ = std::filesystem::current_path();
    currentProjectBrowserPath_ = projectRootPath_;

    RefreshCache();

    // バックグラウンドでの自動監視を開始
    directoryWatcher_ = std::make_unique<DirectoryWatcher>(projectRootPath_, [this]() { isCacheDirty_ = true; });
}

void ProjectBrowserPanel::BuildDirectoryTree(DirectoryNode* node) {
    if (!std::filesystem::exists(node->path) || !std::filesystem::is_directory(node->path)) {
        return;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(node->path)) {
            if (entry.is_directory()) {
                auto childNode = std::make_unique<DirectoryNode>();
                childNode->path = entry.path();
                childNode->folderName = entry.path().filename().string();
                BuildDirectoryTree(childNode.get());
                node->subDirectories.push_back(std::move(childNode));
            } else if (entry.is_regular_file()) {
                FileEntry fileEntry;
                fileEntry.path = entry.path();
                fileEntry.filenameString = entry.path().filename().string();
                fileEntry.ext = entry.path().extension().string();
                std::transform(fileEntry.ext.begin(), fileEntry.ext.end(), fileEntry.ext.begin(), ::tolower);
                fileEntry.isDirectory = false;
                node->files.push_back(fileEntry);
            }
        }
    } catch (...) {
    }
}

void ProjectBrowserPanel::RefreshCache() {
    auto newRoot = std::make_unique<DirectoryNode>();
    newRoot->path = projectRootPath_;
    std::string folderName = projectRootPath_.filename().string();
    if (folderName.empty()) {
        folderName = "Root";
    }
    newRoot->folderName = folderName;

    BuildDirectoryTree(newRoot.get());

    rootNode_ = std::move(newRoot);

    // カレントパスが存在しなくなっていればルートに戻す
    if (!std::filesystem::exists(currentProjectBrowserPath_)) {
        currentProjectBrowserPath_ = projectRootPath_;
    }
}

const DirectoryNode* ProjectBrowserPanel::FindNode(const DirectoryNode* node,
                                                   const std::filesystem::path& targetPath) const {
    if (!node) {
        return nullptr;
    }
    if (node->path == targetPath) {
        return node;
    }
    for (const auto& child : node->subDirectories) {
        if (const DirectoryNode* found = FindNode(child.get(), targetPath)) {
            return found;
        }
    }
    return nullptr;
}

void ProjectBrowserPanel::DrawProjectBrowserTree(const DirectoryNode* node) {
    if (!node) {
        return;
    }

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

    // 現在選択されているフォルダならハイライト
    if (node->path == currentProjectBrowserPath_) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // 中身にフォルダがあるかチェック
    bool hasSubDirectories = !node->subDirectories.empty();

    if (!hasSubDirectories) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // フォルダ名でツリーノードを描画
    std::string pathId = node->path.string();
    std::string treeLabel = std::string(ICON_FA_FOLDER) + " " + node->folderName;
    bool isOpen = ImGui::TreeNodeEx(pathId.c_str(), flags, "%s", treeLabel.c_str());

    // クリックされたら右ペインの表示を切り替え
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        currentProjectBrowserPath_ = node->path;
    }

    if (isOpen && hasSubDirectories) {
        for (const auto& child : node->subDirectories) {
            DrawProjectBrowserTree(child.get());
        }
        ImGui::TreePop();
    }
}

void ProjectBrowserPanel::Draw() {
    if (!editorManager_) {
        return;
    }

    // 監視スレッドからの通知があれば自動リフレッシュ
    if (isCacheDirty_.exchange(false)) {
        RefreshCache();

        // エンジンの各マネージャへの再スキャン通知をメインスレッド上で安全に実行
        if (auto* engine = editorManager_->GetEngine()) {
            if (auto* mm = engine->GetObjModelManager()) {
                mm->RefreshAvailableModels();
            }
            if (auto* tm = engine->GetTextureManager()) {
                tm->LoadAllFromFolder("resources/");
            }
        }
    }

    ImGui::Begin(GetName(), &isOpen_);

    // Refreshボタンを追加
    if (ImGui::Button(ICON_FA_ROTATE_RIGHT " Refresh")) {
        RefreshCache();
    }
    ImGui::SameLine();

    // 上部に「上へ」戻るボタンと現在のパスを表示
    if (ImGui::Button("Up") && currentProjectBrowserPath_.has_parent_path()) {
        currentProjectBrowserPath_ = currentProjectBrowserPath_.parent_path();
        if (currentProjectBrowserPath_.string().length() < projectRootPath_.string().length()) {
            currentProjectBrowserPath_ = projectRootPath_; // ルートより上には行かない
        }
    }
    ImGui::SameLine();

    // パスもUTF-8に変換して表示
    std::string currentPathStr = reinterpret_cast<const char*>(currentProjectBrowserPath_.u8string().c_str());
    ImGui::Text("%s", currentPathStr.c_str());
    ImGui::Separator();

    // 検索フィルタの描画
    projectBrowserFilter_->Draw("Search", ImGui::GetContentRegionAvail().x);
    ImGui::Separator();

    // --- 左右にペインを分割 (ImGui::Table) ---
    if (ImGui::BeginTable("ProjectBrowserTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {

        // --- 左ペイン（フォルダツリー） ---
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        ImGui::BeginChild("ProjectTreePane", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        DrawProjectBrowserTree(rootNode_.get());
        ImGui::EndChild();

        // --- 右ペイン（フォルダの中身） ---
        ImGui::TableSetColumnIndex(1);

        ImGui::BeginChild("ProjectContentPane", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        const DirectoryNode* currentNode = FindNode(rootNode_.get(), currentProjectBrowserPath_);
        if (currentNode) {
            float itemWidth = 80.0f;
            float itemHeight = 90.0f;
            float windowVisibleX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

            bool refreshNeeded = false; // ループ内でフラグを立てる用

            auto drawEntry = [&](const std::filesystem::path& path, const std::string& filenameString,
                                 const std::string& ext, bool isDir) {
                // フィルタリング（マッチしなければスキップ）
                if (!projectBrowserFilter_->PassFilter(filenameString.c_str())) {
                    return;
                }

                // 拡張子による色とアイコンの判定
                ImVec4 textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // デフォルト白
                std::string icon = ICON_FA_FILE;

                if (isDir) {
                    icon = ICON_FA_FOLDER;
                    textColor = ImVec4(1.0f, 1.0f, 0.4f, 1.0f); // 黄色
                } else {
                    if (ext == ".png" || ext == ".jpg" || ext == ".dds" || ext == ".bmp") {
                        icon = ICON_FA_IMAGE;
                        textColor = ImVec4(0.4f, 0.8f, 1.0f, 1.0f); // 水色
                    } else if (ext == ".obj" || ext == ".gltf" || ext == ".fbx" || ext == ".glb" || ext == ".mtl") {
                        icon = ICON_FA_CUBES;
                        textColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); // 緑色
                    } else if (ext == ".prefab") {
                        icon = ICON_FA_CUBE;
                        textColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f); // 青色
                    } else if (ext == ".json") {
                        icon = ICON_FA_MAP;
                        textColor = ImVec4(1.0f, 0.6f, 0.2f, 1.0f); // オレンジ色
                    } else if (ext == ".wav" || ext == ".mp3") {
                        icon = ICON_FA_MUSIC;
                        textColor = ImVec4(1.0f, 0.4f, 0.8f, 1.0f); // ピンク色
                    }
                }

                ImGui::PushID(path.string().c_str());
                ImGui::BeginGroup();

                bool isRenamingThis = (renamingTarget_ == path);

                // --- タイルの下地となるSelectable ---
                bool isSelected = (selectedPath_ == path);
                bool isDoubleClick = false;
                if (ImGui::Selectable("##Tile", isSelected, ImGuiSelectableFlags_AllowDoubleClick,
                                      ImVec2(itemWidth, itemHeight))) {
                    selectedPath_ = path; // クリックで選択状態にする
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        isDoubleClick = true;
                    }
                }

                ImVec2 itemMin = ImGui::GetItemRectMin();

                // フォルダのダブルクリックによる移動
                if (isDoubleClick && isDir) {
                    currentProjectBrowserPath_ = path;
                    ImGui::EndGroup();
                    ImGui::PopID();
                    return; // イテレータ的に問題はないがUIとしては抜ける
                }

                // プレハブのダブルクリックによるPrefabモード移行
                bool isPrefabFile = false;
                if (!isDir && (ext == ".prefab" || ext == ".json")) {
                    std::string pathLower = path.generic_string();
                    std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
                    if (ext == ".prefab" || pathLower.find(".prefab.json") != std::string::npos ||
                        pathLower.find("prefabs/") != std::string::npos || pathLower.find("prefabs\\") != std::string::npos) {
                        isPrefabFile = true;
                    }
                }

                if (isDoubleClick && isPrefabFile) {
                    if (editorManager_) {
                        editorManager_->EnterPrefabMode(path.generic_string());
                    }
                }

                // --- 右クリックメニュー ---
                if (ImGui::BeginPopupContextItem()) {
                    if (isPrefabFile) {
                        if (ImGui::MenuItem(ICON_FA_CUBE " Edit Prefab")) {
                            if (editorManager_) {
                                editorManager_->EnterPrefabMode(path.generic_string());
                            }
                        }
                        ImGui::Separator();
                    }
                    if (ImGui::MenuItem("Rename")) {
                        renamingTarget_ = path;
                        strncpy_s(projectBrowserInputBuffer_, sizeof(projectBrowserInputBuffer_),
                                  filenameString.c_str(), _TRUNCATE);
                    }
                    if (ImGui::MenuItem("Delete")) {
                        try {
                            std::filesystem::remove_all(path);
                            refreshNeeded = true;
                        } catch (...) {
                        }
                    }
                    ImGui::EndPopup();
                }

                // --- ドラッグ＆ドロップソース (ファイルのみ) ---
                if (!isDir && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    std::string payloadPath;
                    try {
                        payloadPath = reinterpret_cast<const char*>(
                            std::filesystem::relative(path, std::filesystem::current_path()).u8string().c_str());
                        std::replace(payloadPath.begin(), payloadPath.end(), '\\', '/');
                    } catch (...) {
                        payloadPath = reinterpret_cast<const char*>(path.u8string().c_str());
                    }
                    ImGui::SetDragDropPayload(EditorDragDrop::PayloadAssetPath, payloadPath.c_str(),
                                              payloadPath.length() + 1);
                    ImGui::Text("Place Asset: %s", filenameString.c_str());
                    ImGui::EndDragDropSource();
                }

                // --- 見た目の描画 (アイコンとファイル名) ---
                if (isRenamingThis) {
                    ImGui::SetCursorScreenPos(ImVec2(itemMin.x + 4.0f, itemMin.y + 40.0f));
                    ImGui::SetKeyboardFocusHere();
                    ImGui::PushItemWidth(itemWidth - 8.0f);
                    if (ImGui::InputText("##rename", projectBrowserInputBuffer_, sizeof(projectBrowserInputBuffer_),
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                        try {
                            std::string newNameString = projectBrowserInputBuffer_;
                            if (!newNameString.empty() && newNameString != filenameString) {
                                std::filesystem::path newPath = path.parent_path() / newNameString;
                                std::filesystem::rename(path, newPath);
                                refreshNeeded = true;
                            }
                        } catch (...) {
                        }
                        renamingTarget_.clear();
                    }
                    ImGui::PopItemWidth();

                    if (!ImGui::IsItemActive() && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1))) {
                        renamingTarget_.clear();
                    }
                } else {
                    // 中央にアイコンを描画
                    ImVec2 iconSize = ImGui::CalcTextSize(icon.c_str());
                    ImGui::GetWindowDrawList()->AddText(
                        ImVec2(itemMin.x + (itemWidth - iconSize.x) * 0.5f, itemMin.y + 20.0f),
                        ImGui::GetColorU32(textColor), icon.c_str());

                    // 下部にファイル名を描画 (切り詰め処理)
                    std::string displayName = filenameString;
                    if (displayName.length() > 10) {
                        displayName = displayName.substr(0, 8) + "..";
                    }
                    ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
                    ImGui::GetWindowDrawList()->AddText(
                        ImVec2(itemMin.x + (itemWidth - textSize.x) * 0.5f, itemMin.y + 50.0f),
                        ImGui::GetColorU32(ImGuiCol_Text), displayName.c_str());
                }

                ImGui::EndGroup();
                ImGui::PopID();

                // --- 折り返しの計算 ---
                float lastItemMaxX = ImGui::GetItemRectMax().x;
                float nextItemMaxX = lastItemMaxX + ImGui::GetStyle().ItemSpacing.x + itemWidth;
                if (nextItemMaxX < windowVisibleX) {
                    ImGui::SameLine();
                }
            }; // drawEntry lambda

            // ディレクトリ一覧を描画
            for (const auto& childDir : currentNode->subDirectories) {
                drawEntry(childDir->path, childDir->folderName, "", true);
                if (currentProjectBrowserPath_ != currentNode->path) {
                    break; // 移動した場合はループを抜ける
                }
            }

            // ファイル一覧を描画
            if (currentProjectBrowserPath_ == currentNode->path) { // 移動していなければ
                for (const auto& file : currentNode->files) {
                    drawEntry(file.path, file.filenameString, file.ext, false);
                }
            }

            // 新規フォルダ作成の入力フィールド
            if (isCreatingFolder_) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.4f, 1.0f)); // フォルダの黄色
                ImGui::Text("%s", ICON_FA_FOLDER);
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::SetKeyboardFocusHere();
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##newfolder", projectBrowserInputBuffer_, sizeof(projectBrowserInputBuffer_),
                                     ImGuiInputTextFlags_EnterReturnsTrue)) {
                    try {
                        std::string newNameString = projectBrowserInputBuffer_;
                        if (!newNameString.empty()) {
                            std::filesystem::path newPath = currentProjectBrowserPath_ / newNameString;
                            std::filesystem::create_directory(newPath);
                            refreshNeeded = true;
                        }
                    } catch (...) {
                    }
                    isCreatingFolder_ = false;
                }
                ImGui::PopItemWidth();

                if (!ImGui::IsItemActive() &&
                    (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
                    isCreatingFolder_ = false;
                }
            }

            if (refreshNeeded) {
                RefreshCache();
            }
        } // if (currentNode)

        // ウィンドウ全体に対する右クリックメニュー（空白部分用）
        if (ImGui::BeginPopupContextWindow("ProjectBrowserContext",
                                           ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
            if (ImGui::MenuItem("Create Folder")) {
                isCreatingFolder_ = true;
                projectBrowserInputBuffer_[0] = '\0'; // バッファをクリア
            }
            ImGui::EndPopup();
        }

        ImGui::EndChild(); // End ProjectContentPane
        ImGui::EndTable(); // End ProjectBrowserTable
    } // End if (BeginTable)

    ImGui::End();
}
#endif // EditorMode
