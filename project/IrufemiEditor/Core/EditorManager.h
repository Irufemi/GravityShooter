#pragma once

#ifdef EditorMode
#include <memory>
#include <vector>
#include <string>
#include "Core/Math/Vector3.h"
#include "Core/System/IEngineExtension.h"

class IrufemiEngine;
class GameObject;
class IEditorPanel;
class EditorActionManager;
class EditorShortcutManager;
class ComponentEditorRegistry;

/**
 * @brief エディタの現在の動作モード
 */
enum class EditorModeState { Edit, Playing, Paused, PrefabEdit };

/**
 * @class EditorManager
 * @brief エディタのUIレイアウト（DockSpace、SceneViewなど）を統括するマネージャ
 */
class EditorManager : public IEngineExtension {
public:
    EditorManager();
    ~EditorManager() override;

    static EditorManager* GetInstance();

    void OnInitialize(IrufemiEngine* engine) override;
    void OnUpdate(float deltaTime) override;
    void OnDrawUI() override;

    /** @name 各パネルからアクセスするための状態管理 Getter/Setter */
    ///@{
    IrufemiEngine* GetEngine() const {
        return engine_;
    }

    EditorActionManager* GetActionManager() const {
        return actionManager_.get();
    }
    EditorShortcutManager* GetShortcutManager() const {
        return shortcutManager_.get();
    }
    ComponentEditorRegistry* GetComponentEditorRegistry() const {
        return componentEditorRegistry_.get();
    }

    std::shared_ptr<GameObject> GetSelectedObject() const;
    void SetSelectedObject(std::shared_ptr<GameObject> obj);
    void ClearSelectedObject();

    EditorModeState GetCurrentMode() const {
        return currentMode_;
    }
    bool IsPlayMode() const {
        return currentMode_ != EditorModeState::Edit;
    }
    bool IsPickingAllowedInPlayMode() const {
        return isPickingAllowedInPlayMode_;
    }
    void SetPickingAllowedInPlayMode(bool allow) {
        isPickingAllowedInPlayMode_ = allow;
    }
    ///@}

    /**
     * @brief 編集モードからプレイモードへ移行し、現在のシーン状態を一時保存する
     */
    void EnterPlayMode();

    /**
     * @brief プレイモードから編集モードへ戻り、シーン状態を一時保存から復元する
     */
    void ExitPlayMode();

    /**
     * @brief プレイ中に一時停止/再開を切り替える
     */
    void TogglePauseMode();

    /**
     * @brief Prefabモードに入る
     */
    void EnterPrefabMode(const std::string& prefabPath);

    /**
     * @brief Prefabモードから出る
     * @param saveChanges trueなら現在のルートオブジェクトをPrefabとして上書き保存する
     */
    void ExitPrefabMode(bool saveChanges);

    /**
     * @brief プレハブ編集用カメラをプレハブ全体が収まる最適な位置へ自動フォーカス（Auto-Framing）する
     */
    void FramePrefabObject();

private:
    IrufemiEngine* engine_ = nullptr;

    EditorModeState currentMode_ = EditorModeState::Edit;
    std::string playModeStartSceneName_ = "";
    std::string editingPrefabPath_ = "";
    std::shared_ptr<GameObject> editingPrefabRoot_ = nullptr; ///< 編集中のプレハブルートオブジェクト（明示的保持）
    std::shared_ptr<GameObject> stageCameraObject_ = nullptr;  ///< ステージ専用エディタカメラ（Transient）
    bool isStepRequested_ = false;            // コマ送りの予約フラグ
    bool isPickingAllowedInPlayMode_ = false; // プレイ中のオブジェクト選択（ピッキング）許可フラグ

    // PrefabMode 突入前のメインシーンカメラ状態の退避・復元
    std::string savedActiveCameraName_ = "";
    Irufemi::Vector3 savedCameraTranslate_{0.0f, 0.0f, -10.0f};
    Irufemi::Vector3 savedCameraRotate_{0.0f, 0.0f, 0.0f};
    float savedCameraFov_ = 0.785398f;
    bool hasSavedCameraState_ = false;

    // レイアウトのリセット用フラグ
    bool resetLayout_ = false;

    std::unique_ptr<EditorActionManager> actionManager_;
    std::unique_ptr<EditorShortcutManager> shortcutManager_;
    std::unique_ptr<ComponentEditorRegistry> componentEditorRegistry_;

    // 各エディタパネル
    std::vector<std::unique_ptr<IEditorPanel>> panels_;
};

#endif // EditorMode
