#pragma once

#ifdef EditorMode
#include <memory>
#include <vector>
#include <string>
#include "Core/Math/Vector3.h"
#include "Core/System/IEngineExtension.h"

class IrufemiEngine;
class GameObject;
class Camera;
class IEditorPanel;
class EditorActionManager;
class EditorShortcutManager;
class ComponentEditorRegistry;

/**
 * @brief シミュレーション実行状態 (Unity: EditorApplication.isPlaying/isPaused)
 */
enum class PlayState {
    Stopped, ///< 停止中（通常編集）
    Playing, ///< 実行中
    Paused   ///< 一時停止中
};

/**
 * @brief 編集対象コンテキスト (Unity: PrefabStage)
 */
enum class EditContext {
    Scene,   ///< 通常シーン編集
    Prefab   ///< プレハブ孤立編集ステージ
};

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

    PlayState GetPlayState() const {
        return playState_;
    }
    EditContext GetEditContext() const {
        return editContext_;
    }

    bool IsPlayMode() const {
        return playState_ == PlayState::Playing || playState_ == PlayState::Paused;
    }
    bool IsPlaying() const {
        return playState_ == PlayState::Playing;
    }
    bool IsPaused() const {
        return playState_ == PlayState::Paused;
    }
    bool IsPrefabMode() const {
        return editContext_ == EditContext::Prefab;
    }
    std::shared_ptr<GameObject> GetEditingPrefabRoot() const {
        return editingPrefabRoot_;
    }
    const std::string& GetEditingPrefabPath() const {
        return editingPrefabPath_;
    }

    /**
     * @brief 現在の編集コンテキストに応じてシーンまたはプレハブを保存する
     */
    void SaveCurrent();

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
     * @brief ゲーム内から安全にプレイモードを終了するための遅延リクエスト
     */
    void RequestExitPlayMode() {
        isExitPlayModeRequested_ = true;
    }

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

    /**
     * @brief ビューポートカメラに対するフレーミング・フォーカスを要求する (Unity: SceneView.FrameSelected)
     * @param target 注視点（ピボット）
     * @param radius バウンディング半径
     */
    void RequestCameraFrame(const Irufemi::Vector3& target, float radius = 2.0f) {
        cameraFrameTarget_ = target;
        cameraFrameRadius_ = radius;
        hasCameraFrameRequest_ = true;
    }

    bool ConsumeCameraFrameRequest(Irufemi::Vector3& outTarget, float& outRadius) {
        if (hasCameraFrameRequest_) {
            outTarget = cameraFrameTarget_;
            outRadius = cameraFrameRadius_;
            hasCameraFrameRequest_ = false;
            return true;
        }
        return false;
    }

private:
    IrufemiEngine* engine_ = nullptr;

    PlayState playState_ = PlayState::Stopped;
    EditContext editContext_ = EditContext::Scene;
    bool hasCameraFrameRequest_ = false;
    Irufemi::Vector3 cameraFrameTarget_{0.0f, 0.0f, 0.0f};
    float cameraFrameRadius_ = 2.0f;
    std::string playModeStartSceneName_ = "";
    std::string editingPrefabPath_ = "";
    std::shared_ptr<GameObject> editingPrefabRoot_ = nullptr; ///< 編集中のプレハブルートオブジェクト（明示的保持）
    std::shared_ptr<Camera> stageCamera_ = nullptr;           ///< ステージ専用エディタカメラ（純粋なViewport Camera）
    bool isStepRequested_ = false;                            // コマ送りの予約フラグ
    bool isPickingAllowedInPlayMode_ = false; // プレイ中のオブジェクト選択（ピッキング）許可フラグ
    bool isExitPlayModeRequested_ = false; // ゲーム内からのプレイモード終了遅延リクエストフラグ

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
