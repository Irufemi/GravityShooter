#pragma once

#include "Renderer/Pipeline/RenderGraph/IRenderPass.h"
#include <functional>
#include <string>
#include <utility>

/**
 * @class LambdaRenderPass
 * @brief ラムダ式で Setup と Execute をインライン記述できる軽量 RenderPass
 * @details Unreal Engine RDG の AddPass と同様の柔軟なパス定義を実現します。
 */
class LambdaRenderPass : public IRenderPass {
public:
    using SetupFunc = std::function<void(RenderGraphBuilder&, const Irufemi::RenderContext&)>;
    using ExecuteFunc = std::function<void(const Irufemi::RenderContext&)>;

    LambdaRenderPass(std::string name, SetupFunc setup, ExecuteFunc execute)
        : name_(std::move(name)), setup_(std::move(setup)), execute_(std::move(execute)) {}

    ~LambdaRenderPass() override = default;

    void Setup(RenderGraphBuilder& builder, const Irufemi::RenderContext& rc) override {
        if (setup_) {
            setup_(builder, rc);
        }
    }

    void Execute(const Irufemi::RenderContext& rc) override {
        if (execute_) {
            execute_(rc);
        }
    }

    const std::string& GetName() const { return name_; }

private:
    std::string name_;
    SetupFunc setup_;
    ExecuteFunc execute_;
};
