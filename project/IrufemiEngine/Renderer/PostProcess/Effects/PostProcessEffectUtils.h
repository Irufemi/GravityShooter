#pragma once

#include "Core/System/IrufemiEngine.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include "Core/Math/MathFunction.h"
#include <d3d12.h>

namespace Irufemi {

inline void SetupOutputViewport(IrufemiEngine* engine, DirectXCommon* dxCommon, bool isBackBufferTarget,
                                const D3D12_VIEWPORT* explicitViewport, const D3D12_RECT* explicitScissor,
                                D3D12_VIEWPORT& outViewport, D3D12_RECT& outScissor) {
    if (explicitViewport && explicitScissor) {
        outViewport = *explicitViewport;
        outScissor = *explicitScissor;
        return;
    }

    if (isBackBufferTarget) {
        float clientW = dxCommon ? static_cast<float>(dxCommon->GetClientWidth()) : 1280.0f;
        float clientH = dxCommon ? static_cast<float>(dxCommon->GetClientHeight()) : 720.0f;
        float gameW = engine ? static_cast<float>(engine->GetGameResolutionWidth()) : 1280.0f;
        float gameH = engine ? static_cast<float>(engine->GetGameResolutionHeight()) : 720.0f;
        Irufemi::Math::LetterboxInfo view = Irufemi::Math::CalculateLetterbox(clientW, clientH, gameW, gameH);

        outViewport.Width = view.viewW;
        outViewport.Height = view.viewH;
        outViewport.TopLeftX = view.viewX;
        outViewport.TopLeftY = view.viewY;
        outScissor.left = static_cast<LONG>(outViewport.TopLeftX);
        outScissor.right = static_cast<LONG>(outViewport.TopLeftX + outViewport.Width);
        outScissor.top = static_cast<LONG>(outViewport.TopLeftY);
        outScissor.bottom = static_cast<LONG>(outViewport.TopLeftY + outViewport.Height);
    } else {
        outViewport.Width = static_cast<FLOAT>(engine ? engine->GetGameResolutionWidth() : 1280);
        outViewport.Height = static_cast<FLOAT>(engine ? engine->GetGameResolutionHeight() : 720);
        outViewport.TopLeftX = 0;
        outViewport.TopLeftY = 0;
        outScissor.left = 0;
        outScissor.right = engine ? engine->GetGameResolutionWidth() : 1280;
        outScissor.top = 0;
        outScissor.bottom = engine ? engine->GetGameResolutionHeight() : 720;
    }
    outViewport.MinDepth = 0.0f;
    outViewport.MaxDepth = 1.0f;
}

} // namespace Irufemi
