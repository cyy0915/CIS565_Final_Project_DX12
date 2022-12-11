#include "DX12LibPCH.h"

#include <dx12lib/ComputeShader.h>

#include <dx12lib/Device.h>
#include <dx12lib/Helpers.h>
#include <dx12lib/RootSignature.h>

#include <dx12lib/d3dx12.h>

#include <ComputeShader.h>


using namespace dx12lib;

ComputeShader::ComputeShader(std::shared_ptr<Device> device, SDF sdfParm, int textureNum) : m_sdfParm(sdfParm), m_textureNum(textureNum)
{

    //Create root signature
    /*CD3DX12_DESCRIPTOR_RANGE1 srcMip(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0,
        D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);*/
    CD3DX12_DESCRIPTOR_RANGE1 resultUAV(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE1 textureSRV(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, textureNum, ComputeShaderRegisterT::textures);


    CD3DX12_ROOT_PARAMETER1 rootParameters[ComputeShaderParm::NumRootParameters];
    rootParameters[ComputeShaderParm::sdf].InitAsConstants(sizeof(SDFGPU) / 4, 0);
    rootParameters[ComputeShaderParm::bias].InitAsConstants(sizeof(DirectX::XMVECTOR) / 4, 1);
    rootParameters[ComputeShaderParm::SDFGrids].InitAsUnorderedAccessView(0);
    rootParameters[ComputeShaderParm::bvhNodes].InitAsShaderResourceView(ComputeShaderRegisterT::bvhNodes);
    rootParameters[ComputeShaderParm::mats].InitAsShaderResourceView(ComputeShaderRegisterT::mats);
    rootParameters[ComputeShaderParm::textures].InitAsDescriptorTable(1, &textureSRV);
    CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(ComputeShaderParm::NumRootParameters, rootParameters, 1, &linearClampSampler);
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
    m_Result = device->CreateStructuredBuffer(sdfParm.resolution.x * sdfParm.resolution.y * sdfParm.resolution.z, sizeof(SDFGrid));
}

//void ComputeShader::resize(int w, int h) {
//    m_Width = w;
//    m_Height = h;
//
//    m_ResultTexture->Resize(w, h);
//}

void ComputeShader::dispatch(CommandQueue& commandQueue, BVHTree& bvhTree, std::vector<std::shared_ptr<Material>> mats) {
    auto resolution = m_sdfParm.resolution;
    for (int i = 0; i < resolution.x; i+= 100) {
        for (int j = 0; j < resolution.y; j += 100) {
            for (int k = 0; k < resolution.z; k += 100) {
                auto  commandList = commandQueue.GetCommandList();

                commandList->SetPipelineState(m_PipelineState);
                commandList->SetComputeRootSignature(m_RootSignature);

                //传参
                commandList->SetCompute32BitConstants(ComputeShaderParm::sdf, m_sdfParm.getGPUData());

                if (i == 0 && j == 0 && k == 0) {
                    std::vector<BVHNodeGPU> bvhDatas;
                    bvhTree.getGPUData(bvhDatas);
                    m_bvhResource = commandList->CopyStructuredBuffer(bvhDatas);
                }
                commandList->SetShaderResourceView(ComputeShaderParm::bvhNodes, m_bvhResource);
                
                std::vector<MaterialSDF> matsDatas;
                int currTexId = 0;
                for (auto m : mats) {
                    MaterialSDF tmp;
                    std::shared_ptr<Texture> currTex = m->GetTexture(Material::TextureType::Diffuse);
                    if (currTex) {
                        commandList->SetShaderResourceView(ComputeShaderParm::textures, currTexId, currTex);
                        tmp.textureId = currTexId;
                        currTexId++;
                    }
                    else {
                        tmp.textureId = -1;
                        auto currColor = m->GetDiffuseColor();
                        tmp.color = glm::vec3(currColor.x, currColor.y, currColor.z);
                    }
                    matsDatas.push_back(tmp);
                }
                for (int i = currTexId; i < m_textureNum; i++) {
                    commandList->SetShaderResourceView(ComputeShaderParm::textures, i, mats[0]->GetTexture(Material::TextureType::Diffuse));
                }

                commandList->SetComputeDynamicStructuredBuffer(ComputeShaderParm::mats, matsDatas);
                commandList->SetUnorderedAccessView(ComputeShaderParm::SDFGrids, m_Result);

                commandList->SetCompute32BitConstants(ComputeShaderParm::bias, glm::vec4(i, j, k, 0));
                commandList->Dispatch(Math::DivideByMultiple(100, 8), Math::DivideByMultiple(100, 8), Math::DivideByMultiple(100, 8));

                commandQueue.ExecuteCommandList(commandList);
                commandQueue.Flush();
            }
        }
    }
}