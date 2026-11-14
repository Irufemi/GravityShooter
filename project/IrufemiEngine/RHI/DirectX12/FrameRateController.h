#pragma once

#include <chrono>

namespace Irufemi {

/**
 * @class FrameRateController
 * @brief FPS（フレームレート）を一定に保つための制御クラス
 * @details 1/60秒などの一定間隔でフレームを更新するようにスリープ処理を行います。
 */
class FrameRateController {
public:
    /**
     * @brief 初期化
     */
    void Initialize();

    /**
     * @brief フレームの更新（待機処理）
     */
    void Update();

private:
    /**
     * @brief 記録時間
     */
    std::chrono::steady_clock::time_point reference_;
};

} // namespace Irufemi
