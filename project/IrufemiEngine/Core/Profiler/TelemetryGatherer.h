#pragma once

#include <string>
#include <vector>
#include <functional>

class IrufemiEngine;

namespace Irufemi {

class TelemetrySender;

/**
 * @brief 外部プロファイラ（Telemetry）へ送信する数値を一括で収集・登録するマニフェストクラス
 */
class TelemetryGatherer {
public:
    TelemetryGatherer() = default;
    ~TelemetryGatherer() = default;

    /**
     * @brief エンジンの各システムから送るプロファイル項目をバインド（登録）する
     * @param engine IrufemiEngine本体のポインタ
     */
    void Initialize(::IrufemiEngine* engine);

    /**
     * @brief 登録されたすべてのラムダ式を評価し、TelemetrySenderに一括送信する
     */
    void DispatchAll();

private:
    /**
     * @brief プロファイル項目のバインディング構造体
     */
    struct MetricBinding {
        std::string name;
        std::function<float()> fetcher;
    };

    /**
     * @brief 単一のメトリクス取得処理を登録するヘルパー
     */
    void RegisterMetric(const std::string& name, std::function<float()> fetcher);

    std::vector<MetricBinding> metrics_;
    TelemetrySender* sender_ = nullptr;
};

} // namespace Irufemi
