#include "Core/Utility/ErrorUtility.h"
#include "RHI/DirectX12/DXRootSignatureManager.h"
#include "Core/Utility/Log.h"
#include <cassert>
#include <iostream>

void DXRootSignatureManager::Initialize(ID3D12Device* device) {
    // --- 通常描画用 RootSignature ---
    {
        // --- ディスクリプタレンジの定義 (Version 1.1) ---
        // Bindless 用の無制限配列 (space1 ~ space6)
        D3D12_DESCRIPTOR_RANGE1 bindlessRanges[6] = {};

        // space1: Texture2D (Material等)
        bindlessRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        bindlessRanges[0].NumDescriptors = -1;    // Unbounded
        bindlessRanges[0].BaseShaderRegister = 0; // t0
        bindlessRanges[0].RegisterSpace = 1;
        bindlessRanges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
        bindlessRanges[0].OffsetInDescriptorsFromTableStart = 0; // Heapの先頭から

        // space2: TextureCube (EnvMap)
        bindlessRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        bindlessRanges[1].NumDescriptors = -1;
        bindlessRanges[1].BaseShaderRegister = 0; // t0
        bindlessRanges[1].RegisterSpace = 2;
        bindlessRanges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
        bindlessRanges[1].OffsetInDescriptorsFromTableStart = 0;

        // space3: Texture2D<float> (ShadowMap/DepthMap)
        bindlessRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        bindlessRanges[2].NumDescriptors = -1;
        bindlessRanges[2].BaseShaderRegister = 0; // t0
        bindlessRanges[2].RegisterSpace = 3;
        bindlessRanges[2].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
        bindlessRanges[2].OffsetInDescriptorsFromTableStart = 0;

        // space4: StructuredBuffer<PointLight>
        bindlessRanges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        bindlessRanges[3].NumDescriptors = -1;
        bindlessRanges[3].BaseShaderRegister = 0; // t0
        bindlessRanges[3].RegisterSpace = 4;
        bindlessRanges[3].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
        bindlessRanges[3].OffsetInDescriptorsFromTableStart = 0;

        // space5: StructuredBuffer<SpotLight>
        bindlessRanges[4].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        bindlessRanges[4].NumDescriptors = -1;
        bindlessRanges[4].BaseShaderRegister = 0; // t0
        bindlessRanges[4].RegisterSpace = 5;
        bindlessRanges[4].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
        bindlessRanges[4].OffsetInDescriptorsFromTableStart = 0;

        // space6: StructuredBuffer<AreaLight>
        bindlessRanges[5].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        bindlessRanges[5].NumDescriptors = -1;
        bindlessRanges[5].BaseShaderRegister = 0; // t0
        bindlessRanges[5].RegisterSpace = 6;
        bindlessRanges[5].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
        bindlessRanges[5].OffsetInDescriptorsFromTableStart = 0;

        // 既存の空間 (space0) のレガシーレンジ
        D3D12_DESCRIPTOR_RANGE1 rangeInstancing[1] = {};
        rangeInstancing[0].BaseShaderRegister = 0; // t0
        rangeInstancing[0].NumDescriptors = 1;
        rangeInstancing[0].RegisterSpace = 0;
        rangeInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeInstancing[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        rangeInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE1 rangeLine[1] = {};
        rangeLine[0].BaseShaderRegister = 1; // t1
        rangeLine[0].NumDescriptors = 1;
        rangeLine[0].RegisterSpace = 0;
        rangeLine[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeLine[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        rangeLine[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE1 rangeLights[3] = {};
        rangeLights[0].BaseShaderRegister = 2; // t2
        rangeLights[0].NumDescriptors = 1;
        rangeLights[0].RegisterSpace = 0;
        rangeLights[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeLights[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        rangeLights[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        rangeLights[1].BaseShaderRegister = 3; // t3
        rangeLights[1].NumDescriptors = 1;
        rangeLights[1].RegisterSpace = 0;
        rangeLights[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeLights[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        rangeLights[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        rangeLights[2].BaseShaderRegister = 4; // t4
        rangeLights[2].NumDescriptors = 1;
        rangeLights[2].RegisterSpace = 0;
        rangeLights[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeLights[2].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        rangeLights[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE1 rangeShadowMap[1] = {};
        rangeShadowMap[0].BaseShaderRegister = 5; // t5
        rangeShadowMap[0].NumDescriptors = 1;
        rangeShadowMap[0].RegisterSpace = 0;
        rangeShadowMap[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeShadowMap[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        rangeShadowMap[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE1 rangeDepthMap[1] = {};
        rangeDepthMap[0].BaseShaderRegister = 6; // t6
        rangeDepthMap[0].NumDescriptors = 1;
        rangeDepthMap[0].RegisterSpace = 0;
        rangeDepthMap[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        rangeDepthMap[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        rangeDepthMap[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // --- ルートパラメータの定義 (Version 1.1) ---
        D3D12_ROOT_PARAMETER1 rootParameters[12] = {};

        // Slot 0: Material (b0, PS)
        rootParameters[(UINT)RootSlot::Material].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[(UINT)RootSlot::Material].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[(UINT)RootSlot::Material].Descriptor.ShaderRegister = 0;
        rootParameters[(UINT)RootSlot::Material].Descriptor.RegisterSpace = 0;
        rootParameters[(UINT)RootSlot::Material].Descriptor.Flags =
            D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

        // Slot 1: Irufemi::Transform (b0, VS)
        rootParameters[(UINT)RootSlot::Transform].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[(UINT)RootSlot::Transform].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[(UINT)RootSlot::Transform].Descriptor.ShaderRegister = 0;
        rootParameters[(UINT)RootSlot::Transform].Descriptor.RegisterSpace = 0;
        rootParameters[(UINT)RootSlot::Transform].Descriptor.Flags =
            D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

        // Slot 2: BindlessSRV (t0, space1-6, ALL)
        rootParameters[(UINT)RootSlot::BindlessSRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[(UINT)RootSlot::BindlessSRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[(UINT)RootSlot::BindlessSRV].DescriptorTable.pDescriptorRanges = bindlessRanges;
        rootParameters[(UINT)RootSlot::BindlessSRV].DescriptorTable.NumDescriptorRanges = _countof(bindlessRanges);

        // Slot 3: LightCommon (b1, ALL)
        rootParameters[(UINT)RootSlot::LightCommon].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[(UINT)RootSlot::LightCommon].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[(UINT)RootSlot::LightCommon].Descriptor.ShaderRegister = 1;
        rootParameters[(UINT)RootSlot::LightCommon].Descriptor.RegisterSpace = 0;
        rootParameters[(UINT)RootSlot::LightCommon].Descriptor.Flags =
            D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

        // Slot 4: Instancing (t0, VS)
        rootParameters[(UINT)RootSlot::Instancing].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[(UINT)RootSlot::Instancing].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[(UINT)RootSlot::Instancing].DescriptorTable.pDescriptorRanges = rangeInstancing;
        rootParameters[(UINT)RootSlot::Instancing].DescriptorTable.NumDescriptorRanges = _countof(rangeInstancing);

        // Slot 5: Camera (b2, ALL)
        rootParameters[(UINT)RootSlot::Camera].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[(UINT)RootSlot::Camera].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[(UINT)RootSlot::Camera].Descriptor.ShaderRegister = 2;
        rootParameters[(UINT)RootSlot::Camera].Descriptor.RegisterSpace = 0;
        rootParameters[(UINT)RootSlot::Camera].Descriptor.Flags =
            D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

        // Slot 6: Special (b6, ALL)
        rootParameters[(UINT)RootSlot::Special].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[(UINT)RootSlot::Special].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters[(UINT)RootSlot::Special].Descriptor.ShaderRegister = 6;
        rootParameters[(UINT)RootSlot::Special].Descriptor.RegisterSpace = 0;
        rootParameters[(UINT)RootSlot::Special].Descriptor.Flags =
            D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

        // Slot 7: LineInstancing (t1, VS)
        rootParameters[(UINT)RootSlot::LineInstancing].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[(UINT)RootSlot::LineInstancing].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        rootParameters[(UINT)RootSlot::LineInstancing].DescriptorTable.pDescriptorRanges = rangeLine;
        rootParameters[(UINT)RootSlot::LineInstancing].DescriptorTable.NumDescriptorRanges = _countof(rangeLine);

        // Slot 8: Lights (t2, t3, t4, PS)
        rootParameters[(UINT)RootSlot::Lights].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[(UINT)RootSlot::Lights].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[(UINT)RootSlot::Lights].DescriptorTable.pDescriptorRanges = rangeLights;
        rootParameters[(UINT)RootSlot::Lights].DescriptorTable.NumDescriptorRanges = _countof(rangeLights);

        // Slot 9: ShadowMap (t5, PS)
        rootParameters[(UINT)RootSlot::ShadowMap].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[(UINT)RootSlot::ShadowMap].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[(UINT)RootSlot::ShadowMap].DescriptorTable.pDescriptorRanges = rangeShadowMap;
        rootParameters[(UINT)RootSlot::ShadowMap].DescriptorTable.NumDescriptorRanges = _countof(rangeShadowMap);

        // Slot 10: DepthMap (t6, PS)
        rootParameters[(UINT)RootSlot::DepthMap].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[(UINT)RootSlot::DepthMap].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[(UINT)RootSlot::DepthMap].DescriptorTable.pDescriptorRanges = rangeDepthMap;
        rootParameters[(UINT)RootSlot::DepthMap].DescriptorTable.NumDescriptorRanges = _countof(rangeDepthMap);

        // Slot 11: CustomEffectParams (b3, PS)
        rootParameters[(UINT)RootSlot::CustomEffectParams].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParameters[(UINT)RootSlot::CustomEffectParams].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[(UINT)RootSlot::CustomEffectParams].Descriptor.ShaderRegister = 3;
        rootParameters[(UINT)RootSlot::CustomEffectParams].Descriptor.RegisterSpace = 0;
        rootParameters[(UINT)RootSlot::CustomEffectParams].Descriptor.Flags =
            D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

        D3D12_STATIC_SAMPLER_DESC staticSamplers[5] = {};
        staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
        staticSamplers[0].ShaderRegister = 0;
        staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
        staticSamplers[1].ShaderRegister = 1;
        staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        staticSamplers[2].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        staticSamplers[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        staticSamplers[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        staticSamplers[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        staticSamplers[2].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        staticSamplers[2].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        staticSamplers[2].MaxLOD = D3D12_FLOAT32_MAX;
        staticSamplers[2].ShaderRegister = 2; // s2
        staticSamplers[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        staticSamplers[3].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSamplers[3].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[3].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[3].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[3].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        staticSamplers[3].MaxLOD = D3D12_FLOAT32_MAX;
        staticSamplers[3].ShaderRegister = 3; // s3
        staticSamplers[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // s4: UはWRAP、VはCLAMP (横スクロール対応等)
        staticSamplers[4].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        staticSamplers[4].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[4].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        staticSamplers[4].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        staticSamplers[4].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        staticSamplers[4].MaxLOD = D3D12_FLOAT32_MAX;
        staticSamplers[4].ShaderRegister = 4; // s4
        staticSamplers[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rsDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        rsDesc.Desc_1_1.pParameters = rootParameters;
        rsDesc.Desc_1_1.NumParameters = _countof(rootParameters);
        rsDesc.Desc_1_1.pStaticSamplers = staticSamplers;
        rsDesc.Desc_1_1.NumStaticSamplers = _countof(staticSamplers);

        Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

        // D3D12SerializeVersionedRootSignature が古いSDKでない場合のチェック
        HRESULT hr =
            D3D12SerializeVersionedRootSignature(&rsDesc, signatureBlob.GetAddressOf(), errorBlob.GetAddressOf());

        if (FAILED(hr)) {
            Log::OutPutLog(std::cerr, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
            IRUFEMI_ASSERT(false);
        }
        hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
                                         IID_PPV_ARGS(graphicsRootSignature_.GetAddressOf()));
        ASSERT_IF_FAILED(hr);
    }

    // --- Compute Shader用 RootSignature ---
    {
        D3D12_DESCRIPTOR_RANGE srvRanges[3];
        srvRanges[0] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}; // t0
        srvRanges[1] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}; // t1
        srvRanges[2] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}; // t2

        D3D12_DESCRIPTOR_RANGE uavRanges[4];
        uavRanges[0] = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}; // u0
        uavRanges[1] = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}; // u1
        uavRanges[2] = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}; // u2
        uavRanges[3] = {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}; // u3

        D3D12_ROOT_PARAMETER computeRootParameters[11] = {};
        computeRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[0].DescriptorTable.pDescriptorRanges = &srvRanges[0];
        computeRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

        computeRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[1].DescriptorTable.pDescriptorRanges = &srvRanges[1];
        computeRootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

        // t2 (Influences / etc)
        computeRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[2].DescriptorTable.pDescriptorRanges = &srvRanges[2];
        computeRootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

        computeRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[3].DescriptorTable.pDescriptorRanges = &uavRanges[0];
        computeRootParameters[3].DescriptorTable.NumDescriptorRanges = 1;

        computeRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        computeRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[4].Descriptor.ShaderRegister = 0; // b0

        computeRootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        computeRootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[5].Descriptor.ShaderRegister = 1; // b1

        computeRootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[6].DescriptorTable.pDescriptorRanges = &uavRanges[1];
        computeRootParameters[6].DescriptorTable.NumDescriptorRanges = 1;

        computeRootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[7].DescriptorTable.pDescriptorRanges = &uavRanges[2];
        computeRootParameters[7].DescriptorTable.NumDescriptorRanges = 1;

        computeRootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        computeRootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[8].DescriptorTable.pDescriptorRanges = &uavRanges[3];
        computeRootParameters[8].DescriptorTable.NumDescriptorRanges = 1;

        computeRootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        computeRootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[9].Constants.ShaderRegister = 2; // b2
        computeRootParameters[9].Constants.Num32BitValues = 2; // k, j
        computeRootParameters[9].Constants.RegisterSpace = 0;

        computeRootParameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        computeRootParameters[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        computeRootParameters[10].Descriptor.ShaderRegister = 3; // t3
        computeRootParameters[10].Descriptor.RegisterSpace = 0;

        D3D12_ROOT_SIGNATURE_DESC computeRSDesc{};
        computeRSDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        computeRSDesc.pParameters = computeRootParameters;
        computeRSDesc.NumParameters = _countof(computeRootParameters);

        Microsoft::WRL::ComPtr<ID3DBlob> computeSignatureBlob = nullptr;
        Microsoft::WRL::ComPtr<ID3DBlob> computeErrorBlob = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(&computeRSDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                                 computeSignatureBlob.GetAddressOf(), computeErrorBlob.GetAddressOf());
        if (FAILED(hr)) {
            Log::OutPutLog(std::cerr, reinterpret_cast<char*>(computeErrorBlob->GetBufferPointer()));
            IRUFEMI_ASSERT(false);
        }
        hr = device->CreateRootSignature(0, computeSignatureBlob->GetBufferPointer(),
                                         computeSignatureBlob->GetBufferSize(),
                                         IID_PPV_ARGS(computeRootSignature_.GetAddressOf()));
        ASSERT_IF_FAILED(hr);
    }
}

void DXRootSignatureManager::Finalize() {
    graphicsRootSignature_.Reset();
    computeRootSignature_.Reset();
}
