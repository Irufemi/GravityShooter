#pragma once
#include "Framework/Component/Component.h"
#include <string>
#include <memory>
#include "Renderer/Object/Particle/ParticleObject.h"

class TransformComponent;

/**
 * @class ParticleEmitterComponent
 * @brief エディタ用コンポーネント。実際の描画・ロジックは内部の ParticleObject に委譲します。
 */
class ParticleEmitterComponent : public Component {
public:
    ParticleEmitterComponent();
    ~ParticleEmitterComponent() override;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize() override;
    /**
     * @brief 生成時の自己完結初期化（ParticleObjectの初期化）を行います
     */
    void OnAwake() override;
    /**
     * @brief ワールド座標・Transform確定時の座標反映を行います
     */
    void OnSpawned() override;
    /**
     * @brief Update を実行する。
     */
    void Update() override;
    /**
     * @brief Draw を実行する。
     */
    void Draw() override;
    /**
     * @brief 非アクティブ化時の処理（放出の即時停止）
     */
    void OnDisable() override;
    /**
     * @brief 破棄時の処理（GPUエミッター登録の即時解除）
     */
    void OnDestroy() override;

    /**
     * @brief Renderable を取得する。
     * @return 取得された Renderable
     */
    IRenderable* GetRenderable() override {
        return nullptr;
    }

    /**
     * @brief CanUpdateInEditMode かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool CanUpdateInEditMode() const override {
        return true;
    } // エディタでのプレビュー更新を許可

    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "ParticleEmitterComponent";
    }
    /**
     * @brief OnRegisterProperties を実行する。
     */
    void OnRegisterProperties() override;

    /**
     * @brief Serialize を実行する。
     */
    nlohmann::json Serialize() override;
    /**
     * @brief Deserialize を実行する。
     */
    void Deserialize(const nlohmann::json& j) override;

    /**
     * @brief クローンを作成する
     */
    std::shared_ptr<Component> Clone() override;

    /**
     * @brief Play を実行する。
     */
    void Play();
    /**
     * @brief Restart を実行する。
     */
    void Restart(bool withChildren = true);
    /**
     * @brief Stop を実行する。
     */
    void Stop();

    /**
     * @brief EmitBurst を実行する。
     */
    void EmitBurst(int count);

    // エディタや他スクリプトから実体へアクセスするためのゲッター
    /**
     * @brief ParticleObject を取得する。
     * @return 取得された ParticleObject
     */
    ParticleObject* GetParticleObject() const {
        return particleObj_.get();
    }

private:
    std::unique_ptr<ParticleObject> particleObj_;
};
