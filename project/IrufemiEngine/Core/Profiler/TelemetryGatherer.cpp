#include "Core/Profiler/TelemetryGatherer.h"
#include "Core/Profiler/TelemetrySender.h"
#include "Core/System/IrufemiEngine.h"
#include "Core/System/ThreadPool.h"
#include "Renderer/System/ParticleGPU/GPUParticleManager.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"
#include "Core/Profiler/GpuProfiler.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Framework/Component/VirtualEntity/VirtualEntityManagerComponent.h"

using namespace Irufemi;

void TelemetryGatherer::RegisterMetric(const std::string& name, std::function<float()> fetcher) {
    metrics_.push_back({name, std::move(fetcher)});
}

void TelemetryGatherer::Initialize(IrufemiEngine* engine) {
    if (!engine) {
        return;
    }
    sender_ = engine->GetTelemetrySender();

    // ==========================================
    // System Metrics
    // ==========================================
    RegisterMetric("System/FPS", [engine]() { return engine->GetEmaFps(); });

    RegisterMetric("System/FrameTime_ms", [engine]() { return engine->GetRealDeltaTime() * 1000.0f; });

    RegisterMetric("System/CPU_Time_ms", [engine]() { return engine->GetPureCpuTimeMs(); });

    RegisterMetric("System/GPU_Time_ms", [engine]() {
        if (auto* dx = engine->GetDirectXCommon()) {
            if (auto* profiler = dx->GetGpuProfiler()) {
                return profiler->GetLastFrameGpuTimeMs();
            }
        }
        return 0.0f;
    });

    // ==========================================
    // Thread Pool Metrics
    // ==========================================
    RegisterMetric("System/ThreadPool_Active", [engine]() -> float {
        if (auto* tp = engine->GetThreadPool()) {
            return static_cast<float>(tp->GetActiveThreadCount());
        }
        return 0.0f;
    });

    RegisterMetric("System/ThreadPool_Queued", [engine]() -> float {
        if (auto* tp = engine->GetThreadPool()) {
            return static_cast<float>(tp->GetQueuedTaskCount());
        }
        return 0.0f;
    });

    // ==========================================
    // Entity Metrics
    // ==========================================
    RegisterMetric("System/VirtualEntities_Active", []() -> float {
        return static_cast<float>(VirtualEntityManagerComponent::GetTotalActiveVirtualInstances());
    });

    // ==========================================
    // Particle Metrics
    // ==========================================
    RegisterMetric("GPU_Particle/Active_Systems", [engine]() -> float {
        if (auto* gpm = engine->GetGPUParticleManager()) {
            return static_cast<float>(gpm->GetActiveSystemCount());
        }
        return 0.0f;
    });

    RegisterMetric("GPU_Particle/Total_Emitters", [engine]() -> float {
        if (auto* gpm = engine->GetGPUParticleManager()) {
            return static_cast<float>(gpm->GetTotalEmittersUsed());
        }
        return 0.0f;
    });

    RegisterMetric("Voxel_Particle/Active_Systems", [engine]() -> float {
        if (auto* vpm = engine->GetVoxelParticleManager()) {
            return static_cast<float>(vpm->GetActiveSystemCount());
        }
        return 0.0f;
    });

    RegisterMetric("Voxel_Particle/Total_Emitters", [engine]() -> float {
        if (auto* vpm = engine->GetVoxelParticleManager()) {
            return static_cast<float>(vpm->GetTotalEmittersUsed());
        }
        return 0.0f;
    });
}

void TelemetryGatherer::DispatchAll() {
    if (!sender_) {
        return;
    }

    for (const auto& metric : metrics_) {
        sender_->SetMetric(metric.name, metric.fetcher());
    }

    // 最後にフレームの終了を通知
    sender_->OnFrameEnd();
}
