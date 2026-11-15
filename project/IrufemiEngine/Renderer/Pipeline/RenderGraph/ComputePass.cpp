#include "Renderer/Pipeline/RenderGraph/ComputePass.h"
#include "Renderer/DrawManager.h"
#include "Renderer/Data/RenderContext.h"

using namespace Irufemi;

void ComputePass::Setup(RenderGraphBuilder& builder, const RenderContext& rc) {
    if (rc.drawManager) {
        for (auto* task : rc.drawManager->GetComputeTasks()) {
            task->Setup(builder);
        }
    }
}

void ComputePass::Execute(const RenderContext& rc) {
    if (rc.drawManager) {
        // 登録されているコンピュートタスクを一斉にディスパッチし、必要に応じてUAVバリアを発行する
        rc.drawManager->ExecuteComputePasses();
    }
}
