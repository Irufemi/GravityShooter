#pragma once
#include "Framework/Component/Component.h"
#include "Renderer/System/ParticleGPU/GPUParticleManager.h"

class TransformComponent;

/**
 * @class ParticleFieldComponent
 * @brief GPUパーティクル用の重力や風などのフォースフィールドを定義するコンポーネント
 * @details 特定領域内のパーティクルに対して持続的な力を与えることで、竜巻や爆発などの複雑なエフェクトを表現します。
 */
class ParticleFieldComponent : public Component {
public:
    /**
     * @brief コンストラクタ
     */
    ParticleFieldComponent();

    /**
     * @brief デストラクタ
     */
    ~ParticleFieldComponent() override;

    /**
     * @brief コンポーネントの初期化
     * @details 後方互換性のためのラッパーです。
     */
    void Initialize() override;

    /**
     * @brief ゲーム開始・シーン参加時のフィールド登録を行います
     */
    void Start() override;

    /**
     * @brief 破棄時にGPUParticleManagerから登録解除します
     */
    void OnDestroy() override;

    /**
     * @brief 毎フレームの更新処理
     * @details インスペクタ等で変更されたフィールドパラメータを、GPUParticleManagerに同期させます。
     */
    void Update() override;

    /**
     * @brief 描画処理（空実装）
     */
    void Draw() override {}

    /**
     * @brief レンダラブルなオブジェクトを取得する（非対応）
     * @return 常にnullptr
     */
    IRenderable* GetRenderable() override {
        return nullptr;
    }

    /**
     * @brief エディタモードでもUpdateを実行するかどうか
     * @return true (エディタ編集中もパーティクルへの影響を可視化するため)
     */
    bool CanUpdateInEditMode() const override {
        return true;
    }

    /**
     * @brief コンポーネント名の取得
     * @return "ParticleFieldComponent"
     */
    std::string GetComponentName() const override {
        return "ParticleFieldComponent";
    }

    /**
     * @brief インスペクタ用プロパティの登録
     * @details フィールドの種類、強さ、サイズなどをエディタから編集できるように登録します。
     */
    void OnRegisterProperties() override;

    /**
     * @brief シリアライズ
     * @return フィールドパラメータを含むJSONオブジェクト
     */
    nlohmann::json Serialize() override;

    /**
     * @brief デシリアライズ
     * @param[in] j 読み込むJSONオブジェクト
     */
    void Deserialize(const nlohmann::json& j) override;

    /**
     * @brief クローンを作成する
     */
    std::shared_ptr<Component> Clone() override;

    /**
     * @brief フィールドデータへの参照を取得する
     * @return 更新や取得のための ParticleField 構造体への参照
     */
    ParticleField& GetFieldData() {
        return fieldData_;
    }
    /**
     * @brief フィールドデータへの参照を取得する（読み取り専用）
     * @return 読み取り専用の ParticleField 構造体への参照
     */
    const ParticleField& GetFieldData() const {
        return fieldData_;
    }

private:
    GPUParticleManager::FieldHandle fieldHandle_;
    GPUParticleManager* gpuParticleManager_ = nullptr;

    ParticleField fieldData_;
    int fieldType_ = 0;
};
