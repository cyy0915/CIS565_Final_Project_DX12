#include "DX12LibPCH.h"

#include <dx12lib/ComputeShader.h>

#include <dx12lib/Device.h>
#include <dx12lib/Helpers.h>
#include <dx12lib/RootSignature.h>

#include <dx12lib/d3dx12.h>

#include <ComputeShader.h>


using namespace dx12lib;

ComputeShader::ComputeShader(std::shared_ptr<Device> device, int w, int h) : m_Width(w), m_Height(h)
{

    //Create root signature
    /*CD3DX12_DESCRIPTOR_RANGE1 srcMip(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0,
        D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);*/
    CD3DX12_DESCRIPTOR_RANGE1 resultUAV(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParameters[ComputeShaderParm::NumRootParameters];
    rootParameters[ComputeShaderParm::Parm1].InitAsConstants(sizeof(ConstantBuffer) / 4, 0);
    rootParameters[ComputeShaderParm::Result].InitAsDescriptorTable(1, &resultUAV);
    //rootParameters[GenerateMips::SrcMip].InitAsDescriptorTable(1, &srcMip);

    CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(ComputeShaderParm::NumRootParameters, rootParameters, 1,
        &linearClampSampler);

    m_RootSignature = device->CreateRootSignature(rootSignatureDesc.Desc_1_1);


    // Create the PSO
    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS             CS;
    } pipelineStateStream;

    pipelineStateStream.pRootSignature = m_RootSignature->GetD3D12RootSignature().Get();
    pipelineStateStream.CS = { g_ComputeShader, sizeof(g_ComputeShader) };

    m_PipelineState = device->CreatePipelineStateObject(pipelineStateStream);


    //Create result texture 
    //格式可以改为不同格式
    auto desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, m_Width, m_Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_ResultTexture = device->CreateTexture(desc);
}

void ComputeShader::resize(int w, int h) {
    m_Width = w;
    m_Height = h;
    m_ResultTexture->Resize(w, h);
}

void ComputeShader::dispatch(std::shared_ptr<CommandList> commandList, DirectX::XMFLOAT4 color) {
    commandList->SetPipelineState(m_PipelineState);
    commandList->SetComputeRootSignature(m_RootSignature);

    //传参
    commandList->SetCompute32BitConstants(ComputeShaderParm::Parm1, ConstantBuffer({color}));
    commandList->SetUnorderedAccessView(ComputeShaderParm::Result, 0, m_ResultTexture, 0);

    Math::DivideByMultiple(m_Width, 8);
    commandList->Dispatch(Math::DivideByMultiple(m_Width, 8), Math::DivideByMultiple(m_Height, 8));
}