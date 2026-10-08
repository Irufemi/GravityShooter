#pragma once
#include "Framework/Component/Component.h"
#include "Renderer/System/VoxelParticle/VoxelParticleSystem.h"
#include "Core/Math/Vector3Int.h"
#include <string>

/**
 * @class VoxelParticleComponent
 * @brief 既存のレンダラーモデル等を利用してボクセルパーティクルを発生させるコンポーネント。
 *        シーンロード時に事前計算（ReservePool）を行い、再生遅延を防ぎます。
 */
class VoxelParticleComponent : public Component {
public:
    VoxelParticleComponent();
    ~VoxelParticleComponent() override;

    /**
     * @brief Initialize を実行する。
     */
    void Initialize() override;
    /**
     * @brief 生成時のモデル名キャッシュを行います
     */
    void OnAwake() override;
    /**
     * @brief ゲーム開始・シーン参加時のVoxelParticleManagerプール予約を行います
     */
    void Start() override;
    /**
     * @brief Update を実行する。
     */
    void Update() override;
    /**
     * @brief Draw を実行する。
     */
    void Draw() override;

    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "VoxelParticleComponent";
    }
    /**
     * @brief Serialize を実行する。
     */
    nlohmann::json Serialize() override;
    /**
     * @brief Deserialize を実行する。
     */
    void Deserialize(const nlohmann::json& j) override;

    /**
     * @brief OverrideModelName を取得する。
     * @return 取得された OverrideModelName
     */
    const std::string& GetOverrideModelName() const {
        return overrideModelName_;
    }
    /**
     * @brief OverrideModelName を設定する。
     * @param[in] name 設定する OverrideModelName の値
     */
    void SetOverrideModelName(const std::string& name) {
        overrideModelName_ = name;
    }

    /**
     * @brief Resolution を取得する。
     * @return 取得された Resolution
     */
    Irufemi::Vector3Int GetResolution() const {
        return resolution_;
    }
    /**
     * @brief Resolution を設定する。
     * @param[in] res 設定する Resolution の値
     */
    void SetResolution(const Irufemi::Vector3Int& res) {
        resolution_ = res;
    }

    /**
     * @brief PreAllocateCount を取得する。
     * @return 取得された PreAllocateCount
     */
    int GetPreAllocateCount() const {
        return preAllocateCount_;
    }
    /**
     * @brief PreAllocateCount を設定する。
     * @param[in] count 設定する PreAllocateCount の値
     */
    void SetPreAllocateCount(int count) {
        preAllocateCount_ = count;
    }

    /**
     * @brief EmitterParams を取得する。
     * @return 取得された EmitterParams
     */
    VoxelEmitter& GetEmitterParams() {
        return emitterParams_;
    }
    /**
     * @brief EmitterParams を取得する（読み取り専用）。
     * @return 取得された EmitterParams
     */
    const VoxelEmitter& GetEmitterParams() const {
        return emitterParams_;
    }

    /**
     * @brief その場にパーティクルを放出します（実装保留・拡張用）
     */
    void Emit();

    /**
     * @brief 爆発的にパーティクルを四散させます。
     * @param velocity 初速
     * @param rotate 回転成分
     * @param scale スケール成分
     */
    void Explode(const Irufemi::Vector3& velocity = {0, 0, 0}, const Irufemi::Vector3& rotate = {0, 0, 0},
                 const Irufemi::Vector3& scale = {1, 1, 1});

private:
    // 詳細パラメータ
    VoxelEmitter emitterParams_{};

    // システム設定
    Irufemi::Vector3Int resolution_ = {32, 32, 32}; ///< ボクセルの分割数
    int preAllocateCount_ = 1;                      ///< シーン開始時に確保・事前計算しておく数
    std::string overrideModelName_; ///< 空欄ならMeshRenderer等を利用、指定があればプロキシモデルを利用

    // キャッシュ用
    std::string cachedModelName_;
    bool isInitialized_ = false;

    /**
     * @brief 使用する対象のモデル名を取得します（オーバーライド優先）
     */
    std::string GetTargetModelName();
};
