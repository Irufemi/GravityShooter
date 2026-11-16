#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/MathFunction.h"
#include "Core/System/ComponentPool.h"

namespace Irufemi {

class TransformComponent : public Component {
public:
    /**
     * @brief Initialize を実行する。
     */
    void Initialize() override {}
    // Update() は一括更新 (UpdateAll) に移行したため完全に削除し、GameObjectのUpdateループから外す

    /**
     * @brief 自身の行列を計算する（Dirtyフラグベースの遅延評価付き）
     * @param force 親が変更された等の理由による強制更新フラグ
     */
    void ComputeMatrix(bool force = false) const;

    /**
     * @brief ダーティフラグが立っている場合、行列を最新化する（Lazy Evaluation用ヘルパー）
     */
    inline void CheckAndComputeMatrix() const {
        ComputeMatrix(false);
    }

    /**
     * @brief プール上の全Transformを一括更新する（DOD）
     */
    static void UpdateAll();

    /**
     * @brief CanUpdateInEditMode かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool CanUpdateInEditMode() const override {
        return true;
    }

    // --- Getters ---
    /**
     * @brief Position を取得する。
     * @return 取得された Position
     */
    const Irufemi::Vector3& GetPosition() const {
        return position_;
    }
    /**
     * @brief Rotation (Euler) を取得する。(後方互換用)
     * @return 取得された Rotation
     */
    Irufemi::Vector3 GetRotation() const {
        return Irufemi::Math::ToEuler(rotation_);
    }
    /**
     * @brief Rotation (Quaternion) を取得する。
     * @return 取得された Rotation
     */
    const Irufemi::Quaternion& GetRotationQuat() const {
        return rotation_;
    }
    /**
     * @brief Scale を取得する。
     * @return 取得された Scale
     */
    const Irufemi::Vector3& GetScale() const {
        return scale_;
    }

    /**
     * @brief WorldMatrix を取得する。
     * @return 取得された WorldMatrix
     */
    const Irufemi::Matrix4x4& GetWorldMatrix() const {
        CheckAndComputeMatrix();
        return worldMatrix_;
    }
    /**
     * @brief LocalMatrix を取得する。
     * @return 取得された LocalMatrix
     */
    const Irufemi::Matrix4x4& GetLocalMatrix() const {
        CheckAndComputeMatrix();
        return localMatrix_;
    }

    // ワールド情報の遅延抽出 Getter
    /**
     * @brief WorldPosition を取得する。
     * @return 取得された WorldPosition
     */
    const Irufemi::Vector3& GetWorldPosition() const;
    /**
     * @brief WorldRotation (Euler) を取得する。(後方互換用)
     * @return 取得された WorldRotation
     */
    Irufemi::Vector3 GetWorldRotation() const;
    /**
     * @brief WorldRotation (Quaternion) を取得する。
     * @return 取得された WorldRotation
     */
    const Irufemi::Quaternion& GetWorldRotationQuat() const;
    /**
     * @brief WorldScale を取得する。
     * @return 取得された WorldScale
     */
    const Irufemi::Vector3& GetWorldScale() const;

    // ワールド方向ベクトルの抽出 (正規化済み)
    /**
     * @brief WorldRight を取得する。
     * @return 取得された WorldRight
     */
    Irufemi::Vector3 GetWorldRight() const;
    /**
     * @brief WorldUp を取得する。
     * @return 取得された WorldUp
     */
    Irufemi::Vector3 GetWorldUp() const;
    /**
     * @brief WorldForward を取得する。
     * @return 取得された WorldForward
     */
    Irufemi::Vector3 GetWorldForward() const;

    // --- Setters (Local) ---
    /**
     * @brief Position を設定する。
     * @param[in] position 設定する Position の値
     */
    void SetPosition(const Irufemi::Vector3& position);
    /**
     * @brief Rotation (Euler) を設定する。(後方互換用)
     * @param[in] rotation 設定する Rotation の値
     */
    void SetRotation(const Irufemi::Vector3& rotation);
    /**
     * @brief Rotation (Quaternion) を設定する。
     * @param[in] rotation 設定する Rotation の値
     */
    void SetRotationQuat(const Irufemi::Quaternion& rotation);
    /**
     * @brief Scale を設定する。
     * @param[in] scale 設定する Scale の値
     */
    void SetScale(const Irufemi::Vector3& scale);

