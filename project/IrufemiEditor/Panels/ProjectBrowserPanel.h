#pragma once

#ifdef EditorMode
#include "Core/IEditorPanel.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <atomic>

struct ImGuiTextFilter;

namespace Irufemi {
class DirectoryWatcher;
}

/**
 * @struct FileEntry
 * @brief キャッシュ用のファイルエントリ情報
 */
struct FileEntry {
    std::filesystem::path path;
    std::string filenameString; // 文字化け防止済みのUTF-8文字列
    std::string ext;            // 小文字化済みの拡張子
    bool isDirectory;
};

/**
 * @struct DirectoryNode
 * @brief キャッシュ用のディレクトリツリーノード
 */
struct DirectoryNode {
    std::filesystem::path path;
    std::string folderName;
    std::vector<FileEntry> files;                               // このフォルダ内のファイル一覧
    std::vector<std::unique_ptr<DirectoryNode>> subDirectories; // サブディレクトリ一覧
};

/**
 * @class ProjectBrowserPanel
 * @brief プロジェクト内のファイルやディレクトリを閲覧・操作するパネル
 */
class ProjectBrowserPanel : public IEditorPanel {
public:
    ProjectBrowserPanel();
    ~ProjectBrowserPanel() override;

    void Initialize(EditorManager* editorManager) override;
    void Draw() override;
    const char* GetName() const override {
        return "Project";
    }

    /**
     * @brief ファイルシステムをスキャンし、キャッシュを最新状態に更新する
     */
    void RefreshCache();

private:
    void DrawProjectBrowserTree(const DirectoryNode* node);
    const DirectoryNode* FindNode(const DirectoryNode* node, const std::filesystem::path& targetPath) const;
    void BuildDirectoryTree(DirectoryNode* node);

    EditorManager* editorManager_ = nullptr;

    // Project Browser 用のパス管理
    std::filesystem::path projectRootPath_;
    std::filesystem::path currentProjectBrowserPath_;
    std::filesystem::path selectedPath_;

    // キャッシュ
    std::unique_ptr<DirectoryNode> rootNode_;

    // Project Browser 用の検索フィルタ
    std::unique_ptr<ImGuiTextFilter> projectBrowserFilter_;

    // Project Browser 用のファイル操作状態
    std::filesystem::path renamingTarget_;
    char projectBrowserInputBuffer_[256] = "";
    bool isCreatingFolder_ = false;

    // 自動ファイル監視
    std::unique_ptr<Irufemi::DirectoryWatcher> directoryWatcher_;
    std::atomic<bool> isCacheDirty_ = false;
};

#endif // EditorMode
