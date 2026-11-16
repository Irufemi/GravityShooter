#pragma once

class IrufemiEngine;

namespace Irufemi {

/**
 * @class IEngineExtension
 * @brief エンジンのメインループに外部から処理を挿入するためのインターフェース。
 *        エディタや外部ツールはこのインターフェースを実装し、エンジンに登録する。
 */
class IEngineExtension {
public:
    virtual ~IEngineExtension() = default;

    /**
     * @brief エンジン初期化後に呼ばれる
     */
    virtual void OnInitialize(IrufemiEngine* engine) {}

    /**
     * @brief 毎フレームの更新処理（ゲームロジックの後）に呼ばれる
     */
    virtual void OnUpdate(float deltaTime) {}

    /**
     * @brief UI描画フェーズ（ImGui等の描画）に呼ばれる
     */
    virtual void OnDrawUI() {}

    /**
     * @brief 描画処理フェーズに呼ばれる（シーン描画の後など）
     */
    virtual void OnDraw() {}

    /**
     * @brief エンジン終了時に呼ばれる
     */
    virtual void OnFinalize() {}
};

} // namespace Irufemi