    // --- Setters (World) ---
    /**
     * @brief ワールド座標を設定し、親の逆行列を用いてローカル座標に反映する。
     * @param[in] worldPosition 設定するワールド座標
     */
    void SetWorldPosition(const Irufemi::Vector3& worldPosition);
    /**
     * @brief ワールド回転を設定し、親の逆行列を用いてローカル回転に反映する。(後方互換用)
     * @param[in] worldRotation 設定するワールド回転 (Euler角)
     */
    void SetWorldRotation(const Irufemi::Vector3& worldRotation);
    /**
     * @brief ワールド回転を設定し、親の逆行列を用いてローカル回転に反映する。
     * @param[in] worldRotation 設定するワールド回転 (Quaternion)
     */
    void SetWorldRotationQuat(const Irufemi::Quaternion& worldRotation);
    /**
     * @brief ワールドスケールを設定し、親の逆スケールを用いてローカルスケールに反映する。
     * @param[in] worldScale 設定するワールドスケール
     */
    void SetWorldScale(const Irufemi::Vector3& worldScale);
    /**
     * @brief ワールド行列を直接設定し、ローカル成分 (Pos/Rot/Scale) に分解して反映する。
     * @param[in] worldMatrix 設定するワールド行列
     */
    void SetWorldMatrix(const Irufemi::Matrix4x4& worldMatrix);

    /**
     * @brief 次のフレームを待たずに、現在の状態から強制的に行列を即時再計算する。
     */
    void UpdateMatrixImmediate();

    /**
     * @brief ワールド行列の再計算を強制する（親が変更された時などに使用）
     */
    void MarkWorldDirty() {
        isWorldDirty_ = true;
    }

    /**
     * @brief ローカル行列の再計算をフラグ立てする（位置・回転・スケール変更時に使用）
     */
    void MarkLocalDirty() {
        isLocalDirty_ = true;
    }

    /**
     * @brief 現在のトランスフォームのバージョン番号を取得する（親が子の更新要否を判定するために使用）
     */
    uint64_t GetTransformVersion() const {
        return transformVersion_;
    }

    /**
     * @brief 子にとっての「親のワールド行列」を取得する（inheritScale_ を考慮）
     */
    Irufemi::Matrix4x4 GetParentMatrixForChild() const;

    /**
     * @brief 親のスケールを継承するかどうかを設定する。
     * @param[in] inheritScale 継承する場合は true
     */
    void SetInheritScale(bool inheritScale) {
        if (inheritScale_ != inheritScale) {
            inheritScale_ = inheritScale;
            MarkWorldDirty();
        }
    }

    /**
     * @brief 親のスケールを継承するかどうかを取得する。
     * @return 継承する場合は true
     */
    bool GetInheritScale() const {
        return inheritScale_;
    }

    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "TransformComponent";
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
     * @brief クローンを作成する (高速なディープコピー)
     */
    std::shared_ptr<Component> Clone() override;

private:
    // --- Local Irufemi::Transform Data ---
    Irufemi::Vector3 position_ = {0.0f, 0.0f, 0.0f};
    Irufemi::Quaternion rotation_ = {0.0f, 0.0f, 0.0f, 1.0f}; // Quaternion (x,y,z,w) = Identity
    Irufemi::Vector3 scale_ = {1.0f, 1.0f, 1.0f};
    bool inheritScale_ = true; // 親のスケールを継承するかどうか

    // --- Matrices ---
    mutable Irufemi::Matrix4x4 localMatrix_ = Irufemi::Math::MakeIdentity4x4();
    mutable Irufemi::Matrix4x4 worldMatrix_ = Irufemi::Math::MakeIdentity4x4();

    // --- World Irufemi::Transform Data (Lazy Evaluated) ---
    mutable Irufemi::Vector3 worldPosition_ = {0.0f, 0.0f, 0.0f};
    mutable Irufemi::Quaternion worldRotation_ = {0.0f, 0.0f, 0.0f, 1.0f};
    mutable Irufemi::Vector3 worldScale_ = {1.0f, 1.0f, 1.0f};

    // --- Flags & Versioning (Lazy Evaluation) ---
    mutable bool isLocalDirty_ = true;
    mutable bool isWorldDirty_ = true;
    mutable bool isWorldTransformExtracted_ = false;

    /**
     * @brief ワールド行列から座標・回転・スケールを再抽出する
     */
    void ExtractWorldTransform() const;

    // トランスフォームの状態が変化したことを追跡するためのバージョン番号
    mutable uint64_t transformVersion_ = 1;
    // 前回 ComputeMatrix したときの親のバージョン番号
    mutable uint64_t parentTransformVersionLastComputed_ = 0;
    mutable uint64_t parentInstanceIdLastComputed_ = 0;
};

} // namespace Irufemi
