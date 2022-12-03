#include "DX12LibPCH.h"

#include <dx12lib/ComputeShader.h>

#include <dx12lib/Device.h>
#include <dx12lib/Helpers.h>
#include <dx12lib/RootSignature.h>

#include <dx12lib/d3dx12.h>

#include <ComputeShader.h>


using namespace dx12lib;

ComputeShader::ComputeShader(std::shared_ptr<Device> device, glm::ivec3 resolution) : m_Resolution(resolution)
{

    //Create root signature
    /*CD3DX12_DESCRIPTOR_RANGE1 srcMip(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0,
        D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);*/
    CD3DX12_DESCRIPTOR_RANGE1 resultUAV(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParameters[ComputeShaderParm::NumRootParameters];
    rootParameters[ComputeShaderParm::sdf].InitAsConstants(sizeof(SDFGPU) / 4, 0);
    rootParameters[ComputeShaderParm::SDFGrids].InitAsUnorderedAccessView(0);
    rootParameters[ComputeShaderParm::bvhNodes].InitAsShaderResourceView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[ComputeShaderParm::geoms].InitAsShaderResourceView(1);

    //rootParameters[GenerateMips::SrcMip].InitAsDescriptorTable(1, &srcMip);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(ComputeShaderParm::NumRootParameters, rootParameters);
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
    m_Result = device->CreateStructuredBuffer(resolution.x * resolution.y * resolution.z, sizeof(SDFGrid));
}

//void ComputeShader::resize(int w, int h) {
//    m_Width = w;
//    m_Height = h;
//
//    m_ResultTexture->Resize(w, h);
//}

void ComputeShader::dispatch(std::shared_ptr<CommandList> commandList, SDF sdf, BVHTree& bvhTree, std::vector<Geom> geoms) {
    commandList->SetPipelineState(m_PipelineState);
    commandList->SetComputeRootSignature(m_RootSignature);

    //传参
    commandList->SetCompute32BitConstants(ComputeShaderParm::sdf, sdf.getGPUData());
    std::vector<BVHNodeGPU> bvhDatas;
    bvhTree.getGPUData(bvhDatas);
    commandList->SetComputeDynamicStructuredBuffer(ComputeShaderParm::bvhNodes, bvhDatas);
    std::vector<GeomGPU> geomDatas;
    for (auto g : geoms) {
        geomDatas.push_back(g.getGPUData());
    }
    commandList->SetComputeDynamicStructuredBuffer(ComputeShaderParm::geoms, geomDatas);
    commandList->SetUnorderedAccessView(ComputeShaderParm::SDFGrids, m_Result);

    commandList->Dispatch(Math::DivideByMultiple(m_Resolution.x, 8), Math::DivideByMultiple(m_Resolution.y, 8), Math::DivideByMultiple(m_Resolution.z, 8));
}