#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "Renderer/Camera/Camera.h"

/**
 * @class CameraManager
 * @brief 複数のカメラを管理し、現在描画に使用する（アクティブな）カメラを切り替えるクラス
 */
class CameraManager {
public:
    CameraManager() = default;
    ~CameraManager() = default;

    /**
     * @brief カメラを追加します
     * @param name カメラの識別名（例: "Main", "Debug"）
     * @param camera カメラのインスタンス
     */
    void AddCamera(const std::string& name, std::shared_ptr<Camera> camera);

    /**
     * @brief カメラを削除します
     * @param name 削除するカメラの識別名
     */
    void RemoveCamera(const std::string& name);

    /**
     * @brief 指定したカメラインスタンスを削除します
     * @param camera 削除するカメラのインスタンス
     */
    void RemoveCamera(const std::shared_ptr<Camera>& camera);

    /**
     * @brief アクティブ（現在描画に使用する）カメラを設定します
     * @param name アクティブにするカメラの識別名
     */
    void SetActiveCamera(const std::string& name);

    /**
     * @brief 現在アクティブなカメラを取得します
     * @return Camera* アクティブなカメラ。見つからない場合は nullptr
     */
    Camera* GetActiveCamera() const;

    /**
     * @brief 現在アクティブなカメラの名前を取得します
     * @return std::string アクティブなカメラの名前
     */
    const std::string& GetActiveCameraName() const;

    /**
     * @brief 指定した名前のカメラを取得します
     * @param name カメラの識別名
     * @return Camera* カメラ。見つからない場合は nullptr
     */
    Camera* GetCamera(const std::string& name) const;

    /**
     * @brief 登録されているすべてのカメラの更新処理を行います
     */
    void Update();

    /**
     * @brief ウィンドウリサイズ時に全てのカメラの解像度を更新します
     */
    void OnResize(int width, int height);

    /**
     * @brief 登録されているすべてのカメラをクリアします（シーン遷移時用）
     */
    void Clear();

private:
    std::unordered_map<std::string, std::shared_ptr<Camera>> cameras_;
    std::string activeCameraName_;
    Camera* activeCameraCache_ = nullptr; ///< 現在アクティブなカメラへのキャッシュ（ホットパス最適化）
};
