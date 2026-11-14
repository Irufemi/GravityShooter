#include "RHI/DirectX12/FrameRateController.h"
#include <thread>
#include <Windows.h>

namespace Irufemi {

/**
 * @brief 初期化
 * @details フレーム計測の基準時間を現在に設定します。
 */
void FrameRateController::Initialize() {
    reference_ = std::chrono::steady_clock::now();
}

/**
 * @brief フレームの更新（待機処理）
 * @details 前回のフレームから1/60秒（16.66ms）経過するまでスリープを行います。
 */
void FrameRateController::Update() {
    // 基準時間の更新（1/60秒 = 約16666マイクロ秒）
    const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));

    // 目標時間までハイブリッド・スリープで待機
    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

        // 目標時間を過ぎていればループを抜ける
        if (elapsed >= kMinTime) {
            break;
        }

        auto remaining = kMinTime - elapsed;

        // 残り時間が2ミリ秒 (2000マイクロ秒) 以上ある場合は、OSにスレッドを譲って省電力化
        // （timeBeginPeriod(1) を適用済みであっても、余裕を見て2msを閾値とする）
        if (remaining.count() >= 2000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
            // 残り2ミリ秒未満になったら、OSへのスリープ要求をやめ、
            // CPU命令 (PAUSE) による超高精度なスピンロック（ビジーウェイト）に移行
            YieldProcessor();
        }
    }

    // 基準時間を更新
    // ジッターの蓄積を防ぎ、安定したペーシングを維持するために
    // 現在時刻ではなく「本来の目標時刻」を次の基準とする
    auto nextReference = reference_ + kMinTime;
    auto nowAfter = std::chrono::steady_clock::now();

    // もし重い処理などで1フレーム以上（16ms超過）遅延した場合は、
    // 巻き返しによる倍速進行を防ぐために現在時刻でリセットする
    if (nowAfter > nextReference + std::chrono::milliseconds(16)) {
        reference_ = nowAfter;
    } else {
        reference_ = nextReference;
    }
}

} // namespace Irufemi
