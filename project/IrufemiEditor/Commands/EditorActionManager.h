#pragma once

#ifdef EditorMode
#include <memory>
#include <string>
#include <deque>
#include "Core/ICommand.h"
#include "Core/Math/Vector3.h"

namespace Irufemi {
class GameObject;
}
using Irufemi::GameObject;
class EditorManager;

/**
 * @class EditorActionManager
 * @brief エディタ経由でのGameObject生成・削除・値変更等のアクションを統括し、Undo/Redoを管理するマネージャ
 */
class EditorActionManager {
public:
    explicit EditorActionManager(EditorManager* editor);

    /** @name コマンド管理 (Undo/Redo) */
    ///@{
    /**
     * @brief 新しいコマンドを実行し、履歴に追加する
     */
    void PushAndExecute(std::unique_ptr<ICommand> command);

    /**
     * @brief 一つ前の操作に戻す
     */
    void Undo();

    /**
     * @brief 戻した操作をやり直す
     */
    void Redo();

    /**
     * @brief 履歴をすべて消去する
     */
    void ClearHistory();
    ///@}

    /** @name オブジェクト操作 */
    ///@{
    /**
     * @brief アセット（モデルや画像）のパスから適切なGameObjectを生成してシーンに追加する
     * @param assetPath アセットのファイルパス
     * @param position 生成するワールド座標（SceneViewなどからのドロップ時に指定）
     */
    void CreateObjectFromAsset(const std::string& assetPath, const Irufemi::Vector3& position = {0.0f, 0.0f, 0.0f});

    /**
     * @brief 空のGameObject、または指定したプリミティブを生成する
     */
    void CreatePrimitiveObject(const std::string& typeName);

    /**
     * @brief 指定したGameObjectを複製する
     */
    void DuplicateObject(std::shared_ptr<GameObject> target);

    /**
     * @brief 指定したGameObjectをシーンから削除し、選択を解除する
     */
    void DeleteObject(std::shared_ptr<GameObject> target);
    ///@}

private:
    EditorManager* editorManager_ = nullptr;

    std::deque<std::unique_ptr<ICommand>> undoStack_;
    std::deque<std::unique_ptr<ICommand>> redoStack_;
    const size_t maxHistory_ = 100;
};

#endif // EditorMode
