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
    rootParameters[RayTraceParm::geomsNum].InitAsConstants(1, 2);
    rootParameters[RayTraceParm::randNum].InitAsConstants(1, 3);
    rootParameters[RayTraceParm::iterNum].InitAsConstants(1, 4);

    rootParameters[RayTraceParm::geoms].InitAsShaderResourceView(RayTraceRegisterT::geoms);
    rootParameters[RayTraceParm::SDFGrids].InitAsShaderResourceView(RayTraceRegisterT::sdf);
    //Radiance Cache
    rootParameters[RayTraceParm::RadianceCacheParam].InitAsShaderResourceView( RayTraceRegisterT::radianceCache );

    rootParameters[RayTraceParm::materials].InitAsShaderResourceView(RayTraceRegisterT::materials);
    rootParameters[RayTraceParm::gbuffers].InitAsDescriptorTable(1, &texturesSRV);
    rootParameters[RayTraceParm::result].InitAsDescriptorTable(1, &resultUAV);
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(RayTraceParm::NumRootParameters, rootParameters);
    m_RootSignature = device->CreateRootSignature(rootSignatureDesc.Desc_1_1);
   //
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
    m_radianceCacheBuffer = device->CreateStructuredBuffer(m_Width*m_Height,sizeof(radianceCache));
    m_ResultTexture = device->CreateTexture(colorDesc);
}

//void RayTrace::resize(int w, int h) {
//    m_Width = w;
//    m_Height = h;
//
//    m_ResultTexture->Resize(w, h);
//}

void RayTrace::dispatch(std::shared_ptr<CommandList> commandList, std::vector<Material1> mats, CameraDataGPU camera, 
    SDF sdfParm, std::vector<Geom> geoms, std::shared_ptr<StructuredBuffer> sdfGrids,
    std::shared_ptr<Texture> normal, std::shared_ptr<Texture> depthMatid, std::shared_ptr<Texture> color,std::shared_ptr<StructuredBuffer> radianceCache) {
    m_iter++;
    commandList->SetPipelineState(m_PipelineState);
    commandList->SetComputeRootSignature(m_RootSignature);
    commandList->SetComputeDynamicConstantBuffer(RayTraceParm::camera, camera);
    commandList->SetCompute32BitConstants(RayTraceParm::SDFParm, sdfParm.getGPUData());
    int geomNum = geoms.size();
    commandList->SetCompute32BitConstants(RayTraceParm::geomsNum, geomNum);
    float randomNum = rand() / (float)RAND_MAX;
    commandList->SetCompute32BitConstants(RayTraceParm::randNum, randomNum);
    commandList->SetCompute32BitConstants(RayTraceParm::iterNum, m_iter);
    std::vector<GeomGPU> geomsGPU;
    for (auto g : geoms) {
        geomsGPU.push_back(g.getGPUData());
    }
    commandList->SetComputeDynamicStructuredBuffer(RayTraceParm::geoms, geomsGPU);
    commandList->SetShaderResourceView(RayTraceParm::SDFGrids, sdfGrids);
    //Radiance Cache
    commandList->SetShaderResourceView( RayTraceParm::RadianceCacheParam, radianceCache );

    std::vector<MaterialGPU> matsData;
    for (auto m : mats) {
        matsData.push_back(m.getGPUData());
    }
    commandList->SetComputeDynamicStructuredBuffer(RayTraceParm::materials, matsData);
    commandList->SetShaderResourceView(RayTraceParm::gbuffers, 0, normal);
    commandList->SetShaderResourceView(RayTraceParm::gbuffers, 1, depthMatid);
    commandList->SetShaderResourceView(RayTraceParm::gbuffers, 2, color);

    commandList->SetUnorderedAccessView(RayTraceParm::result, 0, m_ResultTexture, 0);

    commandList->Dispatch(Math::DivideByMultiple(m_Width, 8), Math::DivideByMultiple(m_Height, 8));
}