#pragma once

#ifdef EditorMode

class EditorManager;

/**
 * @class IEditorPanel
 * @brief エディタの各パネルの基底となるインターフェース
 */
class IEditorPanel {
public:
    virtual ~IEditorPanel() = default;

    /**
     * @brief パネルの初期化
     * @param[in] editorManager 親となるEditorManagerのポインタ
     */
    virtual void Initialize(EditorManager* editorManager) = 0;

    /**
     * @brief パネルの描画
     */
    virtual void Draw() = 0;

    /**
     * @brief パネル名を取得する
     * @return パネル名の文字列ポインタ
     */
    virtual const char* GetName() const = 0;



    /**
     * @brief パネルが開いているか判定する
     * @return 開いている場合 true
     */
    bool IsOpen() const {
        return isOpen_;
    }

    /**
     * @brief パネルの開閉状態を設定する
     * @param[in] open 開く場合 true
     */
    void SetOpen(bool open) {
        isOpen_ = open;
    }

protected:
    bool isOpen_ = true;
};

#endif // EditorMode
