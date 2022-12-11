#include "DX12LibPCH.h"
#include <dx12lib/RayTrace.h>

#include <dx12lib/Device.h>
#include <dx12lib/Helpers.h>
#include <dx12lib/RootSignature.h>

#include <dx12lib/d3dx12.h>

#include <RayTrace.h>

using namespace dx12lib;

RayTrace::RayTrace(std::shared_ptr<Device> device, int width, int height) : m_Width(width), m_Height(height)
{
    CD3DX12_DESCRIPTOR_RANGE1 resultUAV(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE1 texturesSRV(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, RayTraceRegisterT::gbuffers);

    CD3DX12_ROOT_PARAMETER1 rootParameters[RayTraceParm::NumRootParameters];
    //rootParameters[RayTraceParm::camera].InitAsConstants(sizeof(CameraDataGPU) / 4, 0);
    rootParameters[RayTraceParm::camera].InitAsConstantBufferView(0);
    rootParameters[RayTraceParm::SDFParm].InitAsConstants(sizeof(SDFGPU) / 4, 1);
    rootParameters[RayTraceParm::renderParm].InitAsConstants(sizeof(RenderParm) / 4, 2);

    rootParameters[RayTraceParm::SDFGrids].InitAsShaderResourceView(RayTraceRegisterT::sdf);
    rootParameters[RayTraceParm::gbuffers].InitAsDescriptorTable(1, &texturesSRV);
    rootParameters[RayTraceParm::result].InitAsDescriptorTable(1, &resultUAV);
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(RayTraceParm::NumRootParameters, rootParameters);
    m_RootSignature = device->CreateRootSignature(rootSignatureDesc.Desc_1_1);

    // Create the PSO
    struct PipelineStateStream
    {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS             CS;
    } pipelineStateStream;
    pipelineStateStream.pRootSignature = m_RootSignature->GetD3D12RootSignature().Get();
    pipelineStateStream.CS = { g_RayTrace, sizeof(g_RayTrace) };
    m_PipelineState = device->CreatePipelineStateObject(pipelineStateStream);

    //Create result texture 
    auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, m_Width, m_Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    
    m_ResultTexture = device->CreateTexture(colorDesc);
}

void RayTrace::resize(int w, int h) {
    m_Width = w;
    m_Height = h;
    m_change = true;
    m_ResultTexture->Resize(w, h);
}

void RayTrace::dispatch(std::shared_ptr<CommandList> commandList, CameraDataGPU camera, bool change, 
    int depth, bool useSDF, glm::vec3 lightDir, bool screenTracing,
    SDF sdfParm, std::shared_ptr<StructuredBuffer> sdfGrids,
    std::shared_ptr<Texture> normal, std::shared_ptr<Texture> depthMatid, std::shared_ptr<Texture> color) {
    change = change || m_change;
    m_change = false;
    if (change) {
        m_iter = 0;
    }
    m_iter++;
    commandList->SetPipelineState(m_PipelineState);
    commandList->SetComputeRootSignature(m_RootSignature);

    commandList->SetComputeDynamicConstantBuffer(RayTraceParm::camera, camera);
    commandList->SetCompute32BitConstants(RayTraceParm::SDFParm, sdfParm.getGPUData());
    commandList->SetCompute32BitConstants(RayTraceParm::renderParm, RenderParm({m_iter, change, depth, useSDF, lightDir, screenTracing}));

    commandList->SetShaderResourceView(RayTraceParm::SDFGrids, sdfGrids);
    
    commandList->SetShaderResourceView(RayTraceParm::gbuffers, 0, normal);
    commandList->SetShaderResourceView(RayTraceParm::gbuffers, 1, depthMatid);
    commandList->SetShaderResourceView(RayTraceParm::gbuffers, 2, color);

    commandList->SetUnorderedAccessView(RayTraceParm::result, 0, m_ResultTexture, 0);

    commandList->Dispatch(Math::DivideByMultiple(m_Width, 8), Math::DivideByMultiple(m_Height, 8));
}