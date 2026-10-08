#pragma once

#include "Renderer/System/Core/BaseBatch.h"
#include "Renderer/Object/PrimitiveManager.h"

/**
 * @class PrimitiveBatch
 * @brief PrimitiveManager を利用した基本形状を描画する領域クラス
 */
class PrimitiveBatch : public BaseBatch {
public:
    PrimitiveBatch() = default;
    ~PrimitiveBatch() override = default;

    /**
     * @brief プリミティブ形状の領域を初期化する
     * @param type 生成する形状の種類
     * @param textureName 適用するテクスチャパス
     */
    void Initialize(Irufemi::PrimitiveType type, const std::string& textureName = "resources/uvChecker.png");

    /**
     * @brief リング形状専用の初期化
     */
    void InitializeRing(const RingParams& params, const std::string& textureName = "resources/uvChecker.png");

    /**
     * @brief バッチ描画で使用する PrimitiveManager を設定する
     * @param[in] manager 設定する PrimitiveManager のポインタ
     */
    static void SetPrimitiveManager(PrimitiveManager* manager) {
        primitiveManager_ = manager;
    }

    /**
     * @brief インスタンシングバッチ描画を実行する
     */
    void Draw() override;

protected:
    /**
     * @brief フラストゥムカリング用の外接球半径を取得する
     * @return 形状に応じた外接球半径
     */
    float GetBoundingSphereRadius() const override;

private:
    inline static PrimitiveManager* primitiveManager_ = nullptr;

    /**
     * @brief 定数バッファ等のマテリアルリソースを確保・初期化する
     */
    void EnsureMaterialResources();
    /**
     * @brief 共有テクスチャリソースのロードまたは更新を行う
     */
    void EnsureSharedTexture(const std::string& textureName);

private:
    Irufemi::PrimitiveType type_ = Irufemi::PrimitiveType::Sphere;
    bool isCustomPrimitive_ = false;            // リングなどの個別パラメータを使用するか
    PrimitiveResource customPrimitiveResource_; // カスタム用のリソース
    float boundingSphereRadius_ = 1.0f;         // カリング用外接球半径
};
